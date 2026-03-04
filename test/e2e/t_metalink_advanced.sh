#!/usr/bin/env bash
# e2e: Advanced Metalink tests (hashes, mirrors, follow-metalink, select-file)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 6
make_tempdir

_http_server2_pid=""

_cleanup_extra() {
  if [[ -n "${_http_server2_pid:-}" ]]; then
    kill "$_http_server2_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_http_server2_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_http_server2_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_http_server2_pid" 2>/dev/null || true
    _http_server2_pid=""
  fi
}

# Override the cleanup trap to also kill server2
trap '_cleanup_extra; _e2e_cleanup' EXIT

# Create test fixtures
echo "metalink-hash-test-content" > "$E2E_TMPDIR/hashfile.dat"
HASH=$(shasum -a 256 "$E2E_TMPDIR/hashfile.dat" | awk '{print $1}')

echo "mirror-test-content" > "$E2E_TMPDIR/mirrorfile.dat"

echo "select-file1-content" > "$E2E_TMPDIR/sel1.dat"
echo "select-file2-content" > "$E2E_TMPDIR/sel2.dat"

start_http_server --port 18108 --dir "$E2E_TMPDIR"

# 1. Metalink with sha-256 hash — correct hash, download succeeds
cat > "$E2E_TMPDIR/hash_ok.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="hashfile.dat">
    <hash type="sha-256">$HASH</hash>
    <url>http://127.0.0.1:$HTTP_PORT/hashfile.dat</url>
  </file>
</metalink>
EOF

"$ARIA2C" --metalink-file="$E2E_TMPDIR/hash_ok.meta4" \
  -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --check-integrity=true >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out1/hashfile.dat" \
  "metalink-adv: sha-256 correct hash download succeeds"

# 2. Metalink with wrong hash — download fails
cat > "$E2E_TMPDIR/hash_bad.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="hashfile_bad.dat">
    <hash type="sha-256">0000000000000000000000000000000000000000000000000000000000000000</hash>
    <url>http://127.0.0.1:$HTTP_PORT/hashfile.dat</url>
  </file>
</metalink>
EOF

rc=0
"$ARIA2C" --metalink-file="$E2E_TMPDIR/hash_bad.meta4" \
  -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --check-integrity=true >/dev/null 2>&1 || rc=$?
if [[ $rc -ne 0 ]]; then
  tap_ok "metalink-adv: wrong sha-256 hash causes failure"
else
  tap_fail "metalink-adv: wrong sha-256 hash causes failure (expected non-zero exit)"
fi

# 3. Metalink with 2 mirrors — start second server, download succeeds
python3 "$SCRIPT_DIR/http_server.py" --port 18109 --dir "$E2E_TMPDIR" &
_http_server2_pid=$!
# Wait for second server to be ready
tries=0
while ! (echo >/dev/tcp/127.0.0.1/18109) 2>/dev/null; do
  tries=$((tries + 1))
  if [[ $tries -ge 30 ]]; then
    echo "ERROR: second HTTP server failed to start" >&2
    exit 1
  fi
  sleep 0.1
done

cat > "$E2E_TMPDIR/mirrors.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="mirrorfile.dat">
    <url priority="1">http://127.0.0.1:$HTTP_PORT/mirrorfile.dat</url>
    <url priority="2">http://127.0.0.1:18109/mirrorfile.dat</url>
  </file>
</metalink>
EOF

"$ARIA2C" --metalink-file="$E2E_TMPDIR/mirrors.meta4" \
  -d "$E2E_TMPDIR/out3" --allow-overwrite=true >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out3/mirrorfile.dat" \
  "metalink-adv: 2-mirror metalink downloads file"

# Kill second server, no longer needed
_cleanup_extra

# 4. --follow-metalink=mem from URL — serve .meta4, download referenced file
cat > "$E2E_TMPDIR/remote.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="hashfile.dat">
    <url>http://127.0.0.1:$HTTP_PORT/hashfile.dat</url>
  </file>
</metalink>
EOF

"$ARIA2C" --follow-metalink=mem \
  -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  "http://127.0.0.1:$HTTP_PORT/remote.meta4" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out4/hashfile.dat" \
  "metalink-adv: --follow-metalink=mem downloads referenced file"

# 5. --select-file=2 — only second file downloaded
cat > "$E2E_TMPDIR/select.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="sel1.dat">
    <url>http://127.0.0.1:$HTTP_PORT/sel1.dat</url>
  </file>
  <file name="sel2.dat">
    <url>http://127.0.0.1:$HTTP_PORT/sel2.dat</url>
  </file>
</metalink>
EOF

"$ARIA2C" --metalink-file="$E2E_TMPDIR/select.meta4" \
  -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --select-file=2 >/dev/null 2>&1
assert_file_not_exists "$E2E_TMPDIR/out5/sel1.dat" \
  "metalink-adv: --select-file=2 skips file 1"
assert_file_exists "$E2E_TMPDIR/out5/sel2.dat" \
  "metalink-adv: --select-file=2 downloads file 2"

stop_http_server
tap_summary
