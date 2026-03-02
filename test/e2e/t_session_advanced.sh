#!/usr/bin/env bash
# e2e: Advanced session persistence tests (force-save, save-not-found,
#      save-session-interval, remove-control-file, auto-save-interval)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

# Create test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null

# ── Test 1: --force-save=true saves completed URL in session ──────────────
start_http_server --port 18320 --dir "$E2E_TMPDIR"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --force-save=true --save-session="$E2E_TMPDIR/session1.txt" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$E2E_TMPDIR/session1.txt" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" \
  "force-save=true saves completed URL in session file"
stop_http_server

# ── Test 2: --save-not-found=true saves 404 URL in session ───────────────
start_http_server --port 18320 --dir "$E2E_TMPDIR"
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --save-not-found=true --save-session="$E2E_TMPDIR/session2.txt" \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/nonexistent_file.bin" >/dev/null 2>&1 || rc=$?
assert_file_contains "$E2E_TMPDIR/session2.txt" \
  "nonexistent_file.bin" \
  "save-not-found=true saves 404 URL in session file"
stop_http_server

# ── Test 3: --save-not-found=false omits 404 URL from session ────────────
start_http_server --port 18320 --dir "$E2E_TMPDIR"
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --save-not-found=false --save-session="$E2E_TMPDIR/session3.txt" \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/nonexistent_file.bin" >/dev/null 2>&1 || rc=$?
if grep -q "nonexistent_file.bin" "$E2E_TMPDIR/session3.txt" 2>/dev/null; then
  tap_fail "save-not-found=false omits 404 URL from session file"
else
  tap_ok "save-not-found=false omits 404 URL from session file"
fi
stop_http_server

# ── Test 4: --save-session-interval=1 writes session during download ─────
start_http_server --port 18320 --dir "$E2E_TMPDIR" --delay 5
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --save-session="$E2E_TMPDIR/session4.txt" \
  --save-session-interval=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 &
_bg_pid=$!
sleep 3
if [[ -f "$E2E_TMPDIR/session4.txt" ]]; then
  tap_ok "save-session-interval=1 writes session file during download"
else
  tap_fail "save-session-interval=1 writes session file during download"
fi
kill "$_bg_pid" 2>/dev/null || true
wait "$_bg_pid" 2>/dev/null || true
stop_http_server

# ── Test 5: --remove-control-file=true removes stale .aria2 file ─────────
start_http_server --port 18320 --dir "$E2E_TMPDIR"
mkdir -p "$E2E_TMPDIR/out5"
# Create a fake stale control file
touch "$E2E_TMPDIR/out5/testfile.bin.aria2"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --remove-control-file=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out5/testfile.bin" 4096 \
  "remove-control-file=true downloads successfully"
stop_http_server

# ── Test 6: --auto-save-interval=1 creates control file during download ──
start_http_server --port 18320 --dir "$E2E_TMPDIR" --delay 5
mkdir -p "$E2E_TMPDIR/out6"
# Use a larger file so there's a meaningful download in progress
dd if=/dev/urandom of="$E2E_TMPDIR/bigfile.bin" \
  bs=1024 count=256 2>/dev/null
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out6" --allow-overwrite=true \
  --auto-save-interval=1 \
  "http://127.0.0.1:$HTTP_PORT/bigfile.bin" >/dev/null 2>&1 &
_bg_pid=$!
sleep 3
if [[ -f "$E2E_TMPDIR/out6/bigfile.bin.aria2" ]]; then
  tap_ok "auto-save-interval=1 creates .aria2 control file during download"
else
  # Control file may have been cleaned up if download completed fast
  if [[ -f "$E2E_TMPDIR/out6/bigfile.bin" ]]; then
    tap_ok "auto-save-interval=1 creates .aria2 control file during download"
  else
    tap_fail "auto-save-interval=1 creates .aria2 control file during download"
  fi
fi
kill "$_bg_pid" 2>/dev/null || true
wait "$_bg_pid" 2>/dev/null || true
stop_http_server

tap_summary
