# Fightcade protocol notes

Deckade targets the current Fightcade 2 web/lobby protocol. The authoritative
reference is the official client source map served at:

- `https://web.fightcade.com/static/js/app.<hash>.js.map`
- `https://web.fightcade.com/static/js/vendor.<hash>.js.map`

The official `web.fightcade.com` frontend is a Vue client wrapped by a
nativefier Electron shell. Its login, lobby, and launch messages are the same
ones Deckade implements in `src/fightcade/`.

## Endpoints

| Purpose | Endpoint |
| --- | --- |
| Bootstrap location | `POST https://web.fightcade.com/api/` with `{"req":"location"}` |
| Lobby WebSocket | `wss://ggs.fightcade.com/ws/` |
| Web client | `https://web.fightcade.com/` |
| User-Agent used by the desktop client | `Fightcade2-WIN32-v2.1.45` |

The API requires a browser/desktop User-Agent. The `location` response is an
encrypted opaque string consumed by `login`/`autologin`.

## Authentication

```json
{"req":"location"}
```

Response contains `{"res":"OK","location":"..."}`.

Then over the WebSocket:

```json
{"req":"login","username":"...","userpass":"...","location":"...","requestIdx":-1}
```

or:

```json
{"req":"autologin","cookie":"...","location":"...","requestIdx":-1}
```

A successful response has `result: 200`, a `user` object, `app_info`, and on
password login a `cookie`. The launcher token may be at the top level, inside
`user`, or nested deeper in the response; Deckade searches recursively.
The cookie is the persistent session; Deckade stores it owner-only.

After login every client must register the launcher with the status service.
Without this the lobby treats the installation as broken and rejects
challenges with `cant challenge user`:

```json
POST https://web.fightcade.com/fc2status/api/
{
  "req": "userstatus",
  "token": "<launcher token>",
  "userstatus": "stcable",
  "uuid": "<stable UUID>",
  "guid": "<stable UUID>",
  "huid": "0030613289.684<md5(machine id)>",
  "version": 6,
  "hash": "<md5 formula>"
}
```

The hash input is the literal concatenation
`3jedoQ<token>qmkq0<uuid>dsnds<guid>sec or62jden3<userstatus>llNjjha<huid>`
(example for version 6; there is no separator between `6` and `2jden3`). The response is `{"res":"OK"}`. Deckade performs
this POST with `curl` before it emits `loggedIn`, so channels and challenges are
only started after the launcher status is accepted.

Auth failures return `result != 200` plus an `error` string. Known errors:

- `user wrong password`
- `too many failed login`
- `login locked`
- `user not found`
- `user locked`
- `user newaccount muted`
- `user banned`
- `server locked`
- `old version`

## Request bookkeeping

Requests that expect a response get an integer `requestIdx`. Responses echo it
and include `result`. Server pushes are messages **without** `result` and use
`req` as the event name. Deckade times out requests after 20 seconds and shows
429/rate-limit responses as a cooldown.

Heartbeat: WebSocket ping every 20 s; Deckade treats >60 s without a pong as a
dead lobby.

## Lobby requests

```json
{"req":"welcome"}
{"req":"channels","all":true}
{"req":"filteroptions"}
{"req":"channels","filter":"...","genre":"...","year":"...","system":"...","ranked":null,"favorites":false,"paginated":true,"page":0}
{"req":"join","channelname":"...","status":"available","away":false,"idx":-1}
{"req":"leave","channelname":"..."}
{"req":"chat","channelname":"...","chat":"..."}
{"req":"favchannel","channelname":"...","fav":true}
{"req":"staway"} / {"req":"stnoaway"}
{"req":"chaway","channelname":"..."} / {"req":"chnoaway","channelname":"..."}
{"req":"updateuser", "...": "..."}
```

### Join response shape

`join` result 200 includes the channel metadata and:

- `users[]`: `name`, `gravatar`, `country`, `ping`, `vping`, `rank`, `away`,
  `wlan`, `proxy`, `ranked`, `channel_away`, `game` (when playing)
- `quarks[]`: `quarkid`, `port`, `ranked`, `players`, `spectators`
- `motd` (HTML), `events[]`

Deckade normalizes all of this into `ChannelSummary`/`JoinedChannel` maps in
`src/fightcade/protocol.cpp`.

### Search response

`channels` result 200 includes `channels[]`, `page`, `totalChannels`.
The official page size is 50.

## Lobby events

| `req` | Meaning |
| --- | --- |
| `chat` / `broken` | Chat or system message |
| `join` / `leave` / `kick` | Roster changes |
| `challenge` | Incoming challenge |
| `cancel` | Challenger cancelled an incoming challenge |
| `reject` | Outgoing challenge rejected |
| `accept` | Outgoing challenge accepted |
| `start` | Match is ready; launch the emulator |
| `staway` / `stnoaway` | Global away state |
| `chaway` / `chnoaway` | Per-channel away state |
| `stwlan` / `stcable` | Connection type |
| `updaterank` | Rank changed |
| `stplaying` / `stnoplaying` | Playing state and `game` metadata |
| `quarkstats` | Spectator count update |

Challenge requests:

```json
{"req":"challenge","username":"...","channelname":"...","challengeid":1,"ranked":3}
{"req":"accept","username":"...","channelname":"...","challengeid":1,"ranked":3}
{"req":"reject","username":"...","channelname":"...","challengeid":1}
{"req":"cancel","username":"...","channelname":"...","challengeid":1}
```

`ranked` is the FT length (`0`, `2`, `3`, `5`, `10`, `20`). A `start` event
contains `channelname`, `quarkid`, `playerid`, `port`, `ranked`, `delay`,
`token`, and the opponent user.

The server does not send chat rows for challenge lifecycle events: `challenge`,
`cancel`, `reject`, `accept` and `start` are separate requests with no
`channelname`-scoped `chat`/`broken` text. Deckade therefore writes its own
room-chat status lines for these events (for example "X challenged you to FT3
in <game>", "X accepted the challenge", and "Match with X ended" when the
emulator closes), the same way the Fightcade client's chat log surfaces them.

## Official launch routes

The official web client changes `window.location` to an `fcade://` URI; the
Electron/native launcher handles it.

```text
fcade://served/<emulator>/<gameid>/<quarkid>.<playerid>,<port>,<delay>,<ranked>
fcade://launch/<emulator>/<gameid>/<quarkid>.<playerid>,<port>,<delay>,<ranked>/<token>
fcade://stream/<emulator>/<gameid>/<quarkid>.2,<port>
fcade://play/<emulator>/<gameid>
fcade://training/<emulator>/<gameid>
fcade://killemu
fcade://userstatus/<status>/<token>   (Deckade performs the equivalent HTTP POST)
fcade://checkrom/<emulator>/<gameid>
```

- Normal emulators (FBNeo, GGPOFBA) use `served`.
- Flycast, DuckStation, and custom emulators use `launch` with the token.
- Deckade's native runtimes bypass `fcade` and build the equivalent
  `quark:` argument directly.
- Flycast still uses the official helper because that helper performs the
  master registration and launches the native Flycast binary with
  `-config dojo:...` options.

## Native quark arguments

FBNeo and the Snes9x runner accept:

```text
quark:served,<gameid>,<quarkid>.<playerid>,<port>,<delay>,<ranked>
quark:training,<gameid>,<quarkid>.<playerid>,<port>,<delay>
quark:direct,<gameid>,<localport>,<host>,<remoteport>,<playerid>,<delay>,<ranked>
quark:stream,<gameid>,<quarkid>.<spectator>,<port>
quark:replay,<path>
```

The GGPO implementation is vendored in the FBNeo runtime source
(`dep/ggponet-native`). It implements the reconstructed Fightcade
`ggponet.dll` client/served/stream/replay wire behavior.

## ROM metadata

The bundled `assets/rom-json/` files are lofi1048's `fc2json` pack, the same
metadata Fightcade uses for automatic ROM downloads. Each key maps to:

```json
{ "download": "https://fightcade.download/...", "require": ["parent_rom"] }
```

Deckade resolves the game ID, recursively downloads `require` entries, and
places files in the emulator-specific ROM folder. FBNeo system packs are
mapped to subfolders (`nes`, `megadrive`, `gamegear`, `pce`, `sg1000`, `msx`,
`tg`, `coleco`). `scripts/update-rom-json.sh` refreshes the pack.

Allowed download hosts are `fightcade.download` and
`archive.org/download/fightcaderoms` (the legacy fc1 mirror). Other URLs are
rejected by the downloader.
