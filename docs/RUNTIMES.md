# Native runtimes

Deckade does not run emulators through Wine. Each runtime is a native Linux
process launched by `FightcadeLauncher` with a quark/GGPO route or a local game.

## Status

| Runtime | Binary | Match | Spectate | Training | Notes |
| --- | --- | --- | --- | --- | --- |
| FBNeo | `runtime/bin/deckade-fbneo` | yes | yes | launches | Native GGPO/quark |
| Snes9x | `runtime/bin/deckade-snes9x` | experimental | experimental | launches | Native libretro+GGPO runner |
| Flycast | official Flatpak/native helper | yes | yes | yes | Official native Flycast binary; no Wine |
| GGPOFBA / fc1 | — | no | no | no | Native port still missing |

The Settings screen reads `runtimeSummary()` and marks what is present.

## Runtime discovery

1. `$DECKADE_RUNTIME_DIR`
2. `runtime/` beside the development binary
3. `../runtime`
4. `../share/deckade/runtime`
5. `~/.local/share/deckade/runtime`
6. `/usr/lib/deckade/runtime`
7. `/usr/share/deckade/runtime`

Build outputs are local artifacts (`runtime/bin/` is gitignored).

## FBNeo

Source: pinned `Jayian1890/Macade` → `Sources/FightcadeFBNeo`.

```sh
./scripts/build-fbneo.sh
```

### Linux changes

The Macade tree is a native macOS embedded runtime. Deckade keeps the native
GGPO/quark integration but uses FBNeo's normal SDL window. The build script:

1. Changes the makefile to Linux/SDL (`-lGL`, `elf64`, GCC warning flags, an
   explicit C-object rule).
2. Replaces `macade_embedded.cpp` with an inert Linux stub.
3. Restores normal `VidPaint()` presentation.

The `VidPaint()` fix is **mandatory**. Macade's fork had:

```cpp
INT32 VidPaint(INT32 bValidate)
{
    if (bVidOkay) {
        if (bValidate == 3) return pVidOut[nVidActive]->Paint(2 | bValidate);
        return 0;
    }
    return 1;
}
```

Macade's macOS app publishes frames through a shared-memory/Metal bridge inside
`VidFrame`, so it does not need the SDL painter. On Linux with a normal SDL
window, `RunFrame()` calls `VidPaint(0)`, which returned 0 without presenting.
The emulator loaded the ROM, ran the game, and showed a black window. The fix
restores `pVidOut[...]->Paint(bValidate)` for non-embedded builds.

### Controller mapping

`FightcadeLauncher` starts FBNeo with:

```text
-fullscreen -joy
```

`-joy` makes FBNeo's `Init_Joysticks(1)` map Player 1 to controller 0 and
Player 2 to controller 1. Without it, FBNeo's default is keyboard P1 and
controller 0 P2. `MACADE_SINGLE_PLAYER_INPUT=1` is set for local solo/training
so P2/P3/P4 banks are neutralized.

Input mapping follows FBNeo's SDL game-controller layout: D-pad/left stick,
A/B/X/Y, shoulders, Start, Back. FBNeo's legacy bindings use axes for
directions, so the Linux build folds POV hat 0 into the left-stick X/Y axes;
this is required for Steam Deck/Xbox-style D-pads.

### ROM/config layout

The launcher creates:

```text
~/.local/share/Deckade/Deckade/runtime-work/fbneo/
  data/fbneo/config/fbneo.ini
  data/fbneo/states/
  savestates/
  fbneo-training-mode -> optional official data
```

`fbneo.ini` is generated with **absolute** `szAppRomPaths` entries for the
detected ROM root and its subdirectories. It intentionally does not rely on a
symlinked `roms/` directory; FBNeo's zip scanner is unreliable with symlinked
ROM directories.

ROMs are placed by `RomLibrary`:

```text
ROMs/fbneo/                 arcade
ROMs/fbneo/nes/             NES
ROMs/fbneo/megadrive/       Genesis/Mega Drive
ROMs/fbneo/sms/             Master System
ROMs/fbneo/gamegear/        Game Gear
ROMs/fbneo/pce/             PC Engine
ROMs/fbneo/sg1000/          SG-1000
ROMs/fbneo/msx/             MSX
ROMs/fbneo/tg/              PC Engine/TurboGrafx
ROMs/fbneo/coleco/          ColecoVision
```

Training mode runs the bundled `fbneo-training-mode` Lua scripts. Their `gd`
image module is built into the runtime (`runtime/fbneo/deckade_lua_gd.h`), so no
system `libgd` is needed.

## Snes9x

Source: pinned Macade → `Sources/FightcadeSnes9x`, built as a Linux libretro
core plus a new SDL2 runner (`runtime/snes9x/runner.cpp`).

```sh
./scripts/build-snes9x.sh
```

The runner:

- uses `SDL_VIDEO` + `SDL_Renderer`/texture for RGB565/XRGB output;
- uses `SDL_QueueAudio` for S16 stereo;
- maps SDL gamepads to libretro joypad bits;
- reuses Macade's GGPO/quark adapter (`macade_quark.cpp`);
- stores GGPO save states through Snes9x `snapshot.h` (`S9xFreeze*`).

The quark save callback in the imported adapter was corrected to return `true`
after a successful `S9xFreezeGameMem`. Snes9x netplay remains experimental
until direct/served/stream/replay sessions are verified with real peers.

## FBNeo controller profiles

The room-level **Controls** screen uses FBNeo's `--macade-gamepad-inputs
<gameid>` metadata and Macade's profile format:

```text
MACADE_GAMEPADS 1
<guid:serial> <gameid> <inputIndex> 1 <infoHex> <kind> <controlIndex> <direction>
```

The launcher sets `MACADE_GAMEPAD_PROFILE_PATH` when a profile exists. Captured
directions are allowed to replace the legacy axis mapping in
`MacadeGamepadApply`; this is why profiles can bind P1 Up/Down/Left/Right to a
D-pad button/hat even though FBNeo normally initializes those inputs as
`GIT_JOYAXIS`.

Profiles are per game ID and per controller identity. FBNeo's helper returns the
actual P1 action list for that driver, so games with different button counts or
special actions get the correct rows. A game without a profile uses FBNeo's
legacy default mapping. Flycast/Akatsuki control configuration is still owned by
Flycast itself and is not part of this editor yet.

`DECKADE_P1_JOYSTICK_INDEX` selects which SDL joystick becomes Player 1.

## Flycast

The official Fightcade install already ships a **native Linux Flycast binary**
(the Flatpak unbundles an AppImage of `flycast-dojo`). Deckade uses the official
`fcade` helper for Flycast matches, which performs master registration and
starts Flycast with its native `-config dojo:...` options. No Wine is used.

Deckade monitors `/proc/*/cmdline` for the Flycast process and restores the
launcher when it exits. If the helper or Flycast is missing, Settings shows the
runtime as unavailable.

## GGPOFBA / fc1

There is no native Linux GGPOFBA runtime in Deckade yet. The frontend recognizes
`fc1`/`ggpofba` and shows an explicit unsupported error instead of silently
launching Wine.

## ROM folders

The launcher resolves a per-emulator ROM folder:

- configured path from Settings, then `<configured>/<emulator>`
- native Fightcade `<root>/emulator/<emulator>/ROMs`
- native Fightcade `<root>/ROMs/<emulator>`
- Flatpak `~/.var/app/com.fightcade.Fightcade/data/ROMs/<emulator>`
- fallback `~/.local/share/Deckade/Deckade/roms/<emulator>`

## Verification

```sh
# install/runtime discovery
./build/deckade_launcher_smoke

# FBNeo local launch with a real ROM (replace ssf2 with your game)
DECKADE_TEST_MODE=solo DECKADE_TEST_GAME=ssf2 ./build/deckade_launcher_smoke

# isolated partial ROM download and cleanup
./build/deckade_rom_probe

# protocol smoke test
set -a; . ./.env.local; set +a
./build/deckade_live_probe
```

For visual checks, run `./build/Deckade --windowed` and a real FBNeo game with
`-windowed` to inspect the SDL window.
