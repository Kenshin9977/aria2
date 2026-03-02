#!/usr/bin/env bash
# e2e: FTP basic download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_ftp_server --port 18100 --dir "$E2E_TMPDIR" --pasv-range 18101-18110

# 1. Anonymous FTP download — file size correct
"$ARIA2C" ftp://127.0.0.1:$FTP_PORT/testfile.bin \
  -d "$E2E_TMPDIR/out1" --allow-overwrite=true --no-conf >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "anonymous FTP download: file size correct (1024)"

stop_ftp_server

# 2. FTP with credentials — download succeeds
start_ftp_server --port 18100 --dir "$E2E_TMPDIR" \
  --user ftpuser --passwd ftppass --pasv-range 18101-18110
"$ARIA2C" ftp://127.0.0.1:$FTP_PORT/testfile.bin \
  -d "$E2E_TMPDIR/out2" --allow-overwrite=true --no-conf \
  --ftp-user=ftpuser --ftp-passwd=ftppass >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out2/testfile.bin" \
  "FTP with credentials: download succeeds"

# 3. FTP wrong credentials — download fails
rc=0
"$ARIA2C" ftp://127.0.0.1:$FTP_PORT/testfile.bin \
  -d "$E2E_TMPDIR/out3" --allow-overwrite=true --no-conf \
  --ftp-user=wrong --ftp-passwd=wrong --max-tries=1 >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "FTP wrong credentials: download fails (exit $rc)"
else
  tap_fail "FTP wrong credentials: download fails (expected non-zero exit)"
fi

# 4. FTP download sha256 matches original
assert_sha256_match "$E2E_TMPDIR/testfile.bin" \
  "$E2E_TMPDIR/out2/testfile.bin" \
  "FTP download sha256 matches original"

stop_ftp_server
tap_summary
