# Deckade

**Fightcade for the couch — a native, controller-first Linux frontend for Steam Deck.**

Deckade is an independent Fightcade-compatible client written in C++20 with Qt 6/QML. It talks directly to Fightcade's lobby, uses native emulator runtimes, downloads ROMs from the same metadata source Fightcade uses, and is designed to be launched from Steam with a controller and no keyboard.

There is no Electron, no WebView, and no embedded browser. The UI is Qt Quick rendered by the GPU; the emulators are separate native Linux processes.

## What works today

| Area | Status |
| --- | --- |
| Fightcade login / autologin / session persistence | Working |
| Remember-me checkbox + server-side active room restore | Working |
| Joined-room switcher with account slot count | Working |
| Channel browser, welcome sections, filters, search | Working |
| Room roster, chat, challenges, accept/decline/cancel | Working |
| Native **FBNeo** runtime (GGPO/rollback, quark routes) | Working |
| Native **Snes9x** runtime | Experimental (runner builds; validate live netplay) |
| **Flycast** matches | Uses the official Fightcade **native Flycast** binary through the official helper; no Wine |
| **GGPOFBA / fc1 legacy** | Not yet ported natively |
| Automatic ROM download from Fightcade's `fc2json` database | Working |
| Controller navigation / on-screen keyboard | Working |
| Per-game FBNeo P1 binding editor | Working |
| Automatic all-games load on Browse | Working |
| Steam Deck Big Picture-style layout | Working |

Deckade does not include game ROMs. It can download them from the same third-party hosts Fightcade uses, or use a local Fightcade ROM folder.

## Controller bindings

Open a room and select **Controls** to edit Player 1 bindings for that game.
Highlight an action, press **A**, then press a button, D-pad direction, or move
a stick. **X** clears a binding. **B** cancels capture. Profiles are saved per
game under `~/.local/share/Deckade/Deckade/controller-profiles/` and are applied
on the next emulator launch. Unbound actions keep FBNeo's keyboard/default
controller behavior.

The Controls editor is available for FBNeo rooms. The Settings screen cycles
the controller used for both launcher navigation and Player 1; this matters
when Steam Input or a virtual controller occupies SDL joystick index 0.

## Local match test

A second Fightcade account is not required to test the native netplay path. The
two-peer GGPO loopback runs two FBNeo processes locally and draws test names in
the match overlay:

```sh
./tests/quark_loopback.sh sfiii3nr1
```

Both windows should show `Local Test P1 vs Local Test P2`; inserting a coin and
pressing start should advance both peers. See `docs/MATCH_TESTING.md` for
details.

## In-game overlay

The native FBNeo runtime now draws a Fightcade-style top overlay with Player 1
and Player 2 names, connection/system messages, delay, spectator count, scores,
and recent GGPO chat lines. Flycast matches use Flycast Dojo's own native
overlay; Deckade does not replace it.

## Rooms and account slots

The Rooms screen lists every channel you are currently joined to, including
channels restored from your Fightcade account after login. It shows the
account's joined-room limit (for example, `2 joined · 3 slots`) and blocks new
joins when the limit is reached. Leaving a room removes it from the local
remembered list as well as the account session.

## Controller first

Everything the launcher needs is reachable with a standard gamepad:

- **D-pad / left stick** — navigate
- **A** — select
- **B** — back
- **X** — favorite / secondary action
- **Y** — on-screen keyboard for the focused text field
- **LB / RB** — previous / next section
- **Start** — settings
- **LT / RT** — reserved for page actions
- Keyboard: **F11** toggles fullscreen, **F10** minimizes Deckade.

Flycast opens its own game window; if it is fullscreen, use the Steam button (Game Mode) or the window manager to minimize/switch.

The emulator processes use SDL's game-controller mapping. FBNeo is launched with `-joy`, so controller 0 is Player 1 and controller 1 is Player 2. The native FBNeo build folds the controller's POV hat into the left-stick axes so the Steam Deck/Xbox-style D-pad works for movement, not just the analog stick.

Steam Deck Game Mode presents the built-in controls as an Xbox-style controller. No Steam Input mouse/keyboard profile is required.

## Requirements

### Runtime
Deckade uses the system `curl` for Fightcade's launcher-status registration,
which the lobby requires before it allows challenges. `curl` is included on
SteamOS and standard Linux distributions.

- Linux x86_64 (Steam Deck / SteamOS, Arch, Fedora, Ubuntu, etc.)
- A GPU/display stack supported by Qt 6 and SDL2
- Qt 6.5+ runtime libraries, including **Qt WebSockets**
- SDL2
- An existing **Fightcade Flatpak** or a local Fightcade installation for the official Flycast helper and, optionally, its ROM folder
- A Fightcade account

For a Steam Deck, the simplest path is the prebuilt **AppImage** from the GitHub Releases page. It bundles Qt 6 and the native runtimes. You still need the Fightcade Flatpak for the official Flycast helper and, optionally, its ROM folder.

### Build

- CMake 3.21+
- Ninja
- C++20 compiler
- Qt 6.5+ development packages: Core, Gui, Qml, Quick, QuickControls2, Network, WebSockets, Svg, Concurrent
- SDL2 development package
- `pkg-config`, `perl`, `nasm` for the native FBNeo build
- `git` for fetching the runtime source

On Arch:

```sh
sudo pacman -S base-devel cmake ninja qt6-base qt6-declarative qt6-websockets qt6-svg sdl2 pkgconf perl nasm git
```

On Debian/Ubuntu package names vary by release; install the Qt 6 development metapackage plus `libqt6websockets6-dev`, `libsdl2-dev`, `ninja-build`, `cmake`, `pkg-config`, `perl`, and `nasm`.

## Prebuilt AppImage

Every push to `main` and every `v*` tag builds a self-contained AppImage on
GitHub Actions. Download the latest one from
[Releases](https://github.com/Cruz-Caine/Deckade/releases/latest), then:

```sh
chmod +x Deckade-*-x86_64.AppImage
./Deckade-*-x86_64.AppImage
```

The AppImage bundles Qt 6, SDL2, and the native FBNeo/Snes9x runtimes. It does
not contain ROMs or BIOS files. Flycast matches still use the official
Fightcade native helper from the Fightcade Flatpak.

To build the AppImage locally:

```sh
./scripts/build-appimage.sh
```

The output is written to `dist/`.

## Build Deckade

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
```

If Qt is installed somewhere non-standard, pass `-DCMAKE_PREFIX_PATH=/path/to/qt`.

Run:

```sh
./build/Deckade
```

Useful development flags:

```sh
./build/Deckade --windowed   # do not enter fullscreen
```

## Build the native runtimes

The app looks for runtimes in this order:

1. `$DECKADE_RUNTIME_DIR`
2. `./runtime` beside the development binary
3. `../runtime`
4. `../share/deckade/runtime`
5. `~/.local/share/deckade/runtime`
6. `/usr/lib/deckade/runtime`
7. `/usr/share/deckade/runtime`

### FBNeo (required for the main arcade experience)

```sh
./scripts/build-fbneo.sh
```

This clones the pinned Macade/Fightcade FBNeo source into `.runtime-src/`, applies the Linux changes, and writes `runtime/bin/deckade-fbneo`.

The Linux build needs two important patches:

- The macOS embedded-video bridge is stubbed out so FBNeo opens a normal SDL window.
- Macade's modified `VidPaint()` only presented frames in embedded mode. Deckade restores the normal SDL present path. Without this, the emulator loads and runs but the window stays black.

### Snes9x (experimental)

```sh
./scripts/build-snes9x.sh
```

This builds the Snes9x libretro core, the native GGPO/quark runner, and writes `runtime/bin/deckade-snes9x`. Single-player and local training are the safest uses; verify direct/served/stream netplay with real peers before relying on it.

### Flycast

Flycast channels use the official Fightcade helper and its **native Linux Flycast binary**. No Wine is involved. Deckade starts the helper with the same `fcade://` route the official frontend uses and monitors the Flycast process so the launcher can return when the match ends.

The official Fightcade install must be available:
- Flatpak: `com.fightcade.Fightcade`
- Native: set the folder containing `Fightcade2.sh` in Settings

### GGPOFBA / fc1

Legacy Fightcade 1 channels are not ported to a native runtime yet. Deckade reports those runtimes as unavailable rather than silently falling back to Wine.

## ROMs

Deckade bundles lofi1048's **fc2json** metadata (the same pack documented at [fightcade.guide](https://fightcade.guide/)) and can fetch the exact same ROM URLs Fightcade uses.

- **Automatic downloads** are enabled by default. The first time you join a channel, the missing game and its `require` dependencies are downloaded.
- The downloader writes to the detected Fightcade ROM folder, or to `~/.local/share/Deckade/Deckade/roms/<emulator>` when no Fightcade installation exists.
- Settings can disable automatic downloads or show the ROM folder.
- Refresh the metadata with `./scripts/update-rom-json.sh`.

ROM folders:

| Emulator | Folder |
| --- | --- |
| FBNeo | `ROMs/fbneo/` (system subfolders such as `nes/`, `megadrive/`, etc.) |
| Snes9x | `ROMs/snes9x/` |
| Flycast | `ROMs/flycast/` |
| GGPOFBA | `ROMs/ggpofba/` |

Deckade does not host ROMs. Downloading and using ROMs is your responsibility; comply with the laws and licenses that apply where you live.

## Fightcade installs

Deckade is standalone for FBNeo and Snes9x: you only need the native runtime plus ROMs. A Fightcade install is used for:

- Flycast (official native Flycast helper)
- Reading an existing `ROMs/` library
- DuckStation/custom helper routes

For the Flatpak, Deckade looks in:

```text
~/.var/app/com.fightcade.Fightcade/data/ROMs/
```

For a native install, point Settings at the folder that contains `Fightcade2.sh` (`VERSION.txt` and `emulator/fcade` are the markers).

## Install on Steam Deck

A quick development deployment:

```sh
./scripts/package-tarball.sh
tar tzf build-release/Deckade-*.tar.gz | head
```

The tarball uses a `/usr` layout. For a portable local install, copy `build/Deckade` plus `runtime/` somewhere, then add the executable to Steam as a non-Steam game:

1. Copy the build to `~/Applications/Deckade/`.
2. Add `~/Applications/Deckade/build/Deckade` (or the packaged `usr/bin/Deckade`) as a non-Steam game.
3. Launch once in Desktop Mode to confirm controller and runtime detection.
4. Switch to Game Mode; launch from Steam. Deckade requests fullscreen itself.

If Qt WebSockets is not present on the Deck, use the Flatpak/Qt runtime path or build a bundled Flatpak. The packaging template is intentionally not a fake all-in-one artifact yet.

## Security and privacy

- Passwords are never stored. Login uses Fightcade's `location` bootstrap plus WebSocket login.
- The Fightcade session cookie is stored in `~/.config/deckade/deckade.ini` with owner-only permissions.
- `.env.local` is used only for local live-protocol tests and is ignored by git. It is mode `0600`. Never commit it.
- The app does not log passwords or session cookies.

## Verification commands

```sh
# Live protocol smoke test (uses .env.local, read-only login/lobby/join/leave)
set -a; . ./.env.local; set +a
./build/deckade_live_probe

# ROM metadata/download partial test (isolated, cancels and cleans up)
./build/deckade_rom_probe

# Launcher/runtime detection
./build/deckade_launcher_smoke
```

## Relationship to Fightcade and Macade

Deckade is an independent client and is not affiliated with, endorsed by, or sponsored by Fightcade. Fightcade is a trademark of its respective owners.

Native runtime integration builds on the public [Macade](https://github.com/Jayian1890/Macade) work and the public Fightcade FBNeo/Snes9x sources. Those components keep their own licenses and copyrights.

## Roadmap

- Native GGPOFBA/fc1 runtime
- Verified Snes9x netplay (direct, served, stream, replay)
- Flycast native runtime without the official helper
- Flatpak/AppImage bundling for zero-setup Steam Deck installation
- Save-state / replay browser
- More controller-accessible filters and ROM management

## License

Deckade's original code is MIT licensed. Bundled/linked third-party components and Fightcade metadata remain under their respective licenses. See [LICENSE](LICENSE).
