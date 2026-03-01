#!/usr/bin/env bash
# e2e: BitTorrent show-files tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create first test file and its .torrent
echo "hello-aria2-bt-showfiles" > "$E2E_TMPDIR/alpha.dat"
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/alpha.dat" \
  --output "$E2E_TMPDIR/alpha.torrent"

# 1. --show-files on alpha.torrent — output contains file name, exit 0
run_cmd "$ARIA2C" --show-files -T "$E2E_TMPDIR/alpha.torrent"
assert_exit_code "$_last_exit" 0 \
  "show-files alpha.torrent exits 0"
assert_output_contains "alpha.dat" \
  "show-files alpha.torrent lists alpha.dat"

# Create second test file and its .torrent
dd if=/dev/urandom of="$E2E_TMPDIR/bravo.bin" bs=512 count=1 2>/dev/null
python3 "$SCRIPT_DIR/create_torrent.py" \
  --file "$E2E_TMPDIR/bravo.bin" \
  --output "$E2E_TMPDIR/bravo.torrent" \
  --piece-length 16384

# 2. --show-files on bravo.torrent — output contains file info
run_cmd "$ARIA2C" --show-files -T "$E2E_TMPDIR/bravo.torrent"
assert_exit_code "$_last_exit" 0 \
  "show-files bravo.torrent exits 0"
assert_output_contains "bravo.bin" \
  "show-files bravo.torrent lists bravo.bin"

tap_summary
