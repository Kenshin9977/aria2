#!/usr/bin/env bash
# e2e: HTTP basic download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_http_server --port 18081 --dir "$E2E_TMPDIR"

# 1. Basic download, verify file exists and size
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "basic download: file size matches (1024 bytes)"

# 2. Download with --out=custom_name
"$ARIA2C" -d "$E2E_TMPDIR/out2" --out=custom_name.dat \
  --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out2/custom_name.dat" \
  "--out renames output file"

# 3. Download with --dir=subdir
mkdir -p "$E2E_TMPDIR/subdir"
"$ARIA2C" -d "$E2E_TMPDIR/subdir" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/subdir/testfile.bin" \
  "--dir places file in correct directory"

stop_http_server
tap_summary
