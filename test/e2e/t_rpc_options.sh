#!/usr/bin/env bash
# e2e: RPC option management tests (getGlobalOption, changeGlobalOption,
#      getOption, changeOption, getVersion)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

# Create test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18101 --dir "$E2E_TMPDIR" --delay 3
start_aria2_rpc 16802 --dir="$E2E_TMPDIR/downloads"

# 1. getGlobalOption — response contains "dir"
resp=$(rpc_call $RPC_PORT "aria2.getGlobalOption")
if echo "$resp" | grep -q '"dir"'; then
  tap_ok "getGlobalOption returns dir"
else
  tap_fail "getGlobalOption returns dir (response: $resp)"
fi

# 2. changeGlobalOption + verify
resp=$(rpc_call $RPC_PORT "aria2.changeGlobalOption" \
  '[{"max-concurrent-downloads":"3"}]')
if echo "$resp" | grep -q '"OK"'; then
  resp=$(rpc_call $RPC_PORT "aria2.getGlobalOption")
  if echo "$resp" | grep -q '"max-concurrent-downloads":"3"'; then
    tap_ok "changeGlobalOption updates max-concurrent-downloads"
  else
    tap_fail "changeGlobalOption updates max-concurrent-downloads (response: $resp)"
  fi
else
  tap_fail "changeGlobalOption updates max-concurrent-downloads (change failed: $resp)"
fi

# 3. getOption(gid) — add a URI, get per-download options
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"opttest.bin\"}]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.3
resp=$(rpc_call $RPC_PORT "aria2.getOption" "[\"$gid\"]")
if echo "$resp" | grep -q '"out":"opttest.bin"'; then
  tap_ok "getOption returns per-download options"
else
  tap_fail "getOption returns per-download options (response: $resp)"
fi

# 4. changeOption — pause download first, then change option
# Wait a moment, then pause
sleep 0.5
rpc_call $RPC_PORT "aria2.pause" "[\"$gid\"]" >/dev/null 2>&1
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.changeOption" \
  "[\"$gid\",{\"max-download-limit\":\"1024\"}]")
if echo "$resp" | grep -q '"OK"'; then
  resp=$(rpc_call $RPC_PORT "aria2.getOption" "[\"$gid\"]")
  if echo "$resp" | grep -q '"max-download-limit":"1024"'; then
    tap_ok "changeOption updates max-download-limit"
  else
    tap_fail "changeOption updates max-download-limit (response: $resp)"
  fi
else
  tap_fail "changeOption updates max-download-limit (change failed: $resp)"
fi
# Unpause to clean up
rpc_call $RPC_PORT "aria2.unpause" "[\"$gid\"]" >/dev/null 2>&1
sleep 2

# 5. getVersion — returns version and enabledFeatures
resp=$(rpc_call $RPC_PORT "aria2.getVersion")
if echo "$resp" | grep -q '"version"' && \
   echo "$resp" | grep -q '"enabledFeatures"'; then
  tap_ok "getVersion returns version and enabledFeatures"
else
  tap_fail "getVersion returns version and enabledFeatures (response: $resp)"
fi

stop_aria2_rpc
tap_summary
