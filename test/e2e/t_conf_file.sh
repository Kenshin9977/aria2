#!/usr/bin/env bash
# e2e: Configuration file tests (--conf-path, --no-conf)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_http_server --port 18111 --dir "$E2E_TMPDIR"

# 1. --conf-path with dir= in config — file goes to configured directory
mkdir -p "$E2E_TMPDIR/confdir"
cat > "$E2E_TMPDIR/aria2.conf" <<EOF
dir=$E2E_TMPDIR/confdir
allow-overwrite=true
EOF

"$ARIA2C" --conf-path="$E2E_TMPDIR/aria2.conf" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/confdir/testfile.bin" \
  "conf: --conf-path dir= places file in configured directory"

# 2. --no-conf — download succeeds without loading any config
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out2/testfile.bin" \
  "conf: --no-conf download succeeds"

# 3. --conf-path with invalid option — exits non-zero
cat > "$E2E_TMPDIR/bad.conf" <<EOF
max-concurrent-downloads=not_a_number
EOF

rc=0
"$ARIA2C" --conf-path="$E2E_TMPDIR/bad.conf" \
  -d "$E2E_TMPDIR/out3" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "conf: --conf-path with invalid option exits non-zero"
else
  tap_fail "conf: --conf-path with invalid option exits non-zero (got exit 0)"
fi

stop_http_server
tap_summary
