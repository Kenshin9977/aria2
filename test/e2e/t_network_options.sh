#!/usr/bin/env bash
# e2e: Network option tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=1 2>/dev/null
start_http_server --port 18280 --dir "$E2E_TMPDIR"

# 1. --disable-ipv6=true — force IPv4 stack; download succeeds
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --disable-ipv6=true \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out1/testfile.bin" 1024 \
  "network: --disable-ipv6=true download succeeds via IPv4"

# 2. --interface=lo0/lo — bind to loopback; download succeeds
if [[ "$(uname)" == "Darwin" ]]; then iface=lo0; else iface=lo; fi
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --interface="$iface" \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out2/testfile.bin" 1024 \
  "network: --interface=$iface binds to loopback and download succeeds"

# 3. --interface=nonexistent_iface999 — unknown interface; aria2c exits non-zero
rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --interface=nonexistent_iface999 \
  "http://127.0.0.1:$HTTP_PORT/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "network: --interface=nonexistent_iface999 exits non-zero"
else
  tap_fail "network: --interface=nonexistent_iface999 exits non-zero"
fi

stop_http_server
tap_summary
