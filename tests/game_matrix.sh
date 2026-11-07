#!/usr/bin/env bash
# Runtime matrix smoke test for every locally available FBNeo ROM.
#
# This is intentionally a black-box test: it launches the real runtime with
# SDL's offscreen driver, asks FBNeo to dump a frame, and checks that the
# emulator produced a non-empty, non-uniform frame. It also launches training
# mode and fails on Lua thread errors.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${DECKADE_MATRIX_BIN:-$ROOT/runtime/bin/deckade-fbneo}"
ROM_DIR="${DECKADE_MATRIX_ROM_DIR:-$HOME/.var/app/com.fightcade.Fightcade/data/ROMs/fbneo}"
WORK="${DECKADE_MATRIX_WORK:-$HOME/.local/share/Deckade/Deckade/runtime-work/fbneo}"
LIMIT="${DECKADE_MATRIX_LIMIT:-0}"
MODES="${DECKADE_MATRIX_MODES:-solo training}"
OUT="${DECKADE_MATRIX_OUT:-/tmp/deckade-game-matrix}"
mkdir -p "$OUT"
rm -f "$OUT/report.txt"

if [ ! -x "$BIN" ]; then echo "Missing runtime: $BIN" >&2; exit 2; fi
if [ ! -d "$ROM_DIR" ]; then echo "Missing ROM directory: $ROM_DIR" >&2; exit 2; fi
PARENTS="$(mktemp)"
python3 - "$ROOT/assets/rom-json/fbneo_roms.json" >"$PARENTS" <<'PARENT_PY'
import json, sys
try:
    data = json.load(open(sys.argv[1]))
except Exception:
    raise SystemExit(0)
for entry in data.values():
    for required in entry.get("require", []):
        print(required)
PARENT_PY
mkdir -p "$WORK/data/fbneo/config"
# Use the bundled Fightcade state set instead of whatever stale AppImage
# mount the app work directory was last pointed at.
STATE_SOURCE="$ROOT/runtime/fbneo/savestates"
TRAINING_SOURCE="$ROOT/runtime/fbneo/fbneo-training-mode"
if [ ! -d "$STATE_SOURCE" ] || [ ! -d "$TRAINING_SOURCE" ]; then
    echo "Bundled state/training assets are missing" >&2
    exit 2
fi
if [ -L "$WORK/savestates" ] || [ ! -e "$WORK/savestates" ]; then
    rm -f "$WORK/savestates"
    ln -s "$STATE_SOURCE" "$WORK/savestates"
fi
if [ -L "$WORK/fbneo-training-mode" ] || [ ! -e "$WORK/fbneo-training-mode" ]; then
    rm -f "$WORK/fbneo-training-mode"
    ln -s "$TRAINING_SOURCE" "$WORK/fbneo-training-mode"
fi
if [ ! -e "$WORK/gd.so" ]; then
    cp "$ROOT/runtime/fbneo/gd.so" "$WORK/gd.so"
fi

failures=0
tested=0
for zip in "$ROM_DIR"/*.zip; do
    [ -e "$zip" ] || continue
    game="$(basename "$zip" .zip)"
    case "$game" in
        neogeo|neogeo.zip|*bios*|*BIOS*) continue ;;
    esac
    if grep -qx "$game" "$PARENTS"; then
        continue
    fi
    if [ "$LIMIT" -gt 0 ] && [ "$tested" -ge "$LIMIT" ]; then break; fi
    tested=$((tested + 1))
    for mode in $MODES; do
        raw="$OUT/${game}-${mode}.raw"
        log="$OUT/${game}-${mode}.log"
        rm -f "$raw" "$log"
        args=(-joy "$game")
        [ "$mode" = training ] && args=(-joy "macade:training,$game")
        set +e
        (cd "$WORK" && \
            SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
            LUA_CPATH="$WORK/?.so;" \
            XDG_DATA_HOME="$WORK/data" \
            MACADE_FIGHTCADE_RUNTIME="$WORK" \
            MACADE_ROM_DIR="$ROM_DIR" \
            DECKADE_FRAME_DUMP="$raw" DECKADE_FRAME_DUMP_FRAMES=600 \
            timeout 30 "$BIN" "${args[@]}") >"$log" 2>&1
        rc=$?
        set -e
        if [ ! -s "$raw" ]; then
            echo "FAIL $game/$mode: no frame (rc=$rc)" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        if ! python3 - "$raw" <<'PY'
import struct, sys
data = open(sys.argv[1], 'rb').read()
if len(data) < 20:
    raise SystemExit(1)
magic, width, height, depth, pitch = struct.unpack('<5i', data[:20])
body = data[20:]
if magic != 0x44464D52 or width <= 0 or height <= 0 or len(body) < pitch * height:
    raise SystemExit(1)
if depth == 16:
    values = struct.unpack('<%dH' % (len(body) // 2), body[:len(body) // 2 * 2])
elif depth == 32:
    values = struct.unpack('<%dI' % (len(body) // 4), body[:len(body) // 4 * 4])
else:
    raise SystemExit(1)
if len(set(values)) < 4 or (sum(1 for value in values if value) / max(1, len(values))) < 0.03:
    raise SystemExit(1)
PY
        then
            echo "FAIL $game/$mode: blank/uniform frame" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        if [ "$mode" = training ] && grep -q 'Lua thread bombed' "$log"; then
            echo "FAIL $game/training: Lua thread error" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        echo "PASS $game/$mode" | tee -a "$OUT/report.txt"
    done
done

rm -f "$PARENTS"
echo "Matrix: $tested ROM(s), $failures failure(s), output=$OUT"
[ "$failures" -eq 0 ]
