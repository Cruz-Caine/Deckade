# Runtime game matrix

The protocol suite is headless and always runs in CI. The native emulator
matrix is a separate black-box test because it needs emulator ROMs:

```sh
./tests/game_matrix.sh
```

It launches every locally available FBNeo ROM with SDL's offscreen driver,
asks FBNeo to dump a frame, and fails when:

- no frame is produced,
- the frame is blank or nearly uniform,
- a bundled savestate does not load in training mode,
- the 68K CPU runs into garbage (thousands of unmapped reads),
- training mode emits a Lua error.

Environment:

```text
DECKADE_MATRIX_ROM_DIR   FBNeo ROM directory
DECKADE_MATRIX_WORK      runtime work directory (a temporary one by default)
DECKADE_MATRIX_TIMEOUT   seconds per launch, default 30
DECKADE_MATRIX_LIMIT     optional number of ROMs to test
DECKADE_MATRIX_MODES     "solo training" by default
DECKADE_MATRIX_OUT       report/frame output directory
```

This catches launch, video-frame, and training-script regressions across games.
It cannot prove gameplay/input correctness; the controller profile and live
netplay paths have their own tests/probes.

`tests/runtime_assets_tests.py` needs no ROMs and runs in CI. It validates every
bundled savestate, the overlay and score-detector assets, and the build-script
ordering that the frame check and Lua overlays depend on.

The runtime needs `DECKADE_FRAME_DUMP` support (added by
`scripts/patches/fbneo-frame-dump.py` while building FBNeo). The driver writes a
small header followed by the raw framebuffer after the configured frame.
