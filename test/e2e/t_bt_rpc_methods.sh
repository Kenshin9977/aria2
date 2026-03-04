#!/usr/bin/env bash
# e2e: BitTorrent RPC method tests (addTorrent, getPeers, getServers, addMetalink)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

mkdir -p "$E2E_TMPDIR/downloads" "$E2E_TMPDIR/seed_dir"

# 1. addTorrent via base64 — returns GID
dd if=/dev/urandom of="$E2E_TMPDIR/seed_dir/payload.bin" bs=1024 count=16 2>/dev/null
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/seed_dir/payload.bin" \
  --output "$E2E_TMPDIR/test.torrent" \
  --piece-length 16384

start_aria2_rpc 16811 --dir="$E2E_TMPDIR/downloads" \
  --enable-dht=false --bt-external-ip=127.0.0.1 --seed-time=0

torrent_b64=$(base64 < "$E2E_TMPDIR/test.torrent" | tr -d '\n')
resp=$(rpc_call $RPC_PORT "aria2.addTorrent" "[\"$torrent_b64\"]")
gid=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
if [[ -n "$gid" ]]; then
  tap_ok "addTorrent via base64 returns GID ($gid)"
else
  tap_fail "addTorrent via base64 returns GID (response: $resp)"
fi
sleep 1

# 2. getPeers returns peer list for BT download
resp=$(rpc_call $RPC_PORT "aria2.getPeers" "[\"$gid\"]")
if echo "$resp" | grep -q '"result"'; then
  tap_ok "getPeers returns result for BT download"
else
  tap_fail "getPeers returns result for BT download (response: $resp)"
fi

# 3. getServers returns server list for HTTP download
start_http_server --port 18221 --dir "$E2E_TMPDIR" --delay 2
resp=$(rpc_call $RPC_PORT "aria2.addUri" \
  "[[\"http://127.0.0.1:$HTTP_PORT/seed_dir/payload.bin\"]]")
gid_http=$(echo "$resp" | sed -n 's/.*"result":"\([^"]*\)".*/\1/p')
sleep 0.5
resp=$(rpc_call $RPC_PORT "aria2.getServers" "[\"$gid_http\"]")
if echo "$resp" | grep -q '"uri"'; then
  tap_ok "getServers returns server list for HTTP download"
else
  tap_fail "getServers returns server list for HTTP download (response: $resp)"
fi
sleep 3

# 4. addMetalink via base64 metalink XML — returns list of GIDs
cat > "$E2E_TMPDIR/test.metalink" <<'MLEOF'
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="mltest.bin">
    <size>1024</size>
    <url>http://127.0.0.1:18221/seed_dir/payload.bin</url>
  </file>
</metalink>
MLEOF
ml_b64=$(base64 < "$E2E_TMPDIR/test.metalink" | tr -d '\n')
resp=$(rpc_call $RPC_PORT "aria2.addMetalink" "[\"$ml_b64\"]")
if echo "$resp" | grep -q '"result"'; then
  tap_ok "addMetalink via base64 returns GID list"
else
  tap_fail "addMetalink via base64 returns GID list (response: $resp)"
fi

stop_aria2_rpc
stop_http_server
tap_summary
