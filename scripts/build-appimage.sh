#!/bin/bash
# Build an AppImage from a statically-linked aria2c binary.
#
# Usage:
#   scripts/build-appimage.sh /path/to/aria2c [output-dir]
#
# Requires: file, curl (to download appimagetool)
set -euo pipefail

ARIA2C="${1:?Usage: $0 /path/to/aria2c [output-dir]}"
OUTDIR="${2:-.}"
VERSION=$(${ARIA2C} --version 2>/dev/null | head -1 | awk '{print $NF}')
ARCH=$(uname -m)

APPDIR=$(mktemp -d)/aria2.AppDir
mkdir -p "${APPDIR}/usr/bin"

cp "${ARIA2C}" "${APPDIR}/usr/bin/aria2c"
chmod +x "${APPDIR}/usr/bin/aria2c"

# Desktop entry
cat > "${APPDIR}/aria2c.desktop" <<DESKTOP
[Desktop Entry]
Name=aria2c
Exec=aria2c
Icon=aria2c
Type=Application
Categories=Network;
Terminal=true
DESKTOP

# Minimal icon (1x1 transparent PNG)
printf '\x89PNG\r\n\x1a\n' > "${APPDIR}/aria2c.png"

# AppRun
cat > "${APPDIR}/AppRun" <<'APPRUN'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
exec "${HERE}/usr/bin/aria2c" "$@"
APPRUN
chmod +x "${APPDIR}/AppRun"

# Download appimagetool if not present
TOOL="appimagetool-${ARCH}.AppImage"
if ! command -v appimagetool &>/dev/null; then
  if [ ! -f "${TOOL}" ]; then
    curl -fsSL -o "${TOOL}" \
      "https://github.com/AppImage/appimagetool/releases/download/continuous/${TOOL}"
    chmod +x "${TOOL}"
  fi
  APPIMAGETOOL="./${TOOL}"
else
  APPIMAGETOOL="appimagetool"
fi

OUTPUT="${OUTDIR}/aria2c-${VERSION}-${ARCH}.AppImage"
ARCH="${ARCH}" "${APPIMAGETOOL}" "${APPDIR}" "${OUTPUT}"

echo "AppImage created: ${OUTPUT}"
