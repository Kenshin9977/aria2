#!/usr/bin/env bash
# e2e: RPC method tests (getSessionInfo, changePosition, changeUri,
#      purgeDownloadResult, removeDownloadResult, forceShutdown)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

# Use --delay 3 so downloads stay active long enough for queue manipulation
start_http_server --port 18220 --dir "$E2E_TMPDIR" --delay 3
start_aria2_rpc 16810 --dir="$E2E_TMPDIR/downloads" \
  --max-concurrent-downloads=1

# 1. getSessionInfo returns sessionId
resp=$(rpc_call $RPC_PORT "aria2.getSessionInfo")
if echo "$resp" | grep -q '"sessionId"'; then
  tap_ok "getSessionInfo returns sessionId"
else
  tap_fail "getSessionInfo returns sessionId (response: $resp)"
fi

# 2. changePosition moves download in queue (POS_SET)
# With --delay 3 and --max-concurrent-downloads=1, gid1 stays active
# while gid2 sits in the waiting queue, allowing changePosition to work.
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"pos1.bin\"}]")
gid1=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"pos2.bin\"}]")
gid2=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.changePosition" "[\"$gid2\",0,\"POS_SET\"]")
if echo "$resp" | grep -q '"result"'; then
  tap_ok "changePosition moves download in queue"
else
  tap_fail "changePosition moves download in queue (response: $resp)"
fi
# Wait for both downloads to complete
sleep 8

# 3. changeUri adds URI to download
# Add URI in paused state so changeUri can modify it before it starts.
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"churi.bin\",\"pause\":\"true\"}]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.3
resp=$(rpc_call $RPC_PORT "aria2.changeUri" \
  "[\"$gid\",1,[],[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"]]")
if echo "$resp" | grep -q '"result"'; then
  tap_ok "changeUri adds URI to download"
else
  tap_fail "changeUri adds URI to download (response: $resp)"
fi
# Unpause and wait for download to complete
rpc_call $RPC_PORT "aria2.unpause" "[\"$gid\"]" >/dev/null 2>&1
sleep 5

# 4. purgeDownloadResult clears completed results
resp=$(rpc_call $RPC_PORT "aria2.purgeDownloadResult")
if echo "$resp" | grep -q '"OK"'; then
  tap_ok "purgeDownloadResult clears completed results"
else
  tap_fail "purgeDownloadResult clears completed results (response: $resp)"
fi

# 5. removeDownloadResult removes specific result
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"rmresult.bin\"}]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 3
resp=$(rpc_call $RPC_PORT "aria2.removeDownloadResult" "[\"$gid\"]")
if echo "$resp" | grep -q '"OK"'; then
  tap_ok "removeDownloadResult removes specific result"
else
  tap_fail "removeDownloadResult removes specific result (response: $resp)"
fi

# 6. forceShutdown returns OK
resp=$(rpc_call $RPC_PORT "aria2.forceShutdown")
if echo "$resp" | grep -q '"OK"'; then
  tap_ok "forceShutdown returns OK"
else
  tap_fail "forceShutdown returns OK (response: $resp)"
fi
sleep 0.5
_rpc_aria2_pid=""

stop_http_server
tap_summary
