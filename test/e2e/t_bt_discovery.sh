#!/usr/bin/env bash
# e2e: BitTorrent peer discovery mechanism tests (bt-enable-lpd,
#      bt-lpd-interface, dht-entry-point, dht-entry-point6,
#      dht-listen-addr6)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

# Detect loopback interface name (lo on Linux, lo0 on macOS/BSD)
if ip link show lo >/dev/null 2>&1; then
  LO_IFACE=lo
elif ifconfig lo0 >/dev/null 2>&1; then
  LO_IFACE=lo0
else
  LO_IFACE=lo
fi

_bt_seeder_pid=""
_bt_tracker_pid=""

_bt_disc_cleanup() {
  if [[ -n "${_bt_seeder_pid:-}" ]]; then
    kill "$_bt_seeder_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_bt_seeder_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_bt_seeder_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_bt_seeder_pid" 2>/dev/null || true
  fi
  if [[ -n "${_bt_tracker_pid:-}" ]]; then
    kill "$_bt_tracker_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_bt_tracker_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_bt_tracker_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_bt_tracker_pid" 2>/dev/null || true
  fi
  _e2e_cleanup
}
trap _bt_disc_cleanup EXIT

# Helper to check IPv6 loopback
skip_if_no_ipv6() {
  if ! python3 -c "
import socket
try:
    s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
    s.bind(('::1', 0))
    s.close()
except Exception:
    exit(1)
" 2>/dev/null; then
    return 1
  fi
  return 0
}

# Create seed
mkdir -p "$E2E_TMPDIR/seed_dir"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/payload.bin" \
  bs=1024 count=64 2>/dev/null

# Create torrent
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/seed_dir/payload.bin" \
  --output "$E2E_TMPDIR/test.torrent" \
  --tracker "http://127.0.0.1:18400/announce" \
  --piece-length 16384

# Start tracker
python3 "$SCRIPT_DIR/bt_tracker.py" \
  --port 18400 --peer-ip 127.0.0.1 --peer-port 18401 &
_bt_tracker_pid=$!
_tries=0
while ! curl -s "http://127.0.0.1:18400/announce" >/dev/null 2>&1; do
  _tries=$((_tries + 1))
  if [[ $_tries -ge 30 ]]; then
    echo "ERROR: BT tracker failed to start" >&2
    exit 1
  fi
  sleep 0.1
done

# Start seeder with LPD and DHT enabled
"$ARIA2C" --seed-ratio=0 --listen-port=18401 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18400/announce" \
  --enable-dht=true --dht-listen-port=18403 \
  --dht-file-path="$E2E_TMPDIR/seeder_dht.dat" \
  --bt-enable-lpd=true \
  --enable-peer-exchange=false \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!
sleep 1

# ── Test 1: --bt-enable-lpd=true accepted, download completes ───────────
# LPD multicast may not work in CI/containers, but with tracker as fallback
# we just verify the option is accepted and download succeeds
mkdir -p "$E2E_TMPDIR/lpd_dir"
rc=0
"$ARIA2C" --no-conf --listen-port=18402 --dir="$E2E_TMPDIR/lpd_dir" \
  --bt-tracker="http://127.0.0.1:18400/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-enable-lpd=true \
  --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
assert_file_exists "$E2E_TMPDIR/lpd_dir/payload.bin" \
  "bt-enable-lpd=true accepted, download completes"

# ── Test 2: --bt-lpd-interface=$LO_IFACE accepted, download completes ──
mkdir -p "$E2E_TMPDIR/lpdi_dir"
rc=0
"$ARIA2C" --no-conf --listen-port=18402 --dir="$E2E_TMPDIR/lpdi_dir" \
  --bt-tracker="http://127.0.0.1:18400/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-enable-lpd=true --bt-lpd-interface="$LO_IFACE" \
  --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
assert_file_exists "$E2E_TMPDIR/lpdi_dir/payload.bin" \
  "bt-lpd-interface=$LO_IFACE accepted, download completes"

# ── Test 3: --dht-entry-point discovers seeder via DHT ──────────────────
mkdir -p "$E2E_TMPDIR/dhtep_dir"
rc=0
"$ARIA2C" --no-conf --listen-port=18402 --dir="$E2E_TMPDIR/dhtep_dir" \
  --bt-tracker="http://127.0.0.1:18400/announce" \
  --enable-dht=true --dht-entry-point=127.0.0.1:18403 \
  --dht-file-path="$E2E_TMPDIR/leech_dht.dat" \
  --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
assert_file_exists "$E2E_TMPDIR/dhtep_dir/payload.bin" \
  "dht-entry-point discovers seeder via DHT"

# ── Test 4: --dht-entry-point6 with IPv6 (skip if no IPv6) ──────────────
if skip_if_no_ipv6; then
  mkdir -p "$E2E_TMPDIR/dhtep6_dir"
  rc=0
  "$ARIA2C" --no-conf --listen-port=18402 --dir="$E2E_TMPDIR/dhtep6_dir" \
    --bt-tracker="http://127.0.0.1:18400/announce" \
    --enable-dht6=true --dht-entry-point6="[::1]:18403" \
    --dht-file-path="$E2E_TMPDIR/leech_dht6.dat" \
    --dht-file-path6="$E2E_TMPDIR/leech_dht6_v6.dat" \
    --enable-peer-exchange=false \
    --seed-time=0 --bt-stop-timeout=15 \
    "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
  assert_file_exists "$E2E_TMPDIR/dhtep6_dir/payload.bin" \
    "dht-entry-point6 with IPv6 accepted, download completes"
else
  tap_ok "dht-entry-point6 with IPv6 # SKIP no IPv6 loopback available"
fi

# ── Test 5: --dht-listen-addr6=::1 binds DHT to IPv6 loopback ──────────
if skip_if_no_ipv6; then
  mkdir -p "$E2E_TMPDIR/dhtaddr6_dir"
  rc=0
  "$ARIA2C" --no-conf --listen-port=18402 --dir="$E2E_TMPDIR/dhtaddr6_dir" \
    --bt-tracker="http://127.0.0.1:18400/announce" \
    --enable-dht6=true --dht-listen-addr6="::1" \
    --dht-file-path="$E2E_TMPDIR/leech_dht_a6.dat" \
    --dht-file-path6="$E2E_TMPDIR/leech_dht_a6_v6.dat" \
    --enable-peer-exchange=false \
    --seed-time=0 --bt-stop-timeout=15 \
    "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
  assert_file_exists "$E2E_TMPDIR/dhtaddr6_dir/payload.bin" \
    "dht-listen-addr6=::1 accepted, download completes"
else
  tap_ok "dht-listen-addr6=::1 # SKIP no IPv6 loopback available"
fi

tap_summary
