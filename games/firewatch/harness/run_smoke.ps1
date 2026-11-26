# Firewatch co-op smoke suite (two real clients, one PC)
# Assumes: BepInEx + StoryCoop + CoopTestDriver are installed in .\game and .\game2,
# and that the save folder is isolated (junction to .\saves-sandbox).
# Prints "RESULT: PASS" or "RESULT: FAIL".

$ErrorActionPreference = "Stop"
$w = Split-Path -Parent $MyInvocation.MyCommand.Path
$game = Join-Path $w "game"
$game2 = Join-Path $w "game2"
$fails = @()

function Check([bool]$cond, [string]$name, [string]$detail) {
    if ($cond) { "PASS  $name  $detail" } else { "FAIL  $name  $detail"; $script:fails += $name }
}
function Cmd([string]$dir, [string]$line) {
    Set-Content (Join-Path $dir "BepInEx\cooptest_cmd.txt") $line
}
function State([string]$dir, [string]$role) {
    $p = Join-Path $dir "BepInEx\cooptest_$role.json"
    if (-not (Test-Path $p)) { return $null }
    try { return Get-Content $p -Raw | ConvertFrom-Json } catch { return $null }
}
function Launch([string]$dir) {
    $cmd = '"' + (Join-Path $dir "Firewatch.exe") + '" -screen-width 1280 -screen-height 720 -screen-fullscreen 0 -popupwindow'
    return ([wmiclass]"Win32_Process").Create($cmd, $dir, $null).ProcessId
}
function WaitFor([scriptblock]$cond, [int]$seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) { Start-Sleep 2; if (& $cond) { return $true } }
    return $false
}

Get-Process Firewatch -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 4
foreach ($g in @($game, $game2)) {
    Copy-Item (Join-Path $w "build\StoryCoop-patched.dll") (Join-Path $g "BepInEx\plugins\StoryCoop.dll") -Force
    Copy-Item (Join-Path $w "build\CoopTestDriver.dll") (Join-Path $g "BepInEx\plugins\CoopTestDriver.dll") -Force
    Remove-Item (Join-Path $g "BepInEx\LogOutput.log") -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $g "BepInEx\cooptest_*.json") -ErrorAction SilentlyContinue
}

$hostPid = Launch $game
"host pid $hostPid"
$ok = WaitFor { $s = State $game "host"; $s -and $s.scene -eq "Main_Menu" } 90
if (-not $ok) { "RESULT: FAIL (host never reached the main menu)"; exit 1 }
Cmd $game "debugstart"
$ok = WaitFor { $s = State $game "host"; $s -and $s.scene -ne "Main_Menu" -and $s.player } 60
Check $ok "host in level" ""
Cmd $game "hideui SaveLoadCanvas"
Start-Sleep 2

$clientPid = Launch $game2
"client pid $clientPid"
$ok = WaitFor { $s = State $game2 "client"; $s -and $s.synced -eq $true } 90
Check $ok "client synced" ""
if (-not $ok) { "RESULT: FAIL"; exit 1 }

$h = State $game "host"
$c = State $game2 "client"
$dist = 0.0
$offsetOk = WaitFor {
    $c2 = State $game2 "client"; $h2 = State $game "host"
    if (-not $c2 -or -not $c2.pos -or -not $h2.pos) { return $false }
    $script:dist = [math]::Sqrt([math]::Pow(($c2.pos[0] - $h2.pos[0]), 2) + [math]::Pow(($c2.pos[2] - $h2.pos[2]), 2))
    return ($script:dist -gt 1.0)
} 20
$dist = $script:dist
Check $offsetOk "spawn offset" ("distance={0:N2}m" -f $dist)
Check ([single]$c.day -eq [single]$h.day) "day in sync" ("host=$($h.day) client=$($c.day)")

# world interaction: lamp FSM must change on the client
Cmd $game2 "fsms DeskLamp"
Start-Sleep 3
$before = (Select-String -Path (Join-Path $game2 "BepInEx\LogOutput.log") -Pattern "fsm DeskLamp \[DeskLampFSM\] state=(\S+)" | Select-Object -Last 1)
$beforeState = if ($before) { $before.Matches[0].Groups[1].Value } else { "?" }
Cmd $game "interact DeskLamp OnUsed"
Start-Sleep 5
Cmd $game2 "fsms DeskLamp"
Start-Sleep 4
$after = (Select-String -Path (Join-Path $game2 "BepInEx\LogOutput.log") -Pattern "fsm DeskLamp \[DeskLampFSM\] state=(\S+)" | Select-Object -Last 1)
$afterState = if ($after) { $after.Matches[0].Groups[1].Value } else { "?" }
$delivered = (Select-String -Path (Join-Path $game2 "BepInEx\LogOutput.log") -Pattern "Interact in:.*DeskLamp" -Quiet) -eq $true
$notLoaded = (Select-String -Path (Join-Path $game2 "BepInEx\LogOutput.log") -Pattern "object not loaded.*DeskLamp" -Quiet) -eq $true
Check $delivered "interaction delivered" "id resolved on client"
Check (-not $notLoaded) "no unresolved ids" ""
Check ($beforeState -ne $afterState) "world state changed" "DeskLamp $beforeState -> $afterState"

# fact sync
Cmd $game "fact QUEST_Day3_GetBoards 1"
Start-Sleep 2
Cmd $game2 "factget QUEST_Day3_GetBoards"
Start-Sleep 3
$factLine = Select-String -Path (Join-Path $game2 "BepInEx\LogOutput.log") -Pattern "fact QUEST_Day3_GetBoards = (.*)" | Select-Object -Last 1
$factValue = if ($factLine) { $factLine.Matches[0].Groups[1].Value } else { "?" }
Check ($factValue -match ":1$") "fact sync" $factValue

# authority: client must not broadcast facts
$sent = (Select-String -Path (Join-Path $game2 "BepInEx\LogOutput.log") -Pattern "Facts out:" | Measure-Object).Count
Check ($sent -eq 0) "host-authoritative facts" "client sent $sent"

Cmd $game2 "shot"
Start-Sleep 3

"---"
if ($fails.Count -eq 0) { "RESULT: PASS" } else { "RESULT: FAIL (" + ($fails -join ", ") + ")" }


