#!/usr/bin/env bash
set -euo pipefail

RICH_DATA_DIR="${1:-C:/fieldEngineBundle/rich data}"
OUT_DIR="${2:-plugins/_packs}"
SPEC="${3:-plugins/_tools/specs/proteus_v22.json}"

python "$(dirname "$0")/sysex_pack.py" --in-dir "$RICH_DATA_DIR" --out-dir "$OUT_DIR" --spec "$SPEC"

