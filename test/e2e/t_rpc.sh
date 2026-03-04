#!/usr/bin/env bash
# e2e: JSON-RPC tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

RPC_PORT=16800
_aria2_pid=""

# Override cleanup to also kill aria2c RPC daemon
_rpc_cleanup() {
  if [[ -n "$_aria2_pid" ]]; then
    kill "$_aria2_pid" 2>/dev/null || true
    wait "$_aria2_pid" 2>/dev/null || true
  fi
  _e2e_cleanup
}
trap _rpc_cleanup EXIT

# Create test fixture for download
echo "rpc-download-content" > "$E2E_TMPDIR/rpcfile.txt"
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18086 --dir "$E2E_TMPDIR"

# Start aria2c in RPC daemon mode
"$ARIA2C" --enable-rpc --rpc-listen-port=$RPC_PORT \
  --rpc-listen-all=false --dir="$E2E_TMPDIR/downloads" \
  --no-conf --daemon=false --quiet=true &
_aria2_pid=$!

# Wait for RPC to be ready
tries=0
while ! (echo >/dev/tcp/127.0.0.1/$RPC_PORT) 2>/dev/null; do
  tries=$((tries + 1))
  if [[ $tries -ge 30 ]]; then
    tap_fail "rpc: aria2c RPC server failed to start"
    tap_fail "rpc: getVersion (skipped)"
    tap_fail "rpc: addUri+tellStatus (skipped)"
    tap_fail "rpc: shutdown (skipped)"
    exit 1
  fi
  sleep 0.1
done

# 1. aria2c RPC server started
tap_ok "rpc: aria2c RPC server started"

# 2. aria2.getVersion
resp=$(curl -s "http://127.0.0.1:$RPC_PORT/jsonrpc" \
  -d '{"jsonrpc":"2.0","id":"2","method":"aria2.getVersion"}')
if echo "$resp" | grep -q '"version"'; then
  tap_ok "rpc: getVersion returns version"
else
  tap_fail "rpc: getVersion returns version (response: $resp)"
fi

# 3. aria2.addUri + aria2.tellStatus
resp=$(curl -s "http://127.0.0.1:$RPC_PORT/jsonrpc" \
  -d "{\"jsonrpc\":\"2.0\",\"id\":\"3\",\"method\":\"aria2.addUri\",\"params\":[[\"http://127.0.0.1:$HTTP_PORT/rpcfile.txt\"]]}")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
if [[ -n "$gid" ]]; then
  # Brief wait then check status
  sleep 1
  resp=$(curl -s "http://127.0.0.1:$RPC_PORT/jsonrpc" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":\"4\",\"method\":\"aria2.tellStatus\",\"params\":[\"$gid\"]}")
  if echo "$resp" | grep -qE '"status":"(active|complete)"'; then
    tap_ok "rpc: addUri+tellStatus shows active or complete"
  else
    tap_fail "rpc: addUri+tellStatus shows active or complete (response: $resp)"
  fi
else
  tap_fail "rpc: addUri+tellStatus (failed to get gid)"
fi

# 4. aria2.shutdown
resp=$(curl -s "http://127.0.0.1:$RPC_PORT/jsonrpc" \
  -d '{"jsonrpc":"2.0","id":"5","method":"aria2.shutdown"}')
if echo "$resp" | grep -q '"OK"'; then
  tap_ok "rpc: shutdown returns OK"
else
  tap_fail "rpc: shutdown returns OK (response: $resp)"
fi

# Wait for aria2c to shut down cleanly
sleep 0.5
_aria2_pid=""

tap_summary
