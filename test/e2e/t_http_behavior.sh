#!/usr/bin/env bash
# e2e: HTTP protocol-level behavior tests (use-head, http-no-cache,
#      enable-http-keep-alive, reuse-uri, no-want-digest-header)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

# Create a 4KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null

# ── Test 1: --use-head=true sends HEAD before GET ──────────────────────────
LOG1="$E2E_TMPDIR/log1.jsonl"
start_http_server --port 18300 --dir "$E2E_TMPDIR" --log-requests "$LOG1"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --use-head=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$LOG1" '"HEAD"' \
  "use-head=true sends HEAD request"
stop_http_server

# ── Test 2: --http-no-cache=true sends Cache-Control: no-cache ────────────
start_http_server --port 18300 --dir "$E2E_TMPDIR" \
  --require-header "Cache-Control: no-cache"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --http-no-cache=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 4096 \
  "http-no-cache=true sends Cache-Control header, server accepts"
stop_http_server

# ── Test 3: --enable-http-keep-alive=true download succeeds ───────────────
start_http_server --port 18300 --dir "$E2E_TMPDIR"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --enable-http-keep-alive=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out3/testfile.bin" 4096 \
  "enable-http-keep-alive=true download succeeds"
stop_http_server

# ── Test 4: --enable-http-keep-alive=false download succeeds ──────────────
start_http_server --port 18300 --dir "$E2E_TMPDIR"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --enable-http-keep-alive=false \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out4/testfile.bin" 4096 \
  "enable-http-keep-alive=false download succeeds"
stop_http_server

# ── Test 5: --reuse-uri=false download succeeds ──────────────────────────
start_http_server --port 18300 --dir "$E2E_TMPDIR"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --reuse-uri=false --split=2 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out5/testfile.bin" 4096 \
  "reuse-uri=false with split=2 download succeeds"
stop_http_server

# ── Test 6: --no-want-digest-header=true suppresses Want-Digest ──────────
LOG6="$E2E_TMPDIR/log6.jsonl"
start_http_server --port 18300 --dir "$E2E_TMPDIR" --log-requests "$LOG6"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out6" --allow-overwrite=true \
  --no-want-digest-header=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
if grep -q "Want-Digest" "$LOG6" 2>/dev/null; then
  tap_fail "no-want-digest-header=true suppresses Want-Digest header"
else
  tap_ok "no-want-digest-header=true suppresses Want-Digest header"
fi
stop_http_server

tap_summary
