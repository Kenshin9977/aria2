#!/usr/bin/env bash
# e2e: BitTorrent seed and download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

_bt_tracker_pid=""
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
  # Let the default cleanup handle tmpdir removal
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
  --tracker "http://127.0.0.1:18135/announce" \
  --piece-length 16384

# Start the tracker (returns seeder as the only peer)
python3 "$SCRIPT_DIR/bt_tracker.py" \
  --port 18135 --peer-ip 127.0.0.1 --peer-port 18130 &
_bt_tracker_pid=$!

# Wait for tracker to accept connections
_tries=0
while ! curl -s "http://127.0.0.1:18135/announce" >/dev/null 2>&1; do
  _tries=$((_tries + 1))
  if [[ $_tries -ge 30 ]]; then
    echo "ERROR: BT tracker failed to start on port 18135" >&2
    exit 1
  fi
  sleep 0.1
done

# Start seeder — serves the file via BitTorrent on port 18130
"$ARIA2C" --seed-ratio=0 --listen-port=18130 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18135/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!

# Give the seeder a moment to start listening
sleep 1

# Start leecher — downloads to a separate directory
mkdir -p "$E2E_TMPDIR/leech_dir"
"$ARIA2C" --listen-port=18131 --dir="$E2E_TMPDIR/leech_dir" --no-conf \
  --bt-tracker="http://127.0.0.1:18135/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1

# 1. Leecher downloaded the file
assert_file_exists "$E2E_TMPDIR/leech_dir/payload.bin" \
  "BT leecher downloads payload.bin"

# 2. Downloaded file sha256 matches original
assert_sha256_match \
  "$E2E_TMPDIR/seed_dir/payload.bin" \
  "$E2E_TMPDIR/leech_dir/payload.bin" \
  "BT downloaded file sha256 matches original"

tap_summary
