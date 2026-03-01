#!/usr/bin/env bash
# e2e: XML-RPC interface tests (getVersion, addUri, tellStatus, shutdown)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18116 --dir "$E2E_TMPDIR" --delay 2
start_aria2_rpc 16807 --dir="$E2E_TMPDIR/downloads"

# 1. aria2.getVersion via XML-RPC — version string present
resp=$(xmlrpc_call $RPC_PORT "aria2.getVersion")
if echo "$resp" | grep -q "version"; then
  tap_ok "xmlrpc: aria2.getVersion returns version string"
else
  tap_fail "xmlrpc: aria2.getVersion returns version string (response: $resp)"
fi

# 2. aria2.addUri via XML-RPC — GID returned
resp=$(xmlrpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:18116/testfile.bin\"]]")
gid=$(echo "$resp" | sed -n 's/.*"\([0-9a-f]\{16\}\)".*/\1/p')
if [[ -n "$gid" ]]; then
  tap_ok "xmlrpc: aria2.addUri returns GID ($gid)"
else
  tap_fail "xmlrpc: aria2.addUri returns GID (response: $resp)"
fi

# 3. Wait for download, then aria2.tellStatus — status returned
sleep 4
resp=$(xmlrpc_call $RPC_PORT "aria2.tellStatus" "[\"$gid\"]")
if echo "$resp" | grep -q "status"; then
  tap_ok "xmlrpc: aria2.tellStatus returns status for GID"
else
  tap_fail "xmlrpc: aria2.tellStatus returns status for GID (response: $resp)"
fi

# 4. aria2.shutdown via XML-RPC — OK response
resp=$(xmlrpc_call $RPC_PORT "aria2.shutdown")
if echo "$resp" | grep -q "OK"; then
  tap_ok "xmlrpc: aria2.shutdown returns OK"
else
  tap_fail "xmlrpc: aria2.shutdown returns OK (response: $resp)"
fi
sleep 0.5
_rpc_aria2_pid=""

stop_http_server
tap_summary
