#!/usr/bin/env bash
# e2e: Input file with per-URI options (dir=, out=, tab-indented)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 4
make_tempdir

# Create two test fixtures of different sizes
dd if=/dev/urandom of="$E2E_TMPDIR/file_a.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file_b.bin" bs=1024 count=2 2>/dev/null

start_http_server --port 18112 --dir "$E2E_TMPDIR"

# 1. dir= per URI — files land in different directories
mkdir -p "$E2E_TMPDIR/dir_a" "$E2E_TMPDIR/dir_b"
cat > "$E2E_TMPDIR/input1.txt" <<EOF
http://127.0.0.1:$HTTP_PORT/file_a.bin
  dir=$E2E_TMPDIR/dir_a
http://127.0.0.1:$HTTP_PORT/file_b.bin
  dir=$E2E_TMPDIR/dir_b
EOF
"$ARIA2C" --no-conf --allow-overwrite=true \
  -i "$E2E_TMPDIR/input1.txt" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/dir_a/file_a.bin" && -f "$E2E_TMPDIR/dir_b/file_b.bin" ]]; then
  tap_ok "input-file: dir= per URI places files in separate directories"
else
  tap_fail "input-file: dir= per URI places files in separate directories"
fi

# 2. out= per URI — custom output filename
mkdir -p "$E2E_TMPDIR/out_dir"
cat > "$E2E_TMPDIR/input2.txt" <<EOF
http://127.0.0.1:$HTTP_PORT/file_a.bin
  out=custom_a.bin
  dir=$E2E_TMPDIR/out_dir
EOF
"$ARIA2C" --no-conf --allow-overwrite=true \
  -i "$E2E_TMPDIR/input2.txt" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/out_dir/custom_a.bin" \
  "input-file: out= renames download to custom_a.bin"

# 3. Mixed global + per-URI options — both files downloaded
mkdir -p "$E2E_TMPDIR/mix_a" "$E2E_TMPDIR/mix_b"
cat > "$E2E_TMPDIR/input3.txt" <<EOF
http://127.0.0.1:$HTTP_PORT/file_a.bin
  dir=$E2E_TMPDIR/mix_a
http://127.0.0.1:$HTTP_PORT/file_b.bin
  dir=$E2E_TMPDIR/mix_b
EOF
"$ARIA2C" --no-conf --allow-overwrite=true \
  --max-concurrent-downloads=2 \
  -i "$E2E_TMPDIR/input3.txt" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/mix_a/file_a.bin" && -f "$E2E_TMPDIR/mix_b/file_b.bin" ]]; then
  tap_ok "input-file: global --max-concurrent-downloads=2 with per-URI dir= downloads both"
else
  tap_fail "input-file: global --max-concurrent-downloads=2 with per-URI dir= downloads both"
fi

# 4. Tab-indented options — recognized correctly
mkdir -p "$E2E_TMPDIR/tab_dir"
printf 'http://127.0.0.1:%s/file_a.bin\n\tdir=%s\n' \
  "$HTTP_PORT" "$E2E_TMPDIR/tab_dir" > "$E2E_TMPDIR/input4.txt"
"$ARIA2C" --no-conf --allow-overwrite=true \
  -i "$E2E_TMPDIR/input4.txt" >/dev/null 2>&1
assert_file_exists "$E2E_TMPDIR/tab_dir/file_a.bin" \
  "input-file: tab-indented dir= option is recognized"

stop_http_server
tap_summary
