#!/usr/bin/env bash
# e2e: RPC pause/unpause/remove tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

# Create test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18102 --dir "$E2E_TMPDIR" --delay 5
start_aria2_rpc 16803 --dir="$E2E_TMPDIR/downloads"

# 1. pause(gid) — add URI, immediately pause, check status
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"pause1.bin\"}]")
gid1=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.3
resp=$(rpc_call $RPC_PORT "aria2.pause" "[\"$gid1\"]")
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.tellStatus" "[\"$gid1\"]")
if echo "$resp" | grep -q '"status":"paused"'; then
  tap_ok "pause changes status to paused"
else
  tap_fail "pause changes status to paused (response: $resp)"
fi

# 2. unpause(gid) — unpause and check status becomes active or waiting
resp=$(rpc_call $RPC_PORT "aria2.unpause" "[\"$gid1\"]")
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.tellStatus" "[\"$gid1\"]")
if echo "$resp" | grep -qE '"status":"(active|waiting)"'; then
  tap_ok "unpause restores status to active or waiting"
else
  tap_fail "unpause restores status to active or waiting (response: $resp)"
fi

# 3. pauseAll — add another URI, then pauseAll
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"pause2.bin\"}]")
gid2=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.3
resp=$(rpc_call $RPC_PORT "aria2.pauseAll")
sleep 1
resp=$(rpc_call $RPC_PORT "aria2.tellActive")
if echo "$resp" | grep -q '"result":\[\]'; then
  tap_ok "pauseAll clears all active downloads"
else
  tap_fail "pauseAll clears all active downloads (response: $resp)"
fi

# 4. unpauseAll — unpauseAll and check something becomes active/waiting
resp=$(rpc_call $RPC_PORT "aria2.unpauseAll")
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.getGlobalStat")
num_active=$(echo "$resp" | sed -n 's/.*"numActive":"\([^"]*\)".*/\1/p')
num_waiting=$(echo "$resp" | sed -n 's/.*"numWaiting":"\([^"]*\)".*/\1/p')
total=$((${num_active:-0} + ${num_waiting:-0}))
if [[ $total -gt 0 ]]; then
  tap_ok "unpauseAll reactivates downloads"
else
  tap_fail "unpauseAll reactivates downloads (active=$num_active waiting=$num_waiting)"
fi

# 5. forceRemove(gid) — add fresh download, force remove while active
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"remove_test.bin\"}]")
gid3=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.forceRemove" "[\"$gid3\"]")
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.tellStatus" "[\"$gid3\"]")
if echo "$resp" | grep -q '"status":"removed"'; then
  tap_ok "forceRemove changes status to removed"
else
  tap_fail "forceRemove changes status to removed (response: $resp)"
fi

stop_aria2_rpc
tap_summary
