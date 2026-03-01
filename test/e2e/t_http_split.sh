#!/usr/bin/env bash
# e2e: HTTP multi-connection split download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create 4MB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/bigfile.bin" bs=1048576 count=4 2>/dev/null

REQUEST_LOG="$E2E_TMPDIR/requests.jsonl"

start_http_server --port 18091 --dir "$E2E_TMPDIR" \
  --log-requests "$REQUEST_LOG"

# 1. Split download: file sha256 matches original
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --split=4 --max-connection-per-server=4 --min-split-size=1M \
  "http://127.0.0.1:$HTTP_PORT/bigfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/bigfile.bin" \
  "$E2E_TMPDIR/out1/bigfile.bin" \
  "split download: file sha256 matches original"

# 2. Verify server received >= 2 Range requests
range_count=$(grep -c '"range": "bytes=' "$REQUEST_LOG" 2>/dev/null || echo 0)
if [[ "$range_count" -ge 2 ]]; then
  tap_ok "split download: server received >= 2 Range requests ($range_count)"
else
  tap_fail "split download: server received >= 2 Range requests (got $range_count)"
fi

# 3. Single-connection control: file sha256 matches
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --split=1 \
  "http://127.0.0.1:$HTTP_PORT/bigfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/bigfile.bin" \
  "$E2E_TMPDIR/out2/bigfile.bin" \
  "single-connection download: file sha256 matches"

stop_http_server
tap_summary
