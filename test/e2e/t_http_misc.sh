#!/usr/bin/env bash
# e2e: HTTP miscellaneous feature tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# 1. Content-Disposition — server sends filename, file saved with that name
start_http_server --port 18097 --dir "$E2E_TMPDIR" \
  --content-disposition "renamed.dat"
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --content-disposition-default-utf8=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out1/renamed.dat" \
  "misc: Content-Disposition renames file to renamed.dat"
stop_http_server

# 2. --auto-file-renaming=true — download same URL twice, second gets .1 suffix
start_http_server --port 18097 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=false \
  --auto-file-renaming=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=false \
  --auto-file-renaming=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out2/testfile.1.bin" \
  "misc: --auto-file-renaming creates testfile.1.bin on second download"
stop_http_server

# 3. --dry-run=true — exit 0, no file created
start_http_server --port 18097 --dir "$E2E_TMPDIR"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --dry-run=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_exit_code "$rc" 0 \
  "misc: --dry-run exits with code 0"
assert_file_not_exists "$E2E_TMPDIR/out3/testfile.bin" \
  "misc: --dry-run does not create output file"
stop_http_server

# 4. --remote-time=true — file mtime matches Last-Modified from server
start_http_server --port 18097 --dir "$E2E_TMPDIR" \
  --last-modified "Thu, 01 Jan 2022 00:00:00 GMT"
"$ARIA2C" -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --remote-time=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_mtime_year "$E2E_TMPDIR/out4/testfile.bin" "2022" \
  "misc: --remote-time sets file mtime year to 2022"
stop_http_server

# 5. --http-accept-gzip=true — server sends gzip, aria2c decompresses
start_http_server --port 18097 --dir "$E2E_TMPDIR" --gzip
"$ARIA2C" -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --http-accept-gzip=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/testfile.bin" \
  "$E2E_TMPDIR/out5/testfile.bin" \
  "misc: --http-accept-gzip decompresses gzip response correctly"
stop_http_server

tap_summary
