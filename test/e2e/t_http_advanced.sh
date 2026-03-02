#!/usr/bin/env bash
# e2e: Advanced HTTP feature tests (pipelining, auth-challenge, mTLS, min-tls-version)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# 1. --enable-http-pipelining downloads successfully
start_http_server --port 18270 --dir "$E2E_TMPDIR"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --enable-http-pipelining=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "HTTP pipelining downloads successfully"

# 2. --http-auth-challenge delays auth until server challenges
stop_http_server
start_http_server --port 18270 --dir "$E2E_TMPDIR" --auth "auser:apass"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --http-auth-challenge=true \
  --http-user=auser --http-passwd=apass \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "http-auth-challenge delays auth until challenge"

# 3. mTLS with client cert succeeds
stop_http_server

# Generate CA key and cert
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/ca_key.pem" \
  -out "$E2E_TMPDIR/ca_cert.pem" \
  -days 1 -subj "/CN=TestCA" 2>/dev/null

# Generate server cert signed by CA
openssl req -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/srv_key.pem" \
  -out "$E2E_TMPDIR/srv_csr.pem" \
  -subj "/CN=127.0.0.1" 2>/dev/null
openssl x509 -req -in "$E2E_TMPDIR/srv_csr.pem" \
  -CA "$E2E_TMPDIR/ca_cert.pem" -CAkey "$E2E_TMPDIR/ca_key.pem" \
  -CAcreateserial -out "$E2E_TMPDIR/srv_cert.pem" \
  -days 1 -extfile <(echo "subjectAltName=IP:127.0.0.1") 2>/dev/null

# Generate client cert signed by CA
openssl req -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/client_key.pem" \
  -out "$E2E_TMPDIR/client_csr.pem" \
  -subj "/CN=TestClient" 2>/dev/null
openssl x509 -req -in "$E2E_TMPDIR/client_csr.pem" \
  -CA "$E2E_TMPDIR/ca_cert.pem" -CAkey "$E2E_TMPDIR/ca_key.pem" \
  -CAcreateserial -out "$E2E_TMPDIR/client_cert.pem" \
  -days 1 2>/dev/null

start_http_server --port 18271 --dir "$E2E_TMPDIR" \
  --ssl --certfile "$E2E_TMPDIR/srv_cert.pem" --keyfile "$E2E_TMPDIR/srv_key.pem" \
  --require-client-cert "$E2E_TMPDIR/ca_cert.pem"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --check-certificate=false \
  --certificate="$E2E_TMPDIR/client_cert.pem" \
  --private-key="$E2E_TMPDIR/client_key.pem" \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out3/testfile.bin" 1024 \
  "mTLS with client cert succeeds"

# 4. mTLS without client cert fails
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --check-certificate=false --max-tries=1 \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "mTLS without client cert fails"
else
  tap_fail "mTLS without client cert fails (expected non-zero exit)"
fi

# 5. --min-tls-version=TLSv1.3 download succeeds
stop_http_server

# Self-signed cert for simple TLS test
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/tls13_key.pem" \
  -out "$E2E_TMPDIR/tls13_cert.pem" \
  -days 1 -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1" 2>/dev/null

start_http_server --port 18270 --dir "$E2E_TMPDIR" \
  --ssl --certfile "$E2E_TMPDIR/tls13_cert.pem" --keyfile "$E2E_TMPDIR/tls13_key.pem"

"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --check-certificate=false \
  --min-tls-version=TLSv1.3 \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out5/testfile.bin" 1024 \
  "min-tls-version=TLSv1.3 download succeeds"

stop_http_server
tap_summary
