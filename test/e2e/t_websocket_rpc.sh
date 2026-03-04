#!/usr/bin/env bash
# e2e: WebSocket JSON-RPC interface tests (handshake, getVersion, addUri, tellStatus)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18117 --dir "$E2E_TMPDIR" --delay 2
start_aria2_rpc 16808 --dir="$E2E_TMPDIR/downloads"

# 1. WebSocket handshake — ws_rpc_call returns without error
resp=$(ws_rpc_call $RPC_PORT "aria2.getVersion")
if [[ -n "$resp" ]]; then
  tap_ok "websocket: handshake succeeds"
else
  tap_fail "websocket: handshake succeeds (empty response)"
fi

# 2. aria2.getVersion via WebSocket — version present
if echo "$resp" | grep -q "version"; then
  tap_ok "websocket: aria2.getVersion returns version string"
else
  tap_fail "websocket: aria2.getVersion returns version string (response: $resp)"
fi

# 3. aria2.addUri via WebSocket — GID returned
resp=$(ws_rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:18117/testfile.bin\"]]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([0-9a-f]\{16\}\)".*/\1/p')
if [[ -n "$gid" ]]; then
  tap_ok "websocket: aria2.addUri returns GID ($gid)"
else
  tap_fail "websocket: aria2.addUri returns GID (response: $resp)"
fi

# 4. Wait for download, then aria2.tellStatus via WebSocket — status complete
sleep 2
resp=$(ws_rpc_call $RPC_PORT "aria2.tellStatus" "[\"$gid\"]")
if echo "$resp" | grep -q "complete"; then
  tap_ok "websocket: aria2.tellStatus shows complete after download"
else
  tap_fail "websocket: aria2.tellStatus shows complete after download (response: $resp)"
fi

# Graceful shutdown via HTTP RPC (simpler than WS for cleanup)
rpc_call $RPC_PORT "aria2.shutdown" >/dev/null 2>&1 || true
sleep 0.5
_rpc_aria2_pid=""

stop_http_server
tap_summary
