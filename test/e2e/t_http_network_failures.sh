#!/usr/bin/env bash
# e2e: Network failure simulation and recovery tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 5
make_tempdir

# Create test data
mkdir -p "$E2E_TMPDIR/htdocs"
dd if=/dev/urandom of="$E2E_TMPDIR/htdocs/1mb.bin" bs=1024 count=1024 2>/dev/null
expected_hash=$(shasum -a 256 "$E2E_TMPDIR/htdocs/1mb.bin" | awk '{print $1}')

# ── Test 1: Mid-stream disconnect + resume ──
# Server disconnects after ~512KB of a 1MB file; aria2c retries and completes
stop_http_server
start_http_server --port 18188 --dir "$E2E_TMPDIR/htdocs" \
  --disconnect-after-bytes 524288
rc=0
"$ARIA2C" --no-conf --max-tries=3 --retry-wait=1 \
  --connect-timeout=10 --timeout=10 \
  -d "$E2E_TMPDIR" -o "resume1.bin" \
  "http://127.0.0.1:18188/1mb.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 ]]; then
  actual_hash=$(shasum -a 256 "$E2E_TMPDIR/resume1.bin" | awk '{print $1}')
  if [[ "$actual_hash" == "$expected_hash" ]]; then
    tap_ok "network: mid-stream disconnect + resume completes with correct SHA256"
  else
    tap_fail "network: mid-stream disconnect + resume SHA256 mismatch"
  fi
else
  tap_fail "network: mid-stream disconnect + resume failed (exit $rc)"
fi
stop_http_server

# ── Test 2: Complete disconnect (0 bytes body) ──
start_http_server --port 18188 --dir "$E2E_TMPDIR/htdocs" \
  --disconnect-after-bytes 1
rc=0
"$ARIA2C" --no-conf --max-tries=1 \
  --connect-timeout=5 --timeout=5 \
  -d "$E2E_TMPDIR" -o "disconnect.bin" \
  "http://127.0.0.1:18188/1mb.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "network: complete disconnect exits non-zero (exit $rc)"
else
  tap_fail "network: complete disconnect should exit non-zero"
fi
stop_http_server

# ── Test 3: Resume correctness with --continue ──
# First attempt: server disconnects after 512KB
start_http_server --port 18188 --dir "$E2E_TMPDIR/htdocs" \
  --disconnect-after-bytes 524288
# First download attempt — will fail but leave partial file
"$ARIA2C" --no-conf --max-tries=1 \
  --connect-timeout=10 --timeout=10 \
  -d "$E2E_TMPDIR" -o "resume_cont.bin" \
  "http://127.0.0.1:18188/1mb.bin" >/dev/null 2>&1 || true
stop_http_server

# Second attempt: server works normally, aria2c resumes with --continue
start_http_server --port 18188 --dir "$E2E_TMPDIR/htdocs"
rc=0
"$ARIA2C" --no-conf --max-tries=3 --continue=true \
  --connect-timeout=10 --timeout=10 \
  -d "$E2E_TMPDIR" -o "resume_cont.bin" \
  "http://127.0.0.1:18188/1mb.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 ]]; then
  actual_hash=$(shasum -a 256 "$E2E_TMPDIR/resume_cont.bin" | awk '{print $1}')
  if [[ "$actual_hash" == "$expected_hash" ]]; then
    tap_ok "network: resume with --continue completes with correct SHA256"
  else
    tap_fail "network: resume with --continue SHA256 mismatch"
  fi
else
  tap_fail "network: resume with --continue failed (exit $rc)"
fi
stop_http_server

# ── Test 4: Connection refused ──
rc=0
"$ARIA2C" --no-conf --connect-timeout=5 --timeout=5 --max-tries=1 \
  "http://127.0.0.1:18189/file" >/dev/null 2>&1 || rc=$?
assert_exit_code "$rc" 6 "network: connection refused exits 6 (NETWORK_PROBLEM)"

# ── Test 5: Timeout ──
start_http_server --port 18188 --dir "$E2E_TMPDIR/htdocs" --delay 30
rc=0
"$ARIA2C" --no-conf --timeout=2 --connect-timeout=5 --max-tries=1 \
  -d "$E2E_TMPDIR" -o "timeout.bin" \
  "http://127.0.0.1:18188/1mb.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "network: timeout exits non-zero (exit $rc)"
else
  tap_fail "network: timeout should exit non-zero"
fi
stop_http_server

tap_summary
