#!/usr/bin/env bash
# Run Deckade from the source tree with locally bundled Qt WebSockets.
set -euo pipefail
cd "$(dirname "$0")"

if [ -d "$PWD/lib" ]; then
    export LD_LIBRARY_PATH="$PWD/lib:${LD_LIBRARY_PATH:-}"
fi

exec ./build/Deckade "$@"
