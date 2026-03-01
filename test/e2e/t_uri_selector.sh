#!/usr/bin/env bash
# e2e: URI selector strategy tests (inorder, feedback, adaptive)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# Create 64KB test file
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=64 2>/dev/null

# Start fast server (no delay) using helper
start_http_server --port 18118 --dir "$E2E_TMPDIR"

# Start slow server manually (--delay 2)
python3 "$SCRIPT_DIR/http_server.py" --port 18119 --dir "$E2E_TMPDIR" --delay 2 &
_slow_server_pid=$!
tries=0
while ! curl -sk "http://127.0.0.1:18119/health" >/dev/null 2>&1; do
  tries=$((tries + 1))
  [[ $tries -ge 30 ]] && { echo "ERROR: slow server failed to start" >&2; exit 1; }
  sleep 0.1
done

# Custom cleanup to also kill the slow server
_orig_cleanup() {
  _e2e_cleanup
}
_extended_cleanup() {
  if [[ -n "${_slow_server_pid:-}" ]]; then
    kill "$_slow_server_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_slow_server_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_slow_server_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_slow_server_pid" 2>/dev/null || true
    unset _slow_server_pid
  fi
  _orig_cleanup
}
trap _extended_cleanup EXIT

# 1. --uri-selector=inorder: download succeeds with correct SHA256
mkdir -p "$E2E_TMPDIR/out1"
"$ARIA2C" -d "$E2E_TMPDIR/out1" --no-conf \
  --uri-selector=inorder \
  "http://127.0.0.1:18118/testfile.bin" \
  "http://127.0.0.1:18119/testfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/testfile.bin" "$E2E_TMPDIR/out1/testfile.bin" \
  "uri-selector=inorder downloads correctly"

# 2. --uri-selector=feedback: server stat file is created
mkdir -p "$E2E_TMPDIR/out2"
"$ARIA2C" -d "$E2E_TMPDIR/out2" --no-conf \
  --uri-selector=feedback \
  --server-stat-of="$E2E_TMPDIR/server-stat.txt" \
  "http://127.0.0.1:18118/testfile.bin" \
  "http://127.0.0.1:18119/testfile.bin" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/server-stat.txt" \
  "uri-selector=feedback creates server stat file"

# 3. --uri-selector=adaptive: download succeeds with correct SHA256
mkdir -p "$E2E_TMPDIR/out3"
"$ARIA2C" -d "$E2E_TMPDIR/out3" --no-conf \
  --uri-selector=adaptive \
  "http://127.0.0.1:18118/testfile.bin" \
  "http://127.0.0.1:18119/testfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/testfile.bin" "$E2E_TMPDIR/out3/testfile.bin" \
  "uri-selector=adaptive downloads correctly"

stop_http_server
tap_summary
