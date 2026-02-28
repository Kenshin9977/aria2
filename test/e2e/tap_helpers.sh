#!/usr/bin/env bash
# TAP (Test Anything Protocol) helpers for aria2 e2e tests
# Sourced by each t_*.sh test script.

set -euo pipefail

_tap_count=0
_tap_plan=0
_tap_failures=0

tap_plan() {
  _tap_plan=$1
  echo "1..$1"
}

tap_ok() {
  _tap_count=$((_tap_count + 1))
  echo "ok $_tap_count - $1"
}

tap_fail() {
  _tap_count=$((_tap_count + 1))
  _tap_failures=$((_tap_failures + 1))
  echo "not ok $_tap_count - $1"
}

assert_file_exists() {
  if [[ -f "$1" ]]; then
    tap_ok "$2"
  else
    tap_fail "$2 (file not found: $1)"
  fi
}

assert_file_size() {
  local actual
  actual=$(wc -c < "$1" 2>/dev/null | tr -d ' ')
  if [[ "$actual" -eq "$2" ]]; then
    tap_ok "$3"
  else
    tap_fail "$3 (expected $2 bytes, got $actual)"
  fi
}

assert_file_contains() {
  if grep -q "$2" "$1" 2>/dev/null; then
    tap_ok "$3"
  else
    tap_fail "$3 (pattern '$2' not found in $1)"
  fi
}

assert_exit_code() {
  if [[ "$1" -eq "$2" ]]; then
    tap_ok "$3"
  else
    tap_fail "$3 (expected exit $2, got $1)"
  fi
}

assert_output_contains() {
  if echo "$_last_output" | grep -q "$1"; then
    tap_ok "$2"
  else
    tap_fail "$2 (pattern '$1' not in output)"
  fi
}

# Run a command, capture output and exit code
run_cmd() {
  _last_exit=0
  _last_output=$("$@" 2>&1) || _last_exit=$?
}

# Locate the aria2c binary
find_aria2c() {
  if [[ -n "${ARIA2C:-}" ]]; then
    echo "$ARIA2C"
    return
  fi
  local dir
  dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
  if [[ -x "$dir/../src/aria2c" ]]; then
    echo "$dir/../src/aria2c"
  elif command -v aria2c >/dev/null 2>&1; then
    command -v aria2c
  else
    echo "ERROR: aria2c not found" >&2
    exit 1
  fi
}

ARIA2C="$(find_aria2c)"

# Cleanup function called on EXIT
_e2e_cleanup() {
  stop_http_server
  if [[ -n "${E2E_TMPDIR:-}" ]]; then
    rm -rf "$E2E_TMPDIR"
  fi
}

# Create a temporary directory, cleaned up on exit
make_tempdir() {
  E2E_TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/aria2-e2e.XXXXXX")
  trap _e2e_cleanup EXIT
}

# Start the Python HTTP test server
# Usage: start_http_server [--port P] [--dir D] [--auth user:pass]
#        [--redirect URL] [--checksum ALGO:HASH]
start_http_server() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port=18080
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --port) port="$2"; shift 2 ;;
      --dir) args+=(--dir "$2"); shift 2 ;;
      --auth) args+=(--auth "$2"); shift 2 ;;
      --redirect) args+=(--redirect "$2"); shift 2 ;;
      --checksum) args+=(--checksum "$2"); shift 2 ;;
      *) shift ;;
    esac
  done
  args+=(--port "$port")
  python3 "$script_dir/http_server.py" "${args[@]}" &
  _http_server_pid=$!
  # Wait for server to be ready
  local tries=0
  while ! curl -s "http://127.0.0.1:$port/health" >/dev/null 2>&1; do
    tries=$((tries + 1))
    if [[ $tries -ge 30 ]]; then
      echo "ERROR: HTTP server failed to start" >&2
      exit 1
    fi
    sleep 0.1
  done
  HTTP_PORT=$port
}

stop_http_server() {
  if [[ -n "${_http_server_pid:-}" ]]; then
    kill "$_http_server_pid" 2>/dev/null || true
    # Use timeout on wait to prevent hanging
    local i=0
    while kill -0 "$_http_server_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_http_server_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_http_server_pid" 2>/dev/null || true
    unset _http_server_pid
  fi
}

tap_summary() {
  if [[ $_tap_failures -gt 0 ]]; then
    exit 1
  fi
  exit 0
}
