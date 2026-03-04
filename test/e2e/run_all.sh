#!/usr/bin/env bash
# Runner for aria2 e2e tests.
# Executes t_*.sh scripts in parallel, aggregates TAP output.
# Usage: run_all.sh [--filter=pattern] [--jobs=N]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
filter=""
jobs=4
for arg in "$@"; do
  case "$arg" in
    --filter=*) filter="${arg#--filter=}" ;;
    --jobs=*) jobs="${arg#--jobs=}" ;;
  esac
done

# Detect if the aria2c binary supports BitTorrent
ARIA2C="${ARIA2C:-$(dirname "$SCRIPT_DIR")/../src/aria2c}"
export ARIA2C
if [[ -x "$ARIA2C" ]] && "$ARIA2C" -v 2>&1 | grep -q 'BitTorrent'; then
  bt_enabled=true
else
  bt_enabled=false
fi

# Port-conflict groups: tests sharing ports must run sequentially.
# Group members are joined with | to form a regex.
# Port 18100: t_ftp_download, t_ftp_passive, t_ftp_resume, t_rpc_lifecycle
# Port 18120: t_proxy_http, t_proxy_https
# Ports 18261-18262: t_bt_advanced, t_hooks_extended
CONFLICT_GROUP_1="t_ftp_download|t_ftp_passive|t_ftp_resume|t_rpc_lifecycle"
CONFLICT_GROUP_2="t_proxy_http|t_proxy_https"
CONFLICT_GROUP_3="t_bt_advanced|t_hooks_extended"

# Collect test scripts to run
scripts=()
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

  scripts+=("$test_script")
done

# Create output directory for parallel results
results_dir=$(mktemp -d "${TMPDIR:-/tmp}/aria2-e2e-results.XXXXXX")
trap 'rm -rf "$results_dir"' EXIT

per_test_timeout=${E2E_TIMEOUT:-120}

# Run a single test script and write results to a file
run_one_test() {
  local test_script="$1"
  local result_file="$2"
  local name
  name=$(basename "$test_script")

  local exit_code=0
  local output
  output=$(timeout "$per_test_timeout" bash "$test_script" 2>&1) \
    || exit_code=$?
  if [[ $exit_code -eq 124 ]]; then
    output+=$'\n'"not ok - $name timed out after ${per_test_timeout}s"
  fi

  local pass fail
  pass=$(echo "$output" | grep -c "^ok " || true)
  fail=$(echo "$output" | grep -c "^not ok " || true)

  # Write structured result
  {
    echo "EXIT=$exit_code"
    echo "PASS=$pass"
    echo "FAIL=$fail"
    echo "---OUTPUT---"
    echo "# Running $name"
    echo "$output"
    echo ""
  } > "$result_file"
}
export -f run_one_test
export per_test_timeout

# Separate scripts into conflict groups and independent tests
group1_scripts=()
group2_scripts=()
group3_scripts=()
independent_scripts=()

for s in "${scripts[@]}"; do
  name=$(basename "$s" .sh)
  if [[ "$name" =~ ^($CONFLICT_GROUP_1) ]]; then
    group1_scripts+=("$s")
  elif [[ "$name" =~ ^($CONFLICT_GROUP_2) ]]; then
    group2_scripts+=("$s")
  elif [[ "$name" =~ ^($CONFLICT_GROUP_3) ]]; then
    group3_scripts+=("$s")
  else
    independent_scripts+=("$s")
  fi
done

# Run a conflict group sequentially, writing results
run_conflict_group() {
  local group_id="$1"
  shift
  local idx=0
  for s in "$@"; do
    run_one_test "$s" "$results_dir/group${group_id}_${idx}"
    idx=$((idx + 1))
  done
}

# Launch conflict groups as single parallel units
bg_pids=()
if [[ ${#group1_scripts[@]} -gt 0 ]]; then
  run_conflict_group 1 "${group1_scripts[@]}" &
  bg_pids+=($!)
fi
if [[ ${#group2_scripts[@]} -gt 0 ]]; then
  run_conflict_group 2 "${group2_scripts[@]}" &
  bg_pids+=($!)
fi
if [[ ${#group3_scripts[@]} -gt 0 ]]; then
  run_conflict_group 3 "${group3_scripts[@]}" &
  bg_pids+=($!)
fi

# Launch independent tests in parallel (up to $jobs at a time)
idx=0
for s in "${independent_scripts[@]}"; do
  run_one_test "$s" "$results_dir/ind_${idx}" &
  bg_pids+=($!)
  idx=$((idx + 1))

  # Limit concurrency
  if [[ ${#bg_pids[@]} -ge $jobs ]]; then
    wait "${bg_pids[0]}" 2>/dev/null || true
    bg_pids=("${bg_pids[@]:1}")
  fi
done

# Wait for all remaining jobs
for pid in "${bg_pids[@]}"; do
  wait "$pid" 2>/dev/null || true
done

# Aggregate results in original script order
total_pass=0
total_fail=0
failed_scripts=()

for result_file in "$results_dir"/*; do
  [[ -f "$result_file" ]] || continue

  exit_code=$(grep '^EXIT=' "$result_file" | head -1 | cut -d= -f2)
  pass=$(grep '^PASS=' "$result_file" | head -1 | cut -d= -f2)
  fail=$(grep '^FAIL=' "$result_file" | head -1 | cut -d= -f2)

  # Print output section
  sed -n '/^---OUTPUT---$/,$ p' "$result_file" | tail -n +2

  total_pass=$((total_pass + pass))
  total_fail=$((total_fail + fail))

  if [[ $exit_code -ne 0 || $fail -gt 0 ]]; then
    # Extract test name from output
    name=$(sed -n 's/^# Running //p' "$result_file" | head -1)
    failed_scripts+=("${name:-unknown}")
  fi
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
