#!/usr/bin/env bash
# e2e: extended conditional GET tests (re-download, fresh, skip scenarios)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_http_server --port 18127 --dir "$E2E_TMPDIR" \
  --last-modified "Sat, 01 Jan 2022 00:00:00 GMT"

# 1. Local file older than server Last-Modified — file re-downloaded
mkdir -p "$E2E_TMPDIR/out1"
# Create a stale local file with old content and old mtime
echo "stale-placeholder-content" > "$E2E_TMPDIR/out1/testfile.bin"
touch -t 202001010000.00 "$E2E_TMPDIR/out1/testfile.bin"
"$ARIA2C" -d "$E2E_TMPDIR/out1" --no-conf --allow-overwrite=true \
  --conditional-get=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/testfile.bin" \
  "$E2E_TMPDIR/out1/testfile.bin" \
  "conditional-get: older local file re-downloaded from server"

# 2. No local file — fresh download succeeds
mkdir -p "$E2E_TMPDIR/out2"
"$ARIA2C" -d "$E2E_TMPDIR/out2" --no-conf --allow-overwrite=true \
  --conditional-get=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/testfile.bin" \
  "$E2E_TMPDIR/out2/testfile.bin" \
  "conditional-get: fresh download with no local file succeeds"

# 3. Local file newer than server Last-Modified — NOT re-downloaded
mkdir -p "$E2E_TMPDIR/out3"
# Pre-create a file with different content but newer mtime
echo "local-newer-content" > "$E2E_TMPDIR/out3/testfile.bin"
touch -t 202401010000.00 "$E2E_TMPDIR/out3/testfile.bin"
mtime_before=$(stat -f %m "$E2E_TMPDIR/out3/testfile.bin")
"$ARIA2C" -d "$E2E_TMPDIR/out3" --no-conf --allow-overwrite=true \
  --conditional-get=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
mtime_after=$(stat -f %m "$E2E_TMPDIR/out3/testfile.bin")
if [[ "$mtime_before" == "$mtime_after" ]]; then
  tap_ok "conditional-get: newer local file not re-downloaded (mtime unchanged)"
else
  tap_fail "conditional-get: newer local file not re-downloaded (mtime changed from $mtime_before to $mtime_after)"
fi

stop_http_server
tap_summary
