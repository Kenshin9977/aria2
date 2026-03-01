#!/usr/bin/env bash
# e2e: HTTP timeout tests (connect-timeout, transfer timeout, control)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# 1. --connect-timeout=2 to unreachable address — exits non-zero within 10s
#    192.0.2.1 is TEST-NET-1 (RFC 5737), should be unreachable
start_time=$(date +%s)
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --connect-timeout=2 --max-tries=1 \
  "http://192.0.2.1:18095/testfile.bin" >/dev/null 2>&1 || rc=$?
end_time=$(date +%s)
elapsed=$((end_time - start_time))
if [[ $rc -ne 0 && $elapsed -le 10 ]]; then
  tap_ok "timeout: --connect-timeout=2 to unreachable exits within 10s"
else
  tap_fail "timeout: --connect-timeout=2 to unreachable (rc=$rc, elapsed=${elapsed}s)"
fi

# 2. --timeout=2 to server with --delay 10 — exits non-zero (stall)
start_http_server --port 18095 --dir "$E2E_TMPDIR" --delay 10
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --timeout=2 --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "timeout: --timeout=2 with delayed server exits non-zero"
else
  tap_fail "timeout: --timeout=2 with delayed server (expected non-zero exit)"
fi
stop_http_server

# 3. Normal download with --timeout=5 — succeeds (control)
start_http_server --port 18095 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --timeout=5 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out3/testfile.bin" 1024 \
  "timeout: normal download with --timeout=5 succeeds"
stop_http_server

tap_summary
