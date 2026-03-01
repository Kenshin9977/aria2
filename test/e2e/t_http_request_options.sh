#!/usr/bin/env bash
# e2e: Piece selection and checksum option tests (stream-piece-selector,
#      realtime-chunk-checksum, hash-check-only)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

# Create a 512KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" \
  bs=1024 count=512 2>/dev/null
FILE_HASH=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')

start_http_server --port 18310 --dir "$E2E_TMPDIR"

# ── Test 1: --stream-piece-selector=geom completes correctly ──────────────
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --stream-piece-selector=geom --split=4 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_hash "$E2E_TMPDIR/out1/testfile.bin" sha-256 "$FILE_HASH" \
  "stream-piece-selector=geom downloads correctly"

# ── Test 2: --stream-piece-selector=random completes correctly ────────────
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --stream-piece-selector=random --split=4 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_hash "$E2E_TMPDIR/out2/testfile.bin" sha-256 "$FILE_HASH" \
  "stream-piece-selector=random downloads correctly"

# ── Test 3: --realtime-chunk-checksum=true with checksum succeeds ─────────
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --realtime-chunk-checksum=true \
  --checksum="sha-256=$FILE_HASH" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_hash "$E2E_TMPDIR/out3/testfile.bin" sha-256 "$FILE_HASH" \
  "realtime-chunk-checksum=true with checksum downloads correctly"

# ── Test 4: --realtime-chunk-checksum=false download succeeds ─────────────
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --realtime-chunk-checksum=false \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_hash "$E2E_TMPDIR/out4/testfile.bin" sha-256 "$FILE_HASH" \
  "realtime-chunk-checksum=false downloads correctly"

# ── Test 5: --hash-check-only=true verifies without re-downloading ────────
# First download normally
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
# Then verify with hash-check-only (no --allow-overwrite so existing file kept)
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" \
  --hash-check-only=true --check-integrity=true \
  --checksum="sha-256=$FILE_HASH" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_exit_code "$rc" 0 \
  "hash-check-only=true verifies existing file without re-downloading"

stop_http_server
tap_summary
