<#
  Starts the TEST COPY of Darkest Dungeon for the determinism test and keeps the logs of this run.
      powershell -ExecutionPolicy Bypass -File .\run-determinism.ps1 -Run A
      powershell -ExecutionPolicy Bypass -File .\run-determinism.ps1 -Run B
      powershell -ExecutionPolicy Bypass -File .\run-determinism.ps1 -Run P   (only if you need a new dungeon save; the game SAVES)
  Developer options used (built into the game):
      -randomseed 424242   same random seed in every run
      -enable_autobattle   the game's AI fights (test only; the co-op mod will not use this)
      -nosave              runs A and B save nothing, so both start from the same dungeon save
  Your real install and saves are not touched.
#>
param([Parameter(Mandatory = $true)][ValidateSet("P", "A", "B")][string]$Run)
$ErrorActionPreference = "Stop"
$bin = Join-Path $env:USERPROFILE "DD-coop-test\DarkestDungeon\_windows\win64"
$exe = Join-Path $bin "Darkest.exe"
if (-not (Test-Path -LiteralPath (Join-Path $bin "SDL2_orig.dll"))) { throw "No test copy found in $bin. Run install-testkit.ps1 first." }
foreach ($f in "coop-probe.log", "coop-rng.log") {
    Remove-Item -LiteralPath (Join-Path $bin $f) -ErrorAction SilentlyContinue
}
$options = "-randomseed 424242 -enable_autobattle"
if ($Run -ne "P") { $options += " -nosave" }
Write-Host "Starting run $Run with: $options"
Write-Host "Follow README.txt; this window continues when the game has closed."
Start-Process -FilePath $exe -ArgumentList $options -WorkingDirectory $bin -Wait
foreach ($f in "coop-probe", "coop-rng") {
    $src = Join-Path $bin "$f.log"
    if (Test-Path -LiteralPath $src) {
        Move-Item -LiteralPath $src -Destination (Join-Path $bin "$f-$Run.log") -Force
        Write-Host "Saved $bin\$f-$Run.log"
    } else {
        Write-Host "Warning: $f.log was not written."
    }
}
