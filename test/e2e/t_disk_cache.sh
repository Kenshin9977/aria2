#!/usr/bin/env bash
# e2e: disk cache size tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create test fixtures: 1MB and 4MB files
dd if=/dev/urandom of="$E2E_TMPDIR/file1m.bin" bs=1024 count=1024 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file4m.bin" bs=1048576 count=4 2>/dev/null

start_http_server --port 18126 --dir "$E2E_TMPDIR"

# 1. --disk-cache=0 with 1MB file
"$ARIA2C" -d "$E2E_TMPDIR/out1" --no-conf --disk-cache=0 \
  "http://127.0.0.1:$HTTP_PORT/file1m.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/file1m.bin" "$E2E_TMPDIR/out1/file1m.bin" \
  "disk-cache=0: 1MB file SHA256 matches"

# 2. --disk-cache=16M with 1MB file
"$ARIA2C" -d "$E2E_TMPDIR/out2" --no-conf --disk-cache=16M \
  "http://127.0.0.1:$HTTP_PORT/file1m.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/file1m.bin" "$E2E_TMPDIR/out2/file1m.bin" \
  "disk-cache=16M: 1MB file SHA256 matches"

# 3. --disk-cache=1M with 4MB file
"$ARIA2C" -d "$E2E_TMPDIR/out3" --no-conf --disk-cache=1M \
  "http://127.0.0.1:$HTTP_PORT/file4m.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/file4m.bin" "$E2E_TMPDIR/out3/file4m.bin" \
  "disk-cache=1M: 4MB file SHA256 matches"

stop_http_server
tap_summary
