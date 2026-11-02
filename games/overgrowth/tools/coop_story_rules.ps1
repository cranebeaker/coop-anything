# Story co-op rules scenarios for Overgrowth Online on one PC, driven through the test bridge.
# One session, five checks, each printed as TEST <name>: PASS/FAIL; RESULT: PASS only when all pass.
# The first four run in the story's first level (no enemies near the start); the kill-zone check runs
# last in Garden Duel, the only story level whose kill zone kills players, with its enemies knocked out
# first (an enemy there otherwise cuts the host down within a minute).
#
#   dialogue    the guest is held out of the host's opening dialogue, cannot move during it, and is
#               placed beside the host when it ends; the guest saw the dialogues the host started
#   drag_revive the guest is knocked out; the host stands at the body and holds grab for 3 s
#               (coop_revive.as drag); the guest gets up on both machines
#   checkpoint  the guest is knocked out; the host reaches the level's next checkpoint: the guest gets up
#               beside the host (level script IncrementProgress -> engine CoopCheckpoint)
#   all_down    both players knocked out: the level starts over for both within a few seconds
#   zone_kill   the guest is put into a kill zone: the guest goes down on both machines, the host is
#               unharmed, and the guest is stood up beside the host a few seconds later and stays up
#
# Usage: coop_story_rules.ps1 [-Exe <exe>] [-WorkingDir <repo>] [-WriteDirName write-same] [-Only dialogue,zone_kill,...]
param(
    [string]$Lab = $(if ($env:OG_LAB) { $env:OG_LAB } else { Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path "lab" }),
    [string]$Exe = "", [string]$WorkingDir = "", [string]$WriteDirName = "write-same",
    [string]$Campaign = "com-wolfire-overgrowth-campaign",
    [string]$Level1 = "og_story/01_Slaver_Camp_Landing.xml",
    [double[]]$Checkpoint0 = @(45.1, 7.3, 6.9),        # level 1 checkpoint_id 0 (2.2 x 1.7 x 2.6 box)
    [double[]]$Checkpoint1 = @(13.9, 16.6, -20.7),     # level 1 checkpoint_id 1 (16 x 1.4 x 12 box)
    [string]$KillLevel = "og_story/25_Garden_Duel.xml",
    [double[]]$KillZone = @(-136.6, -22.3, -190.2),    # generic_kill id 146 in Garden Duel, KillPlayer=1, 4.9 m box
    [string[]]$Only = @(),
    [switch]$KeepRunning
)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "bridge.ps1")
. (Join-Path $PSScriptRoot "coop_common.ps1")

$pair = New-Pair -Lab $Lab -Exe $Exe -WorkingDir $WorkingDir -WriteDirName $WriteDirName
$results = [ordered]@{}
function Want([string]$n) { return ($Only.Count -eq 0 -or $Only -contains $n) }
function Pass([string]$n, [string]$note) { $results[$n] = "PASS $note"; Say $pair "TEST ${n}: PASS $note" }
function Fail([string]$n, [string]$why) { $results[$n] = "FAIL $why"; Say $pair "TEST ${n}: FAIL $why" }
function Vec($p) { "({0:F1},{1:F1},{2:F1})" -f $p[0], $p[1], $p[2] }
function Teleport($side, [int]$id, [double[]]$to) {
    Invoke-Script $pair $side ("MovementObject@ mo = ReadCharacterID({0}); mo.position = vec3({1}, {2}, {3}); mo.velocity = vec3(0.0f);" -f $id, $to[0], $to[1], $to[2]) | Out-Null
}
function KnockOut($side, [int]$id) { Invoke-Script $pair $side "TakeBloodDamage(1.0f); Ragdoll(_RGDL_FALL);" $id | Out-Null }
# Every non-player character on the host goes down (the host owns them): a safe arena for the tests
function Pacify-NPCs() {
    Invoke-Script $pair "host" 'for(int i = 0; i < GetNumCharacters(); ++i){ MovementObject@ c = ReadCharacter(i); if(!c.is_player && !c.controlled){ c.Execute("TakeBloodDamage(1.0f); Ragdoll(_RGDL_FALL);"); } }' | Out-Null
}
# Knocked-out value of the guest as each side sees it (and the host's own); $null when a view is missing
function Vitals() {
    $hs = Get-GameState $pair.HostBridge; $cs = Get-GameState $pair.ClientBridge
    $cOwn = $cs.characters | Where-Object { $_.id -eq $cs.my_avatar } | Select-Object -First 1
    $hOwn = $hs.characters | Where-Object { $_.id -eq $hs.my_avatar } | Select-Object -First 1
    $hSeesC = if ($cOwn) { $hs.characters | Where-Object { $_.common_id -eq $cOwn.common_id } | Select-Object -First 1 } else { $null }
    return @{ClientOwn = $cOwn; HostOwn = $hOwn; HostSeesClient = $hSeesC}
}
function Wait-Vitals([scriptblock]$Check, [int]$TimeoutS, [string]$What) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutS) {
        try { $v = Vitals; if (& $Check $v) { return $v } } catch { }
        Start-Sleep -Milliseconds 700
    }
    throw "timeout after ${TimeoutS}s waiting for $What [$(Describe-Avatars $pair)]"
}
function Ensure-BothUp([string]$ctx) {
    $v = Vitals
    if ($v.ClientOwn -and $v.HostOwn -and $v.HostOwn.knocked_out -eq 0 -and $v.ClientOwn.knocked_out -ne 0 -and $v.HostSeesClient) {
        # Recovery between checks, not a check: the host stands the guest up through the script's ReviveDo
        Say $pair "recovery before ${ctx}: the guest is down, reviving through ReviveDo"
        $b = @([double]$v.HostSeesClient.pos[0], [double]$v.HostSeesClient.pos[1], [double]$v.HostSeesClient.pos[2])
        Teleport "host" $v.HostOwn.id @(($b[0] + 0.9), ($b[1] + 0.3), $b[2])
        Start-Sleep -Seconds 1
        Invoke-Script $pair "host" ("ReviveDo({0}, {1});" -f $v.HostOwn.id, $v.HostSeesClient.id) | Out-Null
        try { Wait-Vitals { param($x) $x.ClientOwn.knocked_out -eq 0 -and $x.HostSeesClient.knocked_out -eq 0 } 12 "recovery revive" | Out-Null } catch { }
        Start-Sleep -Seconds 2
        $v = Vitals
    }
    if (-not $v.ClientOwn -or -not $v.HostOwn -or $v.ClientOwn.knocked_out -ne 0 -or $v.HostOwn.knocked_out -ne 0) {
        throw "precondition for ${ctx}: both players must be up (host ko=$($v.HostOwn.knocked_out), guest ko=$($v.ClientOwn.knocked_out))"
    }
}
# A new "every player is down" reset since $since would make a revive look like it worked
function Reset-Count() { return @(Get-LogLines $pair "host" "every player is down, the level starts over" 50).Count }

try {
    Start-Session $pair $Campaign $Level1

    # ---------------- dialogue ----------------
    if (Want "dialogue") {
        try {
            # The opening dialogue runs now. The guest's character is hidden or absent from the scene.
            $a0 = Get-Avatars $pair
            $hiddenAtStart = ($null -eq $a0) -or (-not $a0.ClientOwn.visible)
            $cs = Get-GameState $pair.ClientBridge
            $guestBefore = $cs.characters | Where-Object { $_.id -eq $cs.my_avatar } | Select-Object -First 1
            $hostViewBefore = if ($guestBefore) { (Get-GameState $pair.HostBridge).characters | Where-Object { $_.common_id -eq $guestBefore.common_id } | Select-Object -First 1 } else { $null }
            # Push forward on the guest during the dialogue
            Send-Bridge -Port $pair.ClientBridge -Request @{cmd = "input"; hold = @{up = 1.0}; steps = 240} | Out-Null
            Start-Sleep -Seconds 3
            $cs2 = Get-GameState $pair.ClientBridge
            $guestAfter = $cs2.characters | Where-Object { $_.id -eq $cs2.my_avatar } | Select-Object -First 1
            $hostViewAfter = if ($guestAfter) { (Get-GameState $pair.HostBridge).characters | Where-Object { $_.common_id -eq $guestAfter.common_id } | Select-Object -First 1 } else { $null }
            $stillHidden = (-not (Get-Avatars $pair)) -or (-not (Get-Avatars $pair).ClientOwn.visible)
            $moved = if ($guestBefore -and $guestAfter) { Get-Distance $guestBefore.pos $guestAfter.pos } else { 0.0 }
            $movedOnHost = if ($hostViewBefore -and $hostViewAfter) { Get-Distance $hostViewBefore.pos $hostViewAfter.pos } else { -1.0 }
            Say $pair "during the dialogue: guest moved $([math]::Round($moved,2)) m on its machine, $([math]::Round($movedOnHost,2)) m as the host sees it (visible on guest: $($guestAfter.visible), on host: $($hostViewAfter.visible)); still hidden: $stillHidden"
            # Now let the host skip through it
            $a = Wait-BothInScene $pair 120
            $hostDialogues = @((Send-Bridge -Port $pair.HostBridge -Request @{cmd = "level_messages"; prefix = "start_dialogue"}).messages)
            $clientSaw = @(Get-LogLines $pair "client" "received start dialogue:" 20 | ForEach-Object { ($_ -replace '.*received start dialogue: ', '').Trim() })
            $hostNames = @($hostDialogues | ForEach-Object { if ($_ -match 'start_dialogue\s+"?([^"]+)"?') { $Matches[1].Trim() } })
            $gap = Get-Distance $a.HostOwn.pos $a.HostSeesClient.pos
            $notes = "hidden at start=$hiddenAtStart, guest moved $([math]::Round($moved,2)) m while hidden in the dialogue (on both machines), after it $([math]::Round($gap,1)) m from the host; host started [$($hostNames -join ', ')], guest received [$($clientSaw -join ', ')]"
            if (-not $hiddenAtStart) { throw "the guest was visible in the host's dialogue scene ($notes)" }
            if ($gap -gt 8.0) { throw "the guest was not placed beside the host after the dialogue ($notes)" }
            if ($clientSaw.Count -lt 1) { throw "the guest never received a dialogue start ($notes)" }
            if ($moved -gt 1.0) { throw "the guest can move while hidden during the host's dialogue ($notes)" }
            Pass "dialogue" $notes
        } catch { Fail "dialogue" $_.Exception.Message }
    }

    # ---------------- drag_revive ----------------
    if (Want "drag_revive") {
        try {
            $a = Wait-BothInScene $pair 60
            Ensure-BothUp "drag_revive"
            $resets = Reset-Count
            KnockOut "client" $a.ClientOwn.id
            $v = Wait-Vitals { param($v) $v.ClientOwn.knocked_out -ne 0 -and $v.HostSeesClient -and $v.HostSeesClient.knocked_out -ne 0 } 15 "the guest down on both machines"
            Start-Sleep -Seconds 2
            $bv = Vitals
            $body = @([double]$bv.HostSeesClient.pos[0], [double]$bv.HostSeesClient.pos[1], [double]$bv.HostSeesClient.pos[2])
            # Right up to the body: aschar.as CheckForStartBodyDrag needs a limb within about 0.55 m
            # (_body_part_drag_dist 0.2 * 1.5, squared) and holds grab (playercontrol.as WantsToDragBody)
            Teleport "host" $a.HostOwn.id @(($body[0] + 0.35), ($body[1] + 0.1), $body[2])  # (parenthesised: the comma binds tighter than +)
            Start-Sleep -Seconds 1
            Invoke-Script $pair "host" ("this_mo.SetRotationFromFacing(normalize(vec3({0}, {1}, {2}) - this_mo.position));" -f $body[0], $body[1], $body[2]) $a.HostOwn.id | Out-Null
            Send-Bridge -Port $pair.HostBridge -Request @{cmd = "input"; hold = @{grab = 1.0}; steps = 900} | Out-Null
            Start-Sleep -Seconds 2
            $tetherLog = @(Get-LogLines $pair "host" "\[coop_revive\] me .* -> [0-9]+ after" 1)
            Say $pair "host after 2 s of grab: $($tetherLog -join ' | ')"
            $path = "drag"
            try {
                $v2 = Wait-Vitals { param($v) $v.ClientOwn.knocked_out -eq 0 -and $v.HostSeesClient.knocked_out -eq 0 } 14 "the guest revived by the drag"
            } catch {
                Send-Bridge -Port $pair.HostBridge -Request @{cmd = "input"; release_all = $true} | Out-Null
                Say $pair "drag did not revive within 14 s; trying the script's ReviveDo"
                $path = "ReviveDo"
                $hv = Vitals
                Invoke-Script $pair "host" ("ReviveDo({0}, {1});" -f $hv.HostOwn.id, $hv.HostSeesClient.id) | Out-Null
                $v2 = Wait-Vitals { param($v) $v.ClientOwn.knocked_out -eq 0 -and $v.HostSeesClient.knocked_out -eq 0 } 15 "the guest revived by ReviveDo"
            }
            Send-Bridge -Port $pair.HostBridge -Request @{cmd = "input"; release_all = $true} | Out-Null
            if ((Reset-Count) -gt $resets) { throw "the level started over during the drag (the host went down?), so the revive cannot be credited to the drag" }
            $revLog = @(Get-LogLines $pair "client" "coop_revived|coop_revive #" 2)
            if ($path -eq "ReviveDo") { Fail "drag_revive" "the input drag did not revive the guest; the script path ReviveDo did (guest up on both machines). Tether: $($tetherLog -join ' | ')" }
            else { Pass "drag_revive" "guest up on both machines after the host's 3 s drag; tether: $($tetherLog -join ' | ')" }
        } catch { Fail "drag_revive" $_.Exception.Message }
    }

    # ---------------- checkpoint ----------------
    if (Want "checkpoint") {
        try {
            $a = Wait-BothInScene $pair 60
            Ensure-BothUp "checkpoint"
            $resets = Reset-Count
            KnockOut "client" $a.ClientOwn.id
            Wait-Vitals { param($v) $v.ClientOwn.knocked_out -ne 0 -and $v.HostSeesClient -and $v.HostSeesClient.knocked_out -ne 0 } 15 "the guest down on both machines" | Out-Null
            # The host reaches a checkpoint (the first one, then the next if the first did not advance progress) and stays there
            Teleport "host" $a.HostOwn.id $Checkpoint0
            $v2 = $null
            try { $v2 = Wait-Vitals { param($v) $v.ClientOwn.knocked_out -eq 0 -and $v.HostSeesClient.knocked_out -eq 0 } 12 "the guest stood up by the first checkpoint" } catch { }
            if (-not $v2) {
                Say $pair "no revive from the first checkpoint, the host goes on to the next"
                Teleport "host" $a.HostOwn.id $Checkpoint1
                $v2 = Wait-Vitals { param($v) $v.ClientOwn.knocked_out -eq 0 -and $v.HostSeesClient.knocked_out -eq 0 } 20 "the guest stood up by the checkpoint"
            }
            if ((Reset-Count) -gt $resets) { throw "the level started over during the checkpoint check, so the revive cannot be credited to the checkpoint" }
            $a2 = Wait-Avatars $pair { param($a) $a.ClientOwn.visible } 15 "the guest back in the scene"
            $gap = Get-Distance $a2.HostOwn.pos $a2.HostSeesClient.pos
            $cpLog = @(Get-LogLines $pair "host" "checkpoint, player character|IncrementProgress" 3)
            if ($gap -gt 10.0) { throw "the guest got up $([math]::Round($gap,1)) m from the host; log: $($cpLog -join ' | ')" }
            Pass "checkpoint" "guest up $([math]::Round($gap,1)) m from the host after the host's checkpoint; log: $($cpLog -join ' | ')"
        } catch { Fail "checkpoint" $_.Exception.Message }
    }

    # ---------------- all_down ----------------
    if (Want "all_down") {
        try {
            $a = Wait-BothInScene $pair 60
            Ensure-BothUp "all_down"
            $resets = Reset-Count
            KnockOut "client" $a.ClientOwn.id
            KnockOut "host" $a.HostOwn.id
            Wait-Vitals { param($v) $v.ClientOwn.knocked_out -ne 0 -and $v.HostOwn.knocked_out -ne 0 } 15 "both down" | Out-Null
            Say $pair "both down; waiting for the level to start over"
            $t = [Diagnostics.Stopwatch]::StartNew()
            while ($t.Elapsed.TotalSeconds -lt 25 -and (Reset-Count) -le $resets) { Start-Sleep -Seconds 1 }
            if ((Reset-Count) -le $resets) { throw "no 'every player is down' reset in the host log within 25 s" }
            $clientReset = @(Get-LogLines $pair "client" 'Level script received "reset"' 1)
            Say $pair "reset after $([int]$t.Elapsed.TotalSeconds)s; the level's opening dialogue plays again"
            $a2 = Wait-BothInScene $pair 120   # (the opening dialogue again: skipped by the host)
            if ($a2.ClientOwn.knocked_out -ne 0 -or $a2.HostOwn.knocked_out -ne 0) { throw "a player is still down after the reset" }
            $gap = Get-Distance $a2.HostOwn.pos $a2.HostSeesClient.pos
            Pass "all_down" "level restarted for both $([int]$t.Elapsed.TotalSeconds)s after both went down (client got the reset too: $($clientReset.Count -gt 0)); both up again $([math]::Round($gap,1)) m apart"
        } catch { Fail "all_down" $_.Exception.Message }
    }

    # ---------------- zone_kill (last: Garden Duel) ----------------
    if (Want "zone_kill") {
        try {
            Send-Bridge -Port $pair.HostBridge -Request @{cmd = "load_level"; path = $KillLevel} | Out-Null
            Wait-Status $pair.HostBridge { param($s) $s.level_loaded -and $s.level -match [regex]::Escape((Split-Path $KillLevel -Leaf)) } 180 "host in the kill level" | Out-Null
            Wait-Status $pair.ClientBridge { param($s) $s.level_loaded -and $s.level -match [regex]::Escape((Split-Path $KillLevel -Leaf)) -and $s.online.client } 240 "client in the kill level" | Out-Null
            $hs = Get-Status $pair.HostBridge
            if ($hs.waiting_for_input) { Send-Bridge -Port $pair.HostBridge -Request @{cmd = "start_session"} | Out-Null }
            $a = Wait-BothInScene $pair 150
            Pacify-NPCs
            Say $pair "both in $KillLevel (enemies knocked out); guest at $(Vec $a.ClientOwn.pos), host at $(Vec $a.HostOwn.pos)"
            Ensure-BothUp "zone_kill"
            $resets = Reset-Count
            $hostHpBefore = $a.HostOwn.temp_health
            $deathsBefore = @(Get-LogLines $pair "client" "died where nobody can reach it" 50).Count
            Teleport "client" $a.ClientOwn.id $KillZone
            $v = Wait-Vitals { param($v) $v.ClientOwn -and $v.ClientOwn.knocked_out -ne 0 } 15 "the guest to go down in the kill zone"
            Say $pair "guest down on its machine (ko=$($v.ClientOwn.knocked_out)); host own ko=$($v.HostOwn.knocked_out) hp=$($v.HostOwn.temp_health)"
            $v2 = Wait-Vitals { param($v) $v.HostSeesClient -and $v.HostSeesClient.knocked_out -ne 0 } 10 "the host to see the guest down"
            if ($v2.HostOwn.knocked_out -ne 0) { throw "the host went down when the guest hit the kill zone" }
            if ([math]::Abs($v2.HostOwn.temp_health - $hostHpBefore) -gt 0.05) { throw "the host's health changed when the guest hit the kill zone ($hostHpBefore -> $($v2.HostOwn.temp_health))" }
            $v3 = Wait-Vitals { param($v) $v.ClientOwn -and $v.ClientOwn.knocked_out -eq 0 -and $v.HostSeesClient -and $v.HostSeesClient.knocked_out -eq 0 } 25 "the guest to be stood up again"
            Start-Sleep -Seconds 6   # a respawn that drops the guest through the world shows within a second or two
            $deathsAfter = @(Get-LogLines $pair "client" "died where nobody can reach it" 50).Count
            $vFinal = Vitals
            $logs = @(Get-LogLines $pair "client" "died where nobody can reach|back up beside" 4)
            if ($deathsAfter - $deathsBefore -gt 1 -or $vFinal.ClientOwn.knocked_out -ne 0) { throw "the guest died again after the respawn ($($deathsAfter - $deathsBefore) deaths, ko now $($vFinal.ClientOwn.knocked_out)): it was stood up somewhere it fell from; log: $($logs -join ' | ')" }
            if ((Reset-Count) -gt $resets) { throw "the level started over during the zone check" }
            $a2 = Wait-Avatars $pair { param($a) $a.ClientOwn.visible } 20 "the guest back in the scene"
            $gap = Get-Distance $a2.HostOwn.pos $a2.HostSeesClient.pos
            if ($gap -gt 10.0) { throw "the guest got up $([math]::Round($gap,1)) m from the host (expected beside it); log: $($logs -join ' | ')" }
            Pass "zone_kill" "guest down on both machines, host unharmed (ko=$($v2.HostOwn.knocked_out), hp unchanged), guest up again $([math]::Round($gap,1)) m from the host and still up 6 s later; log: $($logs -join ' | ')"
        } catch { Fail "zone_kill" $_.Exception.Message }
    }
} catch {
    Say $pair "SESSION FAILED: $($_.Exception.Message)"
    foreach ($n in "host", "client") { Get-LogLines $pair $n "online\.cpp|gns_net|Online:|\[e\]|closed" 10 | ForEach-Object { Say $pair "  $n | $_" } }
} finally {
    if (-not $KeepRunning) { Stop-Pair $pair }
}
Write-Host ""
foreach ($k in $results.Keys) { Write-Host ("TEST {0,-12} {1}" -f $k, $results[$k]) }
$failed = @($results.Values | Where-Object { $_ -like "FAIL*" }).Count
$ran = $results.Count
Write-Host "RESULT: $(if ($failed -eq 0 -and $ran -gt 0) { 'PASS' } else { 'FAIL' }) ($($ran - $failed) of $ran passed)"
if ($failed -eq 0 -and $ran -gt 0) { exit 0 } else { exit 1 }
