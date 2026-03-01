#!/usr/bin/env bash
# e2e: HTTP 404 not-found tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Server serves from tmpdir which has no files at the requested paths
start_http_server --port 18099 --dir "$E2E_TMPDIR"

# 1. --max-file-not-found=3 --max-tries=5 on nonexistent path — fails
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --max-file-not-found=3 --max-tries=5 \
  "http://127.0.0.1:$HTTP_PORT/nonexistent" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "404: --max-file-not-found=3 --max-tries=5 fails on missing file"
else
  tap_fail "404: --max-file-not-found=3 --max-tries=5 fails on missing file (expected non-zero exit)"
fi

# 2. --max-tries=1 on nonexistent path — fails (control test)
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --max-tries=1 \
  "http://127.0.0.1:$HTTP_PORT/nonexistent" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "404: --max-tries=1 fails on missing file"
else
  tap_fail "404: --max-tries=1 fails on missing file (expected non-zero exit)"
fi

stop_http_server
tap_summary
