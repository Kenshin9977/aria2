#!/usr/bin/env bash
# e2e: Metalink/HTTP (Link + Digest headers) download tests
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 3
make_tempdir

_mirror_pid=""

_metalink_cleanup() {
  if [[ -n "${_mirror_pid:-}" ]]; then
    kill "$_mirror_pid" 2>/dev/null || true
    local i=0
    while kill -0 "$_mirror_pid" 2>/dev/null; do
      i=$((i + 1))
      if [[ $i -ge 10 ]]; then
        kill -9 "$_mirror_pid" 2>/dev/null || true
        break
      fi
      sleep 0.1
    done
    wait "$_mirror_pid" 2>/dev/null || true
    _mirror_pid=""
  fi
}

# Extend the default cleanup trap to also kill mirror
_orig_cleanup=$(trap -p EXIT | sed "s/^trap -- '//;s/' EXIT$//")
trap "$_orig_cleanup; _metalink_cleanup" EXIT

# Create 64KB test fixture
dd if=/dev/urandom of="$E2E_TMPDIR/testfile.bin" bs=1024 count=64 2>/dev/null

# Compute SHA-256 digest in base64 for Digest header
DIGEST_B64=$(openssl dgst -sha-256 -binary "$E2E_TMPDIR/testfile.bin" | base64)

# Start mirror server (18125) manually — serves same file
python3 "$SCRIPT_DIR/http_server.py" --port 18125 --dir "$E2E_TMPDIR" &
_mirror_pid=$!
tries=0
while ! (echo >/dev/tcp/127.0.0.1/18125) 2>/dev/null; do
  tries=$((tries + 1))
  [[ $tries -ge 30 ]] && { echo "ERROR: mirror server failed to start" >&2; exit 1; }
  sleep 0.1
done

# Start primary server (18124) with Link and Digest headers
start_http_server --port 18124 --dir "$E2E_TMPDIR" \
  --link-header '<http://127.0.0.1:18125/testfile.bin>; rel=duplicate; pri=1' \
  --digest-header "sha-256=$DIGEST_B64"

# 1. Metalink/HTTP download with correct digest — file downloaded, SHA256 correct
#    (aria2 processes Link and Digest headers automatically)
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --check-integrity=true \
  "http://127.0.0.1:18124/testfile.bin" >/dev/null 2>&1
assert_sha256_match "$E2E_TMPDIR/testfile.bin" \
  "$E2E_TMPDIR/out1/testfile.bin" \
  "metalink-http: download with Link+Digest headers, SHA256 correct"

# 2. Verify digest integrity explicitly — downloaded file hash matches computed digest
DL_HASH=$(shasum -a 256 "$E2E_TMPDIR/out1/testfile.bin" | awk '{print $1}')
ORIG_HASH=$(shasum -a 256 "$E2E_TMPDIR/testfile.bin" | awk '{print $1}')
if [[ "$DL_HASH" == "$ORIG_HASH" ]]; then
  tap_ok "metalink-http: server Digest header verified against file content"
else
  tap_fail "metalink-http: server Digest header verified against file content ($DL_HASH != $ORIG_HASH)"
fi

# 3. Wrong Digest header — download should fail checksum verification
stop_http_server
start_http_server --port 18124 --dir "$E2E_TMPDIR" \
  --link-header '<http://127.0.0.1:18125/testfile.bin>; rel=duplicate; pri=1' \
  --digest-header "sha-256=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="

rc=0
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --check-integrity=true \
  "http://127.0.0.1:18124/testfile.bin" >/dev/null 2>&1 || rc=$?
if [[ "$rc" -ne 0 ]]; then
  tap_ok "metalink-http: wrong Digest header causes download failure (exit $rc)"
else
  tap_fail "metalink-http: wrong Digest header causes download failure (exit 0, expected non-zero)"
fi

stop_http_server
tap_summary
