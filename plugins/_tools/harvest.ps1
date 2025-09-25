param(
  [string]$RichDataDir = "C:/fieldEngineBundle/rich data",
  [string]$OutDir = "plugins/_packs",
  [string]$Spec = "plugins/_tools/specs/proteus_v22.json"
)

$ErrorActionPreference = 'Stop'

python "${PSScriptRoot}/sysex_pack.py" --in-dir "$RichDataDir" --out-dir "$OutDir" --spec "$Spec"

