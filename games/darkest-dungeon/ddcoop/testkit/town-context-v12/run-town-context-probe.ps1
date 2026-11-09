$ErrorActionPreference = 'Stop'

$expectedExe = '4d78fbfaa65b7d002d2208d02da809200f4a5723119521180e30100d6f1a75f2'
$expectedProbe = 'e3d4796686656c496715ab0d9917d4413b12b34ef8eef3cb6929b9c27c7dd8fc'
$testBin = Join-Path $env:USERPROFILE 'DD-coop-test\DarkestDungeon\_windows\win64'
$exe = Join-Path $testBin 'Darkest.exe'
$origSdl = Join-Path $testBin 'SDL2_orig.dll'
$probeSdl = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'SDL2.dll'
$probeLog = Join-Path $testBin 'coop-probe.log'
$resultLog = Join-Path $testBin 'coop-probe-town-context.log'

if (-not (Test-Path -LiteralPath $origSdl) -or -not (Test-Path -LiteralPath $exe)) {
    throw "The test copy is not ready at $testBin. Install the existing testkit first."
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash.ToLowerInvariant() -ne $expectedExe) {
    throw 'Darkest.exe is not the supported build. Nothing was changed.'
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $probeSdl).Hash.ToLowerInvariant() -ne $expectedProbe) {
    throw 'The probe DLL hash does not match this kit. Nothing was changed.'
}

# Preserve the previously installed test-copy SDL2.dll once, then replace only that test-copy file.
$installedSdl = Join-Path $testBin 'SDL2.dll'
$backupSdl = Join-Path $testBin 'SDL2-before-town-context-probe.dll'
if ((Test-Path -LiteralPath $installedSdl) -and -not (Test-Path -LiteralPath $backupSdl)) {
    Copy-Item -LiteralPath $installedSdl -Destination $backupSdl
}
Copy-Item -LiteralPath $probeSdl -Destination $installedSdl -Force
Remove-Item -LiteralPath $probeLog -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $resultLog -ErrorAction SilentlyContinue

Write-Host 'Starting only the DD-coop-test copy with -nosave.'
Write-Host 'Visit the screens listed in README.txt, then quit the game.'
Start-Process -FilePath $exe -ArgumentList '-nosave' -WorkingDirectory $testBin -Wait

if (Test-Path -LiteralPath $probeLog) {
    Move-Item -LiteralPath $probeLog -Destination $resultLog -Force
    Write-Host "Probe log saved to $resultLog"
} else {
    Write-Warning 'The probe did not create coop-probe.log. Check that the test copy is using the probe SDL2.dll.'
}
