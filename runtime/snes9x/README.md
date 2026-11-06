# Snes9x runtime

Built by `scripts/build-snes9x.sh` from the pinned Macade Snes9x tree plus the
Deckade Linux SDL2 runner (`runner.cpp`) and Macade's GGPO/quark adapter.

The runner is native Linux SDL2 and does not use Wine. Netplay is experimental:
verify direct, served, stream, and replay routes with real peers before treating
it as production-ready.

The binary is `runtime/bin/deckade-snes9x`.
