#!/usr/bin/env bash
# e2e: Logging option tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# 1. --log=FILE --log-level=debug creates a log file
start_http_server --port 18290 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --no-conf \
  --log="$E2E_TMPDIR/aria2.log" --log-level=debug \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/aria2.log" \
  "logging: --log=FILE --log-level=debug creates log file"
stop_http_server

# 2. Debug log file contains DEBUG-level entries
start_http_server --port 18290 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --no-conf \
  --log="$E2E_TMPDIR/debug.log" --log-level=debug \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$E2E_TMPDIR/debug.log" "DEBUG" \
  "logging: debug log contains DEBUG entries"
stop_http_server

# 3. --console-log-level=error — download still succeeds
start_http_server --port 18290 --dir "$E2E_TMPDIR"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --no-conf \
  --console-log-level=error \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_exit_code "$rc" 0 \
  "logging: --console-log-level=error download still succeeds"
stop_http_server

# 4. --download-result=full — output contains GID column header
start_http_server --port 18290 --dir "$E2E_TMPDIR"
output=""
output=$("$ARIA2C" -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --no-conf \
  --download-result=full \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" 2>&1) || true
if echo "$output" | grep -qiE "GID|gid"; then
  tap_ok "logging: --download-result=full output contains GID detail"
else
  tap_fail "logging: --download-result=full output contains GID detail (GID not found in output)"
fi
stop_http_server

# 5. --download-result=hide — suppresses "Download Results" section
start_http_server --port 18290 --dir "$E2E_TMPDIR"
output=""
output=$("$ARIA2C" -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --no-conf \
  --download-result=hide \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" 2>&1) || true
if echo "$output" | grep -q "Download Results"; then
  tap_fail "logging: --download-result=hide suppresses Download Results section (section unexpectedly present)"
else
  tap_ok "logging: --download-result=hide suppresses Download Results section"
fi
stop_http_server

tap_summary
