param(
  [string]$Config = "../pamplejuce.yml"
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..')

if (-not (Get-Command pamplejuce -ErrorAction SilentlyContinue)) {
  Write-Host "pamplejuce CLI not found. Install with: pip install pamplejuce" -ForegroundColor Yellow
}

Push-Location $root
try {
  pamplejuce generate -c $Config
} finally {
  Pop-Location
}

