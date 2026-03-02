#!/usr/bin/env bash
# e2e: checksum algorithm verification tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_http_server --port 18114 --dir "$E2E_TMPDIR"

# Compute hashes for each supported algorithm
hash_md5=$(md5 -q "$E2E_TMPDIR/testfile.bin" 2>/dev/null \
  || md5sum "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')
hash_sha1=$(shasum -a 1 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')
hash_sha224=$(shasum -a 224 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')
hash_sha256=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')
hash_sha384=$(shasum -a 384 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')
hash_sha512=$(shasum -a 512 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')

# 1. md5
mkdir -p "$E2E_TMPDIR/out_md5"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out_md5" --no-conf \
  --checksum=md5="$hash_md5" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 && -f "$E2E_TMPDIR/out_md5/testfile.bin" ]]; then
  tap_ok "checksum: md5 verification succeeds"
else
  tap_fail "checksum: md5 verification succeeds (exit $rc)"
fi

# 2. sha-1
mkdir -p "$E2E_TMPDIR/out_sha1"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out_sha1" --no-conf \
  --checksum=sha-1="$hash_sha1" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 && -f "$E2E_TMPDIR/out_sha1/testfile.bin" ]]; then
  tap_ok "checksum: sha-1 verification succeeds"
else
  tap_fail "checksum: sha-1 verification succeeds (exit $rc)"
fi

# 3. sha-224
mkdir -p "$E2E_TMPDIR/out_sha224"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out_sha224" --no-conf \
  --checksum=sha-224="$hash_sha224" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 && -f "$E2E_TMPDIR/out_sha224/testfile.bin" ]]; then
  tap_ok "checksum: sha-224 verification succeeds"
else
  tap_fail "checksum: sha-224 verification succeeds (exit $rc)"
fi

# 4. sha-256
mkdir -p "$E2E_TMPDIR/out_sha256"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out_sha256" --no-conf \
  --checksum=sha-256="$hash_sha256" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 && -f "$E2E_TMPDIR/out_sha256/testfile.bin" ]]; then
  tap_ok "checksum: sha-256 verification succeeds"
else
  tap_fail "checksum: sha-256 verification succeeds (exit $rc)"
fi

# 5. sha-384
mkdir -p "$E2E_TMPDIR/out_sha384"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out_sha384" --no-conf \
  --checksum=sha-384="$hash_sha384" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 && -f "$E2E_TMPDIR/out_sha384/testfile.bin" ]]; then
  tap_ok "checksum: sha-384 verification succeeds"
else
  tap_fail "checksum: sha-384 verification succeeds (exit $rc)"
fi

# 6. sha-512
mkdir -p "$E2E_TMPDIR/out_sha512"
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out_sha512" --no-conf \
  --checksum=sha-512="$hash_sha512" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 && -f "$E2E_TMPDIR/out_sha512/testfile.bin" ]]; then
  tap_ok "checksum: sha-512 verification succeeds"
else
  tap_fail "checksum: sha-512 verification succeeds (exit $rc)"
fi

stop_http_server
tap_summary
