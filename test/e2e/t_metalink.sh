#!/usr/bin/env bash
# e2e: Metalink tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Find the test directory (where existing test fixtures live)
TEST_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Create fixture files and a metalink that references our local server
echo "metalink-file1-content" > "$E2E_TMPDIR/file1.dat"
echo "metalink-file2-content" > "$E2E_TMPDIR/file2.dat"

start_http_server --port 18088 --dir "$E2E_TMPDIR"

cat > "$E2E_TMPDIR/test.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="file1.dat">
    <url>http://127.0.0.1:$HTTP_PORT/file1.dat</url>
  </file>
  <file name="file2.dat">
    <url>http://127.0.0.1:$HTTP_PORT/file2.dat</url>
  </file>
</metalink>
EOF

# 1. Download using metalink file, verify both files downloaded
"$ARIA2C" --metalink-file="$E2E_TMPDIR/test.meta4" \
  -d "$E2E_TMPDIR/out1" --allow-overwrite=true >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out1/file1.dat" && -f "$E2E_TMPDIR/out1/file2.dat" ]]; then
  tap_ok "metalink: both files from .meta4 downloaded"
else
  tap_fail "metalink: both files from .meta4 downloaded"
fi

# 2. --metalink-file with non-existent file exits with error
run_cmd "$ARIA2C" --metalink-file="$E2E_TMPDIR/nonexistent.meta4" \
  -d "$E2E_TMPDIR"
if [[ $_last_exit -ne 0 ]]; then
  tap_ok "metalink: non-existent file exits non-zero"
else
  tap_fail "metalink: non-existent file exits non-zero (got exit 0)"
fi

stop_http_server
tap_summary
