<#
  Starts the TEST COPY of Darkest Dungeon with -nosave (the game saves nothing) and keeps the probe log.
      powershell -ExecutionPolicy Bypass -File .\run-savecheck.ps1
  Your real install and saves are not touched.
#>
$ErrorActionPreference = "Stop"
$bin = Join-Path $env:USERPROFILE "DD-coop-test\DarkestDungeon\_windows\win64"
$exe = Join-Path $bin "Darkest.exe"
if (-not (Test-Path -LiteralPath (Join-Path $bin "SDL2_orig.dll"))) { throw "No test copy found in $bin. Run install-testkit.ps1 first." }
Remove-Item -LiteralPath (Join-Path $bin "coop-probe.log") -ErrorAction SilentlyContinue
Write-Host "Starting the test copy with -nosave. This window continues when the game has closed."
Start-Process -FilePath $exe -ArgumentList "-nosave" -WorkingDirectory $bin -Wait
$src = Join-Path $bin "coop-probe.log"
if (Test-Path -LiteralPath $src) {
    Move-Item -LiteralPath $src -Destination (Join-Path $bin "coop-probe-saves.log") -Force
    Write-Host "Saved $bin\coop-probe-saves.log"
} else {
    Write-Host "Warning: coop-probe.log was not written."
}
