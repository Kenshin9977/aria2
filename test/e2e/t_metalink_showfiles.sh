#!/usr/bin/env bash
# e2e: Metalink --show-files tests (no HTTP server needed)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 2
make_tempdir

# 1. --show-files on a .meta4 with two files — output lists file names
cat > "$E2E_TMPDIR/show1.meta4" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="alpha.dat">
    <url>http://example.com/alpha.dat</url>
  </file>
  <file name="bravo.dat">
    <url>http://example.com/bravo.dat</url>
  </file>
</metalink>
EOF

run_cmd "$ARIA2C" --show-files --metalink-file="$E2E_TMPDIR/show1.meta4"
assert_exit_code "$_last_exit" 0 \
  "showfiles: --show-files exits 0"

# Verify both file names appear in output
if echo "$_last_output" | grep -q "alpha.dat" && \
   echo "$_last_output" | grep -q "bravo.dat"; then
  tap_ok "showfiles: output lists alpha.dat and bravo.dat"
else
  tap_fail "showfiles: output lists alpha.dat and bravo.dat (output: $_last_output)"
fi

tap_summary
