#!/usr/bin/env bash
# e2e: RPC lifecycle tests (addUri, tellActive, tellWaiting, tellStopped,
#      getFiles, getUris, getGlobalStat, shutdown)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 8
make_tempdir

# Create test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18100 --dir "$E2E_TMPDIR" --delay 3
start_aria2_rpc 16801 --dir="$E2E_TMPDIR/downloads" \
  --max-concurrent-downloads=1

# 1. addUri with custom output name
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"custom.bin\"}]")
gid1=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
if [[ -n "$gid1" ]]; then
  # Wait for the slow download to complete (3s delay + transfer)
  sleep 5
  if [[ -f "$E2E_TMPDIR/downloads/custom.bin" ]]; then
    tap_ok "addUri with custom output name"
  else
    tap_fail "addUri with custom output name (file not found)"
  fi
else
  tap_fail "addUri with custom output name (no gid returned)"
fi

# 2. tellActive — add a URI with delay, immediately check active list
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"active_test.bin\"}]")
gid2=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.tellActive")
if echo "$resp" | grep -q '"result"'; then
  tap_ok "tellActive returns valid response"
else
  tap_fail "tellActive returns valid response (response: $resp)"
fi
# Wait for this download to finish before next tests
sleep 5

# 3. tellWaiting — add 2 URIs quickly with max-concurrent=1
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"wait1.bin\"}]")
gid_w1=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"wait2.bin\"}]")
gid_w2=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.3
resp=$(rpc_call $RPC_PORT "aria2.tellWaiting" "[0,10]")
if echo "$resp" | grep -q '"gid"'; then
  tap_ok "tellWaiting returns waiting downloads"
else
  tap_fail "tellWaiting returns waiting downloads (response: $resp)"
fi
# Wait for both to complete
sleep 8

# 4. tellStopped — previous downloads should be in stopped list
resp=$(rpc_call $RPC_PORT "aria2.tellStopped" "[0,10]")
if echo "$resp" | grep -q '"status":"complete"'; then
  tap_ok "tellStopped shows completed downloads"
else
  tap_fail "tellStopped shows completed downloads (response: $resp)"
fi

# 5. getFiles — use a completed gid
resp=$(rpc_call $RPC_PORT "aria2.getFiles" "[\"$gid1\"]")
if echo "$resp" | grep -q '"path"' && echo "$resp" | grep -q '"length"'; then
  tap_ok "getFiles returns path and length"
else
  tap_fail "getFiles returns path and length (response: $resp)"
fi

# 6. getUris — add a new URI with delay and query while active
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"uri_test.bin\"}]")
gid_uri=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.getUris" "[\"$gid_uri\"]")
if echo "$resp" | grep -q '"uri"' && echo "$resp" | grep -q '"status"'; then
  tap_ok "getUris returns uri and status"
else
  tap_fail "getUris returns uri and status (response: $resp)"
fi
sleep 5

# 7. getGlobalStat
resp=$(rpc_call $RPC_PORT "aria2.getGlobalStat")
if echo "$resp" | grep -q '"downloadSpeed"' && \
   echo "$resp" | grep -q '"numActive"' && \
   echo "$resp" | grep -q '"numWaiting"' && \
   echo "$resp" | grep -q '"numStopped"'; then
  tap_ok "getGlobalStat returns expected fields"
else
  tap_fail "getGlobalStat returns expected fields (response: $resp)"
fi

# 8. shutdown
resp=$(rpc_call $RPC_PORT "aria2.shutdown")
if echo "$resp" | grep -q '"OK"'; then
  tap_ok "shutdown returns OK"
else
  tap_fail "shutdown returns OK (response: $resp)"
fi
sleep 0.5
_rpc_aria2_pid=""

tap_summary
