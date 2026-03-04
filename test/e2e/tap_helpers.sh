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

tap_skip_all() {
  echo "1..0 # SKIP $1"
  exit 0
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

tap_skip() {
  _tap_count=$((_tap_count + 1))
  echo "ok $_tap_count - $1 # SKIP ${2:-}"
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

assert_sha256_match() {
  local hash1 hash2
  hash1=$(shasum -a 256 "$1" | awk '{print $1}')
  hash2=$(shasum -a 256 "$2" | awk '{print $1}')
  if [[ "$hash1" == "$hash2" ]]; then
    tap_ok "$3"
  else
    tap_fail "$3 (sha256 mismatch: $hash1 != $hash2)"
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

# Create a temporary directory, cleaned up on exit
make_tempdir() {
  E2E_TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/aria2-e2e.XXXXXX")
  trap _e2e_cleanup EXIT
}

# Start the Python HTTP test server
# Usage: start_http_server [--port P] [--dir D] [--auth user:pass]
#        [--redirect URL] [--checksum ALGO:HASH] [--ssl]
#        [--certfile F] [--keyfile F] [--fail-first-n N]
#        [--set-cookie NAME=VAL] [--require-cookie NAME=VAL]
#        [--last-modified DATE] [--delay SECS] [--log-requests FILE]
start_http_server() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port=18080
  local use_ssl=false
  local require_client_cert=false
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --port) port="$2"; shift 2 ;;
      --dir) args+=(--dir "$2"); shift 2 ;;
      --auth) args+=(--auth "$2"); shift 2 ;;
      --redirect) args+=(--redirect "$2"); shift 2 ;;
      --checksum) args+=(--checksum "$2"); shift 2 ;;
      --ssl) args+=(--ssl); use_ssl=true; shift ;;
      --certfile) args+=(--certfile "$2"); shift 2 ;;
      --keyfile) args+=(--keyfile "$2"); shift 2 ;;
      --fail-first-n) args+=(--fail-first-n "$2"); shift 2 ;;
      --set-cookie) args+=(--set-cookie "$2"); shift 2 ;;
      --require-cookie) args+=(--require-cookie "$2"); shift 2 ;;
      --last-modified) args+=(--last-modified "$2"); shift 2 ;;
      --delay) args+=(--delay "$2"); shift 2 ;;
      --log-requests) args+=(--log-requests "$2"); shift 2 ;;
      --require-header) args+=(--require-header "$2"); shift 2 ;;
      --content-disposition) args+=(--content-disposition "$2"); shift 2 ;;
      --gzip) args+=(--gzip); shift ;;
      --chunked) args+=(--chunked); shift ;;
      --link-header) args+=(--link-header "$2"); shift 2 ;;
      --digest-header) args+=(--digest-header "$2"); shift 2 ;;
      --disconnect-after-bytes) args+=(--disconnect-after-bytes "$2"); shift 2 ;;
      --max-requests) args+=(--max-requests "$2"); shift 2 ;;
      --require-client-cert) args+=(--require-client-cert "$2"); require_client_cert=true; shift 2 ;;
      *) shift ;;
    esac
  done
  args+=(--port "$port")
  python3 "$script_dir/http_server.py" "${args[@]}" &
  _http_server_pid=$!
  # Wait for server to be ready
  local tries=0
  # Wait for TCP port to be listening (fast bash builtin, no fork)
  while ! (echo >/dev/tcp/127.0.0.1/$port) 2>/dev/null; do
    tries=$((tries + 1))
    if [[ $tries -ge 50 ]]; then
      echo "ERROR: HTTP server on port $port failed to start" >&2
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

assert_file_hash() {
  # Usage: assert_file_hash FILE ALGO EXPECTED MSG
  # ALGO: md5, sha-1, sha-224, sha-256, sha-384, sha-512
  local file="$1" algo="$2" expected="$3" msg="$4"
  local actual
  case "$algo" in
    md5)      actual=$(md5 -q "$file" 2>/dev/null || md5sum "$file" | awk '{print $1}') ;;
    sha-1)    actual=$(shasum -a 1 "$file" | awk '{print $1}') ;;
    sha-224)  actual=$(shasum -a 224 "$file" | awk '{print $1}') ;;
    sha-256)  actual=$(shasum -a 256 "$file" | awk '{print $1}') ;;
    sha-384)  actual=$(shasum -a 384 "$file" | awk '{print $1}') ;;
    sha-512)  actual=$(shasum -a 512 "$file" | awk '{print $1}') ;;
    *) tap_fail "$msg (unsupported algo: $algo)"; return ;;
  esac
  if [[ "$actual" == "$expected" ]]; then
    tap_ok "$msg"
  else
    tap_fail "$msg (expected $expected, got $actual)"
  fi
}

assert_file_not_exists() {
  if [[ ! -e "$1" ]]; then
    tap_ok "$2"
  else
    tap_fail "$2 (file unexpectedly exists: $1)"
  fi
}

get_mtime_epoch() {
  stat -c %Y "$1" 2>/dev/null || stat -f %m "$1" 2>/dev/null || echo "0"
}

assert_file_mtime_year() {
  local mtime_year
  mtime_year=$(date -r "$1" +%Y 2>/dev/null || \
               stat -f '%Sm' -t '%Y' "$1" 2>/dev/null || echo "0000")
  if [[ "$mtime_year" == "$2" ]]; then
    tap_ok "$3"
  else
    tap_fail "$3 (expected mtime year $2, got $mtime_year)"
  fi
}

# ── FTP server management ──

_ftp_server_pid=""

start_ftp_server() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port=18100
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --port) port="$2"; shift 2 ;;
      --dir) args+=(--dir "$2"); shift 2 ;;
      --user) args+=(--user "$2"); shift 2 ;;
      --passwd) args+=(--passwd "$2"); shift 2 ;;
      --pasv-range) args+=(--pasv-range "$2"); shift 2 ;;
      *) shift ;;
    esac
  done
  args+=(--port "$port")
  python3 "$script_dir/ftp_server.py" "${args[@]}" &
  _ftp_server_pid=$!
  # Wait for FTP server to accept connections
  local tries=0
  while ! (echo >/dev/tcp/127.0.0.1/$port) 2>/dev/null; do
    tries=$((tries + 1))
    if [[ $tries -ge 50 ]]; then
      echo "ERROR: FTP server failed to start on port $port" >&2
      exit 1
    fi
    sleep 0.1
  done
  FTP_PORT=$port
}

stop_ftp_server() {
  if [[ -n "${_ftp_server_pid:-}" ]]; then
    kill "$_ftp_server_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_ftp_server_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_ftp_server_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_ftp_server_pid" 2>/dev/null || true
    unset _ftp_server_pid
  fi
}

# ── Proxy server management ──

_proxy_server_pid=""

start_proxy_server() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port=18120
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --port) port="$2"; shift 2 ;;
      --auth) args+=(--auth "$2"); shift 2 ;;
      --log-requests) args+=(--log-requests "$2"); shift 2 ;;
      *) shift ;;
    esac
  done
  args+=(--port "$port")
  python3 "$script_dir/proxy_server.py" "${args[@]}" &
  _proxy_server_pid=$!
  local tries=0
  while ! (echo >/dev/tcp/127.0.0.1/$port) 2>/dev/null; do
    tries=$((tries + 1))
    if [[ $tries -ge 50 ]]; then
      echo "ERROR: Proxy server failed to start on port $port" >&2
      exit 1
    fi
    sleep 0.1
  done
  PROXY_PORT=$port
}

stop_proxy_server() {
  if [[ -n "${_proxy_server_pid:-}" ]]; then
    kill "$_proxy_server_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_proxy_server_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_proxy_server_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_proxy_server_pid" 2>/dev/null || true
    unset _proxy_server_pid
  fi
}

# ── SOCKS5 proxy management ──

_socks5_server_pid=""

start_socks5_server() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port=18210
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --port) port="$2"; shift 2 ;;
      --auth) args+=(--auth "$2"); shift 2 ;;
      --log-requests) args+=(--log-requests "$2"); shift 2 ;;
      *) shift ;;
    esac
  done
  args+=(--port "$port")
  python3 "$script_dir/socks5_server.py" "${args[@]}" &
  _socks5_server_pid=$!
  # Wait for SOCKS5 server to accept connections
  local tries=0
  while ! (echo >/dev/tcp/127.0.0.1/$port) 2>/dev/null; do
    tries=$((tries + 1))
    if [[ $tries -ge 50 ]]; then
      echo "ERROR: SOCKS5 server failed to start on port $port" >&2
      exit 1
    fi
    sleep 0.1
  done
  SOCKS5_PORT=$port
}

stop_socks5_server() {
  if [[ -n "${_socks5_server_pid:-}" ]]; then
    kill "$_socks5_server_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_socks5_server_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_socks5_server_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_socks5_server_pid" 2>/dev/null || true
    unset _socks5_server_pid
  fi
}

# ── BT tracker management ──

_bt_tracker_pid=""

start_bt_tracker() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port=18135
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --port) port="$2"; shift 2 ;;
      --peer-ip) args+=(--peer-ip "$2"); shift 2 ;;
      --peer-port) args+=(--peer-port "$2"); shift 2 ;;
      *) shift ;;
    esac
  done
  args+=(--port "$port")
  python3 "$script_dir/bt_tracker.py" "${args[@]}" &
  _bt_tracker_pid=$!
  local tries=0
  while ! (echo >/dev/tcp/127.0.0.1/$port) 2>/dev/null; do
    tries=$((tries + 1))
    if [[ $tries -ge 50 ]]; then
      echo "ERROR: BT tracker failed to start on port $port" >&2
      exit 1
    fi
    sleep 0.1
  done
  BT_TRACKER_PORT=$port
}

stop_bt_tracker() {
  if [[ -n "${_bt_tracker_pid:-}" ]]; then
    kill "$_bt_tracker_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_bt_tracker_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_bt_tracker_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_bt_tracker_pid" 2>/dev/null || true
    unset _bt_tracker_pid
  fi
}

# ── RPC helpers ──

_rpc_aria2_pid=""

start_aria2_rpc() {
  local port="$1"; shift
  "$ARIA2C" --enable-rpc --rpc-listen-port="$port" \
    --rpc-listen-all=false --no-conf --daemon=false --quiet=true \
    "$@" &
  _rpc_aria2_pid=$!
  local tries=0
  while ! (echo >/dev/tcp/127.0.0.1/$port) 2>/dev/null; do
    tries=$((tries + 1))
    if [[ $tries -ge 50 ]]; then
      echo "ERROR: aria2c RPC failed to start on port $port" >&2
      exit 1
    fi
    sleep 0.1
  done
  RPC_PORT=$port
}

stop_aria2_rpc() {
  if [[ -n "${_rpc_aria2_pid:-}" ]]; then
    # Try graceful RPC shutdown first
    curl -s "http://127.0.0.1:${RPC_PORT:-0}/jsonrpc" \
      -d '{"jsonrpc":"2.0","id":"sd","method":"aria2.shutdown"}' \
      >/dev/null 2>&1 || true
    local i=0
    while kill -0 "$_rpc_aria2_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 20 ]]; then
        kill -9 "$_rpc_aria2_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_rpc_aria2_pid" 2>/dev/null || true
    unset _rpc_aria2_pid
  fi
}

rpc_call() {
  local port="$1"
  local method="$2"
  local params="${3:-}"
  local data
  if [[ -n "$params" ]]; then
    data="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"$method\",\"params\":$params}"
  else
    data="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"$method\"}"
  fi
  curl -s "http://127.0.0.1:$port/jsonrpc" -d "$data"
}

rpc_call_tls() {
  local port="$1"
  local method="$2"
  local params="${3:-}"
  local data
  if [[ -n "$params" ]]; then
    data="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"$method\",\"params\":$params}"
  else
    data="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"$method\"}"
  fi
  curl -sk "https://127.0.0.1:$port/jsonrpc" -d "$data"
}

rpc_call_tls_cacert() {
  local port="$1"
  local cacert="$2"
  local method="$3"
  local params="${4:-}"
  local data
  if [[ -n "$params" ]]; then
    data="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"$method\",\"params\":$params}"
  else
    data="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"$method\"}"
  fi
  curl -s --cacert "$cacert" "https://127.0.0.1:$port/jsonrpc" -d "$data"
}

xmlrpc_call() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port="$1"
  local method="$2"
  local params="${3:-}"
  local args=(--port "$port" --method "$method")
  if [[ -n "$params" ]]; then
    args+=(--params "$params")
  fi
  python3 "$script_dir/xmlrpc_client.py" "${args[@]}"
}

ws_rpc_call() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local port="$1"
  local method="$2"
  shift 2
  local params=""
  local extra_args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --params) params="$2"; shift 2 ;;
      --wait-notifications|--secret|--timeout) extra_args+=("$1" "$2"); shift 2 ;;
      --ssl|--no-verify) extra_args+=("$1"); shift ;;
      *) params="$1"; shift ;;
    esac
  done
  local args=(--port "$port" --method "$method")
  if [[ -n "$params" ]]; then
    args+=(--params "$params")
  fi
  python3 "$script_dir/ws_client.py" "${args[@]}" "${extra_args[@]}"
}

# ── Extended cleanup ──

_e2e_cleanup() {
  stop_http_server
  stop_ftp_server
  stop_proxy_server
  stop_socks5_server
  stop_bt_tracker
  stop_aria2_rpc
  if [[ -n "${E2E_TMPDIR:-}" ]]; then
    rm -rf "$E2E_TMPDIR" 2>/dev/null || true
  fi
}

tap_summary() {
  if [[ $_tap_failures -gt 0 ]]; then
    exit 1
  fi
  exit 0
}
