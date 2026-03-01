#!/usr/bin/env bash
# e2e: HTTP proxy tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# Create a separate file for the no-proxy test
dd if=/dev/urandom of="$E2E_TMPDIR/noProxy.bin" bs=1024 count=1 2>/dev/null

# Start backend HTTP server on 18121
start_http_server --port 18121 --dir "$E2E_TMPDIR"

# Start proxy on 18120 with request logging
PROXY_LOG="$E2E_TMPDIR/proxy.log"
start_proxy_server --port 18120 --log-requests "$PROXY_LOG"

# 1. --http-proxy downloads through proxy
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --http-proxy="http://127.0.0.1:$PROXY_PORT" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "http-proxy: download through proxy succeeds"

# 2. --all-proxy downloads through proxy
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --all-proxy="http://127.0.0.1:$PROXY_PORT" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "all-proxy: download through proxy succeeds"

# 3. --no-proxy bypasses proxy for matching host
#    Download a unique path and verify proxy log doesn't contain it
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --http-proxy="http://127.0.0.1:$PROXY_PORT" \
  --no-proxy="127.0.0.1" \
  "http://127.0.0.1:$HTTP_PORT/noProxy.bin" >/dev/null 2>&1
# Check both: file downloaded AND proxy was bypassed
if [[ -f "$E2E_TMPDIR/out3/noProxy.bin" ]] && \
   ! grep -q "noProxy.bin" "$PROXY_LOG" 2>/dev/null; then
  tap_ok "no-proxy: download bypasses proxy (file ok, not in proxy log)"
else
  tap_fail "no-proxy: download bypasses proxy (file missing or found in proxy log)"
fi

# 4. Proxy with auth — stop proxy, restart with auth, download with credentials
stop_proxy_server
start_proxy_server --port 18120 --auth "proxyuser:proxypass"

"$ARIA2C" -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --http-proxy="http://127.0.0.1:$PROXY_PORT" \
  --http-proxy-user=proxyuser --http-proxy-passwd=proxypass \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out4/testfile.bin" 1024 \
  "proxy auth: download with correct proxy credentials succeeds"

stop_proxy_server
stop_http_server
tap_summary
