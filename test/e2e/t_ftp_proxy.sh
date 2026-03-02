#!/usr/bin/env bash
# e2e: FTP download through HTTP CONNECT proxy tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create 4KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null

start_ftp_server --port 18241 --dir "$E2E_TMPDIR" --pasv-range "18242-18251"
start_proxy_server --port 18240

# 1. FTP download through HTTP CONNECT proxy — download succeeds
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --all-proxy="http://127.0.0.1:$PROXY_PORT" \
  --proxy-method=tunnel \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 4096 \
  "FTP download through CONNECT proxy succeeds"

# 2. Proxy log shows CONNECT method
stop_proxy_server
start_proxy_server --port 18240 --log-requests "$E2E_TMPDIR/proxy_log.jsonl"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --all-proxy="http://127.0.0.1:$PROXY_PORT" \
  --proxy-method=tunnel \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$E2E_TMPDIR/proxy_log.jsonl" "CONNECT" \
  "proxy log shows CONNECT method for FTP"

# 3. FTP proxy with auth (proxy credentials)
stop_proxy_server
start_proxy_server --port 18240 --auth "puser:ppass"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --all-proxy="http://127.0.0.1:$PROXY_PORT" \
  --proxy-method=tunnel \
  --all-proxy-user=puser --all-proxy-passwd=ppass \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out3/testfile.bin" 4096 \
  "FTP through CONNECT proxy with auth succeeds"

stop_ftp_server
stop_proxy_server
tap_summary
