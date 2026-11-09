#!/usr/bin/env bash
# Local two-peer Fightcade/FBNeo GGPO smoke test.
#
# This does not need a Fightcade account or a second machine. It starts two
# native FBNeo processes connected through the built-in "quark:direct" route,
# sends controller/keyboard input through GGPO, and draws the same P1/P2
# overlay names Deckade uses for a real match.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${DECKADE_FBNEO_BIN:-$ROOT/runtime/bin/deckade-fbneo}"
GAME="${1:-sfiii3nr1}"
ROM="${DECKADE_ROM_DIR:-$HOME/.local/share/deckade/roms/fbneo}"
WORK="${DECKADE_RUNTIME_WORK:-$HOME/.local/share/Deckade/Deckade/runtime-work/fbneo}"
HOLD="${DECKADE_LOOPBACK_HOLD:-30}"

if [ ! -x "$BIN" ]; then
    echo "FBNeo runtime not found: $BIN" >&2
    echo "Build it with ./scripts/build-fbneo.sh first." >&2
    exit 1
fi
if [ ! -f "$ROM/$GAME.zip" ]; then
    echo "ROM not found: $ROM/$GAME.zip" >&2
    echo "Set DECKADE_ROM_DIR to an FBNeo ROM folder." >&2
    exit 1
fi

mkdir -p "$WORK/data/fbneo/config" "$WORK/savestates"
if [ ! -f "$WORK/data/fbneo/config/fbneo.ini" ]; then
    {
        printf 'nIniVersion 0x029744\n'
        printf 'szAppRomPaths[0] %s/\n' "$ROM"
    } >"$WORK/data/fbneo/config/fbneo.ini"
fi

COMMON=(
    env
    "XDG_DATA_HOME=$WORK/data"
    "MACADE_FIGHTCADE_RUNTIME=$WORK"
    "MACADE_ROM_DIR=$ROM"
    "WINEDEBUG=-all"
    "DECKADE_OVERLAY_P1=Local Test P1"
    "DECKADE_OVERLAY_P2=Local Test P2"
)
if [ -n "${DISPLAY:-}" ]; then
    COMMON+=("SDL_VIDEODRIVER=x11")
fi

rm -f /tmp/deckade-quark-p1.log /tmp/deckade-quark-p2.log
P1=0
P2=0
cleanup() {
    [ "$P1" -gt 0 ] && kill "$P1" 2>/dev/null || true
    [ "$P2" -gt 0 ] && kill "$P2" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# P1 listens on UDP/TCP 6000 and dials P2 on 6001.
"${COMMON[@]}" setsid "$BIN" -joy \
    "quark:direct,$GAME,6000,127.0.0.1,6001,0,2,0" \
    >/tmp/deckade-quark-p1.log 2>&1 </dev/null &
P1=$!
sleep 2

# P2 listens on 6001 and dials P1 on 6000. Player id 1, same delay/ranked.
"${COMMON[@]}" setsid "$BIN" -joy \
    "quark:direct,$GAME,6001,127.0.0.1,6000,1,2,0" \
    >/tmp/deckade-quark-p2.log 2>&1 </dev/null &
P2=$!

echo "Started local GGPO peers:"
echo "  P1 pid=$P1  quark:direct,$GAME,6000 -> 6001  name='Local Test P1'"
echo "  P2 pid=$P2  quark:direct,$GAME,6001 -> 6000  name='Local Test P2'"
echo
echo "Both windows should show the SF3/FBNeo game with the overlay:"
echo "  Local Test P1  vs  Local Test P2"
echo "Insert coin (key 5) / start (key 1) in P1; both peers should advance."
echo "Closing in ${HOLD}s (set DECKADE_LOOPBACK_HOLD=0 to keep them running)."

if [ "$HOLD" -gt 0 ]; then
    sleep "$HOLD"
fi
