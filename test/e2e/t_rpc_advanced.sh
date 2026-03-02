#!/usr/bin/env bash
# e2e: Advanced RPC option tests (rpc-allow-origin-all, rpc-max-request-size,
#      rpc-save-upload-metadata, max-download-result, gid, pause)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
mkdir -p "$E2E_TMPDIR/downloads"

start_http_server --port 18380 --dir "$E2E_TMPDIR"

# ── Test 1: --rpc-allow-origin-all=true adds CORS header ────────────────
start_aria2_rpc 16816 --dir="$E2E_TMPDIR/downloads" \
  --rpc-allow-origin-all=true
resp=$(curl -si -X POST "http://127.0.0.1:$RPC_PORT/jsonrpc" \
  -H "Content-Type: application/json" \
  -H "Origin: http://example.com" \
  -d '{"jsonrpc":"2.0","id":"1","method":"aria2.getVersion"}' 2>/dev/null)
if echo "$resp" | grep -qi "Access-Control-Allow-Origin"; then
  tap_ok "rpc-allow-origin-all=true adds CORS header"
else
  tap_fail "rpc-allow-origin-all=true adds CORS header"
fi
stop_aria2_rpc

# ── Test 2: --rpc-max-request-size=100 rejects oversized request ────────
start_aria2_rpc 16816 --dir="$E2E_TMPDIR/downloads" \
  --rpc-max-request-size=100
# Generate a 2KB JSON body (well over 100 bytes)
big_body=$(printf '{"jsonrpc":"2.0","id":"1","method":"aria2.getVersion","params":["%0*d"]}' 2000 0)
resp=$(curl -s -X POST "http://127.0.0.1:$RPC_PORT/jsonrpc" \
  -H "Content-Type: application/json" \
  -d "$big_body" 2>/dev/null) || true
if echo "$resp" | grep -qi "error\|too large\|exceeded"; then
  tap_ok "rpc-max-request-size=100 rejects oversized request"
else
  # Connection might be closed without response for oversized requests
  if [[ -z "$resp" ]]; then
    tap_ok "rpc-max-request-size=100 rejects oversized request (connection closed)"
  else
    tap_fail "rpc-max-request-size=100 rejects oversized request (got: $resp)"
  fi
fi
stop_aria2_rpc

# ── Test 3: --rpc-save-upload-metadata=true saves .torrent on addTorrent ─
# Create a minimal torrent for upload
dd if=/dev/urandom of="$E2E_TMPDIR/tiny.bin" bs=1024 count=1 2>/dev/null
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/tiny.bin" \
  --output "$E2E_TMPDIR/tiny.torrent" \
  --piece-length 16384
torrent_b64=$(base64 < "$E2E_TMPDIR/tiny.torrent" | tr -d '\n')
start_aria2_rpc 16816 --dir="$E2E_TMPDIR/downloads" \
  --rpc-save-upload-metadata=true --bt-stop-timeout=3 --seed-time=0
resp=$(rpc_call $RPC_PORT "aria2.addTorrent" "[\"$torrent_b64\"]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 1
# Check if a .torrent file was saved in the download directory
torrent_saved=false
for f in "$E2E_TMPDIR/downloads"/*.torrent; do
  if [[ -f "$f" ]]; then
    torrent_saved=true
    break
  fi
done
if $torrent_saved; then
  tap_ok "rpc-save-upload-metadata=true saves .torrent file"
else
  tap_fail "rpc-save-upload-metadata=true saves .torrent file"
fi
stop_aria2_rpc

# ── Test 4: --max-download-result=3 caps result queue ───────────────────
start_aria2_rpc 16816 --dir="$E2E_TMPDIR/downloads" \
  --max-download-result=3
# Download 5 files via RPC
for i in 1 2 3 4 5; do
  rpc_call $RPC_PORT "aria2.addUri" \
    "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"result_$i.bin\"}]" \
    >/dev/null 2>&1
  sleep 0.5
done
sleep 3
resp=$(rpc_call $RPC_PORT "aria2.tellStopped" "[0,100]")
# Count the number of GIDs in the result
count=$(echo "$resp" | grep -o '"gid"' | wc -l | tr -d ' ')
if [[ "$count" -le 3 ]]; then
  tap_ok "max-download-result=3 caps stopped result queue ($count entries)"
else
  tap_fail "max-download-result=3 caps stopped result queue ($count entries, expected <=3)"
fi
stop_aria2_rpc

# ── Test 5: --gid=CUSTOM assigns custom GID via RPC option ──────────────
start_aria2_rpc 16816 --dir="$E2E_TMPDIR/downloads"
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"gid\":\"0123456789abcdef\",\"out\":\"gid_test.bin\"}]")
sleep 1
resp=$(rpc_call $RPC_PORT "aria2.tellStatus" "[\"0123456789abcdef\"]")
if echo "$resp" | grep -q '"0123456789abcdef"'; then
  tap_ok "custom GID 0123456789abcdef assigned via RPC"
else
  tap_fail "custom GID 0123456789abcdef assigned via RPC (response: $resp)"
fi
stop_aria2_rpc

# ── Test 6: pause option in addUri starts download paused ────────────────
start_aria2_rpc 16816 --dir="$E2E_TMPDIR/downloads"
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/testfile.bin\"],{\"out\":\"paused_test.bin\",\"pause\":\"true\"}]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.tellStatus" "[\"$gid\"]")
if echo "$resp" | grep -q '"paused"'; then
  tap_ok "pause=true in addUri starts download in paused state"
else
  tap_fail "pause=true in addUri starts download in paused state (response: $resp)"
fi
stop_aria2_rpc

stop_http_server
tap_summary
