# FBNeo runtime

Built by `scripts/build-fbneo.sh` from the pinned Macade Fightcade FBNeo tree.

Linux-specific changes:

- SDL/OpenGL makefile flags instead of macOS frameworks.
- `macade_embedded.cpp` replaced with an inert stub so the normal SDL window is used.
- `VidPaint()` restored to present every frame outside embedded mode. Without
  this fix the game runs but the window is black.
- The launcher passes `-fullscreen -joy`; `-joy` maps controller 0 to P1.
- POV hat 0 is folded into the left-stick X/Y axes so D-pad directions work
  with FBNeo's legacy axis bindings.

- Official Fightcade Neo Geo savestates predate the `Neo68KFix` offset field;
  `scripts/patches/fbneo-state-compat.py` detects and loads that layout.
- Lua `gd` is built in, and SDL paints the Lua gui layer each frame. See
  `scripts/patches/fbneo-lua-gd.py`.
- `ui/` holds the match overlay art and font, and `detector/` the per-game score
  detection data.

The binary is `runtime/bin/deckade-fbneo`.
