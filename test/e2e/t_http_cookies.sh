#!/usr/bin/env bash
# e2e: HTTP cookie tests (save-cookies, load-cookies, cookie enforcement)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

COOKIE_FILE="$E2E_TMPDIR/cookies.txt"

# 1. Download with --save-cookies from server with --set-cookie
start_http_server --port 18093 --dir "$E2E_TMPDIR" \
  --set-cookie "session=abc123"
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --save-cookies="$COOKIE_FILE" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$COOKIE_FILE" "session" \
  "cookie: --save-cookies writes cookie file with cookie name"
stop_http_server

# 2. Restart server with --require-cookie, use --load-cookies — succeeds
start_http_server --port 18093 --dir "$E2E_TMPDIR" \
  --require-cookie "session=abc123"
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --load-cookies="$COOKIE_FILE" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "cookie: --load-cookies sends cookie, server accepts"
stop_http_server

# 3. Same server, no --load-cookies — fails 403
start_http_server --port 18093 --dir "$E2E_TMPDIR" \
  --require-cookie "session=abc123"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "cookie: no --load-cookies, server returns 403"
else
  tap_fail "cookie: no --load-cookies, server returns 403 (expected non-zero exit)"
fi
stop_http_server

tap_summary
