#!/usr/bin/env bash
# Refresh the bundled Fightcade ROM metadata from lofi1048's fc2json pack.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
curl -fL https://fightcade.download/fc2json.zip -o "$TMP/fc2json.zip"
command -v unzip >/dev/null 2>&1 || { echo "unzip is required" >&2; exit 1; }
unzip -q "$TMP/fc2json.zip" -d "$TMP/json"
mkdir -p "$ROOT/assets/rom-json"
cp "$TMP"/json/*.json "$ROOT/assets/rom-json/"
cp "$TMP"/json/frmJson-changelog.txt "$ROOT/assets/rom-json/" 2>/dev/null || true
echo "Updated $ROOT/assets/rom-json"
