#!/usr/bin/env bash
# e2e: BitTorrent multi-file torrent and selective download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
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

# Create seed directory with three files of different sizes
# Use files larger than piece length so select-file can skip entire pieces
mkdir -p "$E2E_TMPDIR/seed_dir/multi"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/multi/alpha.dat" \
  bs=1024 count=32 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/multi/bravo.dat" \
  bs=1024 count=64 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/multi/charlie.dat" \
  bs=1024 count=32 2>/dev/null

# Create multi-file .torrent from the directory
# Use small piece length so each file spans its own pieces
python3 "$SCRIPT_DIR/create_torrent.py" \
  --dir "$E2E_TMPDIR/seed_dir/multi" \
  --output "$E2E_TMPDIR/multi.torrent" \
  --tracker "http://127.0.0.1:18137/announce" \
  --piece-length 16384

# Start tracker (returns seeder as the only peer)
start_bt_tracker --port 18137 --peer-ip 127.0.0.1 --peer-port 18143

# Start seeder — serves the multi-file torrent on port 18143
"$ARIA2C" --seed-ratio=0 --listen-port=18143 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18137/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/multi.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!

# Give the seeder a moment to start listening
sleep 2

# ── Test 1: Download all files (default behavior) ──
mkdir -p "$E2E_TMPDIR/leech_all"
"$ARIA2C" --listen-port=18144 --dir="$E2E_TMPDIR/leech_all" --no-conf \
  --bt-tracker="http://127.0.0.1:18137/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  "$E2E_TMPDIR/multi.torrent" >/dev/null 2>&1

# 1. All three files downloaded
if [[ -f "$E2E_TMPDIR/leech_all/multi/alpha.dat" ]] && \
   [[ -f "$E2E_TMPDIR/leech_all/multi/bravo.dat" ]] && \
   [[ -f "$E2E_TMPDIR/leech_all/multi/charlie.dat" ]]; then
  tap_ok "multi-file torrent downloads all three files"
else
  tap_fail "multi-file torrent downloads all three files"
fi

# ── Test 2-4: Selective download with --select-file=1,3 ──
# File indices (1-based, alphabetical): 1=alpha.dat, 2=bravo.dat, 3=charlie.dat
mkdir -p "$E2E_TMPDIR/leech_sel"
"$ARIA2C" --listen-port=18144 --dir="$E2E_TMPDIR/leech_sel" --no-conf \
  --bt-tracker="http://127.0.0.1:18137/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  --select-file=1,3 \
  "$E2E_TMPDIR/multi.torrent" >/dev/null 2>&1

# 2. Selected file alpha.dat is present
assert_file_exists "$E2E_TMPDIR/leech_sel/multi/alpha.dat" \
  "select-file=1,3 downloads alpha.dat (index 1)"

# 3. Unselected file bravo.dat is absent
assert_file_not_exists "$E2E_TMPDIR/leech_sel/multi/bravo.dat" \
  "select-file=1,3 skips bravo.dat (index 2)"

# 4. Selected file charlie.dat matches original sha256
assert_sha256_match \
  "$E2E_TMPDIR/seed_dir/multi/charlie.dat" \
  "$E2E_TMPDIR/leech_sel/multi/charlie.dat" \
  "select-file=1,3 charlie.dat sha256 matches original"

tap_summary
