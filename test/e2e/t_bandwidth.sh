#!/usr/bin/env bash
# e2e: Bandwidth limiting and parallel download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/100k.bin" bs=1024 count=100 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/small.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file1.dat" bs=1024 count=2 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file2.dat" bs=1024 count=3 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file3.dat" bs=1024 count=4 2>/dev/null

# 1. --max-download-limit=50K on 100KB file — file correct size
start_http_server --port 18107 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --max-download-limit=50K \
  "http://127.0.0.1:$HTTP_PORT/100k.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/100k.bin" 102400 \
  "bandwidth: --max-download-limit=50K downloads file with correct size"
stop_http_server

# 2. --max-overall-download-limit=50K on 100KB file — file correct size
start_http_server --port 18107 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true --no-conf \
  --max-overall-download-limit=50K \
  "http://127.0.0.1:$HTTP_PORT/100k.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/100k.bin" 102400 \
  "bandwidth: --max-overall-download-limit=50K downloads file correctly"
stop_http_server

# 3. -j 3 -Z with 3 URIs — all 3 files downloaded
start_http_server --port 18107 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  -j 3 -Z \
  "http://127.0.0.1:$HTTP_PORT/file1.dat" \
  "http://127.0.0.1:$HTTP_PORT/file2.dat" \
  "http://127.0.0.1:$HTTP_PORT/file3.dat" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out3/file1.dat" && \
      -f "$E2E_TMPDIR/out3/file2.dat" && \
      -f "$E2E_TMPDIR/out3/file3.dat" ]]; then
  tap_ok "bandwidth: -j 3 -Z downloads all 3 files in parallel"
else
  tap_fail "bandwidth: -j 3 -Z downloads all 3 files in parallel"
fi
stop_http_server

tap_summary
