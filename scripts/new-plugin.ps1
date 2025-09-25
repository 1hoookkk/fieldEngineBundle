param(
  [Parameter(Mandatory = $true)][string]$Name,
  [string]$Description = "",
  [string]$Version = "0.1.0"
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $here '..')
$template = Join-Path $repo 'plugins/_template'
$dest = Join-Path $repo (Join-Path 'plugins' $Name)

if (Test-Path $dest) {
  Write-Error "Destination '$dest' already exists. Choose another name."
}

Copy-Item -Recurse -Force $template $dest

$manifestPath = Join-Path $dest 'plugin.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json

$manifest.name = $Name
$manifest.version = $Version
if ($Description) { $manifest.description = $Description }

$manifest | ConvertTo-Json -Depth 8 | Out-File -FilePath $manifestPath -Encoding UTF8 -NoNewline

Write-Host "Created plugin at $dest"
Write-Host "Next: implement entry at '$(Join-Path $dest $manifest.entry)'."

