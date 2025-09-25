param(
  [Parameter(Mandatory = $true)][string]$Input,
  [Parameter(Mandatory = $true)][string]$Out
)

$ErrorActionPreference = 'Stop'

python "${PSScriptRoot}/sysex_pack.py" $Input --out $Out

