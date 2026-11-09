#!/usr/bin/env bash
# Install the pinned Flycast Dojo release as Deckade's Flycast runtime.
#
# Dojo 6.46 is the exact build official Fightcade ships, so it is used
# unmodified to stay compatible with official players. Flycast Dojo is GPL-2.0;
# source: https://github.com/blueminder/flycast-dojo/tree/dojo-6.46
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="dojo-6.46"
ARCHIVE="linux-flycast-dojo-6.46.zip"
SHA256="ae72479e050ce40ece31debadfffb362d75861e33ae5869fc6f5cd18329d4181"
URL="https://github.com/blueminder/flycast-dojo/releases/download/$VERSION/$ARCHIVE"
SRC_ROOT="${DECKADE_RUNTIME_SRC:-$ROOT/.runtime-src}/flycast"

mkdir -p "$SRC_ROOT"
if [ ! -f "$SRC_ROOT/$ARCHIVE" ]; then
    curl --fail --location --retry 3 --output "$SRC_ROOT/$ARCHIVE" "$URL"
fi
echo "$SHA256  $SRC_ROOT/$ARCHIVE" | sha256sum --check --quiet

EXTRACT="$SRC_ROOT/extract"
rm -rf "$EXTRACT"
mkdir -p "$EXTRACT"
python3 -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' \
    "$SRC_ROOT/$ARCHIVE" "$EXTRACT"
chmod +x "$EXTRACT/flycast-dojo-x86_64.AppImage"
(cd "$EXTRACT" && ./flycast-dojo-x86_64.AppImage --appimage-extract >/dev/null)

rm -rf "$ROOT/runtime/flycast"
cp -a "$EXTRACT/squashfs-root" "$ROOT/runtime/flycast"

mkdir -p "$ROOT/runtime/bin"
cat >"$ROOT/runtime/bin/deckade-flycast" <<'WRAPPER'
#!/bin/sh
# Flycast Dojo carries its own libraries. Drop the paths Deckade's AppImage
# exports for Qt so they cannot override them.
unset LD_LIBRARY_PATH LD_PRELOAD QT_PLUGIN_PATH QML2_IMPORT_PATH
here="$(dirname "$(readlink -f "$0")")"
exec "$here/../flycast/AppRun" "$@"
WRAPPER
chmod 755 "$ROOT/runtime/bin/deckade-flycast"
echo "Installed Flycast $VERSION at $ROOT/runtime/flycast"
