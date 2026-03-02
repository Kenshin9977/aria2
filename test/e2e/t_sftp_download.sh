#!/usr/bin/env bash
# e2e: SFTP download error paths and option acceptance tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

# 1. SFTP URI against non-SSH TCP stub — aria2c exits non-zero
python3 -c "
import socket, time
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', 18140))
s.listen(1)
c, _ = s.accept()
c.send(b'garbage')
time.sleep(0.5)
c.close()
s.close()
" &
stub_pid=$!
# Wait for the stub to start listening
tries=0
while ! python3 -c "
import socket, sys
s = socket.socket()
try:
    s.connect(('127.0.0.1', 18140))
    s.close()
except:
    sys.exit(1)
" 2>/dev/null; do
  tries=$((tries + 1))
  if [[ $tries -ge 30 ]]; then
    echo "ERROR: TCP stub failed to start on port 18140" >&2
    kill "$stub_pid" 2>/dev/null || true
    exit 1
  fi
  sleep 0.1
done

rc=0
"$ARIA2C" --no-conf --connect-timeout=5 --timeout=5 \
  "sftp://127.0.0.1:18140/file" >/dev/null 2>&1 || rc=$?
# Clean up stub
kill "$stub_pid" 2>/dev/null || true
wait "$stub_pid" 2>/dev/null || true
assert_exit_code "$rc" 6 "sftp: connection to non-SSH stub exits 6 (NETWORK_PROBLEM)"

# 2. sftp URI scheme is recognized (not rejected as unknown scheme)
#    Use --help with a dummy sftp URI to verify option parsing.
#    If sftp is compiled in, aria2c will recognize the scheme.
run_cmd "$ARIA2C" --no-conf --connect-timeout=2 --timeout=2 --max-tries=1 \
  "sftp://127.0.0.1:1/file"
# Exit 28 = unrecognized option/scheme; anything else means SFTP is known
if [[ $_last_exit -eq 28 ]]; then
  tap_fail "sftp: URI scheme not recognized (exit 28)"
elif echo "$_last_output" | grep -qi "unknown scheme\|unsupported.*scheme"; then
  tap_fail "sftp: URI scheme not recognized"
else
  tap_ok "sftp: URI scheme recognized (exit $_last_exit)"
fi

# 3. --ssh-host-key-md option is recognized (not "unrecognized option")
run_cmd "$ARIA2C" --no-conf \
  --ssh-host-key-md=sha-1=0000000000000000000000000000000000000000 \
  --help
if echo "$_last_output" | grep -qi "unrecognized"; then
  tap_fail "sftp: --ssh-host-key-md option not recognized"
else
  tap_ok "sftp: --ssh-host-key-md option accepted"
fi

tap_summary
