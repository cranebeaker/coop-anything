# Level-change scenario for Overgrowth Online on one PC: host + client in story level 1,
# the host loads level 2, the client must follow, and both players must be back in the
# scene afterwards. Prints RESULT: PASS or RESULT: FAIL.
param(
    [string]$Lab = $(if ($env:OG_LAB) { $env:OG_LAB } else { Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path "lab" }),  # <project root>\lab next to the clone, or $env:OG_LAB
    [string]$Level1 = "og_story/01_Slaver_Camp_Landing.xml",
    [string]$Level2 = "og_story/02_Slaver_Camp.xml",
    [string]$Campaign = "com-wolfire-overgrowth-campaign",
    [int]$HostBridge = 47001,
    [int]$ClientBridge = 47002,
    [int]$GamePort = 27960,
    [switch]$KeepRunning
)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "bridge.ps1")
$result = "FAIL"; $reason = ""; $started = @(); $t0 = Get-Date
function Elapsed { "{0:F0}s" -f ((Get-Date) - $t0).TotalSeconds }
function Say($m) { Write-Host ("[{0}] {1}" -f (Elapsed), $m) }
function Start-Copy([string]$Name, [int]$Bridge) {
    $dir = Join-Path $Lab $Name
    $exe = Join-Path $dir "game\OvergrowthOnline\OvergrowthOnline.exe"
    $log = Join-Path $dir "write\logfile.txt"
    if (Test-Path $log) { Remove-Item $log -Force }
    $p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru -WindowStyle Minimized `
        -ArgumentList @("--write-dir", "`"$dir\write`"", "--test-bridge", "$Bridge", "--clear-log")
    Say "$Name started, pid $($p.Id)"; return $p
}
# Skip dialogue from the host until the client's own avatar is controlled locally (cutscenes hide guests)
function Wait-BothInScene([int]$TimeoutS = 120) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutS) {
        Send-Bridge -Port $HostBridge -Request @{cmd = "input"; hold = @{skip_dialogue = 1.0}; steps = 10} | Out-Null
        Start-Sleep -Seconds 3
        $cs = Get-GameState $ClientBridge; $hs = Get-GameState $HostBridge
        $cOwn = @($cs.characters | Where-Object { $_.id -eq $cs.my_avatar -and $_.controlled -and -not $_.remote })
        $hOwn = @($hs.characters | Where-Object { $_.id -eq $hs.my_avatar -and $_.controlled -and -not $_.remote })
        # (the other machine's view of an avatar: by common_id, local ids differ between machines)
        $hSees = @(if ($cOwn.Count -eq 1) { $hs.characters | Where-Object { $_.common_id -eq $cOwn[0].common_id -and $_.remote } })
        $cSees = @(if ($hOwn.Count -eq 1) { $cs.characters | Where-Object { $_.common_id -eq $hOwn[0].common_id -and $_.remote } })
        if ($cOwn.Count -eq 1 -and $hOwn.Count -eq 1 -and $hSees.Count -eq 1 -and $cSees.Count -eq 1 -and $hOwn[0].common_id -ne $cOwn[0].common_id) {
            return @{host = $hs; client = $cs}
        }
    }
    throw "both players not in the scene within ${TimeoutS}s (host avatar $($hs.my_avatar), client avatar $($cs.my_avatar))"
}
try {
    foreach ($b in $HostBridge, $ClientBridge) {
        if (Get-NetTCPConnection -LocalPort $b -State Listen -ErrorAction SilentlyContinue) { throw "bridge port $b is already in use" }
    }
    $started += Start-Copy "host" $HostBridge
    $started += Start-Copy "client" $ClientBridge
    Wait-Status $HostBridge { param($s) $s.ok } 90 "host bridge" | Out-Null
    Wait-Status $ClientBridge { param($s) $s.ok } 90 "client bridge" | Out-Null

    Send-Bridge -Port $HostBridge -Request @{cmd = "host"; port = $GamePort; campaign = $Campaign; level = $Level1} | Out-Null
    Wait-Status $HostBridge { param($s) $s.level_loaded -and $s.online.hosting } 180 "host level 1" | Out-Null
    Send-Bridge -Port $ClientBridge -Request @{cmd = "connect"; ip = "127.0.0.1"; port = $GamePort} | Out-Null
    Wait-Status $ClientBridge { param($s) $s.level_loaded -and $s.online.client } 180 "client in level 1" | Out-Null
    Send-Bridge -Port $HostBridge -Request @{cmd = "start_session"} | Out-Null
    Wait-Status $ClientBridge { param($s) -not $s.waiting_for_input -and $s.online.players.Count -ge 2 } 60 "session started" | Out-Null
    $in1 = Wait-BothInScene
    Say "level 1: both in scene (host avatar $($in1.host.my_avatar), client avatar $($in1.client.my_avatar))"

    # The host changes level, as the campaign script would at the level's end
    Say "host loads level 2: $Level2"
    Send-Bridge -Port $HostBridge -Request @{cmd = "load_level"; path = $Level2} | Out-Null
    $hs = Wait-Status $HostBridge { param($s) $s.level_loaded -and $s.level -match [regex]::Escape((Split-Path $Level2 -Leaf)) } 180 "host in level 2"
    Say "host in '$($hs.level)' hosting=$($hs.online.hosting) waiting_for_input=$($hs.waiting_for_input) peers=$($hs.online.peers.Count)"
    $cs = Wait-Status $ClientBridge { param($s) $s.level_loaded -and $s.level -match [regex]::Escape((Split-Path $Level2 -Leaf)) -and $s.online.client } 240 "client followed to level 2"
    Say "client in '$($cs.level)' client=$($cs.online.client) waiting_for_input=$($cs.waiting_for_input)"
    if ($hs.waiting_for_input) {
        $ss = Send-Bridge -Port $HostBridge -Request @{cmd = "start_session"}
        Say "host start_session after the change: started=$($ss.started)"
    }
    $in2 = Wait-BothInScene 150
    Say "level 2: both in scene (host avatar $($in2.host.my_avatar), client avatar $($in2.client.my_avatar))"
    $hs2 = Get-Status $HostBridge; $cs2 = Get-Status $ClientBridge
    Say "after the change: host peers=$($hs2.online.peers.Count) players=$($hs2.online.players.Count); client=$($cs2.online.client) players=$($cs2.online.players.Count)"
    if ($hs2.online.peers.Count -lt 1 -or $hs2.online.players.Count -lt 2) { throw "host lost its peer across the level change" }
    if (-not $cs2.online.client -or $cs2.online.players.Count -lt 2) { throw "client is no longer in the session after the level change" }
    if ($in2.host.my_avatar -eq $in1.host.my_avatar -and $in2.client.my_avatar -eq $in1.client.my_avatar -and $hs.level -eq $in1.host.level) { throw "nothing changed" }
    $result = "PASS"
} catch {
    $reason = $_.Exception.Message; Say "FAILED: $reason"
    foreach ($n in "host", "client") {
        $log = Join-Path $Lab "$n\write\logfile.txt"
        if (Test-Path $log) {
            Say "--- $n log, last online lines ---"
            Get-Content $log | Select-String -Pattern "online\.cpp|gns_net|Online:|\[e\]|\[f\]|closed|Loading level|level_name" | Select-Object -Last 14 -ExpandProperty Line | ForEach-Object { Say "  $_" }
        }
    }
} finally {
    if (-not $KeepRunning) {
        foreach ($b in $HostBridge, $ClientBridge) { try { Send-Bridge -Port $b -Request @{cmd = "quit"} -TimeoutMs 3000 | Out-Null } catch {} }
        Start-Sleep -Seconds 3
        foreach ($p in $started) { if (-not $p.HasExited) { try { Stop-Process -Id $p.Id -Force } catch {} } }
    }
}
Write-Host "RESULT: $result $reason"
if ($result -eq "PASS") { exit 0 } else { exit 1 }
