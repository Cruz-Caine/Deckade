# Architecture

```text
┌─────────────────────────────────────────────────────────────┐
│ Qt Quick / QML (native GPU window, no WebView)              │
│  Login · Home · Browse · Room · Settings · Keyboard · Modal │
└───────────────┬───────────────────────────────┬─────────────┘
                │ signals/properties            │ INVOKABLE
┌───────────────▼──────────────┐  ┌─────────────▼─────────────┐
│ FightcadeClient              │  │ FightcadeLauncher         │
│ QWebSocket + QNetworkAccess  │  │ QProcess, runtime paths   │
│ auth · lobby · events         │  │ native / helper routes    │
└───────────────┬──────────────┘  └─────────────┬─────────────┘
                │                               │
                │                        ┌──────▼─────────────┐
                │                        │ RomLibrary          │
                │                        │ fc2json + downloads │
                │                        └──────┬─────────────┘
                │                               │
        Fightcade master/lobby          ┌───────▼──────────────┐
        wss://ggs.fightcade.com         │ Native emulator      │
                                        │ FBNeo / Snes9x / ... │
                                        │ or official Flycast  │
                                        └──────────────────────┘
```

## UI and state ownership

- `Main.qml` owns presentation state: current screen, joined channels, room
  messages, challenge queue, toasts, ROM download progress, and pending
  launch actions.
- `FightcadeClient` owns the live socket, request IDs, auth, and normalized
  Fightcade payloads. QML never parses raw JSON.
- `FightcadeLauncher` owns install/runtime detection and child processes.
- `RomLibrary` owns ROM metadata lookup and downloads.
- `SettingsStore` owns persisted settings and the session cookie.
- `AppController` owns desktop integration (window visibility, clipboard,
  external links, fullscreen).
- `GamepadManager` emits logical controller actions.
- `FocusController` owns spatial focus across registered `Focusable` items.

## Auth/lobby flow

1. QML/`FightcadeClient` ask `POST /api/` for an encrypted location.
2. `QWebSocket` connects to `wss://ggs.fightcade.com/ws/`.
3. `login`/`autologin` is sent with `requestIdx = -1`.
4. On success, the client emits `loggedIn(user, appInfo)` and persists the
   session through `SettingsStore::saveSession`.
5. QML loads welcome/channels/filters, auto-joins remembered channels, and
   starts the ROM prefetch for joined rooms.

## Requests and events

- Requests that expect a response get a monotonically increasing
  `requestIdx`; the response is routed to a callback and timed out after 20 s.
- Server pushes omit `result`; `FightcadeClient::handleEvent` normalizes them
  into Qt signals (`chatReceived`, `challengeReceived`, `matchStarted`, ...).
- `matchStarted` carries the server's `quarkid`, player side, port, delay,
  ranked mode, and token. QML fills the channel's `gameid`/`emulator` if the
  event omits them.

## Launch flow

1. User activates Test/Training/Spectate, or the server emits `start`.
2. QML calls `RomLibrary` first. If the ROM is missing and auto-download is
   enabled, the launch action is stored in `pendingRomAction`.
3. When `romReady` fires, the pending action executes.
4. `FightcadeLauncher` picks a native runtime or the official helper:
   - FBNeo: `-fullscreen -joy quark:...`
   - Snes9x: `quark:...` (experimental)
   - Flycast: official `fcade://launch/...` helper, then process monitoring
   - GGPOFBA/fc1: unsupported until a native runtime exists
5. QML hides Deckade while a process runs and restores it when the process
   monitor reports the emulator has exited.

## Controller focus

- Each focusable item registers with `FocusController` on completion.
- `FocusController` filters by visibility, enabled state, size, and `navGroup`.
- Directional input scores candidates by geometric distance and perpendicular
  overlap; the best candidate receives `forceActiveFocus`.
- Overlays switch `focusController.activeGroup` to `modal`/`keyboard`.
- `Focusable` draws the focus ring; `onFocusMoved` reveals the item inside the
  nearest `Flickable`/`ListView`.
- Text inputs are `DeckField`, which exposes `deckTextInput: true`; A on a
  text field opens the on-screen keyboard, while A on a button activates it.

## ROM downloader

- `RomLibrary` loads the bundled `:/assets/rom-json/*.json` metadata.
- It resolves the game key, recursively collects `require` entries, checks
  destination files, and downloads missing files sequentially.
- Downloads use HTTPS only, a Fightcade User-Agent, redirects, `.part` files,
  and atomic renames.
- A partial download is aborted/cleaned up on cancel or failure.
- FBNeo system JSONs map to ROM subfolders; the FBNeo config is regenerated
  with absolute ROM paths so the runtime does not depend on a `roms` symlink.

## Security boundaries

- No passwords are persisted or logged.
- Session cookies are stored in an owner-only QSettings INI.
- Test credentials live in `.env.local` (mode 0600), ignored by git.
- Download hosts are allowlisted.
- `fcade` helper invocation is limited to native emulator routes; there is no
  implicit Wine fallback.
