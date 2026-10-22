# Launches one game instance from the test copy, clicks Play on the PC launcher and waits until the
# city level has loaded. Prints the PID. Usage: start_instance.ps1 [-TimeoutSec 300]
param([int]$TimeoutSec = 300)
$dir = $env:COOP_GAME_DIR  # the test copy of the game folder, never the real install
if (-not $dir) { throw "set COOP_GAME_DIR to the test copy of the game folder" }
$p = Start-Process -FilePath "$dir\Spider-Man.exe" -WorkingDirectory $dir -PassThru
$log = "$dir\coop_log_$($p.Id).txt"
& powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\click_play.ps1" -GamePid $p.Id | Out-Null
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    if (-not (Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) { "exited $($p.Id)"; exit 1 }
    if ((Test-Path $log) -and (Select-String -Path $log -Pattern 'i20_city.level' -Quiet)) {
        Start-Sleep -Seconds 20  # let startup threads settle
        "ready $($p.Id)"; exit 0
    }
    Start-Sleep -Seconds 2
}
"timeout $($p.Id)"; exit 2
