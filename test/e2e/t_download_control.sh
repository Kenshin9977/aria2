#!/usr/bin/env bash
# e2e: Process lifecycle and concurrency control tests (stop,
#      stop-with-process, lowest-speed-limit, deferred-input,
#      optimize-concurrent-downloads, piece-length)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 7
make_tempdir

# Create test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=4 2>/dev/null
# 512KB file for piece-length test
dd if=/dev/urandom of="$E2E_TMPDIR/bigfile.bin" \
  bs=1024 count=512 2>/dev/null
BIG_HASH=$(shasum -a 256 "$E2E_TMPDIR/bigfile.bin" | awk '{print $1}')
# Three small files for concurrent download tests
dd if=/dev/urandom of="$E2E_TMPDIR/file_a.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file_b.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file_c.bin" bs=1024 count=1 2>/dev/null

# ── Test 1: --stop=2 terminates after N seconds ──────────────────────────
start_http_server --port 18330 --dir "$E2E_TMPDIR" --delay 10
t_start=$SECONDS
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --stop=2 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
t_elapsed=$((SECONDS - t_start))
if [[ $t_elapsed -le 8 ]]; then
  tap_ok "stop=2 terminates aria2c within timeout (${t_elapsed}s)"
else
  tap_fail "stop=2 terminates aria2c within timeout (took ${t_elapsed}s)"
fi
stop_http_server

# ── Test 2: --stop-with-process=PID exits when PID dies ──────────────────
start_http_server --port 18331 --dir "$E2E_TMPDIR" --delay 10
sleep 2 &
_sleep_pid=$!
t_start=$SECONDS
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --stop-with-process="$_sleep_pid" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
t_elapsed=$((SECONDS - t_start))
if [[ $t_elapsed -le 8 ]]; then
  tap_ok "stop-with-process exits after monitored process dies (${t_elapsed}s)"
else
  tap_fail "stop-with-process exits after monitored process dies (took ${t_elapsed}s)"
fi
stop_http_server

# Start a single server instance for tests 3-7 (all normal, no delay)
start_http_server --port 18332 --dir "$E2E_TMPDIR"

# ── Test 3: --lowest-speed-limit=1M on fast connection succeeds ───────────
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --lowest-speed-limit=1048576 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out3/testfile.bin" 4096 \
  "lowest-speed-limit=1M on fast local connection succeeds"

# ── Test 4: --lowest-speed-limit=1 keeps fast connection ─────────────────
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --lowest-speed-limit=1 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out4/testfile.bin" 4096 \
  "lowest-speed-limit=1 keeps fast connection, download succeeds"

# ── Test 5: --deferred-input=true downloads all URIs from input file ─────
cat > "$E2E_TMPDIR/input.txt" <<EOF
http://127.0.0.1:$HTTP_PORT/file_a.bin
http://127.0.0.1:$HTTP_PORT/file_b.bin
http://127.0.0.1:$HTTP_PORT/file_c.bin
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --deferred-input=true -i "$E2E_TMPDIR/input.txt" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out5/file_a.bin" ]] && \
   [[ -f "$E2E_TMPDIR/out5/file_b.bin" ]] && \
   [[ -f "$E2E_TMPDIR/out5/file_c.bin" ]]; then
  tap_ok "deferred-input=true downloads all URIs from input file"
else
  tap_fail "deferred-input=true downloads all URIs from input file"
fi

# ── Test 6: --optimize-concurrent-downloads=true succeeds ────────────────
cat > "$E2E_TMPDIR/input6.txt" <<EOF
http://127.0.0.1:$HTTP_PORT/file_a.bin
http://127.0.0.1:$HTTP_PORT/file_b.bin
http://127.0.0.1:$HTTP_PORT/file_c.bin
EOF
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out6" --allow-overwrite=true \
  -j 3 --optimize-concurrent-downloads=true \
  -i "$E2E_TMPDIR/input6.txt" >/dev/null 2>&1 || rc=$?
if [[ -f "$E2E_TMPDIR/out6/file_a.bin" ]] && \
   [[ -f "$E2E_TMPDIR/out6/file_b.bin" ]] && \
   [[ -f "$E2E_TMPDIR/out6/file_c.bin" ]]; then
  tap_ok "optimize-concurrent-downloads=true downloads all files"
else
  tap_fail "optimize-concurrent-downloads=true downloads all files (exit $rc)"
fi

# ── Test 7: --piece-length=1048576 downloads with custom segment size ────
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out7" --allow-overwrite=true \
  --piece-length=1048576 --split=4 \
  "http://127.0.0.1:$HTTP_PORT/bigfile.bin" >/dev/null 2>&1
assert_file_hash "$E2E_TMPDIR/out7/bigfile.bin" sha-256 "$BIG_HASH" \
  "piece-length=1M with split=4 downloads correctly"

stop_http_server
tap_summary
