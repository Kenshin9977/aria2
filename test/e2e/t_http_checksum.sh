#!/usr/bin/env bash
# e2e: HTTP checksum (Digest header) tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Create test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null
correct_hash=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | cut -d' ' -f1)

start_http_server --port 18085 --dir "$E2E_TMPDIR" --checksum "sha-256"

# 1. Download with server Digest header, aria2 verifies, succeeds
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --checksum="sha-256=$correct_hash" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "checksum: correct sha-256 passes verification"

# 2. Download with wrong --checksum, aria2 detects mismatch
run_cmd "$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --max-tries=1 --retry-wait=0 \
  --checksum="sha-256=0000000000000000000000000000000000000000000000000000000000000000" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin"
if [[ $_last_exit -ne 0 ]]; then
  tap_ok "checksum: wrong sha-256 exits non-zero"
else
  tap_fail "checksum: wrong sha-256 exits non-zero (got exit 0)"
fi

stop_http_server
tap_summary
