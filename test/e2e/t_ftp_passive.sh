#!/usr/bin/env bash
# e2e: FTP passive mode download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Create 1KB test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/file1.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file2.bin" bs=1024 count=1 2>/dev/null

start_ftp_server --port 18100 --dir "$E2E_TMPDIR" --pasv-range 18101-18110

# 1. Explicit --ftp-pasv download — file size correct
"$ARIA2C" ftp://127.0.0.1:$FTP_PORT/file1.bin \
  -d "$E2E_TMPDIR/out1" --allow-overwrite=true --no-conf \
  --ftp-pasv >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/file1.bin" 1024 \
  "FTP passive mode: file size correct (1024)"

# 2. Download a second file — verify FTP works for multiple files
"$ARIA2C" ftp://127.0.0.1:$FTP_PORT/file2.bin \
  -d "$E2E_TMPDIR/out2" --allow-overwrite=true --no-conf \
  --ftp-pasv >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/file2.bin" 1024 \
  "FTP passive mode: second file download correct (1024)"

stop_ftp_server
tap_summary
