#!/usr/bin/env bash
# Generate test coverage report for aria2
# Usage: ./scripts/coverage.sh [--html]
#
# Without --html: prints summary to stdout
# With --html:    generates coverage-report/ with browsable HTML

set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

HTML=false
[[ "${1:-}" == "--html" ]] && HTML=true

COV_INFO="/tmp/aria2-coverage.info"

# macOS configure options
CONFIGURE_OPTS=(
  --with-libcares --with-sqlite3 --with-libssh2
  --without-gnutls --without-libgcrypt --without-libnettle
)
PKG_CFG="PKG_CONFIG_PATH=/opt/homebrew/opt/openssl@3/lib/pkgconfig:/opt/homebrew/opt/sqlite/lib/pkgconfig:/opt/homebrew/opt/c-ares/lib/pkgconfig:/opt/homebrew/opt/libssh2/lib/pkgconfig:/opt/homebrew/opt/libxml2/lib/pkgconfig:/opt/homebrew/opt/cppunit/lib/pkgconfig"

# lcov 2.4 needs separate --ignore-errors for each type
LCOV_IGNORE=(
  --ignore-errors inconsistent
  --ignore-errors format
  --ignore-errors count
  --ignore-errors unsupported
)

echo "==> Cleaning previous build..."
make clean >/dev/null 2>&1 || true

echo "==> Configuring with coverage flags..."
./configure \
  CXXFLAGS="-O0 -g --coverage" \
  LDFLAGS="--coverage" \
  "${CONFIGURE_OPTS[@]}" \
  "$PKG_CFG" \
  >/dev/null 2>&1

echo "==> Building..."
make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" >/dev/null 2>&1

echo "==> Building test binary..."
make -C test aria2c >/dev/null 2>&1

echo "==> Running tests..."
(cd test && ./aria2c 2>/tmp/aria2-cov-stderr.txt) || true
tail -1 /tmp/aria2-cov-stderr.txt

echo "==> Capturing coverage data..."
lcov --capture --directory src --output-file "${COV_INFO}.raw" \
  --no-external "${LCOV_IGNORE[@]}" >/dev/null 2>&1

echo "==> Filtering untestable code..."
# Exclude code that cannot be unit-tested:
#  - TLS/SSH platform code (platform-specific, not testable in CI)
#  - SocketCore (low-level network I/O)
#  - main.cc (entry point)
#  - DHTSetup/BtSetup (orchestration requiring full engine)
#  - AsyncNameResolver* (c-ares async DNS, requires real DNS)
#  - DefaultBtInteractive (BT protocol FSM, requires live connections)
#  - DHT*Task* (DHT network tasks, require routing table)
#  - DHTMessageReceiver (DHT network message handling)
#  - HttpServer (requires real HTTP server socket)
#  - PeerConnection (BT peer socket I/O)
#  - *Entry.cc (engine entry points requiring event loop)
#  - AdaptiveURISelector (needs ServerStatMan with live stats)
#  - BtPieceMessage (BT piece transfer, needs PeerConnection)
#  - Iteratable*Validator (checksum validation with DiskAdaptor I/O)
lcov --remove "${COV_INFO}.raw" \
  '*/AppleTLS*' '*/LibsslTLS*' '*/GnuTLS*' '*/WinTLS*' \
  '*/LibgcryptMessage*' \
  '*/SSHSession*' '*/SftpNegotiationCommand*' \
  '*/SocketCore.cc' '*/SocketCore.h' \
  '*/main.cc' \
  '*/DHTSetup.cc' '*/BtSetup.cc' \
  '*/AsyncNameResolver.cc' '*/AsyncNameResolverMan.cc' \
  '*/DefaultBtInteractive.cc' \
  '*/DHTMessageReceiver.cc' '*/DHTTaskFactoryImpl.cc' \
  '*/DHTAbstractNodeLookupTask.h' '*/DHTPeerLookupTask.cc' \
  '*/DHTReplaceNodeTask.cc' \
  '*/HttpServer.cc' '*/PeerConnection.cc' \
  '*/StreamFileAllocationEntry.cc' \
  '*/BtCheckIntegrityEntry.cc' '*/BtFileAllocationEntry.cc' \
  '*/ChecksumCheckIntegrityEntry.cc' \
  '*/AdaptiveURISelector.cc' \
  '*/BtPieceMessage.cc' \
  '*/IteratableChunkChecksumValidator.cc' \
  '*/IteratableChecksumValidator.cc' \
  -o "$COV_INFO" "${LCOV_IGNORE[@]}" >/dev/null 2>&1

echo ""
echo "=== Coverage Summary (testable code) ==="
lcov --summary "$COV_INFO" "${LCOV_IGNORE[@]}" 2>&1 \
  | grep -E "lines|functions|source"
echo ""
echo "=== Raw Coverage (all src) ==="
lcov --summary "${COV_INFO}.raw" "${LCOV_IGNORE[@]}" 2>&1 \
  | grep -E "lines|functions|source"

if $HTML; then
  echo ""
  echo "==> Generating HTML report..."
  genhtml "$COV_INFO" --output-directory coverage-report \
    "${LCOV_IGNORE[@]}" >/dev/null 2>&1
  echo "Report: coverage-report/index.html"
fi

echo ""
echo "==> Restoring normal build..."
make clean >/dev/null 2>&1 || true
./configure \
  "${CONFIGURE_OPTS[@]}" \
  "$PKG_CFG" \
  >/dev/null 2>&1
make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" >/dev/null 2>&1
make -C test aria2c >/dev/null 2>&1
echo "==> Normal build restored."
