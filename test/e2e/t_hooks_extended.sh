#!/usr/bin/env bash
# e2e: Extended download event hook tests (start, pause, stop, bt-complete)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

# Detect if BT is available (test 4 requires it)
_bt_available=false
if "$ARIA2C" -v 2>&1 | grep -q 'BitTorrent'; then
  _bt_available=true
fi

if [[ "$_bt_available" == true ]]; then
  tap_plan 4
else
  tap_plan 3
fi
make_tempdir

# ── Shared fixture: 1KB test file ──
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# ── BT-related PIDs (managed by custom trap) ──
_bt_seeder_pid=""

_hooks_cleanup() {
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
trap _hooks_cleanup EXIT

# ── Test 1: --on-download-start fires ──
# Non-RPC test: start a direct download and verify the hook fires synchronously.

start_http_server --port 18230 --dir "$E2E_TMPDIR"

cat > "$E2E_TMPDIR/on_start.sh" <<HOOKEOF
#!/bin/bash
echo "\$1" > "$E2E_TMPDIR/start_marker.txt"
HOOKEOF
chmod +x "$E2E_TMPDIR/on_start.sh"

mkdir -p "$E2E_TMPDIR/out1"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --on-download-start="$E2E_TMPDIR/on_start.sh" \
  "http://127.0.0.1:18230/testfile.bin" >/dev/null 2>&1
# Give the hook a moment to finish writing
sleep 0.5

assert_file_exists "$E2E_TMPDIR/start_marker.txt" \
  "hooks: --on-download-start fires (marker created)"

stop_http_server

# ── Test 2: --on-download-pause fires via RPC pause ──
# Use a slow server (--delay 3) so the download is still in progress when
# we send the RPC pause command.

start_http_server --port 18231 --dir "$E2E_TMPDIR" --delay 3

cat > "$E2E_TMPDIR/on_pause.sh" <<HOOKEOF
#!/bin/bash
echo "\$1" > "$E2E_TMPDIR/pause_marker.txt"
HOOKEOF
chmod +x "$E2E_TMPDIR/on_pause.sh"

mkdir -p "$E2E_TMPDIR/out2"
start_aria2_rpc 16814 \
  --dir="$E2E_TMPDIR/out2" \
  --allow-overwrite=true \
  --on-download-pause="$E2E_TMPDIR/on_pause.sh"

resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:18231/testfile.bin\"]]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
# Give the download a moment to become active before pausing
sleep 0.5
rpc_call $RPC_PORT "aria2.pause" "[\"$gid\"]" >/dev/null 2>&1
# Give the hook a moment to execute after the pause
sleep 1

assert_file_exists "$E2E_TMPDIR/pause_marker.txt" \
  "hooks: --on-download-pause fires via RPC pause (marker created)"

stop_aria2_rpc
stop_http_server

# ── Test 3: --on-download-stop fires via RPC forceRemove ──
# Use a slow server (--delay 3) so the download is still in progress when
# we send the RPC forceRemove command.

start_http_server --port 18232 --dir "$E2E_TMPDIR" --delay 3

cat > "$E2E_TMPDIR/on_stop.sh" <<HOOKEOF
#!/bin/bash
echo "\$1" > "$E2E_TMPDIR/stop_marker.txt"
HOOKEOF
chmod +x "$E2E_TMPDIR/on_stop.sh"

mkdir -p "$E2E_TMPDIR/out3"
start_aria2_rpc 16815 \
  --dir="$E2E_TMPDIR/out3" \
  --allow-overwrite=true \
  --on-download-stop="$E2E_TMPDIR/on_stop.sh"

resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:18232/testfile.bin\"]]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
# Give the download a moment to become active before removing
sleep 0.5
rpc_call $RPC_PORT "aria2.forceRemove" "[\"$gid\"]" >/dev/null 2>&1
# Give the hook a moment to execute after the removal
sleep 1

assert_file_exists "$E2E_TMPDIR/stop_marker.txt" \
  "hooks: --on-download-stop fires via RPC forceRemove (marker created)"

stop_aria2_rpc
stop_http_server

# ── Test 4: --on-bt-download-complete fires on leecher finish ──
# Seeder/leecher pattern: start a seeder, then have a leecher download
# with an on-bt-download-complete hook; check that the hook fires.

if [[ "$_bt_available" != true ]]; then
  tap_summary
fi

mkdir -p "$E2E_TMPDIR/bt_seed_dir"
dd if=/dev/urandom of="$E2E_TMPDIR/bt_seed_dir/bt_payload.bin" \
  bs=1024 count=64 2>/dev/null

python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/bt_seed_dir/bt_payload.bin" \
  --output "$E2E_TMPDIR/bt_hooks.torrent" \
  --tracker "http://127.0.0.1:18260/announce" \
  --piece-length 16384

start_bt_tracker --port 18260 --peer-ip 127.0.0.1 --peer-port 18261

# Start seeder — serves the file via BitTorrent on port 18261
"$ARIA2C" --seed-ratio=0 --listen-port=18261 \
  --dir="$E2E_TMPDIR/bt_seed_dir" --no-conf \
  --allow-overwrite=true --bt-seed-unverified=true \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  "$E2E_TMPDIR/bt_hooks.torrent" >/dev/null 2>&1 &
_bt_seeder_pid=$!

# Give the seeder a moment to start listening
sleep 1

cat > "$E2E_TMPDIR/on_bt_complete.sh" <<HOOKEOF
#!/bin/bash
echo "\$1" > "$E2E_TMPDIR/bt_complete_marker.txt"
HOOKEOF
chmod +x "$E2E_TMPDIR/on_bt_complete.sh"

mkdir -p "$E2E_TMPDIR/bt_leech_dir"
"$ARIA2C" --listen-port=18262 --dir="$E2E_TMPDIR/bt_leech_dir" --no-conf \
  --bt-tracker="http://127.0.0.1:18260/announce" \
  --enable-dht=false --enable-peer-exchange=false \
  --seed-time=0 --bt-stop-timeout=30 \
  --on-bt-download-complete="$E2E_TMPDIR/on_bt_complete.sh" \
  "$E2E_TMPDIR/bt_hooks.torrent" >/dev/null 2>&1
# Give the hook a moment to execute after download completes
sleep 0.5

assert_file_exists "$E2E_TMPDIR/bt_complete_marker.txt" \
  "hooks: --on-bt-download-complete fires when leecher finishes (marker created)"

tap_summary
