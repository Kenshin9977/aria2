#!/usr/bin/env bash
# e2e: Download event hook tests (on-download-complete, on-download-error)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create a 1KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null

start_http_server --port 18110 --dir "$E2E_TMPDIR"

# Create hook scripts
cat > "$E2E_TMPDIR/on_complete.sh" <<HOOKEOF
#!/bin/bash
echo "\$1" > "$E2E_TMPDIR/complete_marker.txt"
HOOKEOF
chmod +x "$E2E_TMPDIR/on_complete.sh"

cat > "$E2E_TMPDIR/on_error.sh" <<HOOKEOF
#!/bin/bash
echo "\$1" > "$E2E_TMPDIR/error_marker.txt"
HOOKEOF
chmod +x "$E2E_TMPDIR/on_error.sh"

cat > "$E2E_TMPDIR/on_complete_404.sh" <<HOOKEOF
#!/bin/bash
echo "\$1" > "$E2E_TMPDIR/complete_404_marker.txt"
HOOKEOF
chmod +x "$E2E_TMPDIR/on_complete_404.sh"

# 1. --on-download-complete fires on success — marker file created with GID
"$ARIA2C" -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --on-download-complete="$E2E_TMPDIR/on_complete.sh" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
# Give the hook a moment to execute
sleep 0.5
assert_file_exists "$E2E_TMPDIR/complete_marker.txt" \
  "hooks: --on-download-complete fires on success (marker created)"

# 2. --on-download-error fires on 404 — error marker created
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --max-tries=1 \
  --on-download-error="$E2E_TMPDIR/on_error.sh" \
  "http://127.0.0.1:$HTTP_PORT/nonexistent_file.bin" >/dev/null 2>&1 || rc=$?
# Give the hook a moment to execute
sleep 0.5
assert_file_exists "$E2E_TMPDIR/error_marker.txt" \
  "hooks: --on-download-error fires on 404 (error marker created)"

# 3. --on-download-complete does NOT fire on 404 — marker absent
rc=0
"$ARIA2C" -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --max-tries=1 \
  --on-download-complete="$E2E_TMPDIR/on_complete_404.sh" \
  "http://127.0.0.1:$HTTP_PORT/another_missing_file.bin" >/dev/null 2>&1 || rc=$?
# Give time for any potential (erroneous) hook execution
sleep 0.5
assert_file_not_exists "$E2E_TMPDIR/complete_404_marker.txt" \
  "hooks: --on-download-complete does NOT fire on 404"

stop_http_server
tap_summary
