#!/usr/bin/env bash
# e2e: HTTP redirect tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Create test fixture
echo "redirect-target-content" > "$E2E_TMPDIR/redirected"

start_http_server --port 18083 --dir "$E2E_TMPDIR" \
  --redirect "http://127.0.0.1:18083/redirected"

# 1. 301 redirect followed, final file downloaded
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/original" >/dev/null 2>&1
assert_file_contains "$E2E_TMPDIR/out1/redirected" \
  "redirect-target-content" \
  "redirect: 301 followed and file downloaded"

# 2. --max-redirect=0 prevents following
run_cmd "$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --max-redirect=0 \
  "http://127.0.0.1:$HTTP_PORT/original"
if [[ $_last_exit -ne 0 ]]; then
  tap_ok "redirect: --max-redirect=0 blocks redirect"
else
  tap_fail "redirect: --max-redirect=0 blocks redirect (got exit 0)"
fi

stop_http_server
tap_summary
