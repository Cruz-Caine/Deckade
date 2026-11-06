#!/usr/bin/env bash
# Build a relocatable-ish /usr-layout tarball. Qt 6 and Qt WebSockets must be
# available on the target system; use the Flatpak template for a self-contained
# distribution later.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    ${CMAKE_PREFIX_PATH:+-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH"}
cmake --build "$BUILD" -j"$(nproc)"
cmake --build "$BUILD" --target package
ls -lh "$BUILD"/Deckade-*.tar.gz
