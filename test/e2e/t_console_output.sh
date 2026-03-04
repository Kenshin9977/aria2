#!/usr/bin/env bash
# e2e: Console output and resume option tests (show-console-readout,
#      summary-interval, stderr, always-resume, human-readable)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

# Create a 4KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
FILE_HASH=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')

# ── Test 1: --show-console-readout=false suppresses progress ──────────────
start_http_server --port 18390 --dir "$E2E_TMPDIR"
output=$("$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --show-console-readout=false \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" 2>&1) || true
if echo "$output" | grep -q '\[#'; then
  tap_fail "show-console-readout=false suppresses progress lines"
else
  tap_ok "show-console-readout=false suppresses progress lines"
fi
stop_http_server

# ── Test 2: --summary-interval=0 completes without periodic summaries ─────
start_http_server --port 18390 --dir "$E2E_TMPDIR"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --summary-interval=0 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 4096 \
  "summary-interval=0 accepted, download completes"
stop_http_server

# ── Test 3: --stderr=true redirects console output to stderr ──────────────
start_http_server --port 18390 --dir "$E2E_TMPDIR"
stdout_out=$("$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --stderr=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" 2>/dev/null) || true
if [[ -z "$stdout_out" ]]; then
  tap_ok "stderr=true redirects console output to stderr (stdout empty)"
else
  tap_fail "stderr=true redirects console output to stderr (stdout not empty)"
fi
stop_http_server

# ── Test 4: --always-resume=true --continue=true resumes partial ──────────
# First pass: server disconnects after 512 bytes
start_http_server --port 18390 --dir "$E2E_TMPDIR" \
  --disconnect-after-bytes 512
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || true
stop_http_server
# Second pass: full server, resume partial download
start_http_server --port 18390 --dir "$E2E_TMPDIR"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" \
  --always-resume=true --continue=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_hash "$E2E_TMPDIR/out4/testfile.bin" sha-256 "$FILE_HASH" \
  "always-resume=true --continue=true resumes partial download"
stop_http_server

# ── Test 5: --human-readable=false accepted without error ──────────────────
start_http_server --port 18390 --dir "$E2E_TMPDIR"
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --human-readable=false \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out5/testfile.bin" 4096 \
  "human-readable=false accepted, download completes"
stop_http_server

tap_summary
