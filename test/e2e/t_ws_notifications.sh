#!/usr/bin/env bash
# e2e: WebSocket notification tests (onDownloadStart, onComplete, onError, etc.)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

# ── Test 1 & 2: onDownloadStart and onDownloadComplete ──
# Start a slow-enough server that aria2 has time to connect and emit Start,
# then serve the file so it completes and emits Complete.
start_http_server --port 18222 --dir "$E2E_TMPDIR" --delay 2
start_aria2_rpc 16812 --dir="$E2E_TMPDIR/downloads"

# addUri via WebSocket, then wait up to 8 s for notification frames.
resp=$(python3 "$SCRIPT_DIR/ws_client.py" --port "$RPC_PORT" \
  --method "aria2.addUri" \
  --params "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"]]" \
  --wait-notifications 8)

# 1. onDownloadStart fires after addUri
if echo "$resp" | grep -q "aria2.onDownloadStart"; then
  tap_ok "WebSocket: onDownloadStart notification fires"
else
  tap_fail "WebSocket: onDownloadStart notification fires (output: $resp)"
fi

# 2. onDownloadComplete fires once the file is fetched
if echo "$resp" | grep -q "aria2.onDownloadComplete"; then
  tap_ok "WebSocket: onDownloadComplete notification fires"
else
  tap_fail "WebSocket: onDownloadComplete notification fires (output: $resp)"
fi

# ── Test 3: onDownloadError fires on 404 ──
stop_aria2_rpc
start_aria2_rpc 16812 --dir="$E2E_TMPDIR/downloads"

resp=$(python3 "$SCRIPT_DIR/ws_client.py" --port "$RPC_PORT" \
  --method "aria2.addUri" \
  --params "[[\"http://127.0.0.1:$HTTP_PORT/nonexistent.bin\"],{\"max-tries\":\"1\"}]" \
  --wait-notifications 8)

if echo "$resp" | grep -q "aria2.onDownloadError"; then
  tap_ok "WebSocket: onDownloadError fires on 404"
else
  tap_fail "WebSocket: onDownloadError fires on 404 (output: $resp)"
fi

# ── Test 4: onDownloadPause fires ──
stop_aria2_rpc
stop_http_server
# Use a 5-second delay so the download stays active while we issue pause.
start_http_server --port 18222 --dir "$E2E_TMPDIR" --delay 5
start_aria2_rpc 16812 --dir="$E2E_TMPDIR/downloads"

# Add the URI via plain HTTP RPC so the download is already active when we
# open the WebSocket connection for the pause call.
resp=$(rpc_call "$RPC_PORT" "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"]]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5

# Pause via WebSocket and collect notification frames for 5 s.
resp=$(python3 "$SCRIPT_DIR/ws_client.py" --port "$RPC_PORT" \
  --method "aria2.pause" \
  --params "[\"$gid\"]" \
  --wait-notifications 5)

if echo "$resp" | grep -q "aria2.onDownloadPause"; then
  tap_ok "WebSocket: onDownloadPause fires"
else
  tap_fail "WebSocket: onDownloadPause fires (output: $resp)"
fi

# ── Test 5: onDownloadStop fires on forceRemove ──
stop_aria2_rpc
stop_http_server
start_http_server --port 18222 --dir "$E2E_TMPDIR" --delay 5
start_aria2_rpc 16812 --dir="$E2E_TMPDIR/downloads"

resp=$(rpc_call "$RPC_PORT" "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"]]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5

resp=$(python3 "$SCRIPT_DIR/ws_client.py" --port "$RPC_PORT" \
  --method "aria2.forceRemove" \
  --params "[\"$gid\"]" \
  --wait-notifications 5)

if echo "$resp" | grep -q "aria2.onDownloadStop"; then
  tap_ok "WebSocket: onDownloadStop fires on remove"
else
  tap_fail "WebSocket: onDownloadStop fires on remove (output: $resp)"
fi

stop_aria2_rpc
stop_http_server
tap_summary
