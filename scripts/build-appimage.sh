#!/usr/bin/env bash
# Build a self-contained Deckade AppImage.
#
# The native runtimes must exist in runtime/bin first. This script builds them
# automatically when they are missing, so a clean checkout only needs:
#   ./scripts/build-appimage.sh
#
# Requirements: CMake, Ninja, a C++20 compiler, Qt 6 dev packages (Core, Gui,
# Qml, Quick, QuickControls2, Network, WebSockets, Svg, Concurrent), SDL2,
# curl, and the native-runtime build dependencies. linuxdeploy and
# appimagetool are downloaded into .appimage-tools/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${DECKADE_APPIMAGE_BUILD_DIR:-$ROOT/build-appimage}"
APPDIR="${DECKADE_APPDIR:-$ROOT/AppDir}"
TOOLS_DIR="${DECKADE_APPIMAGE_TOOLS:-$ROOT/.appimage-tools}"
DIST_DIR="${DECKADE_DIST_DIR:-$ROOT/dist}"
VERSION="$(sed -n 's/^project(Deckade VERSION \([^ ]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
VERSION="${VERSION:-0.1.0}"
ARCH="x86_64"

mkdir -p "$TOOLS_DIR" "$DIST_DIR"

download() {
    local url="$1" output="$2"
    if [ ! -x "$output" ]; then
        echo "Downloading $(basename "$output")..."
        curl --fail --location --retry 3 --output "$output" "$url"
        chmod +x "$output"
    fi
}

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-x86_64.AppImage"

download "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "$LINUXDEPLOY"
download "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" "$LINUXDEPLOY_QT"
download "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" "$APPIMAGETOOL"

# linuxdeploy looks for its Qt plugin on PATH.
ln -sf "$LINUXDEPLOY_QT" "$TOOLS_DIR/linuxdeploy-plugin-qt"

if [ ! -x "$ROOT/runtime/bin/deckade-fbneo" ]; then
    echo "Native FBNeo runtime is missing; building it first."
    "$ROOT/scripts/build-fbneo.sh"
fi
if [ ! -x "$ROOT/runtime/bin/deckade-snes9x" ]; then
    echo "Native Snes9x runtime is missing; building it first."
    "$ROOT/scripts/build-snes9x.sh"
fi

if [ ! -x "$ROOT/runtime/bin/deckade-flycast" ]; then
    echo "Flycast runtime is missing; installing it first."
    "$ROOT/scripts/build-flycast.sh"
fi

CMAKE_EXTRA=()
if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
    CMAKE_EXTRA+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH")
fi
if [ -n "${Qt6WebSockets_DIR:-}" ]; then
    CMAKE_EXTRA+=("-DQt6WebSockets_DIR=$Qt6WebSockets_DIR")
    QT_LIB_DIR="$(cd "$Qt6WebSockets_DIR/../../.." && pwd)/lib"
    if [ -d "$QT_LIB_DIR" ]; then
        export LD_LIBRARY_PATH="$QT_LIB_DIR:${LD_LIBRARY_PATH:-}"
    fi
fi
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "${CMAKE_EXTRA[@]}"
cmake --build "$BUILD_DIR" -j"$(nproc)"

rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr

DESKTOP_FILE="$APPDIR/usr/share/applications/com.deckade.Deckade.desktop"
ICON_FILE="$APPDIR/usr/share/icons/hicolor/256x256/apps/com.deckade.Deckade.png"
if [ ! -f "$DESKTOP_FILE" ] || [ ! -f "$ICON_FILE" ]; then
    echo "AppDir install is missing the desktop file or icon." >&2
    exit 1
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export QML_SOURCES_PATHS="$ROOT/qml"
# Prefer Qt 6's qmake when both Qt 5 and Qt 6 are installed (e.g. Arch).
if command -v qmake6 >/dev/null 2>&1; then
    ln -sf "$(command -v qmake6)" "$TOOLS_DIR/qmake"
    export QMAKE="$(command -v qmake6)"
fi
export PATH="$TOOLS_DIR:$PATH"
export ARCH="$ARCH"
# The bundled strip is older than some distro Qt binaries (notably .relr.dyn
# sections). Stripping is optional; keep the packaged binaries intact.
export NO_STRIP=1

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --plugin qt \
    --desktop-file "$DESKTOP_FILE" \
    --icon-file "$ICON_FILE" \
    --executable "$APPDIR/usr/bin/Deckade" \
    --executable "$APPDIR/usr/share/deckade/runtime/bin/deckade-fbneo" \
    --executable "$APPDIR/usr/share/deckade/runtime/bin/deckade-snes9x"

# linuxdeploy copies each extra executable into usr/bin while scanning its
# dependencies. Deckade launches runtimes from usr/share/deckade/runtime, so
# drop the duplicate copies before squashing the image.
rm -f "$APPDIR/usr/bin/deckade-fbneo" "$APPDIR/usr/bin/deckade-snes9x"

OUTPUT="$DIST_DIR/Deckade-${VERSION}-${ARCH}.AppImage"
rm -f "$OUTPUT"
"$APPIMAGETOOL" "$APPDIR" "$OUTPUT"
chmod +x "$OUTPUT"
echo "Built $OUTPUT"
