#!/usr/bin/env bash
# e2e: HTTP resume (Range) tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_http_server --port 18082 --dir "$E2E_TMPDIR"

# 1. Create partial file (first 512 bytes), resume with -c
mkdir -p "$E2E_TMPDIR/resume1"
dd if="$E2E_TMPDIR/testfile.bin" of="$E2E_TMPDIR/resume1/testfile.bin" \
  bs=512 count=1 2>/dev/null
"$ARIA2C" -d "$E2E_TMPDIR/resume1" -c --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/resume1/testfile.bin" 1024 \
  "resume: partial file completed to full size"

# 2. Resume when file is already complete (no-op)
mkdir -p "$E2E_TMPDIR/resume2"
cp "$E2E_TMPDIR/testfile.bin" "$E2E_TMPDIR/resume2/testfile.bin"
"$ARIA2C" -d "$E2E_TMPDIR/resume2" -c --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/resume2/testfile.bin" 1024 \
  "resume: already-complete file stays correct size"

# 3. Verify resumed file matches original
mkdir -p "$E2E_TMPDIR/resume3"
dd if="$E2E_TMPDIR/testfile.bin" of="$E2E_TMPDIR/resume3/testfile.bin" \
  bs=512 count=1 2>/dev/null
"$ARIA2C" -d "$E2E_TMPDIR/resume3" -c --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
original_hash=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | cut -d' ' -f1)
resumed_hash=$(shasum -a 256 "$E2E_TMPDIR/resume3/testfile.bin" | cut -d' ' -f1)
if [[ "$original_hash" == "$resumed_hash" ]]; then
  tap_ok "resume: file content matches original after resume"
else
  tap_fail "resume: file content matches original after resume"
fi

stop_http_server
tap_summary
