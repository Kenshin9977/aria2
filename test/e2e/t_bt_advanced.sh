#!/usr/bin/env bash
# e2e: Advanced BitTorrent feature tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

_bt_seeder_pid=""
_bt_tracker_pid=""

_bt_adv_cleanup() {
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
trap _bt_adv_cleanup EXIT

# Create seed directory
mkdir -p "$E2E_TMPDIR/seed_dir"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/payload.bin" \
  bs=1024 count=64 2>/dev/null

# Create .torrent
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/seed_dir/payload.bin" \
  --output "$E2E_TMPDIR/test.torrent" \
  --tracker "http://127.0.0.1:18260/announce" \
  --piece-length 16384

# Start tracker
python3 "$SCRIPT_DIR/bt_tracker.py" \
  --port 18260 --peer-ip 127.0.0.1 --peer-port 18261 &
_bt_tracker_pid=$!
_tries=0
while ! (echo >/dev/tcp/127.0.0.1/18260) 2>/dev/null; do
  _tries=$((_tries + 1))
  if [[ $_tries -ge 30 ]]; then
    echo "ERROR: BT tracker failed to start" >&2
    exit 1
  fi
  sleep 0.1
done

# Start seeder
"$ARIA2C" --seed-ratio=0 --listen-port=18261 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!
sleep 1

# ── Test 1: bt-force-encryption downloads with encrypted connections ──────────
mkdir -p "$E2E_TMPDIR/enc_dir"
"$ARIA2C" --no-conf --listen-port=18262 --dir="$E2E_TMPDIR/enc_dir" \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-force-encryption=true --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/enc_dir/payload.bin" \
  "bt-force-encryption completes download"

# ── Test 2: bt-hash-check-seed verifies data before seeding ──────────────────
mkdir -p "$E2E_TMPDIR/hashcheck_dir"
"$ARIA2C" --no-conf --listen-port=18263 --dir="$E2E_TMPDIR/hashcheck_dir" \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-hash-check-seed=true --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/hashcheck_dir/payload.bin" \
  "bt-hash-check-seed downloads and verifies payload"

# ── Test 3: bt-prioritize-piece=head completes download ──────────────────────
mkdir -p "$E2E_TMPDIR/prio_dir"
"$ARIA2C" --no-conf --listen-port=18264 --dir="$E2E_TMPDIR/prio_dir" \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-prioritize-piece=head --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/prio_dir/payload.bin" \
  "bt-prioritize-piece=head completes download"

# ── Test 4: index-out (-O) renames output file ───────────────────────────────
mkdir -p "$E2E_TMPDIR/idxout_dir"
"$ARIA2C" --no-conf --listen-port=18265 --dir="$E2E_TMPDIR/idxout_dir" \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --index-out=1=renamed_payload.bin --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/idxout_dir/renamed_payload.bin" \
  "index-out renames output file"

# ── Test 5: bt-max-peers=1 download succeeds with limited peers ──────────────
mkdir -p "$E2E_TMPDIR/maxpeer_dir"
"$ARIA2C" --no-conf --listen-port=18266 --dir="$E2E_TMPDIR/maxpeer_dir" \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-max-peers=1 --seed-time=0 --bt-stop-timeout=15 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/maxpeer_dir/payload.bin" \
  "bt-max-peers=1 download succeeds"

tap_summary
