# Macade → Deckade audit

This is the working audit of Macade's source tree. The goal is to classify
every area as **Mac-only**, **already covered**, **partial**, or **missing**,
instead of assuming that "the native emulator works" means the match path works.

## Verdict for the black-screen match

The ROM, launcher, video, and overlay paths work. The missing piece is Macade's
**app-side Fightcade UDP netplay proxy**:

- `FightcadeMasterClient`
- `FightcadeNetplaySession` / UDP hole punching
- `FightcadeUDPProxy`
- `FightcadeEmbeddedProxyBootstrap` environment wiring
- the `useports` fallback

Without those, native FBNeo does direct UDP to the opponent. It receives the
match-info events (the `5000`–`5004` log codes) but never gets a peer. FBNeo
therefore waits in GGPO synchronization, draws no usable match frame, and the
other client stays on its black "waiting for connection" screen.

## `Core/Fightcade`

| File | What it does | Mac-only? | Deckade status | Action |
| --- | --- | --- | --- | --- |
| `EmbeddedVideoDiagnostics` | Embedded video stats | Mac | — | Skip |
| `EmbeddedVideoDisplayPolicy` | macOS presentation policy | Mac | — | Skip |
| `EmbeddedVideoFrameBuffer` | macOS shared-memory frames | Mac | — | Skip |
| `EmbeddedVideoUpload` | Copy frames into Mac view | Mac | — | Skip |
| `FightcadeBootstrapService` | Get encrypted login location | No | Done | — |
| `FightcadeEmbeddedInputClient` | macOS input socket | Mac | — | Skip |
| `FightcadeEmbeddedProxyBootstrap` | Start proxy, set FBNeo proxy env | No | **Missing** | **Do now** |
| `FightcadeEmbeddedSession` | Embedded session lifecycle | Mac | — | Skip |
| `FightcadeEmbeddedVideoStream` | macOS shared video stream | Mac | — | Skip |
| `FightcadeEndpoint` | API/WS/status URLs | No | Done | — |
| `FightcadeFBNeoConfig` | FBNeo config edits | Mostly | Partial | Port later if needed |
| `FightcadeFBNeoSettings` | FBNeo option storage | Mac UI-specific | — | Skip/port later |
| `FightcadeGGPOPeerUDPProtocol` | Peer UDP packet parsing | No | Missing as app module | Add with proxy |
| `FightcadeGGPOProtocol` | GGPO TCP framing/records | No | Missing as app module | Add tests/module |
| `FightcadeGGPOServedHandshake` | Served TCP handshake | No | Missing as app module | Tests/reference |
| `FightcadeLaunchModels` | Match launch data | No | Done equivalents | — |
| `FightcadeLaunchRoutes` | Route parsing/metadata | No | Partial | Add tests when proxy lands |
| `FightcadeLaunchSupport` | Process/env/launch helpers | Partial | Partial | Port proxy env |
| `FightcadeLauncher+Embedded` | Embedded launch orchestration | Mac | — | Skip |
| `FightcadeLauncher` | Launch/stop emulator | Partial | Done native equivalent | Proxy integration |
| `FightcadeMasterClient` | UDP master registration/hole punch | No | **Missing** | **Do now** |
| `FightcadeNetplayPreferences` | Port-mapping preference | No | Missing | Later |
| `FightcadeNetplayPreflight` | Network/port checks | No | Missing | Later |
| `FightcadeNetplaySession` | Endpoint/hole-punch protocol | No | **Missing** | **Do now** |
| `FightcadePortMapping` | PCP/NAT-PMP/UPnP | No | Missing | Later |
| `FightcadeProxyDiagnostics` | Proxy packet logs | No | Missing | Add with proxy |
| `FightcadeROMDownloader` | ROM download/deps | No | Done equivalent | — |
| `FightcadeROMLibrary` | ROM index abstraction | No | Partial (bundled JSON) | Optional |
| `FightcadeRuntime` | Runtime bundle discovery | Partial | Done equivalent | — |
| `FightcadeRuntimeManifest` | Runtime capabilities | No | Partial | Add proxy capability |
| `FightcadeServiceError` | Error mapping | No | Done equivalents | — |
| `FightcadeUDPProxy` | Local UDP relay | No | **Missing** | **Do now** |
| `FightcadeUserStatusReporter` | Launcher token report | No | Done | — |
| `FightcadeZipArchive` | ZIP index extraction | No | Not needed (bundled JSON) | Skip |
| `MacadeControllerPreferences` | mac controller storage | Mac | — | Skip |
| `MacadeGamepadPreferences` | mac gamepad storage | Mac | SDL equivalent | — |
| `MacadeRelayProfile` | Relay mode | Mac/relay-specific | — | Skip |
| `MacadeRelayRunner` | Relay runner | Mac/relay-specific | — | Skip |
| `MacadeResourceBundle` | mac bundle lookup | Mac | — | Skip |

## `Features/Lobby` models

| Area | Macade file(s) | Status | Action |
| --- | --- | --- | --- |
| Challenge | `FightcadeChallenge` | Done | — |
| Channel/launch metadata | `FightcadeChannel*` | Done/partial | — |
| Chat messages | `FightcadeChatMessage` | Done | — |
| MOTD formatting | `FightcadeMotdTextFormatter` | Missing | Optional |
| Chat mentions | `FightcadeChatMention` | Missing | Optional |
| Auto-match | `FightcadeAutoMatch` | Missing | Not required for manual match |
| Friends | `FightcadeFriend` | Missing | Optional feature |
| Recent matches | `FightcadeRecentMatch` | Missing | Optional feature |
| Dashboard/welcome | `FightcadeDashboard` | Done | — |
| Lobby events | `FightcadeLobbyEvent` | Done equivalents | — |

## `Features/Lobby/Services`

| Area | Macade file(s) | Status | Action |
| --- | --- | --- | --- |
| Payload parsing | `FightcadeLobbyPayloadParser*` | Partial, 18 tests | Add cases as needed |
| Connection/autologin | `FightcadeLobbyService+Connection` | Done | — |
| Lobby operations | `FightcadeLobbyService` | Done | — |
| Browser | `FightcadeLobbyService+Browser` | Done | — |
| Recent matches | `FightcadeLobbyService+RecentMatches` | Missing | Optional |
| Joined-channel store | `JoinedChannelStore` | Done equivalent | — |
| Auto-match store | `FightcadeAutoMatchConfigurationStore` | Missing | Optional |
| Friend store | `FightcadeFriendStore` | Missing | Optional |
| Diagnostics | `FightcadeLobbyDiagnostics` | Partial | Optional |
| Layout store | `LobbyLayoutStore` | Mac UI | Skip |

## `Features/Lobby/ViewModels`

| Area | Status | Action |
| --- | --- | --- |
| Challenge accept/decline/cancel | Done | — |
| Game launch / ROM | Done/partial | Add proxy |
| Events / users | Done | — |
| Joined channels | Done | — |
| Chat | Done/partial | Search/mentions later |
| Auto-match | Missing | Optional |
| Friends | Missing | Optional |
| Recent matches | Missing | Optional |
| Channel TV / spectate | Partial | Existing basic spectate |
| Embedded emulation | Mac-only | Skip |

## Views / App / Auth / Settings / Updates

These are SwiftUI/AppKit views, macOS window management, Sparkle/GitHub
updater, Keychain/IOKit identity, and mac-specific embedded video. They are
**Mac-only**. Deckade's Qt/QML screens, SDL gamepad layer, settings store, and
AppImage pipeline are the Linux equivalents. Do not port these file-by-file.

## Tests

| Macade test | Status |
| --- | --- |
| `FightcadeLobbyPayloadParserTests` | Ported subset, 18-case suite |
| `FightcadeWelcomeParserTests` | Ported |
| `FightcadeMatchLaunchTests` | Ported as quark-command tests |
| `FightcadeUserStatusReporterTests` | Ported hash vector |
| `FightcadePreviewURLTests` | Ported |
| `FightcadeROMIndexTests` | Partial: data metadata test only |
| `FightcadeGGPOProtocolTests` | **Missing** — add with proxy |
| `FightcadeGGPOServedHandshakeTests` | **Missing** — add with proxy |
| `FightcadeNetplaySessionTests` | **Missing** — add with proxy |
| `FightcadeNetplayLifecycleTests` | **Missing** — add with proxy |
| UI/embedded/gamepad/settings tests | Mac-only, skip |

## Immediate priority

Implement the netplay path in this order:

1. Endpoint/master protocol and UDP transport.
2. Hole punching and peer-address parsing.
3. Local UDP proxy.
4. Launcher integration and FBNeo proxy environment.
5. Proxy tests and regression tests.
6. Port mapping/preflight as optional diagnostics.
