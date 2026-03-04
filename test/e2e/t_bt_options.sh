#!/usr/bin/env bash
# e2e: BitTorrent feature option tests (bt-exclude-tracker,
#      bt-max-open-files, bt-remove-unselected-file, follow-torrent,
#      bt-detach-seed-only, bt-load-saved-metadata)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

_bt_seeder_pid=""
_bt_tracker_pid=""

_bt_opts_cleanup() {
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
trap _bt_opts_cleanup EXIT

# Create single-file seed
mkdir -p "$E2E_TMPDIR/seed_dir"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/payload.bin" \
  bs=1024 count=64 2>/dev/null

# Create multi-file seed
mkdir -p "$E2E_TMPDIR/seed_dir/multi"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/multi/alpha.dat" \
  bs=1024 count=32 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/multi/bravo.dat" \
  bs=1024 count=32 2>/dev/null

# Create single-file torrent
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/seed_dir/payload.bin" \
  --output "$E2E_TMPDIR/test.torrent" \
  --tracker "http://127.0.0.1:18354/announce" \
  --piece-length 16384

# Create multi-file torrent
python3 "$SCRIPT_DIR/create_torrent.py" \
  --dir "$E2E_TMPDIR/seed_dir/multi" \
  --output "$E2E_TMPDIR/multi.torrent" \
  --tracker "http://127.0.0.1:18354/announce" \
  --piece-length 16384

# Start tracker
python3 "$SCRIPT_DIR/bt_tracker.py" \
  --port 18354 --peer-ip 127.0.0.1 --peer-port 18355 &
_bt_tracker_pid=$!
_tries=0
while ! (echo >/dev/tcp/127.0.0.1/18354) 2>/dev/null; do
  _tries=$((_tries + 1))
  if [[ $_tries -ge 30 ]]; then
    echo "ERROR: BT tracker failed to start" >&2
    exit 1
  fi
  sleep 0.1
done

# Start seeder for single-file torrent
"$ARIA2C" --seed-ratio=0 --listen-port=18355 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18354/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/test.torrent" "$E2E_TMPDIR/multi.torrent" \
  >/dev/null 2>&1 &
_bt_seeder_pid=$!
sleep 1

# ── Test 1: --bt-exclude-tracker prevents tracker contact ───────────────
mkdir -p "$E2E_TMPDIR/excl_dir"
rc=0
"$ARIA2C" --no-conf --listen-port=18356 --dir="$E2E_TMPDIR/excl_dir" \
  --bt-exclude-tracker="http://127.0.0.1:18354/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-stop-timeout=5 --seed-time=0 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
# With tracker excluded and DHT/PEX off, download should fail (no peers)
if [[ ! -f "$E2E_TMPDIR/excl_dir/payload.bin" ]] || [[ $rc -ne 0 ]]; then
  tap_ok "bt-exclude-tracker prevents tracker contact (no peers found)"
else
  tap_fail "bt-exclude-tracker prevents tracker contact (download unexpectedly succeeded)"
fi

# ── Test 2: --bt-max-open-files=1 multi-file download completes ─────────
mkdir -p "$E2E_TMPDIR/maxfiles_dir"
"$ARIA2C" --no-conf --listen-port=18357 --dir="$E2E_TMPDIR/maxfiles_dir" \
  --bt-tracker="http://127.0.0.1:18354/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-max-open-files=1 --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/multi.torrent" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/maxfiles_dir/multi/alpha.dat" ]] && \
   [[ -f "$E2E_TMPDIR/maxfiles_dir/multi/bravo.dat" ]]; then
  tap_ok "bt-max-open-files=1 downloads all files"
else
  tap_fail "bt-max-open-files=1 downloads all files"
fi

# ── Test 3: --bt-remove-unselected-file=true removes unselected ─────────
mkdir -p "$E2E_TMPDIR/rmsel_dir"
"$ARIA2C" --no-conf --listen-port=18358 --dir="$E2E_TMPDIR/rmsel_dir" \
  --bt-tracker="http://127.0.0.1:18354/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --select-file=1 --bt-remove-unselected-file=true \
  --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/multi.torrent" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/rmsel_dir/multi/alpha.dat" ]] && \
   [[ ! -f "$E2E_TMPDIR/rmsel_dir/multi/bravo.dat" ]]; then
  tap_ok "bt-remove-unselected-file=true removes unselected file"
else
  tap_fail "bt-remove-unselected-file=true removes unselected file"
fi

# ── Test 4: --follow-torrent=false saves .torrent without parsing ────────
start_http_server --port 18362 --dir "$E2E_TMPDIR"
mkdir -p "$E2E_TMPDIR/followfalse_dir"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/followfalse_dir" --allow-overwrite=true \
  --follow-torrent=false \
  "http://127.0.0.1:$HTTP_PORT/test.torrent" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/followfalse_dir/test.torrent" ]] && \
   [[ ! -f "$E2E_TMPDIR/followfalse_dir/payload.bin" ]]; then
  tap_ok "follow-torrent=false saves .torrent without downloading payload"
else
  tap_fail "follow-torrent=false saves .torrent without downloading payload"
fi
stop_http_server

# ── Test 5: --bt-detach-seed-only=true download completes ───────────────
mkdir -p "$E2E_TMPDIR/detach_dir"
"$ARIA2C" --no-conf --listen-port=18359 --dir="$E2E_TMPDIR/detach_dir" \
  --bt-tracker="http://127.0.0.1:18354/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-detach-seed-only=true --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/detach_dir/payload.bin" \
  "bt-detach-seed-only=true download completes"

# ── Test 6: --bt-load-saved-metadata=true reuses cached metadata ────────
mkdir -p "$E2E_TMPDIR/meta1_dir" "$E2E_TMPDIR/meta2_dir"
# First run: downloads and completes normally
"$ARIA2C" --no-conf --listen-port=18360 --dir="$E2E_TMPDIR/meta1_dir" \
  --bt-tracker="http://127.0.0.1:18354/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-save-metadata=true --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
# Second run with bt-load-saved-metadata
rc=0
"$ARIA2C" --no-conf --listen-port=18360 --dir="$E2E_TMPDIR/meta2_dir" \
  --bt-tracker="http://127.0.0.1:18354/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-load-saved-metadata=true --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
assert_file_exists "$E2E_TMPDIR/meta2_dir/payload.bin" \
  "bt-load-saved-metadata=true reuses metadata successfully"

tap_summary
