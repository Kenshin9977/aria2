#!/usr/bin/env bash
# e2e: HTTPS download tests (TLS, cert verification)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# Generate self-signed certificate
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$E2E_TMPDIR/key.pem" \
  -out "$E2E_TMPDIR/cert.pem" \
  -days 1 -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1" 2>/dev/null

start_http_server --port 18090 --dir "$E2E_TMPDIR" \
  --ssl --certfile "$E2E_TMPDIR/cert.pem" --keyfile "$E2E_TMPDIR/key.pem"

# 1. Download with --check-certificate=false — succeeds
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --check-certificate=false \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "HTTPS download with --check-certificate=false succeeds"

# 2. Download with --ca-certificate=cert.pem — succeeds (trusted)
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --check-certificate=true \
  --ca-certificate="$E2E_TMPDIR/cert.pem" \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "HTTPS download with --ca-certificate succeeds"

# 3. Download without cert trust, --check-certificate=true — fails
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --check-certificate=true \
  "https://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "HTTPS download without trusted cert fails"
else
  tap_fail "HTTPS download without trusted cert fails (expected non-zero exit)"
fi

stop_http_server
tap_summary
