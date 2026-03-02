#!/usr/bin/env bash
# e2e: HTTP retry tests (503 fail-first-n, max-tries, retry-wait)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# aria2c only retries 503 when --retry-wait > 0 (see HttpSkipResponseCommand.cc)

# 1. --max-tries=3 --retry-wait=1 with fail-first-n=2 — succeeds on 3rd attempt
start_http_server --port 18092 --dir "$E2E_TMPDIR" --fail-first-n 2
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --max-tries=3 --retry-wait=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "retry: --max-tries=3 --retry-wait=1 succeeds after 2 failures"
stop_http_server

# 2. --max-tries=1 --retry-wait=1 — fails (only 1 attempt, server returns 503)
start_http_server --port 18092 --dir "$E2E_TMPDIR" --fail-first-n 2
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --max-tries=1 --retry-wait=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "retry: --max-tries=1 fails on 503"
else
  tap_fail "retry: --max-tries=1 fails on 503 (expected non-zero exit)"
fi
stop_http_server

# 3. --max-tries=5 --retry-wait=1 — succeeds with delay
start_http_server --port 18092 --dir "$E2E_TMPDIR" --fail-first-n 2
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --max-tries=5 --retry-wait=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out3/testfile.bin" 1024 \
  "retry: --max-tries=5 --retry-wait=1 succeeds"
stop_http_server

tap_summary
