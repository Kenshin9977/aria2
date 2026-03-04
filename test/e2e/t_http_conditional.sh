#!/usr/bin/env bash
# e2e: HTTP conditional GET tests (Last-Modified, If-Modified-Since, 304)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_http_server --port 18094 --dir "$E2E_TMPDIR" \
  --last-modified "Sat, 01 Jan 2022 00:00:00 GMT"

# 1. First download creates file — content correct
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --conditional-get=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "conditional GET: first download creates correct file"

# 2. Set file mtime to Jan 2023 (newer than Last-Modified),
#    re-download with --conditional-get=true — 304 skip, file unchanged
touch -t 202301010000.00 "$E2E_TMPDIR/out1/testfile.bin"
mtime_before=$(get_mtime_epoch "$E2E_TMPDIR/out1/testfile.bin")
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --conditional-get=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
mtime_after=$(get_mtime_epoch "$E2E_TMPDIR/out1/testfile.bin")
if [[ "$mtime_before" == "$mtime_after" ]]; then
  tap_ok "conditional GET: file mtime unchanged (304 skip)"
else
  tap_fail "conditional GET: file mtime unchanged (304 skip) (mtime changed from $mtime_before to $mtime_after)"
fi

stop_http_server
tap_summary
