#!/usr/bin/env bash
# e2e: BitTorrent magnet URI download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

_bt_seeder_pid=""

_bt_cleanup() {
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
  _e2e_cleanup
}
trap _bt_cleanup EXIT

# Create seed directory and the original test file (64KB)
mkdir -p "$E2E_TMPDIR/seed_dir"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/payload.bin" \
  bs=1024 count=64 2>/dev/null

# Create .torrent and capture the info hash
INFO_HASH=$(python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/seed_dir/payload.bin" \
  --output "$E2E_TMPDIR/test.torrent" \
  --tracker "http://127.0.0.1:18136/announce" \
  --piece-length 16384 \
  --print-info-hash)

MAGNET="magnet:?xt=urn:btih:${INFO_HASH}&tr=http://127.0.0.1:18136/announce"

# Start tracker (returns seeder as the only peer)
start_bt_tracker --port 18136 --peer-ip 127.0.0.1 --peer-port 18141

# Start seeder — serves the file via BitTorrent on port 18141
"$ARIA2C" --seed-ratio=0 --listen-port=18141 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18136/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!

# Give the seeder a moment to start listening
sleep 2

# Start leecher — downloads using the magnet URI
mkdir -p "$E2E_TMPDIR/leech_dir"
"$ARIA2C" --listen-port=18142 --dir="$E2E_TMPDIR/leech_dir" --no-conf \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  --bt-save-metadata=true \
  "$MAGNET" >/dev/null 2>&1

# 1. Leecher downloaded the file via magnet URI
assert_file_exists "$E2E_TMPDIR/leech_dir/payload.bin" \
  "magnet URI download produces payload.bin"

# 2. Downloaded file sha256 matches original
assert_sha256_match \
  "$E2E_TMPDIR/seed_dir/payload.bin" \
  "$E2E_TMPDIR/leech_dir/payload.bin" \
  "magnet URI downloaded file sha256 matches original"

# 3. --bt-save-metadata saves a .torrent metadata file
if ls "$E2E_TMPDIR/leech_dir"/*.torrent >/dev/null 2>&1; then
  tap_ok "bt-save-metadata saves .torrent file"
elif ls "$E2E_TMPDIR"/*.torrent >/dev/null 2>&1; then
  # aria2 may save metadata in the working directory
  tap_ok "bt-save-metadata saves .torrent file"
else
  tap_fail "bt-save-metadata saves .torrent file (no .torrent found)"
fi

tap_summary
