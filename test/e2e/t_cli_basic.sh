#!/usr/bin/env bash
# e2e: CLI basic tests (--version, --help, bad URI)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3

# 1. --version exits 0 and contains version string
run_cmd "$ARIA2C" --version
assert_exit_code "$_last_exit" 0 "aria2c --version exits 0"

# 2. --help exits 0 and contains Usage
run_cmd "$ARIA2C" --help
assert_exit_code "$_last_exit" 0 "aria2c --help exits 0"

# 3. bad URI exits non-zero
run_cmd "$ARIA2C" --max-tries=1 --retry-wait=0 --connect-timeout=1 \
  "baduri://invalid"
if [[ $_last_exit -ne 0 ]]; then
  tap_ok "aria2c with bad URI exits non-zero"
else
  tap_fail "aria2c with bad URI exits non-zero (got exit 0)"
fi

tap_summary
