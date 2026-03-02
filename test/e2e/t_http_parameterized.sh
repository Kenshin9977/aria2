#!/usr/bin/env bash
# e2e: HTTP parameterized URI and force-sequential tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 7
make_tempdir

# Create test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/file1.dat" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file2.dat" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/data1.txt" bs=512 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/data2.txt" bs=512 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/data3.txt" bs=512 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/a.bin" bs=256 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/b.bin" bs=256 count=1 2>/dev/null

start_http_server --port 18098 --dir "$E2E_TMPDIR"

# 1. Parameterized URI with braces — {host1,host2} treated as mirrors
# Braces in aria2c mean the same file from multiple mirrors
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true --no-conf \
  -P "http://127.0.0.1:$HTTP_PORT/{file1,file2}.dat" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out1/file1.dat" \
  "parameterized: brace expansion downloads file (mirrors)"
# file2.dat used as mirror, so file1.dat is the saved name
assert_file_size "$E2E_TMPDIR/out1/file1.dat" 1024 \
  "parameterized: brace expansion file size correct"

# 2. Numeric range — data[1-3].txt — needs -Z for separate downloads
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true --no-conf \
  -P -Z "http://127.0.0.1:$HTTP_PORT/data[1-3].txt" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out2/data1.txt" \
  "parameterized: numeric range downloads data1.txt"
assert_file_exists "$E2E_TMPDIR/out2/data2.txt" \
  "parameterized: numeric range downloads data2.txt"
assert_file_exists "$E2E_TMPDIR/out2/data3.txt" \
  "parameterized: numeric range downloads data3.txt"

# 3. -Z (force-sequential) — download multiple URIs sequentially
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  -Z \
  "http://127.0.0.1:$HTTP_PORT/a.bin" \
  "http://127.0.0.1:$HTTP_PORT/b.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out3/a.bin" \
  "parameterized: -Z downloads a.bin"
assert_file_exists "$E2E_TMPDIR/out3/b.bin" \
  "parameterized: -Z downloads b.bin"

stop_http_server
tap_summary
