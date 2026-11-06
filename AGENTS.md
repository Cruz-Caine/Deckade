# Deckade Agent Rules

## Shape

- Native C++20 + Qt 6/QML application. No Electron, no WebView, no embedded browser.
- UI lives in `qml/`; networking/protocol in `src/fightcade/`; platform and controller code in `src/core/`.
- Emulators are separate native processes under `runtime/`. They must not be launched through Wine by Deckade.
- The app must be fully usable with a standard gamepad. Every interactive control needs a visible focus state and must be reachable with D-pad/stick navigation.

## Controller and focus

- Wrap interactive items in `Focusable` or register the item with `focusController.registerItem()`.
- Assign `navGroup` (`main`, `modal`, `keyboard`) and set `focusController.activeGroup` when opening/closing overlays.
- Never detect a text input by checking `item.text`; buttons also have `text`. Use `item.deckTextInput === true` or `item.echoMode !== undefined`.
- Signal handlers with parameters must declare them explicitly, e.g. `onJoinRequested: function(channelName) { ... }`. Qt 6.11 does not reliably inject implicit signal parameters.
- Context properties (`settings`, `roms`, `client`, `launcher`, `gamepad`, `focusController`, `theme`, `appController`) are globals. Do not write `root.settings` or `root.focusController`.

## Fightcade

- Read `docs/PROTOCOL.md` before changing auth, lobby, challenge, or launch flows.
- Use `requestIdx` for every request that expects a response; `-1` for fire-and-forget.
- Never change the UDP/GGPO wire format or invent matchmaking. Deckade targets Fightcade compatibility.
- The official `fcade` helper is allowed only for native emulator routes (currently Flycast native and custom/external routes). Do not use it to fall back to Wine.
- Keep `start`/`challenge`/`accept` semantics identical to the official web client.
- ROM metadata and download URLs must stay in the bundled `fc2json` pack; validate URLs against the allowed Fightcade/archive hosts.

## Runtimes

- FBNeo: the pinned Macade source needs the Linux stub and the `VidPaint` SDL present fix. Do not remove the fix. `-joy` must be passed so P1 is controller 0.
- Snes9x: experimental. Do not claim verified netplay without live peer tests.
- Flycast: currently official native helper. GGPOFBA/fc1 remains unsupported until a native runtime exists.
- Runtime binaries are local build artifacts and are gitignored. Build scripts must remain reproducible.
- Never bundle ROMs or BIOS files.

## Security

- Never commit `.env`, `.env.local`, session cookies, or credentials.
- `.env.local` must stay mode `0600`; `.gitignore` must keep ignoring it.
- Do not log passwords, cookies, tokens, or full login payloads.
- Keep `~/.config/deckade/deckade.ini` owner-only.

## Build and test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"

set -a; . ./.env.local; set +a
./build/deckade_live_probe        # read-only live login/lobby/join/leave
./build/deckade_rom_probe         # isolated partial ROM download + cleanup
./build/deckade_launcher_smoke    # install/runtime detection
```

- Do not leave test sessions, ROMs, or emulator processes running after verification.
- Keep new source files reasonably focused; split screens/components before they become unmanageable.
