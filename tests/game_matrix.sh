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
ROM_DIR="${DECKADE_MATRIX_ROM_DIR:-$HOME/.local/share/deckade/roms/fbneo}"
WORK="${DECKADE_MATRIX_WORK:-$(mktemp -d)}"
LIMIT="${DECKADE_MATRIX_LIMIT:-0}"
MODES="${DECKADE_MATRIX_MODES:-solo training}"
OUT="${DECKADE_MATRIX_OUT:-/tmp/deckade-game-matrix}"
RUN_SECONDS="${DECKADE_MATRIX_TIMEOUT:-30}"
mkdir -p "$OUT"
rm -f "$OUT/report.txt"

if [ ! -x "$BIN" ]; then echo "Missing runtime: $BIN" >&2; exit 2; fi
if [ ! -d "$ROM_DIR" ]; then echo "Missing ROM directory: $ROM_DIR" >&2; exit 2; fi
mkdir -p "$WORK/data/fbneo/config"
{
    printf 'nIniVersion 0x029744\n'
    printf 'szAppRomPaths[0] %s/\n' "$ROM_DIR"
} >"$WORK/data/fbneo/config/fbneo.ini"

# Link the bundled assets the launcher links. Training mode writes per-game
# config files into its folder, so it gets a copy to keep the repo clean.
TRAINING_SOURCE="$ROOT/runtime/fbneo/fbneo-training-mode"
for bundled in savestates ui detector; do
    if [ ! -d "$ROOT/runtime/fbneo/$bundled" ]; then
        echo "Bundled $bundled assets are missing" >&2
        exit 2
    fi
    rm -rf "$WORK/$bundled"
    ln -s "$ROOT/runtime/fbneo/$bundled" "$WORK/$bundled"
done
rm -rf "$WORK/fbneo-training-mode"
cp -a "$TRAINING_SOURCE" "$WORK/fbneo-training-mode"

failures=0
tested=0
for zip in "$ROM_DIR"/*.zip; do
    [ -e "$zip" ] || continue
    game="$(basename "$zip" .zip)"
    # Board/BIOS sets have no game to boot. Parent ROMs like kof98 do, even
    # when clones list them as required, so they must stay in the matrix.
    case "$game" in
        neogeo|pgm|skns|nmk004|ym2608|decocass|isgsm|midssio|cchip|bubsys|*bios*|*BIOS*) continue ;;
    esac
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
            XDG_DATA_HOME="$WORK/data" \
            MACADE_FIGHTCADE_RUNTIME="$WORK" \
            MACADE_ROM_DIR="$ROM_DIR" \
            DECKADE_FRAME_DUMP="$raw" DECKADE_FRAME_DUMP_FRAMES=600 \
            timeout "$RUN_SECONDS" "$BIN" "${args[@]}") >"$log" 2>&1
        rc=$?
        set -e
        if [ ! -s "$raw" ]; then
            echo "FAIL $game/$mode: no frame (rc=$rc)" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        if ! python3 - "$raw" "$mode" <<'PY'
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
colors = len(set(values))
lit = sum(1 for value in values if value) / max(1, len(values))
# A plain boot can legitimately be sparse, like a blue-on-black region warning.
# Training starts from a loaded state, so a near-empty frame there means the
# game fell into something like the Neo Geo BIOS crosshatch.
if sys.argv[2] == 'training':
    if colors < 4 or lit < 0.03:
        raise SystemExit(1)
elif colors < 2:
    raise SystemExit(1)
PY
        then
            echo "FAIL $game/$mode: blank/uniform frame" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        # A state that fails to load silently reboots the game, which looks fine
        # on screen but cannot sync with official Fightcade players.
        if [ "$mode" = training ] && [ -f "$ROOT/runtime/fbneo/savestates/${game}_fbneo.fs" ] \
            && ! grep -q "Deckade state load: $game result=0" "$log"; then
            echo "FAIL $game/$mode: bundled savestate did not load" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        # Thousands of unmapped reads means the 68K has run off into garbage.
        bad_reads="$(grep -c ' read (' "$log" || true)"
        if [ "$bad_reads" -gt 1000 ]; then
            echo "FAIL $game/$mode: CPU crashed ($bad_reads bad reads)" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        if [ "$mode" = training ] && grep -qE 'Lua thread bombed|Lua error in' "$log"; then
            echo "FAIL $game/training: Lua thread error" | tee -a "$OUT/report.txt"
            failures=$((failures + 1))
            continue
        fi
        echo "PASS $game/$mode" | tee -a "$OUT/report.txt"
    done
done

echo "Matrix: $tested ROM(s), $failures failure(s), output=$OUT"
[ "$failures" -eq 0 ]
