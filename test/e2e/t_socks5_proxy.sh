#!/usr/bin/env bash
# e2e: SOCKS5 proxy download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# 1. HTTP download through SOCKS5 (no auth)
start_http_server --port 18211 --dir "$E2E_TMPDIR"
start_socks5_server --port 18210

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --socks5-proxy="127.0.0.1:$SOCKS5_PORT" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "HTTP download through SOCKS5 no-auth succeeds"

# 2. HTTP download through SOCKS5 with auth
stop_socks5_server
start_socks5_server --port 18210 --auth "suser:spass"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --socks5-proxy="127.0.0.1:$SOCKS5_PORT" \
  --socks5-proxy-user=suser --socks5-proxy-passwd=spass \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "HTTP download through SOCKS5 with auth succeeds"

# 3. SOCKS5 wrong credentials fails
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --socks5-proxy="127.0.0.1:$SOCKS5_PORT" \
  --socks5-proxy-user=wrong --socks5-proxy-passwd=creds \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "SOCKS5 wrong credentials fails"
else
  tap_fail "SOCKS5 wrong credentials fails (expected non-zero exit)"
fi

# 4. HTTPS download through SOCKS5 tunnel
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/key.pem" \
  -out "$E2E_TMPDIR/cert.pem" \
  -days 1 -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1" 2>/dev/null

stop_http_server
start_http_server --port 18212 --dir "$E2E_TMPDIR" \
  --ssl --certfile "$E2E_TMPDIR/cert.pem" --keyfile "$E2E_TMPDIR/key.pem"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --socks5-proxy="127.0.0.1:$SOCKS5_PORT" \
  --socks5-proxy-user=suser --socks5-proxy-passwd=spass \
  --check-certificate=false \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out4/testfile.bin" 1024 \
  "HTTPS download through SOCKS5 tunnel succeeds"

# 5. SOCKS5 log records CONNECT target
stop_socks5_server
start_socks5_server --port 18210 --log-requests "$E2E_TMPDIR/socks5_log.jsonl"

stop_http_server
start_http_server --port 18211 --dir "$E2E_TMPDIR"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --socks5-proxy="127.0.0.1:$SOCKS5_PORT" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_contains "$E2E_TMPDIR/socks5_log.jsonl" "CONNECT" \
  "SOCKS5 log records CONNECT target"

stop_socks5_server
stop_http_server
tap_summary
