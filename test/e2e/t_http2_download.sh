#!/usr/bin/env bash
# e2e: HTTP/2 downloads via nghttpd
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

NGHTTPD=$(command -v nghttpd 2>/dev/null || echo "")
H2_PORT=18200

# Skip all if nghttpd is not available
if [[ -z "$NGHTTPD" ]]; then
  tap_skip_all "nghttpd not available"
fi

# Skip all if aria2c was not compiled with HTTP/2 support (libnghttp2)
if ! "$ARIA2C" --version 2>&1 | grep -q "Enabled Features:.*HTTP/2"; then
  tap_skip_all "aria2c not compiled with HTTP/2 support"
fi

tap_plan 5
make_tempdir

# ── Setup: TLS certs and test data ──

mkdir -p "$E2E_TMPDIR/htdocs"

# Generate self-signed cert for nghttpd (TLS required for h2)
openssl req -x509 -newkey rsa:2048 -keyout "$E2E_TMPDIR/key.pem" \
  -out "$E2E_TMPDIR/cert.pem" -days 1 -nodes \
  -subj "/CN=localhost" 2>/dev/null

# Create test files
dd if=/dev/urandom of="$E2E_TMPDIR/htdocs/small.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/htdocs/1mb.bin" bs=1024 count=1024 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/htdocs/4mb.bin" bs=1024 count=4096 2>/dev/null

small_hash=$(shasum -a 256 "$E2E_TMPDIR/htdocs/small.bin" | awk '{print $1}')
mb1_hash=$(shasum -a 256 "$E2E_TMPDIR/htdocs/1mb.bin" | awk '{print $1}')
mb4_hash=$(shasum -a 256 "$E2E_TMPDIR/htdocs/4mb.bin" | awk '{print $1}')

# Start nghttpd with TLS
"$NGHTTPD" -d "$E2E_TMPDIR/htdocs" "$H2_PORT" \
  "$E2E_TMPDIR/key.pem" "$E2E_TMPDIR/cert.pem" &
nghttpd_pid=$!

# Wait for port readiness
tries=0
while ! (echo >/dev/tcp/127.0.0.1/$H2_PORT) 2>/dev/null; do
  tries=$((tries + 1))
  if [[ $tries -ge 50 ]]; then
    kill "$nghttpd_pid" 2>/dev/null || true
    tap_skip_all "nghttpd failed to start on port $H2_PORT"
  fi
  sleep 0.1
done

_cleanup_nghttpd() {
  kill "$nghttpd_pid" 2>/dev/null || true
  wait "$nghttpd_pid" 2>/dev/null || true
}
_orig_cleanup=$(declare -f _e2e_cleanup | tail -n +2)
_e2e_cleanup() {
  _cleanup_nghttpd
  eval "$_orig_cleanup"
}

# ── Tests ──

# 1. Download 1KB file via HTTP/2
rc=0
"$ARIA2C" --no-conf --check-certificate=false \
  --connect-timeout=10 --timeout=10 \
  -d "$E2E_TMPDIR" -o "h2_small.bin" \
  "https://127.0.0.1:$H2_PORT/small.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 ]]; then
  actual=$(shasum -a 256 "$E2E_TMPDIR/h2_small.bin" | awk '{print $1}')
  if [[ "$actual" == "$small_hash" ]]; then
    tap_ok "http2: 1KB file download SHA256 matches"
  else
    tap_fail "http2: 1KB file SHA256 mismatch ($actual != $small_hash)"
  fi
else
  tap_fail "http2: 1KB file download failed (exit $rc)"
fi

# 2. Download 1MB file via HTTP/2
rc=0
"$ARIA2C" --no-conf --check-certificate=false \
  --connect-timeout=10 --timeout=10 \
  -d "$E2E_TMPDIR" -o "h2_1mb.bin" \
  "https://127.0.0.1:$H2_PORT/1mb.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 ]]; then
  actual=$(shasum -a 256 "$E2E_TMPDIR/h2_1mb.bin" | awk '{print $1}')
  if [[ "$actual" == "$mb1_hash" ]]; then
    tap_ok "http2: 1MB file download SHA256 matches"
  else
    tap_fail "http2: 1MB file SHA256 mismatch ($actual != $mb1_hash)"
  fi
else
  tap_fail "http2: 1MB file download failed (exit $rc)"
fi

# 3. Two files with --max-concurrent-downloads=2
rc=0
"$ARIA2C" --no-conf --check-certificate=false \
  --connect-timeout=10 --timeout=10 \
  --max-concurrent-downloads=2 \
  -d "$E2E_TMPDIR" \
  "https://127.0.0.1:$H2_PORT/small.bin" \
  "https://127.0.0.1:$H2_PORT/1mb.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 ]]; then
  h1=$(shasum -a 256 "$E2E_TMPDIR/small.bin" | awk '{print $1}')
  h2=$(shasum -a 256 "$E2E_TMPDIR/1mb.bin" | awk '{print $1}')
  if [[ "$h1" == "$small_hash" && "$h2" == "$mb1_hash" ]]; then
    tap_ok "http2: concurrent downloads both SHA256 match"
  else
    tap_fail "http2: concurrent downloads SHA256 mismatch"
  fi
else
  tap_fail "http2: concurrent downloads failed (exit $rc)"
fi

# 4. Large file (4MB) with --split=4
rc=0
"$ARIA2C" --no-conf --check-certificate=false \
  --connect-timeout=10 --timeout=10 \
  --split=4 --min-split-size=1M \
  -d "$E2E_TMPDIR" -o "h2_4mb.bin" \
  "https://127.0.0.1:$H2_PORT/4mb.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -eq 0 ]]; then
  actual=$(shasum -a 256 "$E2E_TMPDIR/h2_4mb.bin" | awk '{print $1}')
  if [[ "$actual" == "$mb4_hash" ]]; then
    tap_ok "http2: 4MB split download SHA256 matches"
  else
    tap_fail "http2: 4MB split download SHA256 mismatch ($actual != $mb4_hash)"
  fi
else
  tap_fail "http2: 4MB split download failed (exit $rc)"
fi

# 5. Non-existent file → exits non-zero
rc=0
"$ARIA2C" --no-conf --check-certificate=false \
  --connect-timeout=5 --timeout=5 --max-tries=1 \
  -d "$E2E_TMPDIR" -o "h2_404.bin" \
  "https://127.0.0.1:$H2_PORT/no_such_file.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "http2: non-existent file exits non-zero (exit $rc)"
else
  tap_fail "http2: non-existent file should exit non-zero"
fi

tap_summary
