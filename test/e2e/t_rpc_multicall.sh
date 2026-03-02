#!/usr/bin/env bash
# e2e: RPC system.multicall tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18104 --dir "$E2E_TMPDIR"
start_aria2_rpc 16805 --dir="$E2E_TMPDIR/downloads"

# 1. system.multicall with getVersion + getGlobalStat — both succeed
resp=$(rpc_call $RPC_PORT "system.multicall" \
  '[[{"methodName":"aria2.getVersion","params":[]},{"methodName":"aria2.getGlobalStat","params":[]}]]')
# Response should contain both version info and downloadSpeed
if echo "$resp" | grep -q '"version"' && \
   echo "$resp" | grep -q '"downloadSpeed"'; then
  tap_ok "multicall: getVersion + getGlobalStat both succeed"
else
  tap_fail "multicall: getVersion + getGlobalStat both succeed (response: $resp)"
fi

# 2. system.multicall with valid + invalid method — mixed results
resp=$(rpc_call $RPC_PORT "system.multicall" \
  '[[{"methodName":"aria2.getVersion","params":[]},{"methodName":"aria2.nonExistentMethod","params":[]}]]')
# Response should contain version from valid call and an error from invalid
if echo "$resp" | grep -q '"version"' && \
   echo "$resp" | grep -qE '("code"|"error"|"faultCode")'; then
  tap_ok "multicall: valid + invalid gives mixed results"
else
  tap_fail "multicall: valid + invalid gives mixed results (response: $resp)"
fi

stop_aria2_rpc
tap_summary
