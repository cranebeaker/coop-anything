# Two-instance test driven only by the player-facing commands (F7 coophost / F9 coopjoin). Run with Windows PowerShell 5.1.
#   1. host instance: launch, `setmax 2` + `host` while still at the intro (net state must be 0),
#      then drive it through the intro and Continue into the city
#   2. client instance: launch, focus it so it boots by itself, `leave` its local session,
#      `browse` + `join 0` (late join into the running host)
# Prints both PIDs and the interesting log lines. Usage: coop_test.ps1 [-SkipHost -HostPid N] [-WaitSec 45]
param([switch]$SkipHost, [int]$HostPid = 0, [int]$WaitSec = 45)
$dir = $env:COOP_GAME_DIR  # the test copy of the game folder, never the real install
if (-not $dir) { throw "set COOP_GAME_DIR to the test copy of the game folder" }
$ps = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"

function Start-Instance {
    $r = & $ps -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\start_instance.ps1"
    if ($r -notmatch '^ready (\d+)') { throw "start failed: $r" }
    [int]$Matches[1]
}
function Send-Cmd($id, $cmd, $settle = 3) {
    $c = "$dir\coop_cmd_$id.txt"
    Set-Content -Path $c -Value $cmd -NoNewline -Encoding ascii
    for ($t = 0; (Test-Path $c) -and $t -lt 300; $t++) { Start-Sleep -Milliseconds 100 }  # 30 s: the instance may be dead
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

if (-not $SkipHost) {
    $HostPid = Start-Instance
    Send-Cmd $HostPid 'coophost'  # = F7 at the title screen
    Drive $HostPid 'ENTER'; Start-Sleep 3
    Drive $HostPid 'ESC,ENTER' 1500; Start-Sleep 5
    Drive $HostPid 'ENTER'  # main menu: Continue
    if (-not (Wait-Log $HostPid 'OnGameplayRunning' 90)) { "host never reached gameplay" }
}
"host $HostPid"

$client = Start-Instance
"client $client"
Drive $client ''  # focus only: an unfocused client gets stuck in GameMode Boot
if (-not (Wait-Log $client 'netmgr state \d+ -> 9' 60)) { "client never booted" }
Send-Cmd $client 'coopjoin'  # = F9 in the city
Wait-Log $client 'warping to the host' 60 | Out-Null
Start-Sleep $WaitSec

$pat = 'coop:|sync|dress|puppet:|warp|Client index|exception'
foreach ($p in @(@('client', $client), @('host', $HostPid))) {
    "--- $($p[0]) $($p[1]) alive=$([bool](Get-Process -Id $p[1] -EA 0))"
    Get-Content "$dir\coop_log_$($p[1]).txt" | Where-Object { $_ -match $pat } | Select-Object -Last 25
}
