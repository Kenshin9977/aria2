#!/usr/bin/env bash
# e2e: Session save/restore tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# 1. --save-session with failing download (503) — session file created
start_http_server --port 18106 --dir "$E2E_TMPDIR" --fail-first-n 999
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --save-session="$E2E_TMPDIR/session.txt" \
  --max-tries=1 --retry-wait=0 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_file_exists "$E2E_TMPDIR/session.txt" \
  "session: --save-session creates session file on failure"

# 2. Session file contains the failed URL
assert_file_contains "$E2E_TMPDIR/session.txt" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" \
  "session: session file contains the failed URL"
stop_http_server

# 3. Reload session with working server — file downloaded
# Session file preserves original dir=, so file goes to out1
start_http_server --port 18106 --dir "$E2E_TMPDIR"
"$ARIA2C" --allow-overwrite=true --no-conf \
  -i "$E2E_TMPDIR/session.txt" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out1/testfile.bin" \
  "session: reload session downloads the file successfully"
stop_http_server

# 4. --save-session with successful download — session file exists
start_http_server --port 18106 --dir "$E2E_TMPDIR"
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --save-session="$E2E_TMPDIR/session_ok.txt" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/session_ok.txt" \
  "session: --save-session creates session file on success"
stop_http_server

tap_summary
