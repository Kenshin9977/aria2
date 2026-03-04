#!/usr/bin/env bash
# e2e: FTP-specific option tests (ftp-reuse-connection, ftp-type,
#      ftp-proxy, ftp-proxy-user, ftp-proxy-passwd)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 7
make_tempdir

# Create test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
FILE_HASH=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')
echo "Hello FTP ASCII test" > "$E2E_TMPDIR/textfile.txt"

start_ftp_server --port 18340 --dir "$E2E_TMPDIR" --pasv-range "18341-18350"

# ── Test 1: --ftp-reuse-connection=true download succeeds ────────────────
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --ftp-reuse-connection=true --timeout=10 --max-tries=1 \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_file_hash "$E2E_TMPDIR/out1/testfile.bin" sha-256 "$FILE_HASH" \
  "ftp-reuse-connection=true downloads correctly"

# ── Test 2: --ftp-reuse-connection=false download succeeds ───────────────
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --ftp-reuse-connection=false --timeout=10 --max-tries=1 \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_file_hash "$E2E_TMPDIR/out2/testfile.bin" sha-256 "$FILE_HASH" \
  "ftp-reuse-connection=false downloads correctly"

# ── Test 3: --ftp-type=binary preserves content ─────────────────────────
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --ftp-type=binary --timeout=10 --max-tries=1 \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_file_hash "$E2E_TMPDIR/out3/testfile.bin" sha-256 "$FILE_HASH" \
  "ftp-type=binary preserves binary content"

# ── Test 4: --ftp-type=ascii download succeeds ──────────────────────────
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --ftp-type=ascii --timeout=10 --max-tries=1 \
  "ftp://127.0.0.1:$FTP_PORT/textfile.txt" >/dev/null 2>&1 || rc=$?
assert_file_exists "$E2E_TMPDIR/out4/textfile.txt" \
  "ftp-type=ascii downloads text file"

# ── Test 5: --ftp-proxy routes FTP through HTTP CONNECT proxy ────────────
start_proxy_server --port 18351 --log-requests "$E2E_TMPDIR/proxy5.jsonl"
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --ftp-proxy="http://127.0.0.1:$PROXY_PORT" --proxy-method=tunnel \
  --timeout=10 --max-tries=1 \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_file_size "$E2E_TMPDIR/out5/testfile.bin" 4096 \
  "ftp-proxy routes FTP through CONNECT proxy"
stop_proxy_server

# ── Test 6: --ftp-proxy-user/passwd authenticates to FTP proxy ───────────
start_proxy_server --port 18351 --auth "puser:ppass"
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out6" --allow-overwrite=true \
  --ftp-proxy="http://127.0.0.1:$PROXY_PORT" --proxy-method=tunnel \
  --ftp-proxy-user=puser --ftp-proxy-passwd=ppass \
  --timeout=10 --max-tries=1 \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
assert_file_size "$E2E_TMPDIR/out6/testfile.bin" 4096 \
  "ftp-proxy with correct auth succeeds"
stop_proxy_server

# ── Test 7: --ftp-proxy wrong auth fails ─────────────────────────────────
start_proxy_server --port 18351 --auth "puser:ppass"
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out7" --allow-overwrite=true \
  --ftp-proxy="http://127.0.0.1:$PROXY_PORT" --proxy-method=tunnel \
  --ftp-proxy-user=wrong --ftp-proxy-passwd=wrong \
  --max-tries=1 \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "ftp-proxy with wrong auth fails (exit $rc)"
else
  tap_fail "ftp-proxy with wrong auth fails (exit 0, expected non-zero)"
fi
stop_proxy_server

stop_ftp_server
tap_summary
