#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-../pamplejuce.yml}"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if ! command -v pamplejuce >/dev/null 2>&1; then
  echo "pamplejuce CLI not found. Install with: pip install pamplejuce" >&2
fi

cd "$ROOT_DIR"
pamplejuce generate -c "$CONFIG"

