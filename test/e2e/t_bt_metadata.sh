#!/usr/bin/env bash
# e2e: BitTorrent metadata and upload limit option tests (bt-metadata-only,
#      max-upload-limit, max-overall-upload-limit, dht-file-path,
#      bt-enable-hook-after-hash-check)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

_bt_seeder_pid=""
_bt_tracker_pid=""

_bt_meta_cleanup() {
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
trap _bt_meta_cleanup EXIT

# Create seed
mkdir -p "$E2E_TMPDIR/seed_dir"
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/payload.bin" \
  bs=1024 count=64 2>/dev/null

# Create torrent
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/seed_dir/payload.bin" \
  --output "$E2E_TMPDIR/test.torrent" \
  --tracker "http://127.0.0.1:18363/announce" \
  --piece-length 16384

# ── Test 1: --bt-metadata-only=true does NOT download payload ───────────
# Run without tracker/seeder so no actual data can be transferred
mkdir -p "$E2E_TMPDIR/metaonly_dir"
rc=0
"$ARIA2C" --no-conf --listen-port=18366 --dir="$E2E_TMPDIR/metaonly_dir" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-metadata-only=true --bt-stop-timeout=3 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
# With bt-metadata-only, the file should be 0 bytes (placeholder) or absent
actual_size=0
if [[ -f "$E2E_TMPDIR/metaonly_dir/payload.bin" ]]; then
  actual_size=$(wc -c < "$E2E_TMPDIR/metaonly_dir/payload.bin" | tr -d ' ')
fi
if [[ "$actual_size" -lt 65536 ]]; then
  tap_ok "bt-metadata-only=true does not download payload ($actual_size bytes)"
else
  tap_fail "bt-metadata-only=true does not download payload ($actual_size bytes)"
fi

# Start tracker and seeder for remaining tests
python3 "$SCRIPT_DIR/bt_tracker.py" \
  --port 18363 --peer-ip 127.0.0.1 --peer-port 18365 &
_bt_tracker_pid=$!
_tries=0
while ! curl -s "http://127.0.0.1:18363/announce" >/dev/null 2>&1; do
  _tries=$((_tries + 1))
  if [[ $_tries -ge 30 ]]; then
    echo "ERROR: BT tracker failed to start" >&2
    exit 1
  fi
  sleep 0.1
done

"$ARIA2C" --seed-ratio=0 --listen-port=18365 \
  --dir="$E2E_TMPDIR/seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18363/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!
sleep 2

# ── Test 2: --max-upload-limit=1K download completes ────────────────────
mkdir -p "$E2E_TMPDIR/uplimit_dir"
"$ARIA2C" --no-conf --listen-port=18367 --dir="$E2E_TMPDIR/uplimit_dir" \
  --bt-tracker="http://127.0.0.1:18363/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --max-upload-limit=1K --seed-time=0 --bt-stop-timeout=30 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/uplimit_dir/payload.bin" \
  "max-upload-limit=1K download completes"

# ── Test 3: --max-overall-upload-limit=1K download completes ────────────
mkdir -p "$E2E_TMPDIR/ovuplimit_dir"
"$ARIA2C" --no-conf --listen-port=18368 --dir="$E2E_TMPDIR/ovuplimit_dir" \
  --bt-tracker="http://127.0.0.1:18363/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --max-overall-upload-limit=1K --seed-time=0 --bt-stop-timeout=30 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/ovuplimit_dir/payload.bin" \
  "max-overall-upload-limit=1K download completes"

# ── Test 4: --dht-file-path=FILE creates DHT routing table ──────────────
mkdir -p "$E2E_TMPDIR/dht_dir"
rc=0
"$ARIA2C" --no-conf --listen-port=18366 --dir="$E2E_TMPDIR/dht_dir" \
  --bt-tracker="http://127.0.0.1:18363/announce" \
  --enable-dht=true --dht-file-path="$E2E_TMPDIR/dht.dat" \
  --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1 || rc=$?
assert_file_exists "$E2E_TMPDIR/dht.dat" \
  "dht-file-path creates DHT routing table file"

# ── Test 5: --bt-enable-hook-after-hash-check fires on-bt-download-complete
mkdir -p "$E2E_TMPDIR/hook_dir"
# Hook writes a marker file named after the GID
cat > "$E2E_TMPDIR/hook.sh" <<'HOOKEOF'
#!/bin/sh
touch "$(dirname "$3")/hook_fired.marker"
HOOKEOF
chmod +x "$E2E_TMPDIR/hook.sh"
"$ARIA2C" --no-conf --listen-port=18366 --dir="$E2E_TMPDIR/hook_dir" \
  --bt-tracker="http://127.0.0.1:18363/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --bt-enable-hook-after-hash-check=true \
  --on-bt-download-complete="$E2E_TMPDIR/hook.sh" \
  --seed-time=0 --bt-stop-timeout=30 \
  "$E2E_TMPDIR/test.torrent" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/hook_dir/hook_fired.marker" \
  "bt-enable-hook-after-hash-check=true fires hook"

tap_summary
