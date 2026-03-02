#!/usr/bin/env bash
# e2e: RPC session save/restore tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Create test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18105 --dir "$E2E_TMPDIR" --delay 5
start_aria2_rpc 16806 --dir="$E2E_TMPDIR/downloads" \
  --save-session="$E2E_TMPDIR/session.txt"

# Add a URI (slow download so it stays in the queue)
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"session_test.bin\"}]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 1

# 1. saveSession — session file is created
resp=$(rpc_call $RPC_PORT "aria2.saveSession")
if echo "$resp" | grep -q '"OK"'; then
  assert_file_exists "$E2E_TMPDIR/session.txt" \
    "saveSession creates session file"
else
  tap_fail "saveSession creates session file (RPC returned: $resp)"
fi

# 2. Session file contains the URI
assert_file_contains "$E2E_TMPDIR/session.txt" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" \
  "session file contains the download URI"

stop_aria2_rpc
tap_summary
