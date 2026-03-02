#!/usr/bin/env bash
# e2e: HTTP Basic auth tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Create test fixture
echo "authenticated-content" > "$E2E_TMPDIR/secret.txt"

start_http_server --port 18084 --dir "$E2E_TMPDIR" --auth "testuser:testpass"

# 1. Correct credentials succeed
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --http-user=testuser --http-passwd=testpass \
  "http://127.0.0.1:$HTTP_PORT/secret.txt" >/dev/null 2>&1
assert_file_contains "$E2E_TMPDIR/out1/secret.txt" \
  "authenticated-content" \
  "auth: correct credentials download file"

# 2. Wrong password fails
run_cmd "$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --max-tries=1 --retry-wait=0 \
  --http-user=testuser --http-passwd=wrongpass \
  "http://127.0.0.1:$HTTP_PORT/secret.txt"
if [[ $_last_exit -ne 0 ]]; then
  tap_ok "auth: wrong password exits non-zero"
else
  tap_fail "auth: wrong password exits non-zero (got exit 0)"
fi

stop_http_server
tap_summary
