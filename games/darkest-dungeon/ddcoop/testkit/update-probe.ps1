<#
  Puts a new probe SDL2.dll into the EXISTING test copy (%USERPROFILE%\DD-coop-test\DarkestDungeon).
  Your real install and saves are not touched. Run from the folder with this script and SDL2.dll:
      powershell -ExecutionPolicy Bypass -File .\update-probe.ps1
#>
$ErrorActionPreference = "Stop"
$ExpectedProbe = "8ecac5bc817942e7b5cc36a7a1c589c73f16075b8fcc8feb2d19488dca375e3c"
$bin = Join-Path $env:USERPROFILE "DD-coop-test\DarkestDungeon\_windows\win64"
$KitDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Test-Path -LiteralPath (Join-Path $bin "SDL2_orig.dll"))) {
    throw "No test copy with SDL2_orig.dll found in $bin. Run install-testkit.ps1 first."
}
$probe = Join-Path $KitDir "SDL2.dll"
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $probe).Hash.ToLower() -ne $ExpectedProbe) {
    throw "The SDL2.dll in this kit does not match its expected hash. Nothing was changed."
}
$old = Join-Path $bin "coop-probe.log"
if (Test-Path -LiteralPath $old) {
    Move-Item -LiteralPath $old -Destination (Join-Path $bin ("coop-probe-old-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".log"))
}
Copy-Item -LiteralPath $probe -Destination (Join-Path $bin "SDL2.dll") -Force
Write-Host "Probe installed in the test copy: $bin"
