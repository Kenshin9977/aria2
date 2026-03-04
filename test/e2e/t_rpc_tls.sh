#!/usr/bin/env bash
# e2e: RPC over TLS tests (HTTPS JSON-RPC, WSS WebSocket)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Generate self-signed certificate
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/rpc_key.pem" \
  -out "$E2E_TMPDIR/rpc_cert.pem" \
  -days 1 -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1" 2>/dev/null

# Start aria2 RPC with TLS
"$ARIA2C" --enable-rpc --rpc-listen-port=16813 \
  --rpc-listen-all=false --no-conf --daemon=false --quiet=true \
  --rpc-secure=true \
  --rpc-certificate="$E2E_TMPDIR/rpc_cert.pem" \
  --rpc-private-key="$E2E_TMPDIR/rpc_key.pem" &
_rpc_aria2_pid=$!
RPC_PORT=16813

# Wait for TLS RPC to be ready
tries=0
while ! (echo >/dev/tcp/127.0.0.1/$RPC_PORT) 2>/dev/null; do
  tries=$((tries + 1))
  if [[ $tries -ge 30 ]]; then
    echo "ERROR: aria2c TLS RPC failed to start on port $RPC_PORT" >&2
    exit 1
  fi
  sleep 0.1
done

# 1. JSON-RPC over HTTPS — getVersion succeeds
resp=$(curl -sk "https://127.0.0.1:$RPC_PORT/jsonrpc" \
  -d '{"jsonrpc":"2.0","id":"1","method":"aria2.getVersion"}')
if echo "$resp" | grep -q '"version"'; then
  tap_ok "JSON-RPC over HTTPS: getVersion succeeds"
else
  tap_fail "JSON-RPC over HTTPS: getVersion succeeds (response: $resp)"
fi

# 2. JSON-RPC with --cacert — trusted CA cert works
resp=$(curl -s --cacert "$E2E_TMPDIR/rpc_cert.pem" \
  "https://127.0.0.1:$RPC_PORT/jsonrpc" \
  -d '{"jsonrpc":"2.0","id":"1","method":"aria2.getVersion"}')
if echo "$resp" | grep -q '"version"'; then
  tap_ok "JSON-RPC with cacert: trusted CA works"
else
  tap_fail "JSON-RPC with cacert: trusted CA works (response: $resp)"
fi

# 3. WebSocket over wss:// — getVersion succeeds
resp=$(ws_rpc_call $RPC_PORT "aria2.getVersion" --ssl --no-verify)
if echo "$resp" | grep -q '"version"'; then
  tap_ok "WebSocket over wss://: getVersion succeeds"
else
  tap_fail "WebSocket over wss://: getVersion succeeds (response: $resp)"
fi

# 4. Plain HTTP to TLS port fails
resp=$(curl -s "http://127.0.0.1:$RPC_PORT/jsonrpc" \
  -d '{"jsonrpc":"2.0","id":"1","method":"aria2.getVersion"}' 2>&1 || true)
if echo "$resp" | grep -q '"version"'; then
  tap_fail "plain HTTP to TLS port fails (should not succeed)"
else
  tap_ok "plain HTTP to TLS port fails"
fi

stop_aria2_rpc
tap_summary
