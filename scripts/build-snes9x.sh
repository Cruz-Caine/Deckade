#!/usr/bin/env bash
# Build the native Linux Snes9x libretro runner with Fightcade GGPO/quark.
# This runtime is experimental; validate netplay against real peers before
# treating it as production-ready.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MACADE_REPO="${DECKADE_MACADE_REPO:-https://github.com/Jayian1890/Macade.git}"
MACADE_COMMIT="${DECKADE_MACADE_COMMIT:-b99fd6a8851a11db94c641bab287f712991dacdf}"
SRC_ROOT="${DECKADE_RUNTIME_SRC:-$ROOT/.runtime-src}"
MACADE_DIR="$SRC_ROOT/Macade"
SNES_DIR="$MACADE_DIR/Sources/FightcadeSnes9x"
FBNEO_DIR="$MACADE_DIR/Sources/FightcadeFBNeo"
BUILD_DIR="$ROOT/.runtime-src/snes9x-build"

for command in git make g++ pkg-config perl; do
    command -v "$command" >/dev/null 2>&1 || {
        echo "Missing build dependency: $command" >&2
        exit 1
    }
done
pkg-config --exists sdl2 || {
    echo "Missing SDL2 development files (pkg-config sdl2)." >&2
    exit 1
}

mkdir -p "$SRC_ROOT"
if [ ! -d "$MACADE_DIR/.git" ]; then
    git clone --filter=blob:none --no-checkout "$MACADE_REPO" "$MACADE_DIR"
fi
git -C "$MACADE_DIR" fetch --depth 1 origin "$MACADE_COMMIT"
git -C "$MACADE_DIR" checkout --detach FETCH_HEAD

# The bundled unzip helper predates zlib's z_crc_t typedef.
sed -i 's/s->pcrc_32_tab = get_crc_table();/s->pcrc_32_tab = (const unsigned long*)get_crc_table();/' \
    "$SNES_DIR/upstream/src/unzip/unzip.c"

make -C "$SNES_DIR/upstream/src/libretro" platform=unix STATIC_LINKING=1 LTO= \
    GIT_VERSION='" deckade"' -j"$(nproc)"

mkdir -p "$BUILD_DIR"
CXXFLAGS=(
    -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Wno-unused-parameter
    -I"$SNES_DIR/upstream/src/libretro"
    -I"$SNES_DIR/upstream/src"
    -I"$FBNEO_DIR/src/dep/ggponet-native/include"
    -I"$ROOT/runtime/snes9x"
)
CXXFLAGS+=($(pkg-config --cflags sdl2))

GGPO_OBJECTS=()
for source in "$FBNEO_DIR"/src/dep/ggponet-native/src/*.cpp; do
    object="$BUILD_DIR/$(basename "${source%.cpp}.o")"
    g++ "${CXXFLAGS[@]}" -c "$source" -o "$object"
    GGPO_OBJECTS+=("$object")
done

g++ "${CXXFLAGS[@]}" -c "$ROOT/runtime/snes9x/runner.cpp" -o "$BUILD_DIR/runner.o"
g++ "${CXXFLAGS[@]}" -c "$ROOT/runtime/snes9x/macade_quark.cpp" -o "$BUILD_DIR/macade_quark.o"

g++ -O2 \
    "$BUILD_DIR/runner.o" "$BUILD_DIR/macade_quark.o" "${GGPO_OBJECTS[@]}" \
    "$SNES_DIR/upstream/src/libretro/snes9x_libretro.so" \
    $(pkg-config --libs sdl2) -lz -lpthread -ldl -lm \
    -o "$ROOT/runtime/bin/deckade-snes9x"
chmod 755 "$ROOT/runtime/bin/deckade-snes9x"
echo "Built $ROOT/runtime/bin/deckade-snes9x (experimental netplay)"
