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

LCOV_IGNORE="--ignore-errors inconsistent,format,count,unsupported"
COV_INFO="/tmp/aria2-coverage.info"

echo "==> Cleaning previous build..."
make clean >/dev/null 2>&1 || true

echo "==> Configuring with coverage flags..."
./configure \
  CXXFLAGS="-O0 -g --coverage" \
  LDFLAGS="--coverage" \
  --enable-bittorrent --enable-metalink --enable-xml-rpc \
  >/dev/null 2>&1

echo "==> Building..."
make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" >/dev/null 2>&1

echo "==> Building test binary..."
make -C test aria2c >/dev/null 2>&1

echo "==> Running tests..."
(cd test && ./aria2c 2>/tmp/aria2-cov-stderr.txt) || true
tail -1 /tmp/aria2-cov-stderr.txt

echo "==> Capturing coverage data..."
lcov --capture --directory src --output-file "$COV_INFO" \
  --no-external $LCOV_IGNORE >/dev/null 2>&1

echo ""
echo "=== Coverage Summary ==="
lcov --summary "$COV_INFO" $LCOV_IGNORE 2>&1 \
  | grep -E "lines|functions|source"

if $HTML; then
  echo ""
  echo "==> Generating HTML report..."
  genhtml "$COV_INFO" --output-directory coverage-report \
    $LCOV_IGNORE >/dev/null 2>&1
  echo "Report: coverage-report/index.html"
fi

echo ""
echo "==> Restoring normal build..."
make clean >/dev/null 2>&1 || true
./configure --enable-bittorrent --enable-metalink --enable-xml-rpc >/dev/null 2>&1
make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" >/dev/null 2>&1
make -C test aria2c >/dev/null 2>&1
echo "==> Normal build restored."
