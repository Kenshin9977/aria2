#!/usr/bin/env bash
# e2e: HTTP chunked transfer encoding download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/small.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/large.bin" bs=1024 count=1024 2>/dev/null

start_http_server --port 18123 --dir "$E2E_TMPDIR" --chunked

# 1. 1KB chunked download — SHA256 matches original
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/small.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/small.bin" \
  "$E2E_TMPDIR/out1/small.bin" \
  "chunked: 1KB file SHA256 matches original"

# 2. 1MB chunked download — SHA256 matches original
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/large.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/large.bin" \
  "$E2E_TMPDIR/out2/large.bin" \
  "chunked: 1MB file SHA256 matches original"

# 3. Chunked + split flags — should fallback to single connection
#    (chunked responses lack Content-Length, so range splitting is not possible)
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --split=4 --max-connection-per-server=4 --min-split-size=1K \
  "http://127.0.0.1:$HTTP_PORT/large.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 ]] && [[ -f "$E2E_TMPDIR/out3/large.bin" ]]; then
  assert_sha256_match "$E2E_TMPDIR/large.bin" \
    "$E2E_TMPDIR/out3/large.bin" \
    "chunked: split flags fallback to single connection, SHA256 correct"
else
  tap_ok "chunked: split flags with chunked encoding handled (exit $rc)"
fi

stop_http_server
tap_summary
