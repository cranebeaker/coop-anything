# Two-client co-op smoke test for Overgrowth Online (fork build) on one PC.
# Starts the host and client test copies (made by make_test_copy.ps1 + the fork's installer),
# hosts a story level, joins by IP, starts the session, and checks that both machines see
# both players' characters. Prints RESULT: PASS or RESULT: FAIL and exits 0 or 1.
#
# Usage: coop_smoke.ps1 [-Lab <lab root>] [-Level og_story/01_Slaver_Camp_Landing.xml]
#        [-Campaign com-wolfire-overgrowth-campaign] [-KeepRunning]
param(
    [string]$Lab = $(if ($env:OG_LAB) { $env:OG_LAB } else { Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path "lab" }),  # <project root>\lab next to the clone, or $env:OG_LAB
    [string]$Level = "og_story/01_Slaver_Camp_Landing.xml",
    [string]$Campaign = "com-wolfire-overgrowth-campaign",
    [int]$HostBridge = 47001,
    [int]$ClientBridge = 47002,
    [int]$GamePort = 27960,
    [string]$Exe = "",            # engine exe to run instead of the installed one (a fresh build: <repo>\Build\Release\OvergrowthOnline.exe)
    [string]$WorkingDir = "",     # passed as --working-dir (a source checkout: its Data overlay is used, as the fork's own harness does)
    [string]$WriteDirName = "write",  # which <lab>\<copy>\<name> write dir to use
    [switch]$KeepRunning
)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "bridge.ps1")

$result = "FAIL"
$reason = ""
$started = @()
$t0 = Get-Date
function Elapsed { "{0:F0}s" -f ((Get-Date) - $t0).TotalSeconds }
function Say($m) { Write-Host ("[{0}] {1}" -f (Elapsed), $m) }

function Start-Copy([string]$Name, [int]$Bridge) {
    $dir = Join-Path $Lab $Name
    $exe = if ($Exe) { $Exe } else { Join-Path $dir "game\OvergrowthOnline\OvergrowthOnline.exe" }
    $write = Join-Path $dir $WriteDirName
    $log = Join-Path $write "logfile.txt"
    if (Test-Path $log) { Remove-Item $log -Force }
    $argv = @("--write-dir", "`"$write`"", "--test-bridge", "$Bridge", "--clear-log")
    if ($WorkingDir) { $argv += @("--working-dir", "`"$($WorkingDir.TrimEnd('\'))/`"", "--game-data", "`"$dir\game`"") }
    $p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru -WindowStyle Minimized -ArgumentList $argv
    Say "$Name started, pid $($p.Id) ($exe)"
    return $p
}

try {
    foreach ($b in $HostBridge, $ClientBridge) {
        if (Get-NetTCPConnection -LocalPort $b -State Listen -ErrorAction SilentlyContinue) { throw "bridge port $b is already in use: a copy is still running" }
    }
    $hostProc = Start-Copy "host" $HostBridge; $started += $hostProc
    $clientProc = Start-Copy "client" $ClientBridge; $started += $clientProc
    Wait-Status $HostBridge { param($s) $s.ok } 90 "host bridge up" | Out-Null
    Wait-Status $ClientBridge { param($s) $s.ok } 90 "client bridge up" | Out-Null
    Say "both bridges answer"

    Send-Bridge -Port $HostBridge -Request @{cmd = "host"; port = $GamePort; campaign = $Campaign; level = $Level} | Out-Null
    $hs = Wait-Status $HostBridge { param($s) $s.level_loaded -and $s.online.hosting } 180 "host level loaded"
    Say "host loaded '$($hs.level)', waiting_for_input=$($hs.waiting_for_input)"

    $cn = Send-Bridge -Port $ClientBridge -Request @{cmd = "connect"; ip = "127.0.0.1"; port = $GamePort}
    Say "client connect: active=$($cn.active)"
    $cs = Wait-Status $ClientBridge { param($s) $s.level_loaded -and $s.online.client } 180 "client joined and loaded the level"
    Say "client loaded '$($cs.level)' as client, peers=$($cs.online.peers.Count)"

    $ss = Send-Bridge -Port $HostBridge -Request @{cmd = "start_session"}
    Say "host start_session: started=$($ss.started)"
    $hs = Wait-Status $HostBridge { param($s) -not $s.waiting_for_input -and $s.online.players.Count -ge 2 } 60 "host sees 2 players"
    $cs = Wait-Status $ClientBridge { param($s) -not $s.waiting_for_input -and $s.online.players.Count -ge 2 -and $s.my_avatar -ge 0 } 60 "client sees 2 players and owns an avatar"
    Say "players on host: $(($hs.online.players | ForEach-Object { $_.name + '#' + $_.avatar_id }) -join ', ')"
    Say "players on client: $(($cs.online.players | ForEach-Object { $_.name + '#' + $_.avatar_id }) -join ', ')"

    # Story levels open with a cutscene; the fork hides guests' characters until it ends
    # (online.cpp CoopCutsceneUpdate). Skip the dialogue from the host and wait for the
    # client's own character to be in its scene, controlled and not remote.
    # Note: a guest's own avatar carries is_player=false on the guest's machine (observed
    # 2026-10-25), so ownership is judged by controlled && !remote, not by is_player.
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $cstate = $null
    while ($sw.Elapsed.TotalSeconds -lt 120) {
        Send-Bridge -Port $HostBridge -Request @{cmd = "input"; hold = @{skip_dialogue = 1.0}; steps = 10} | Out-Null
        Start-Sleep -Seconds 3
        $cstate = Get-GameState $ClientBridge
        $own = @($cstate.characters | Where-Object { $_.id -eq $cstate.my_avatar -and $_.controlled -and -not $_.remote })
        if ($own.Count -eq 1) { break }
    }
    Say "client's own character present after $([int]$sw.Elapsed.TotalSeconds)s of skipping dialogue"
    $hstate = Get-GameState $HostBridge
    Say "host characters:";  Format-Chars $hstate | Where-Object { $_ -match "player=True|remote=True" } | ForEach-Object { Say "  $_" }
    Say "client characters:"; Format-Chars $cstate | Where-Object { $_ -match "player=True|remote=True|proxy=False" } | ForEach-Object { Say "  $_" }

    # Assertions: each side controls exactly one non-remote character (its own avatar) and sees the other's as remote.
    # Local object ids differ between machines (builds after 0.40.0), so the other machine's view of an avatar is
    # found through the bridge's common_id (the host's id, the same everywhere).
    if ($hstate.my_avatar -lt 0 -or $cstate.my_avatar -lt 0) { throw "an avatar id is missing (host $($hstate.my_avatar), client $($cstate.my_avatar))" }
    $hOwn = @($hstate.characters | Where-Object { $_.id -eq $hstate.my_avatar -and $_.controlled -and -not $_.remote })
    $cOwn = @($cstate.characters | Where-Object { $_.id -eq $cstate.my_avatar -and $_.controlled -and -not $_.remote })
    if ($hOwn.Count -ne 1) { throw "host does not control its own avatar $($hstate.my_avatar) locally" }
    if ($cOwn.Count -ne 1) { throw "client does not control its own avatar $($cstate.my_avatar) locally (cutscene still on?)" }
    $hCommon = $hOwn[0].common_id; $cCommon = $cOwn[0].common_id
    if ($hCommon -eq $cCommon) { throw "both sides claim the same avatar (common id $hCommon)" }
    $hOther = @($hstate.characters | Where-Object { $_.common_id -eq $cCommon -and $_.remote })
    $cOther = @($cstate.characters | Where-Object { $_.common_id -eq $hCommon -and $_.remote })
    if ($hOther.Count -ne 1) { throw "host does not see the client's avatar (common id $cCommon) as remote" }
    if ($cOther.Count -ne 1) { throw "client does not see the host's avatar (common id $hCommon) as remote" }

    # Movement sync: the client walks forward for 2 s; the host must see that avatar move
    $before = $hOther[0].pos
    Send-Bridge -Port $ClientBridge -Request @{cmd = "input"; hold = @{up = 1.0}; steps = 240} | Out-Null
    Start-Sleep -Seconds 4
    $hstate2 = Get-GameState $HostBridge
    $cstate2 = Get-GameState $ClientBridge
    $afterH = ($hstate2.characters | Where-Object { $_.common_id -eq $cCommon }).pos
    $afterC = ($cstate2.characters | Where-Object { $_.common_id -eq $cCommon }).pos
    $movedH = [math]::Sqrt(($afterH[0]-$before[0])*($afterH[0]-$before[0]) + ($afterH[2]-$before[2])*($afterH[2]-$before[2]))
    $gap = [math]::Sqrt(($afterH[0]-$afterC[0])*($afterH[0]-$afterC[0]) + ($afterH[1]-$afterC[1])*($afterH[1]-$afterC[1]) + ($afterH[2]-$afterC[2])*($afterH[2]-$afterC[2]))
    Say ("client avatar moved {0:F2} m as seen by the host; host/client position gap {1:F2} m" -f $movedH, $gap)
    if ($movedH -lt 0.5) { throw "the host did not see the client's avatar move (moved $movedH m)" }
    if ($gap -gt 2.0) { throw "host and client disagree on the client's position by $gap m" }
    $pingMs = ($cs.online.peers | ForEach-Object { $_.ping_ms }) -join ","
    Say "client ping to host: $pingMs ms; host bytes_out=$($hs.online.bytes_out) client bytes_in=$($cs.online.bytes_in)"
    $result = "PASS"
} catch {
    $reason = $_.Exception.Message
    Say "FAILED: $reason"
    foreach ($n in "host", "client") {
        $log = Join-Path $Lab "$n\$WriteDirName\logfile.txt"
        if (Test-Path $log) {
            Say "--- $n log, last online/error lines ---"
            Get-Content $log | Select-String -Pattern "online\.cpp|gns_net|Online:|\[e\]|\[f\]|closed" | Select-Object -Last 12 -ExpandProperty Line | ForEach-Object { Say "  $_" }
        }
    }
} finally {
    if (-not $KeepRunning) {
        foreach ($b in $HostBridge, $ClientBridge) { try { Send-Bridge -Port $b -Request @{cmd = "quit"} -TimeoutMs 3000 | Out-Null } catch {} }
        Start-Sleep -Seconds 3
        foreach ($p in $started) { if (-not $p.HasExited) { try { Stop-Process -Id $p.Id -Force } catch {} } }  # only processes this run started
    }
}
Write-Host "RESULT: $result $reason"
if ($result -eq "PASS") { exit 0 } else { exit 1 }
