#!/usr/bin/env bash
# Runner for aria2 e2e tests.
# Executes all t_*.sh scripts, aggregates TAP output.
# Usage: run_all.sh [--filter=pattern]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
filter=""
for arg in "$@"; do
  case "$arg" in
    --filter=*) filter="${arg#--filter=}" ;;
  esac
done

total_pass=0
total_fail=0
failed_scripts=()

# Detect if the aria2c binary supports BitTorrent
ARIA2C="${ARIA2C:-$(dirname "$SCRIPT_DIR")/../src/aria2c}"
if [[ -x "$ARIA2C" ]] && "$ARIA2C" -v 2>&1 | grep -q 'BitTorrent'; then
  bt_enabled=true
else
  bt_enabled=false
fi

for test_script in "$SCRIPT_DIR"/t_*.sh; do
  name=$(basename "$test_script")

  # Skip BT tests if BitTorrent support is disabled
  if [[ "$bt_enabled" != true && "$name" =~ ^t_bt_ ]]; then
    echo "# Skipping $name (BitTorrent disabled)"
    continue
  fi

  # Apply filter if set
  if [[ -n "$filter" && ! "$name" =~ $filter ]]; then
    continue
  fi

  echo "# Running $name"
  exit_code=0
  output=$(bash "$test_script" 2>&1) || exit_code=$?

  echo "$output"

  pass=$(echo "$output" | grep -c "^ok " || true)
  fail=$(echo "$output" | grep -c "^not ok " || true)
  total_pass=$((total_pass + pass))
  total_fail=$((total_fail + fail))

  if [[ $exit_code -ne 0 || $fail -gt 0 ]]; then
    failed_scripts+=("$name")
  fi
  echo ""
done

echo "# =========================="
echo "# Total: $((total_pass + total_fail)) tests"
echo "# Pass:  $total_pass"
echo "# Fail:  $total_fail"
echo "# =========================="

if [[ ${#failed_scripts[@]} -gt 0 ]]; then
  echo "# FAILED scripts:"
  for s in "${failed_scripts[@]}"; do
    echo "#   $s"
  done
  exit 1
fi

echo "# All tests passed."
exit 0
