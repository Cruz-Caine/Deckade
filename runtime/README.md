# Native runtimes

This directory holds Deckade's native emulator runtimes. Built binaries live in
`bin/` and are intentionally gitignored.

- `./scripts/build-fbneo.sh` — native FBNeo + Fightcade GGPO/quark
- `./scripts/build-snes9x.sh` — experimental native Snes9x runner
- `./scripts/build-flycast.sh` — pinned Flycast Dojo 6.46 release (GPL-2.0; [source](https://github.com/blueminder/flycast-dojo/tree/dojo-6.46))
- GGPOFBA/fc1 is not ported yet

The app discovers this directory automatically when run from the source tree.
For an installed app, the directory is installed to
`/usr/share/deckade/runtime` (or `../share/deckade/runtime` relative to the
binary).

See `docs/RUNTIMES.md` for build details, controller mapping, and known issues.
