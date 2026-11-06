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

The binary is `runtime/bin/deckade-fbneo`.
