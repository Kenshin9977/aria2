#!/usr/bin/env bash
# e2e: RPC secret token authentication tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18103 --dir "$E2E_TMPDIR"
start_aria2_rpc 16804 --dir="$E2E_TMPDIR/downloads" \
  --rpc-secret=mysecret

# 1. Valid token — getVersion succeeds
resp=$(rpc_call $RPC_PORT "aria2.getVersion" '["token:mysecret"]')
if echo "$resp" | grep -q '"version"'; then
  tap_ok "valid token: getVersion succeeds"
else
  tap_fail "valid token: getVersion succeeds (response: $resp)"
fi

# 2. Missing token — returns error (Unauthorized)
resp=$(rpc_call $RPC_PORT "aria2.getVersion")
if echo "$resp" | grep -q '"error"'; then
  tap_ok "missing token: returns error"
else
  tap_fail "missing token: returns error (response: $resp)"
fi

# 3. Wrong token — returns error
resp=$(rpc_call $RPC_PORT "aria2.getVersion" '["token:wrongsecret"]')
if echo "$resp" | grep -q '"error"'; then
  tap_ok "wrong token: returns error"
else
  tap_fail "wrong token: returns error (response: $resp)"
fi

stop_aria2_rpc
tap_summary
