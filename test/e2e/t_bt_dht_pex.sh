#!/usr/bin/env bash
# e2e: BitTorrent DHT and PEX option acceptance smoke tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create a small test file and start HTTP server so downloads succeed
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=64 count=1 2>/dev/null
start_http_server --port 18150 --dir "$E2E_TMPDIR"

# 1. --enable-dht=true accepted
run_cmd "$ARIA2C" --no-conf --enable-dht=true \
  -d "$E2E_TMPDIR/out1" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin"
assert_exit_code "$_last_exit" 0 \
  "dht: --enable-dht=true accepted"

# 2. --enable-dht6=true accepted
run_cmd "$ARIA2C" --no-conf --enable-dht6=true \
  -d "$E2E_TMPDIR/out2" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin"
assert_exit_code "$_last_exit" 0 \
  "dht: --enable-dht6=true accepted"

# 3. --enable-peer-exchange=true accepted
run_cmd "$ARIA2C" --no-conf --enable-peer-exchange=true \
  -d "$E2E_TMPDIR/out3" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin"
assert_exit_code "$_last_exit" 0 \
  "pex: --enable-peer-exchange=true accepted"

# 4. --dht-listen-port=18151 accepted
run_cmd "$ARIA2C" --no-conf --dht-listen-port=18151 \
  -d "$E2E_TMPDIR/out4" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin"
assert_exit_code "$_last_exit" 0 \
  "dht: --dht-listen-port=18151 accepted"

stop_http_server
tap_summary
