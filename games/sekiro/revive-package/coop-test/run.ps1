<#
  Record one side of a Sekiro Online co-op session.

  Host PC:
    .\run.ps1 -Label host -Minutes 15
  Friend PC:
    .\run.ps1 -Label guest -Minutes 15

  It launches Sekiro if needed, records the mod's HUD (OCR) and the game's TCP
  connections, and writes <Label>.jsonl next to this script. Send both files to
  the host and run: python report.py host.jsonl guest.jsonl
#>
param(
  [ValidateSet('host', 'guest')][string]$Label = 'host',
  [int]$Minutes = 15,
  [switch]$NoLaunch
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here "$Label.jsonl"

if (-not $NoLaunch) {
  $running = Get-Process sekiro -ErrorAction SilentlyContinue
  if (-not $running) {
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    Write-Host "Launching Sekiro through Steam..." -ForegroundColor Cyan
    Start-Process 'steam://rungameid/814380'
    Start-Sleep -Seconds 25
  }
}

Write-Host "Recording $Label for $Minutes minute(s) -> $out" -ForegroundColor Cyan
Write-Host "Host: load a save, use the Cursed Emblem, invite from the Steam friends list." -ForegroundColor Yellow
Write-Host "Guest: accept the Steam invite." -ForegroundColor Yellow

python (Join-Path $here 'agent.py') --seconds ($Minutes * 60) --out $out --label $Label --save-on-change
python (Join-Path $here 'report.py') $out
