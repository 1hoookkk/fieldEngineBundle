#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <name> [description] [version]" >&2
  exit 1
fi

NAME="$1"
DESC="${2:-}"
VER="${3:-0.1.0}"

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
TEMPLATE="$REPO/plugins/_template"
DEST="$REPO/plugins/$NAME"

if [[ -e "$DEST" ]]; then
  echo "Destination '$DEST' already exists." >&2
  exit 1
fi

cp -R "$TEMPLATE" "$DEST"

MANIFEST="$DEST/plugin.json"
TMP="$DEST/.plugin.tmp.json"

# Use jq if available for robust JSON edits; otherwise fallback to sed.
if command -v jq >/dev/null 2>&1; then
  jq \
    --arg name "$NAME" \
    --arg ver "$VER" \
    --arg desc "$DESC" \
    '.name=$name | .version=$ver | (if $desc != "" then .description=$desc else . end)' \
    "$MANIFEST" > "$TMP"
  mv "$TMP" "$MANIFEST"
else
  # naive replacements; ensure template defaults are present
  sed -i.bak "s/\"name\": \"your-plugin-name\"/\"name\": \"$NAME\"/" "$MANIFEST" || true
  sed -i.bak "s/\"version\": \"0.1.0\"/\"version\": \"$VER\"/" "$MANIFEST" || true
  if [[ -n "$DESC" ]]; then
    sed -i.bak "s/\"description\": \"Short description of what this plugin does.\"/\"description\": \"$DESC\"/" "$MANIFEST" || true
  fi
  rm -f "$MANIFEST.bak"
fi

echo "Created plugin at $DEST"
echo "Next: implement entry at '$(jq -r .entry "$MANIFEST" 2>/dev/null || echo src/index)'."

