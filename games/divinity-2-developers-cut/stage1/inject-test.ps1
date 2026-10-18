# Stage 1 injection test: place dinput8 proxy into TEST COPY B only, launch, verify log.
# Live install (<game dir>) is NEVER touched.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$binB = Join-Path $env:LOCALAPPDATA 'Temp\opencode\divinity2-test-b\bin'
$exe = Join-Path $binB 'Divinity2.exe'
if (-not (Test-Path $exe)) { throw "test copy B exe not found: $exe" }
if (-not (Test-Path (Join-Path $here 'dinput8.dll'))) { throw 'build dinput8.dll first (build.ps1)' }

Copy-Item (Join-Path $here 'dinput8.dll') (Join-Path $binB 'dinput8.dll') -Force
$log = Join-Path $binB 'dinput8proxy.log'
if (Test-Path $log) { Remove-Item $log -Force }

$before = Get-Date
$p = Start-Process -FilePath $exe -WorkingDirectory $binB -PassThru
Write-Host "launched PID $($p.Id), waiting 60s (past the ~40s crash window)..."
Start-Sleep -Seconds 60

$alive = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
Write-Host "process alive after 60s: $([bool]$alive)"
if ($alive) {
    $mods = $null
    try { $mods = $alive.Modules } catch {}
    if ($mods) {
        $d = $mods | Where-Object { $_.ModuleName -match 'dinput8' }
        Write-Host 'dinput8 modules loaded:'
        $d | ForEach-Object { Write-Host ("  " + $_.FileName) }
    }
}
if (Test-Path $log) {
    Write-Host '--- dinput8proxy.log ---'
    Get-Content $log
    Write-Host '------------------------'
} else {
    Write-Host 'LOG NOT CREATED'
}
# check for a fresh crash since launch
$crash = Get-WinEvent -FilterHashtable @{LogName='Application'; Id=1000; StartTime=$before} -ErrorAction SilentlyContinue |
    Where-Object { $_.Message -match 'Divinity' }
if ($crash) { Write-Host 'CRASH DETECTED (Application Error 1000 since launch)' } else { Write-Host 'no crash event since launch' }
