#!/usr/bin/env bash
# e2e: HTTPS proxy (CONNECT tunnel) tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# Generate self-signed certificate for the HTTPS backend
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/key.pem" \
  -out "$E2E_TMPDIR/cert.pem" \
  -days 1 -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1" 2>/dev/null

# Start HTTPS backend server on 18122
start_http_server --port 18122 --dir "$E2E_TMPDIR" \
  --ssl --certfile "$E2E_TMPDIR/cert.pem" --keyfile "$E2E_TMPDIR/key.pem"

# Start proxy on 18120
start_proxy_server --port 18120

# 1. HTTPS download through CONNECT proxy tunnel
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --check-certificate=false \
  --https-proxy="http://127.0.0.1:$PROXY_PORT" \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "https-proxy: CONNECT tunnel download succeeds"

# 2. HTTPS proxy with auth — restart proxy with credentials
stop_proxy_server
start_proxy_server --port 18120 --auth "proxyuser:proxypass"

"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --check-certificate=false \
  --https-proxy="http://127.0.0.1:$PROXY_PORT" \
  --https-proxy-user=proxyuser --https-proxy-passwd=proxypass \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "https-proxy auth: download with correct credentials succeeds"

# 3. HTTPS proxy with wrong credentials — should fail
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --check-certificate=false \
  --max-tries=1 --retry-wait=0 \
  --https-proxy="http://127.0.0.1:$PROXY_PORT" \
  --https-proxy-user=proxyuser --https-proxy-passwd=wrongpass \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "https-proxy auth: wrong credentials exits non-zero"
else
  tap_fail "https-proxy auth: wrong credentials exits non-zero (got exit 0)"
fi

stop_proxy_server
stop_http_server
tap_summary
