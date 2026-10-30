# One instance: into the city, spawn the puppet next to the hero, screenshot. Run with Windows PowerShell 5.1.
# Usage: puppet_test.ps1 [-Shot <png>] [-Pid N to reuse a running instance]
param([string]$Shot = "", [int]$GamePid = 0)
$dir = $env:COOP_GAME_DIR  # the test copy of the game folder, never the real install
if (-not $dir) { throw "set COOP_GAME_DIR to the test copy of the game folder" }
$ps = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"

function Send-Cmd($id, $cmd, $settle = 3) {
    $c = "$dir\coop_cmd_$id.txt"
    Set-Content -Path $c -Value $cmd -NoNewline -Encoding ascii
    for ($t = 0; (Test-Path $c) -and $t -lt 300; $t++) { Start-Sleep -Milliseconds 100 }
    Remove-Item $c -ErrorAction SilentlyContinue
    Start-Sleep $settle
}
function Drive($id, $keys, $delay = 600) {
    $a = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "$PSScriptRoot\drive.ps1", '-GamePid', $id, '-DelayMs', $delay)
    if ($keys) { $a += @('-Keys', $keys) }
    & $ps @a | Out-Null
}
function Wait-Log($id, $pattern, $sec) {
    for ($t = 0; $t -lt $sec; $t += 2) {
        if (Select-String -Path "$dir\coop_log_$id.txt" -Pattern $pattern -Quiet) { return $true }
        Start-Sleep 2
    }
    $false
}

if (-not $GamePid) {
    $r = & $ps -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\start_instance.ps1"
    if ($r -notmatch '^ready (\d+)') { throw "start failed: $r" }
    $GamePid = [int]$Matches[1]
    Drive $GamePid 'ENTER'; Start-Sleep 3
    Drive $GamePid 'ESC,ENTER' 1500; Start-Sleep 5
    Drive $GamePid 'ENTER'  # main menu: Continue
    if (-not (Wait-Log $GamePid 'OnGameplayRunning' 90)) { "never reached gameplay" }
    Start-Sleep 15
}
"game $GamePid"

Send-Cmd $GamePid 'sync loop' 8



Get-Content "$dir\coop_log_$GamePid.txt" | Where-Object { $_ -match 'sync|dress|puppet:|faulted|exception' } | Select-Object -Last 25
"alive=$([bool](Get-Process -Id $GamePid -EA 0))"
