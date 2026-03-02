#!/usr/bin/env bash
# e2e: HTTP custom headers tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# 1. --header with required header — server accepts, download succeeds
start_http_server --port 18096 --dir "$E2E_TMPDIR" \
  --require-header "X-Custom: test123"
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --header="X-Custom: test123" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "header: --header sends custom header, server accepts"
stop_http_server

# 2. Missing required header — server returns 403, download fails
start_http_server --port 18096 --dir "$E2E_TMPDIR" \
  --require-header "X-Custom: test123"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "header: missing required header returns 403, aria2c fails"
else
  tap_fail "header: missing required header returns 403, aria2c fails (expected non-zero exit)"
fi
stop_http_server

# 3. --user-agent — server logs request, verify User-Agent in log
LOG3="$E2E_TMPDIR/log3.jsonl"
start_http_server --port 18096 --dir "$E2E_TMPDIR" \
  --log-requests "$LOG3"
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --user-agent="TestBot/1.0" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$LOG3" "TestBot/1.0" \
  "header: --user-agent sends custom User-Agent"
stop_http_server

# 4. --referer — server logs request, verify Referer in log
LOG4="$E2E_TMPDIR/log4.jsonl"
start_http_server --port 18096 --dir "$E2E_TMPDIR" \
  --log-requests "$LOG4"
"$ARIA2C" -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --referer="http://example.com" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$LOG4" "example.com" \
  "header: --referer sends Referer header"
stop_http_server

# 5. Multiple --header options — both appear in log
LOG5="$E2E_TMPDIR/log5.jsonl"
start_http_server --port 18096 --dir "$E2E_TMPDIR" \
  --log-requests "$LOG5"
"$ARIA2C" -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --header="X-One: aaa" --header="X-Two: bbb" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$LOG5" "X-One" \
  "header: multiple --header first header (X-One) present"
stop_http_server

tap_summary
