#!/usr/bin/env bash
# e2e: BitTorrent encryption and crypto negotiation tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

_bt_seeder_pid=""

_kill_seeder() {
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
    _bt_seeder_pid=""
  fi
}

_bt_cleanup() {
  _kill_seeder
  _e2e_cleanup
}
trap _bt_cleanup EXIT

# Create seed directory and the original test file (64KB)
mkdir -p "$E2E_TMPDIR/seed_dir"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/payload.bin" \
  bs=1024 count=64 2>/dev/null

# Create .torrent with tracker URL
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/seed_dir/payload.bin" \
  --output "$E2E_TMPDIR/test.torrent" \
  --tracker "http://127.0.0.1:18138/announce" \
  --piece-length 16384

# Start tracker (returns seeder as the only peer)
start_bt_tracker --port 18138 --peer-ip 127.0.0.1 --peer-port 18145

# ── Run A: Seeder with forced encryption ──
"$ARIA2C" --seed-ratio=0 --listen-port=18145 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18138/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-force-encryption=true --bt-require-crypto=true \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!
sleep 2

# 1. Leecher with --bt-force-encryption=true downloads successfully
mkdir -p "$E2E_TMPDIR/leech_enc1"
"$ARIA2C" --listen-port=18146 --dir="$E2E_TMPDIR/leech_enc1" --no-conf \
  --bt-tracker="http://127.0.0.1:18138/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  --bt-force-encryption=true \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1

assert_sha256_match \
  "$E2E_TMPDIR/seed_dir/payload.bin" \
  "$E2E_TMPDIR/leech_enc1/payload.bin" \
  "forced encryption: leecher download sha256 matches"

# 2. Leecher with --bt-require-crypto + arc4 level downloads successfully
mkdir -p "$E2E_TMPDIR/leech_enc2"
"$ARIA2C" --listen-port=18146 --dir="$E2E_TMPDIR/leech_enc2" --no-conf \
  --bt-tracker="http://127.0.0.1:18138/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  --bt-require-crypto=true --bt-min-crypto-level=arc4 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1

assert_sha256_match \
  "$E2E_TMPDIR/seed_dir/payload.bin" \
  "$E2E_TMPDIR/leech_enc2/payload.bin" \
  "require-crypto + arc4: leecher download sha256 matches"

# ── Run B: Restart seeder without forced encryption ──
_kill_seeder

"$ARIA2C" --seed-ratio=0 --listen-port=18145 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18138/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!
sleep 2

# 3. Default seeder, leecher requires crypto — negotiation succeeds
mkdir -p "$E2E_TMPDIR/leech_enc3"
"$ARIA2C" --listen-port=18146 --dir="$E2E_TMPDIR/leech_enc3" --no-conf \
  --bt-tracker="http://127.0.0.1:18138/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  --bt-require-crypto=true \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1

assert_file_exists "$E2E_TMPDIR/leech_enc3/payload.bin" \
  "crypto negotiation: default seeder accepts leecher requiring crypto"

# 4. Verify the negotiated-encryption download matches original
assert_sha256_match \
  "$E2E_TMPDIR/seed_dir/payload.bin" \
  "$E2E_TMPDIR/leech_enc3/payload.bin" \
  "crypto negotiation: downloaded file sha256 matches original"

tap_summary
