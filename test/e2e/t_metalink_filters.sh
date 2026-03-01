#!/usr/bin/env bash
# e2e: Metalink file/mirror selection filter tests (metalink-language,
#      metalink-os, metalink-location, metalink-preferred-protocol,
#      metalink-servers, metalink-base-uri, metalink-enable-unique-protocol)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tap_helpers.sh
source "$SCRIPT_DIR/tap_helpers.sh"

tap_plan 7
make_tempdir

# Create test fixtures
dd if=/dev/urandom of="$E2E_TMPDIR/file_en.bin" bs=1024 count=4 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file_fr.bin" bs=1024 count=4 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file_linux.bin" bs=1024 count=4 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/file_macos.bin" bs=1024 count=4 2>/dev/null
dd if=/dev/urandom of="$E2E_TMPDIR/payload.dat" bs=1024 count=4 2>/dev/null

start_http_server --port 18370 --dir "$E2E_TMPDIR"

# ── Test 1: --metalink-language=en selects English file ──────────────────
cat > "$E2E_TMPDIR/lang.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="file_en.bin">
    <language>en</language>
    <url>http://127.0.0.1:$HTTP_PORT/file_en.bin</url>
  </file>
  <file name="file_fr.bin">
    <language>fr</language>
    <url>http://127.0.0.1:$HTTP_PORT/file_fr.bin</url>
  </file>
</metalink>
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out1" --allow-overwrite=true \
  --metalink-language=en \
  --metalink-file="$E2E_TMPDIR/lang.meta4" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out1/file_en.bin" ]] && \
   [[ ! -f "$E2E_TMPDIR/out1/file_fr.bin" ]]; then
  tap_ok "metalink-language=en selects English file only"
else
  tap_fail "metalink-language=en selects English file only"
fi

# ── Test 2: --metalink-os=linux selects matching OS ──────────────────────
cat > "$E2E_TMPDIR/os.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="file_linux.bin">
    <os>linux</os>
    <url>http://127.0.0.1:$HTTP_PORT/file_linux.bin</url>
  </file>
  <file name="file_macos.bin">
    <os>macos</os>
    <url>http://127.0.0.1:$HTTP_PORT/file_macos.bin</url>
  </file>
</metalink>
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out2" --allow-overwrite=true \
  --metalink-os=linux \
  --metalink-file="$E2E_TMPDIR/os.meta4" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out2/file_linux.bin" ]] && \
   [[ ! -f "$E2E_TMPDIR/out2/file_macos.bin" ]]; then
  tap_ok "metalink-os=linux selects Linux file only"
else
  tap_fail "metalink-os=linux selects Linux file only"
fi

# ── Test 3: --metalink-location=us prefers US mirror ─────────────────────
cat > "$E2E_TMPDIR/loc.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="payload.dat">
    <url location="us">http://127.0.0.1:$HTTP_PORT/payload.dat</url>
  </file>
</metalink>
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out3" --allow-overwrite=true \
  --metalink-location=us \
  --metalink-file="$E2E_TMPDIR/loc.meta4" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out3/payload.dat" 4096 \
  "metalink-location=us downloads from US mirror"

# ── Test 4: --metalink-preferred-protocol=http uses HTTP ─────────────────
cat > "$E2E_TMPDIR/proto.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="payload.dat">
    <url>http://127.0.0.1:$HTTP_PORT/payload.dat</url>
  </file>
</metalink>
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out4" --allow-overwrite=true \
  --metalink-preferred-protocol=http \
  --metalink-file="$E2E_TMPDIR/proto.meta4" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out4/payload.dat" 4096 \
  "metalink-preferred-protocol=http downloads via HTTP"

# ── Test 5: --metalink-version selects matching version ───────────────────
cat > "$E2E_TMPDIR/ver.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="file_en.bin">
    <version>1.0</version>
    <url>http://127.0.0.1:$HTTP_PORT/file_en.bin</url>
  </file>
  <file name="file_fr.bin">
    <version>2.0</version>
    <url>http://127.0.0.1:$HTTP_PORT/file_fr.bin</url>
  </file>
</metalink>
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out5" --allow-overwrite=true \
  --metalink-version=1.0 \
  --metalink-file="$E2E_TMPDIR/ver.meta4" >/dev/null 2>&1
if [[ -f "$E2E_TMPDIR/out5/file_en.bin" ]] && \
   [[ ! -f "$E2E_TMPDIR/out5/file_fr.bin" ]]; then
  tap_ok "metalink-version=1.0 selects matching version only"
else
  tap_fail "metalink-version=1.0 selects matching version only"
fi

# ── Test 6: --metalink-base-uri resolves relative URLs ───────────────────
cat > "$E2E_TMPDIR/relative.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="payload.dat">
    <url>payload.dat</url>
  </file>
</metalink>
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out6" --allow-overwrite=true \
  --metalink-base-uri="http://127.0.0.1:$HTTP_PORT/" \
  --metalink-file="$E2E_TMPDIR/relative.meta4" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out6/payload.dat" 4096 \
  "metalink-base-uri resolves relative URL"

# ── Test 7: --metalink-enable-unique-protocol=true accepted ──────────────
cat > "$E2E_TMPDIR/unique.meta4" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">
  <file name="payload.dat">
    <url>http://127.0.0.1:$HTTP_PORT/payload.dat</url>
  </file>
</metalink>
EOF
"$ARIA2C" --no-conf -d "$E2E_TMPDIR/out7" --allow-overwrite=true \
  --metalink-enable-unique-protocol=true \
  --metalink-file="$E2E_TMPDIR/unique.meta4" >/dev/null 2>&1
assert_file_size "$E2E_TMPDIR/out7/payload.dat" 4096 \
  "metalink-enable-unique-protocol=true accepted, download succeeds"

stop_http_server
tap_summary
