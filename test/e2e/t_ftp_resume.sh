#!/usr/bin/env bash
# e2e: FTP resume/continue download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_ftp_server --port 18100 --dir "$E2E_TMPDIR" --pasv-range 18101-18110

# 1. FTP resume from partial — create 512-byte partial, continue download
mkdir -p "$E2E_TMPDIR/out1"
dd if="$E2E_TMPDIR/testfile.bin" of="$E2E_TMPDIR/out1/testfile.bin" \
  bs=1 count=512 2>/dev/null
"$ARIA2C" ftp://127.0.0.1:$FTP_PORT/testfile.bin \
  -d "$E2E_TMPDIR/out1" -c --no-conf >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "FTP resume from partial: file size is 1024"

# 2. FTP already-complete file — stays 1024 bytes
mkdir -p "$E2E_TMPDIR/out2"
cp "$E2E_TMPDIR/testfile.bin" "$E2E_TMPDIR/out2/testfile.bin"
"$ARIA2C" ftp://127.0.0.1:$FTP_PORT/testfile.bin \
  -d "$E2E_TMPDIR/out2" -c --no-conf >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "FTP already-complete file: stays 1024 bytes"

# 3. FTP resumed file sha256 matches original
assert_sha256_match "$E2E_TMPDIR/testfile.bin" \
  "$E2E_TMPDIR/out1/testfile.bin" \
  "FTP resumed file sha256 matches original"

stop_ftp_server
tap_summary
