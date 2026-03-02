#!/usr/bin/env bash
# e2e: Input file (-i) tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# Create test fixtures
echo "file-one-content" > "$E2E_TMPDIR/file1.txt"
echo "file-two-content" > "$E2E_TMPDIR/file2.txt"

start_http_server --port 18087 --dir "$E2E_TMPDIR"

# 1. Input file with 2 URIs, both downloaded
cat > "$E2E_TMPDIR/input.txt" <<EOF
http://127.0.0.1:$HTTP_PORT/file1.txt
http://127.0.0.1:$HTTP_PORT/file2.txt
EOF
"$ARIA2C" -i "$E2E_TMPDIR/input.txt" -d "$E2E_TMPDIR/out1" \
  --allow-overwrite=true >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out1/file1.txt" && -f "$E2E_TMPDIR/out1/file2.txt" ]]; then
  tap_ok "input-file: both URIs downloaded"
else
  tap_fail "input-file: both URIs downloaded"
fi

# 2. Input file with comments and blank lines
cat > "$E2E_TMPDIR/input_comments.txt" <<EOF
# This is a comment
http://127.0.0.1:$HTTP_PORT/file1.txt

# Another comment

http://127.0.0.1:$HTTP_PORT/file2.txt
EOF
"$ARIA2C" -i "$E2E_TMPDIR/input_comments.txt" -d "$E2E_TMPDIR/out2" \
  --allow-overwrite=true >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out2/file1.txt" && -f "$E2E_TMPDIR/out2/file2.txt" ]]; then
  tap_ok "input-file: comments and blank lines handled"
else
  tap_fail "input-file: comments and blank lines handled"
fi

stop_http_server
tap_summary
