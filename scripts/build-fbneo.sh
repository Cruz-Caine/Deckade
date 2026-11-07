#!/usr/bin/env bash
# Build the native Linux FBNeo + Fightcade GGPO/quark runtime.
#
# Source: Jayian1890/Macade's vendored Fightcade FBNeo tree, pinned to the
# revision Deckade was developed against. Macade is only used as the source of
# the native GGPO/Quark integration; Deckade builds the normal SDL window path.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MACADE_REPO="${DECKADE_MACADE_REPO:-https://github.com/Jayian1890/Macade.git}"
MACADE_COMMIT="${DECKADE_MACADE_COMMIT:-b99fd6a8851a11db94c641bab287f712991dacdf}"
SRC_ROOT="${DECKADE_RUNTIME_SRC:-$ROOT/.runtime-src}"
MACADE_DIR="$SRC_ROOT/Macade"
FBNEO_DIR="$SRC_ROOT/FightcadeFBNeo"
CPUTYPE="${CPUTYPE:-$(uname -m)}"

for command in git make g++ pkg-config perl nasm; do
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

rm -rf "$FBNEO_DIR"
cp -a "$MACADE_DIR/Sources/FightcadeFBNeo" "$FBNEO_DIR"
cd "$FBNEO_DIR"

# Linux/SDL build fixes.
# FBNeo's vendored Lua is built without dynamic module loading by default.
# Lua-GD (used by the bundled training mode) is a C module, so enable dlopen.
python3 - <<'LUAFLAGS'
from pathlib import Path
p = Path('makefile.sdl')
s = p.read_text()
if '-DLUA_USE_DLOPEN' not in s:
    s = s.replace('CFLAGS = -O2', 'CFLAGS = -DLUA_USE_DLOPEN -O2', 1)
    s = s.replace('LDFLAGS += -s', 'LDFLAGS += -s\nLDFLAGS += -ldl\nLDFLAGS += -rdynamic', 1)
p.write_text(s)
LUAFLAGS

sed -i 's/^NAME\t\t= fbneosdl/NAME\t\t= deckade-fbneo/' makefile.sdl
sed -i 's/-framework OpenGL -lz/-lGL -lz/' makefile.sdl
sed -i 's/ASFLAGS += -f macho64/ASFLAGS += -f elf64/' makefile.sdl
sed -i 's/-Wno-c++11-narrowing/-Wno-narrowing/g' makefile.sdl
python3 - <<'PY'
from pathlib import Path
p = Path('makefile.sdl')
s = p.read_text()
needle = '''$(objdir)%.o: $(srcdir)%.cpp
\t@echo Compiling $<...
\t@mkdir -p $(dir $@)
\t@$(CXX) $(CXXFLAGS) -c $< -o $@
'''
replacement = needle + '''
$(objdir)%.o: $(srcdir)%.c
\t@echo Compiling $<...
\t@mkdir -p $(dir $@)
\t@$(CC) $(CFLAGS) -Wno-unused -Wno-conversion -Wno-missing-prototypes -c $< -o $@
'''
if needle not in s:
    raise SystemExit('Could not find the makefile C++ rule')
p.write_text(s.replace(needle, replacement, 1))
PY

# Fold POV hat 0 into the left-stick axes. FBNeo's legacy joystick defaults
# map directions to axes; console pads and the Steam Deck expose the D-pad as
# a hat, so D-pad directions would otherwise be ignored in-game.
python3 - <<'PYPATCH'
from pathlib import Path
p = Path('src/intf/input/sdl/inp_sdl2.cpp')
s = p.read_text()
old = """int SDLinpJoyAxis(int i, int nAxis)
{
\tif (i < 0 || i >= nJoystickCount) {\t\t\t\t// This joystick number isn't connected
\t\treturn 0;
\t}

\tif (ReadJoystick() != 0) {\t\t\t\t\t\t// There was an error polling the joystick
\t\treturn 0;
\t}

\tif (nAxis >= SDL_JoystickNumAxes(JoyList[i])) {
\t\treturn 0;
\t}

\treturn SDL_JoystickGetAxis(JoyList[i], nAxis) << 1;
}"""
new = """int SDLinpJoyAxis(int i, int nAxis)
{
\tif (i < 0 || i >= nJoystickCount) {\t\t\t\t// This joystick number isn't connected
\t\treturn 0;
\t}

\tif (ReadJoystick() != 0) {\t\t\t\t\t\t// There was an error polling the joystick
\t\treturn 0;
\t}

\tif (nAxis >= SDL_JoystickNumAxes(JoyList[i])) {
\t\treturn 0;
\t}

\tint value = SDL_JoystickGetAxis(JoyList[i], nAxis) << 1;

\t// Steam Deck and most console-style pads expose the D-pad as a POV hat,
\t// while FBNeo's legacy joystick bindings use axes for directions. Fold hat
\t// 0 into the left stick X/Y axes so both the stick and D-pad work.
\tif ((nAxis == 0 || nAxis == 1) && SDL_JoystickNumHats(JoyList[i]) > 0) {
\t\tconst Uint8 hat = SDL_JoystickGetHat(JoyList[i], 0);
\t\tif (nAxis == 0) {
\t\t\tif (hat & SDL_HAT_LEFT) value = -65536;
\t\t\telse if (hat & SDL_HAT_RIGHT) value = 65534;
\t\t} else {
\t\t\tif (hat & SDL_HAT_UP) value = -65536;
\t\t\telse if (hat & SDL_HAT_DOWN) value = 65534;
\t\t}
\t}

\treturn value;
}"""
if old not in s:
    raise SystemExit('Could not find SDLinpJoyAxis block')
p.write_text(s.replace(old, new, 1))
PYPATCH

python3 - <<'P2PATCH'
from pathlib import Path
p = Path('src/macade/macade_gamepad.cpp')
s = p.read_text()
old = """bool MacadeGamepadPlayerOneOnly()
{
    // A network route always uses its existing P1 input bank/GGPO side routing,
    // even if a caller accidentally supplies the local-only environment flag.
    if (kNetGame || kNetSpectator) return false;
    const char* mode = getenv("MACADE_SINGLE_PLAYER_INPUT");
    return mode && !strcmp(mode, "1");
}"""
new = """bool MacadeGamepadPlayerOneOnly()
{
    // Training and single-player both run through the local training route but
    // are still kNetGame sessions in FBNeo. The environment flag is the reliable
    // distinction; network matches never set it.
    if (kNetSpectator) return false;
    const char* mode = getenv("MACADE_SINGLE_PLAYER_INPUT");
    return mode && !strcmp(mode, "1");
}"""
if old not in s:
    raise SystemExit('MacadeGamepadPlayerOneOnly block missing')
p.write_text(s.replace(old, new, 1))
P2PATCH

python3 - <<'TRAININGSTATE'
from pathlib import Path
p = Path('src/burner/sdl/main.cpp')
s = p.read_text()
if 'DECKADE training savestate' not in s:
    s = s.replace('#include "burner.h"', '#include "burner.h"\n\nextern bool MacadeLoadTrainingState(const char *name); // DECKADE training savestate', 1)
    old = """	if (!DrvInit(gameToRun, 0))
	{
		MediaInit();
		if (bMacadeTrainingLaunch) FBA_LoadLuaCode("fbneo-training-mode/fbneo-training-mode.lua");"""
    new = """	if (!DrvInit(gameToRun, 0))
	{
		MediaInit();
		if (bMacadeTrainingLaunch) {
			// Fightcade training opens on the same character-select state as
			// the online client rather than replaying the full boot sequence.
			MacadeLoadTrainingState(BurnDrvGetTextA(DRV_NAME));
			FBA_LoadLuaCode("fbneo-training-mode/fbneo-training-mode.lua");
		}"""
    if old not in s:
        raise SystemExit('DoGame training block missing')
    p.write_text(s.replace(old, new, 1))

q = Path('src/burner/sdl/fbn_quark_sdl.cpp')
qtext = q.read_text()
if 'MacadeLoadTrainingState' not in qtext:
    qtext = qtext.replace('} // namespace\n', '} // namespace\n\nbool MacadeLoadTrainingState(const char *name) { return load_fightcade_state(name); }\n', 1)
    q.write_text(qtext)
TRAININGSTATE

python3 "$ROOT/scripts/patches/fbneo-controller-fixes.py"

# The macOS embedded video/input bridge is intentionally inert on Linux.
cp "$ROOT/runtime/fbneo/macade_embedded_stub.cpp" src/macade/macade_embedded.cpp

# Macade's fork's VidPaint only presents in embedded mode. Restore the normal
# SDL window presentation used by Deckade's native Linux runtime.
python3 - <<'PY'
from pathlib import Path
p = Path('src/intf/video/vid_interface.cpp')
s = p.read_text()
if '#include "macade_embedded.h"' not in s:
    s = s.replace('#include "burner.h"\n', '#include "burner.h"\n#include "macade_embedded.h"\n', 1)
old = '''INT32 VidPaint(INT32 bValidate)
{
\tif (bVidOkay /* && bDrvOkay */) {
\t\tif (bValidate == 3) {
\t\t\tint ret = pVidOut[nVidActive]->Paint(2 | bValidate);
\t\t\treturn ret;
\t\t}
\t\telse {
\t\t\treturn 0;
\t\t}
\t} else {
\t\treturn 1;
\t}
}'''
new = '''INT32 VidPaint(INT32 bValidate)
{
\tif (!bVidOkay /* && !bDrvOkay */) {
\t\treturn 1;
\t}

\t// Macade's embedded macOS bridge publishes frames from VidFrame and only
\t// needs the SDL painter for explicit validation. The Linux SDL window path
\t// must present every painted frame normally.
\tif (MacadeEmbeddedEnabled()) {
\t\tif (bValidate == 3) {
\t\t\treturn pVidOut[nVidActive]->Paint(2 | bValidate);
\t\t}
\t\treturn 0;
\t}

\treturn pVidOut[nVidActive]->Paint(bValidate);
}'''
if old not in s:
    raise SystemExit('Could not find the Macade VidPaint block')
p.write_text(s.replace(old, new, 1))
PY

make -f makefile.sdl -j"$(nproc)" CPUTYPE="$CPUTYPE" DEPEND= PERL=perl

mkdir -p "$ROOT/runtime/bin"
cp "deckade-fbneo$CPUTYPE" "$ROOT/runtime/bin/deckade-fbneo"
chmod 755 "$ROOT/runtime/bin/deckade-fbneo"
echo "Built $ROOT/runtime/bin/deckade-fbneo"
