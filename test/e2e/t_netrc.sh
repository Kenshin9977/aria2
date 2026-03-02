#!/usr/bin/env bash
# e2e: Netrc authentication for HTTP and FTP
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

# Start HTTP server with basic auth
start_http_server --port 18113 --dir "$E2E_TMPDIR" \
  --auth "testuser:testpass"

# Start FTP server with credentials
start_ftp_server --port 18160 --dir "$E2E_TMPDIR" \
  --user ftpuser --passwd ftppass --pasv-range 18161-18170

# 1. HTTP with netrc — auth succeeds
NETRC_FILE="$E2E_TMPDIR/netrc_http"
cat > "$NETRC_FILE" <<EOF
machine 127.0.0.1
login testuser
password testpass
EOF
chmod 600 "$NETRC_FILE"
"$ARIA2C" --no-conf --allow-overwrite=true \
  -d "$E2E_TMPDIR/out1" \
  --netrc-path="$NETRC_FILE" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "netrc: HTTP download with netrc credentials succeeds (1024 bytes)"

# 2. FTP with netrc — auth succeeds
NETRC_FTP="$E2E_TMPDIR/netrc_ftp"
cat > "$NETRC_FTP" <<EOF
machine 127.0.0.1
login ftpuser
password ftppass
EOF
chmod 600 "$NETRC_FTP"
"$ARIA2C" --no-conf --allow-overwrite=true \
  -d "$E2E_TMPDIR/out2" \
  --netrc-path="$NETRC_FTP" \
  "ftp://127.0.0.1:$FTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out2/testfile.bin" \
  "netrc: FTP download with netrc credentials succeeds"

# 3. --no-netrc=true — HTTP download fails (401)
rc=0
"$ARIA2C" --no-conf --allow-overwrite=true \
  -d "$E2E_TMPDIR/out3" \
  --no-netrc=true --netrc-path="$NETRC_FILE" \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "netrc: --no-netrc=true ignores netrc file, HTTP auth fails (exit $rc)"
else
  tap_fail "netrc: --no-netrc=true ignores netrc file, HTTP auth fails (expected non-zero exit)"
fi

# 4. Wrong netrc credentials — download fails
NETRC_BAD="$E2E_TMPDIR/netrc_bad"
cat > "$NETRC_BAD" <<EOF
machine 127.0.0.1
login testuser
password wrongpass
EOF
chmod 600 "$NETRC_BAD"
rc=0
"$ARIA2C" --no-conf --allow-overwrite=true \
  -d "$E2E_TMPDIR/out4" \
  --netrc-path="$NETRC_BAD" \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "netrc: wrong netrc credentials fail HTTP auth (exit $rc)"
else
  tap_fail "netrc: wrong netrc credentials fail HTTP auth (expected non-zero exit)"
fi

stop_http_server
stop_ftp_server
tap_summary
