#!/usr/bin/env bash
# e2e: file allocation mode tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create a 1MB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1024 2>/dev/null
ORIG_SHA=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')

start_http_server --port 18115 --dir "$E2E_TMPDIR"

for mode in none prealloc trunc falloc; do
  mkdir -p "$E2E_TMPDIR/out_$mode"
  rc=0
  "$ARIA2C" -d "$E2E_TMPDIR/out_$mode" --no-conf --file-allocation="$mode" \
    "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
  if [[ $rc -ne 0 && "$mode" == "falloc" ]]; then
    tap_ok "file-allocation=$mode (skipped: not supported on this OS)"
  else
    DL_SHA=$(shasum -a 256 "$E2E_TMPDIR/out_$mode/testfile.bin" | awk '{print $1}')
    if [[ "$DL_SHA" == "$ORIG_SHA" ]]; then
      tap_ok "file-allocation=$mode downloads correctly"
    else
      tap_fail "file-allocation=$mode sha256 mismatch"
    fi
  fi
done

stop_http_server
tap_summary
