#
# kotor-coop two-client automated QA suite.
#
# Starts a host and a client engine on one PC (both headless), connects them over
# localhost UDP, then drives co-op state and actions through each engine's TCP
# control port and asserts the shared state on both sides.
#
# One gate per test; the scorer below prints RESULT: PASS/FAIL.
# Rerun after every merge.  Usage:
#   powershell -ExecutionPolicy Bypass -File harness.ps1
#
param(
    [string]$GamePath = "$PSScriptRoot\..\game-copy\swkotor",
    [string]$BinDir   = "$PSScriptRoot\..\build\bin",
    [int]$HostPort    = 51920,
    [int]$ClientPort  = 51921,
    [int]$GamePort    = 27125
)

$ErrorActionPreference = 'Stop'
$script:results = New-Object 'System.Collections.Generic.List[object]'
function Check($name, $pass, $detail) {
    $tag = if ($pass) { 'PASS' } else { 'FAIL' }
    $script:results.Add([pscustomobject]@{ name=$name; pass=[bool]$pass; detail="$detail" })
    Write-Host "[$tag] $name :: $detail"
}

function New-Ctrl($port) {
    $c = New-Object System.Net.Sockets.TcpClient
    $c.Connect('127.0.0.1', $port)
    $s = $c.GetStream(); $s.ReadTimeout = 120000
    $w = New-Object System.IO.StreamWriter($s); $w.AutoFlush = $true
    $r = New-Object System.IO.StreamReader($s)
    [pscustomobject]@{ Client=$c; Stream=$s; Writer=$w; Reader=$r }
}
function Ctrl-Send($ctrl, $cmd) {
    $ctrl.Writer.WriteLine($cmd)
    try { return ($ctrl.Reader.ReadLine() | ConvertFrom-Json) }
    catch { return [pscustomobject]@{ ok=$false; output=@("READ-TIMEOUT: $($_.Exception.Message)") } }
}
function Ctrl-WaitPort($port, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        try { $c = New-Object System.Net.Sockets.TcpClient; $c.Connect('127.0.0.1',$port); $c.Close(); return $true }
        catch { Start-Sleep -Milliseconds 400 }
    }
    return $false
}
function Start-Engine($extraArgs, $tag, $workDir) {
    $base = @('--game', "`"$GamePath`"", '--hud', 'k1', '--headless', '1',
              '--logsev', '3', '--run-unfocused', '1')
    $exe = if ($workDir) { Join-Path $workDir 'engine.exe' } else { Join-Path $BinDir 'engine.exe' }
    $wd  = if ($workDir) { $workDir } else { $BinDir }
    $p = Start-Process -FilePath $exe -ArgumentList ($base + $extraArgs) `
        -WorkingDirectory $wd -PassThru
    Write-Host "$tag engine started, PID $($p.Id)"
    return $p
}
# Drive the host until it is back in normal gameplay: out of the intro movie
# (which holds the simulation) and out of the opening conversation (which makes
# guest commands answer "busy"). Does exactly what a player's clicks do.
#
# The host passes through in_game briefly BEFORE the intro movie and dialogue
# start, so a single check is not enough: require the state to hold (and ticks
# to advance) across several checks before calling it in game.
function Wait-InGame($ctrl, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $stable = 0
    $lastTick = -1
    while ((Get-Date) -lt $deadline) {
        # The movie holds the sim entirely, so skip it first; ticks do not run
        # while it is on screen.
        $null = Ctrl-Send $ctrl 'skipmovie'
        $null = Ctrl-Send $ctrl 'autoskipenable 1'
        $null = Ctrl-Send $ctrl 'autoskipentries 1 1 1 1 1 1 1 1 1 1 1 1'
        $null = Ctrl-Send $ctrl 'autoskipreplies 1 1 1 1 1 1 1 1 1 1 1 1'
        $r = Ctrl-Send $ctrl 'dumpstate'
        $ok = $false
        try {
            $j = ($r.output -join '') | ConvertFrom-Json
            $ok = $j.screen -eq 'in_game' -and -not $j.movie -and $null -eq $j.conversation -and $j.tick -gt $lastTick
            $lastTick = $j.tick
        } catch {}
        if ($ok) {
            ++$stable
            if ($stable -ge 3) {
                $null = Ctrl-Send $ctrl 'autoskipenable 0'
                return $true
            }
        } else {
            $stable = 0
        }
        Start-Sleep -Milliseconds 900
    }
    $null = Ctrl-Send $ctrl 'autoskipenable 0'
    return $false
}

# Poll for a condition in a control reply (rule: poll with a timeout, do not check once).
function Wait-For($ctrl, $cmd, [scriptblock]$predicate, $timeoutSec, $intervalMs = 500) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $last = $null
    while ((Get-Date) -lt $deadline) {
        $last = Ctrl-Send $ctrl $cmd
        if ( & $predicate $last ) { return @{ hit=$true; reply=$last } }
        Start-Sleep -Milliseconds $intervalMs
    }
    return @{ hit=$false; reply=$last }
}

# --- launch --------------------------------------------------------------------
# Note: --host on the command line means the host engine auto-hosts; do NOT also
# send 'coophost' (it answers "a co-op session is already running").
# Run each engine from its own directory so their engine.log files do not clash
# (they hardlink the same bin contents, so the binaries stay current).
$hostDir   = Join-Path (Split-Path $BinDir -Parent) 'runhost'
$clientDir = Join-Path (Split-Path $BinDir -Parent) 'runclient'
$hostEngine   = Start-Engine @('--control-port', "$HostPort",   '--host', "$GamePort", '--coop-name', 'HostPlayer') 'HOST'   $hostDir
$clientEngine = Start-Engine @('--control-port', "$ClientPort", '--coop-name', 'GuestPlayer')                      'CLIENT' $clientDir

$hostUp   = Ctrl-WaitPort $HostPort   120; Check 'host control port opens'   $hostUp   "port $HostPort"
$clientUp = Ctrl-WaitPort $ClientPort 120; Check 'client control port opens' $clientUp "port $ClientPort"
if (-not ($hostUp -and $clientUp)) {
    foreach ($e in @($hostEngine,$clientEngine)) { if ($e -and -not $e.HasExited) { $e.Kill() } }
    Write-Host 'RESULT: FAIL (control ports never opened)'; exit 1
}

$h = New-Ctrl $HostPort
$c = New-Ctrl $ClientPort

# --- gate 1: host is hosting and playing ---------------------------------------
$r = Ctrl-Send $h 'newgame 0'; Check 'G1 host newgame ok' ($r.ok) "$($r.output -join ';')"
Start-Sleep -Seconds 5
# newgame starts the intro movie, which HOLDS the simulation, then the opening
# conversation with Trask, which makes the host answer "busy" to guest commands.
# A player's clicks clear both; Wait-InGame does the same over the harness.
$inGame = Wait-InGame $h 60
Check 'G1 host reached normal gameplay' $inGame "screen=in_game, no movie, no conversation"
$running = Wait-For $h 'dumpstate' { param($x) try { $j=(($x.output -join '') | ConvertFrom-Json); $j.tick -gt 20 } catch { $false } } 30
Check 'G1 host simulation running (tick>20)' $running.hit "tick=$((($running.reply.output -join '') | ConvertFrom-Json).tick)"
# A fresh game starts with only the player in the party, so give the host a
# companion for the guest to take (the harness way of the tutorial where Trask
# joins after his dialogue). spawncompanion adds it to the party.
$r = Ctrl-Send $h 'spawncompanion end_trask 1'
Check 'G1 host companion spawned' ($r.ok) "$($r.output -join ';')"
$r = Ctrl-Send $h 'netstatus'
$hostRole = $false
try { $hostRole = (($r.output -join '') | ConvertFrom-Json).role -eq 'host' } catch {}
Check 'G1 host role=host' $hostRole "ok=$($r.ok)"
$hn = ($r.output -join '') | ConvertFrom-Json
Check 'G1 host detected its module (generation=1)' ($hn.generation -ge 1) "generation=$($hn.generation) module=$($hn.module)"

# --- gate 2: client joins, both sides see 2 players ----------------------------
$r = Ctrl-Send $c "coopjoin 127.0.0.1:$GamePort"; Check 'G2 client coopjoin accepted' ($r.ok) "$($r.output -join ';')"
$joined = Wait-For $c 'netstatus' { param($x) try { (($x.output -join '') | ConvertFrom-Json).state -eq 'joined' } catch { $false } } 60
Check 'G2 client state=joined' $joined.hit (($joined.reply.output -join '') -replace '.*"state":"([^"]*)".*','$1')

# --- gate 3: host counts 2 players, no drops -----------------------------------
$twoPlayers = Wait-For $h 'netstatus' { param($x) try { (($x.output -join '') | ConvertFrom-Json).players -eq 2 } catch { $false } } 30
Check 'G3 host sees players=2' $twoPlayers.hit "players=$((($twoPlayers.reply.output -join '') | ConvertFrom-Json).players)"

$net = ($twoPlayers.reply.output -join '') | ConvertFrom-Json
$dropped = $false
try { $dropped = [double]([regex]::Match($net.clients[0].route,'Dropped\s*:\s*([0-9.]+)\s*pkts').Groups[1].Value) -gt 0 } catch {}
Check 'G3 host reports 0 dropped pkts' (-not $dropped) "client route Dropped=0"

# --- gate 4: K2 absent yet the session works (K1-only route) -------------------
Check 'G4 host fingerprints k2=absent' ($net.k2 -eq 'absent') "k2=$($net.k2)"
Check 'G4 host fingerprints k1 present' ($net.k1 -match 'key=') "$($net.k1)"

# --- gate 5: guest is assigned a companion slot --------------------------------
# In Story Mode the host auto-assigns a free companion. A companion was spawned
# in gate 1, so the guest should get it without an explicit pick. The client's
# assigned slot is reported in netstatus as slot.status / slot.id / slot.tag.
$assigned = Wait-For $c 'netstatus' { param($x) try { $j=(($x.output -join '') | ConvertFrom-Json); $j.slot.status -eq 'assigned' } catch { $false } } 60
$cj = ($assigned.reply.output -join '') | ConvertFrom-Json
if (-not $assigned.hit) {
    $pick = Ctrl-Send $c 'cooppick end_trask'
    Write-Host "cooppick fallback: ok=$($pick.ok) $($pick.output -join ';')"
    $assigned = Wait-For $c 'netstatus' { param($x) try { $j=(($x.output -join '') | ConvertFrom-Json); $j.slot.status -eq 'assigned' } catch { $false } } 30
    $cj = ($assigned.reply.output -join '') | ConvertFrom-Json
}
Check 'G5 guest bound to a companion' ($cj.slot.status -eq 'assigned' -and $cj.slot.id -gt 0) "slotStatus=$($cj.slot.status) slotTag=$($cj.slot.tag) slotId=$($cj.slot.id) bound=$($cj.bound) replicas=$($cj.replicas)"

# --- gate 6: the client loads the host's module (snapshots flow) ---------------
# This is the real "co-op is working" gate: the client receives ModuleInfo,
# loads the module locally, and applies the host's snapshots.
$clientLoaded = Wait-For $c 'netstatus' { param($x) try { $j=(($x.output -join '') | ConvertFrom-Json); $j.loaded -and $j.replicas -gt 0 } catch { $false } } 60
$cj2 = ($clientLoaded.reply.output -join '') | ConvertFrom-Json
Check 'G6 client loaded host module' ($cj2.loaded -and $cj2.module -ne '') "module=$($cj2.module) loaded=$($cj2.loaded) appliedTick=$($cj2.appliedTick)"
Check 'G6 client replicates world objects' ($cj2.replicas -gt 0) "replicas=$($cj2.replicas) spawned=$($cj2.spawned) bound=$($cj2.bound)"

# --- gate 7: client-driven movement replicates back to the host ----------------
# Read the controlled companion's position on both seats, move it from the
# client, and check the host accepted it and the replica follows the host.
function Get-CoopObj($ctrl) {
    $r = Ctrl-Send $ctrl 'coopdump'
    try { return (($r.output -join '') | ConvertFrom-Json) } catch { return $null }
}
$cd0 = Get-CoopObj $c
$obj0 = $cd0.objects | Where-Object { $_.id -eq $cd0.controlled } | Select-Object -First 1
# A scripted trigger can pull the host back into a conversation at any time and
# the host only accepts guest commands in normal gameplay. Re-assert gameplay and
# retry the move until the host accepts it (or the deadline passes).
$accepted = $false
$move = $null
$deadline = (Get-Date).AddSeconds(60)
$attempt = 0
while ((Get-Date) -lt $deadline) {
    ++$attempt
    $cd0 = Get-CoopObj $c
    $obj0 = $cd0.objects | Where-Object { $_.id -eq $cd0.controlled } | Select-Object -First 1
    if (-not $obj0) { Start-Sleep -Seconds 2; continue }
    # Move 3 units from where the companion stands (a nearby reachable point).
    $tx = [double]$obj0.pos[0] + 3.0; $ty = [double]$obj0.pos[1]
    $move = Ctrl-Send $c "coopmove $tx $ty 0"
    Start-Sleep -Seconds 2
    $accepted = ((Ctrl-Send $c 'netstatus').output -join '') -match '"accepted":true'
    if ($accepted) { break }
    Write-Host "    attempt $attempt busy; waiting for normal gameplay"
    $null = Wait-InGame $h 30
}
Check 'G7 client coopmove sent' ($move.ok) "attempts=$attempt $($move.output -join ';')"
Check 'G7 host accepted the client command' $accepted ($(if($accepted){'accepted=true'}else{((Ctrl-Send $c 'netstatus').output -join '') -replace '.*("lastResult":\{[^}]*\}).*','$1'}))
Start-Sleep -Seconds 3
$cs = (Get-CoopObj $c)
$moved = $false; $agrees = $false
$a = $cd0.objects | Where-Object { $_.id -eq $cs.controlled } | Select-Object -First 1
$b = $cs.objects  | Where-Object { $_.id -eq $cs.controlled } | Select-Object -First 1
if ($a -and $b) {
    $d = [math]::Sqrt([math]::Pow($b.pos[0]-$a.pos[0],2) + [math]::Pow($b.pos[1]-$a.pos[1],2))
    $moved = $d -gt 0.5
    # The client's own copy (localPos) must track the host's authoritative pos.
    $diff = [math]::Sqrt([math]::Pow($b.pos[0]-$b.localPos[0],2) + [math]::Pow($b.pos[1]-$b.localPos[1],2))
    $agrees = $diff -lt 0.5
    Write-Host ("    controlled id {0}: ({1:N2},{2:N2}) -> ({3:N2},{4:N2}) d={5:N2}; localVsHost={6:N3}" -f $cs.controlled, $a.pos[0], $a.pos[1], $b.pos[0], $b.pos[1], $d, $diff)
}
Check 'G7 client sees its companion move' $moved "controlled=$($cs.controlled) from=($($a.pos[0] -as [int]),$($a.pos[1] -as [int]))"
Check 'G7 client replica agrees with host state' $agrees "pos==localPos within 0.5"

# --- shared helpers for the gameplay gates ------------------------------------
function Get-DumpState($ctrl, $radius = 200) {
    $r = Ctrl-Send $ctrl "dumpstate $radius"
    try { return (($r.output -join '') | ConvertFrom-Json) } catch { return $null }
}
function Get-NetStatus($ctrl) {
    $r = Ctrl-Send $ctrl 'netstatus'
    try { return (($r.output -join '') | ConvertFrom-Json) } catch { return $null }
}
# Poll the client netstatus for a field, returning the last reply either way.
function Wait-ClientNet($predicate, $timeoutSec, $intervalMs = 500) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $last = $null
    while ((Get-Date) -lt $deadline) {
        $last = Get-NetStatus $c
        if (& $predicate $last) { return @{ hit=$true; ns=$last } }
        Start-Sleep -Milliseconds $intervalMs
    }
    return @{ hit=$false; ns=$last }
}
# The host only accepts guest commands in normal gameplay: a scripted trigger or
# the opening dialogue makes it answer "busy". Re-assert gameplay before acting.
function Wait-Gameplay($timeoutSec = 30) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $cj = Get-DumpState $h
        if ($cj -and $cj.screen -eq 'in_game' -and -not $cj.movie -and $null -eq $cj.conversation) { return $true }
        $null = Ctrl-Send $h 'skipmovie'
        $null = Ctrl-Send $h 'autoskipenable 1'
        $null = Ctrl-Send $h 'autoskipentries 1 1 1 1 1 1 1 1 1 1 1 1'
        $null = Ctrl-Send $h 'autoskipreplies 1 1 1 1 1 1 1 1 1 1 1 1'
        Start-Sleep -Milliseconds 700
    }
    return $false
}
# The client's own copy of a replicated object, by the host id the coopdump lists.
function Get-ClientReplica($ctrlObj, $hostId) {
    if (-not $ctrlObj) { return $null }
    return $ctrlObj.objects | Where-Object { $_.id -eq $hostId } | Select-Object -First 1
}

# --- gate 9: dialogue replication (the host leads, the guest sees the scene) ---
# Story Mode: the host owns conversations. The client shows a replica of the
# host's dialogue, and its commands answer "busy" while the host is talking.
$null = Wait-Gameplay 30
$dlg = $null
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $deadline) {
    $hj = Get-DumpState $h
    if ($hj -and $hj.conversation) { $dlg = $hj.conversation; break }
    Start-Sleep -Milliseconds 500
}
Check 'G9 host is in a conversation' ($null -ne $dlg) "dialog=$(if($dlg){$dlg.dialog}else{'none'})"
if ($dlg) {
    $seen = Wait-ClientNet { param($n) $n.conversationReplica -eq $dlg.dialog } 30
    Check 'G9 client replicates the host dialogue' $seen.hit "conversationReplica=$($seen.ns.conversationReplica) hostDialog=$($dlg.dialog)"
    $cdj = Get-DumpState $c
    $sameText = $cdj.conversation -and $cdj.conversation.text -eq $dlg.text
    Check 'G9 client shows the same entry text' $sameText "entry=$($cdj.conversation.entry) speaker=$($cdj.conversation.speaker)"
    $null = Ctrl-Send $c 'coopmove 20 20 0'
    Start-Sleep -Seconds 2
    $hr = (Get-NetStatus $h).clients[0]
    $busy = $false
    try { $busy = [int]$hr.rejects.busy -ge 1 } catch {}
    Check 'G9 guest commands are busy during dialogue' $busy "host rejects=$($hr.rejects | ConvertTo-Json -Compress)"
}
# Clear the dialogue so the following gates act in normal gameplay.
$null = Wait-Gameplay 60
Start-Sleep -Seconds 1

# --- gate 10: the shared party wallet -----------------------------------------
# The host owns the party gold; every guest sees it in the shared inventory.
$r = Ctrl-Send $h 'givegold 1234'
Check 'G10 host givegold ok' ($r.ok) "$($r.output -join ';')"
$wallet = Wait-ClientNet { param($n) $n.inventory.credits -ge 1234 } 30
$hn = Get-NetStatus $h
Check 'G10 client sees the shared wallet' $wallet.hit "client credits=$($wallet.ns.inventory.credits) host partyGold=$($hn.partyGold)"
Check 'G10 both sides agree on the wallet' ($wallet.ns.inventory.credits -eq $hn.partyGold -and $hn.partyGold -ge 1234) "client=$($wallet.ns.inventory.credits) host=$($hn.partyGold)"

# --- gate 11: containers (the guest opens, views and loots) --------------------
# The guest clicks a footlocker: the companion walks to it and the client opens
# its own container screen. Taking sends the request to the host, who moves the
# real items into the shared inventory.
$null = Wait-Gameplay 30
$locker = (Get-DumpState $h).nearby | Where-Object { $_.tag -eq 'end_locker01' } | Select-Object -First 1
Check 'G11 footlocker end_locker01 is present' ($null -ne $locker) "id=$(if($locker){$locker.id}else{'none'}) itemCount=$(if($locker){$locker.itemCount}else{'0'})"
$r = Ctrl-Send $c 'coopuse end_locker01'
Check 'G11 client coopuse sent' ($r.ok) "$($r.output -join ';')"
$opened = $false; $view = $null
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $deadline) {
    $view = Get-NetStatus $c
    if ($view.viewing -and $view.viewing -ne 0) { $opened = $true; break }
    Start-Sleep -Milliseconds 500
}
Check 'G11 client container screen opened' $opened "viewing=$($view.viewing)"
$countBefore = 0
if ($opened) { $countBefore = @($view.containerItems).Count }
Check 'G11 client sees the container contents' ($countBefore -gt 0) "items=$countBefore [$((($view.containerItems | ForEach-Object { $_.tag }) -join ','))]"
$invBefore = [int]$view.inventory.items
# The host's reject counters are cumulative (G9 leaves a "busy" from its own
# test), so the take must be judged against what the host had before it.
$acceptedBefore = [int](Get-NetStatus $h).clients[0].commandsAccepted
$rejectsBefore = (Get-NetStatus $h).clients[0].rejects
$r = Ctrl-Send $c 'cooptake all'
Check 'G11 client cooptake sent' ($r.ok) "$($r.output -join ';')"
$looted = Wait-ClientNet { param($n) @($n.containerItems).Count -eq 0 -and $n.inventory.items -gt $invBefore } 30 700
$after = $looted.ns
Check 'G11 looted items reach the shared inventory' $looted.hit "containerItems=$(@($after.containerItems).Count) inventory=$($after.inventory.items) (was $invBefore)"
$hr = (Get-NetStatus $h).clients[0]
$newRejects = $false
foreach ($prop in $hr.rejects.PSObject.Properties) {
    $was = 0
    if ($rejectsBefore.PSObject.Properties.Name -contains $prop.Name) { $was = [int]$rejectsBefore.$($prop.Name) }
    if ([int]$prop.Value -gt $was) { $newRejects = $true }
}
$acceptedTake = ($hr.commandsAccepted -gt $acceptedBefore) -and (-not $newRejects)
Check 'G11 host accepted the take with no new rejects' $acceptedTake "accepted $acceptedBefore -> $($hr.commandsAccepted) rejects=$($hr.rejects | ConvertTo-Json -Compress)"
$null = Ctrl-Send $c 'coopclose'
Start-Sleep -Milliseconds 500

# --- gate 12: level-up replication (the guest levels its companion) ------------
# A companion may only level up to the host's own level, so raise the host player
# first, then make the guest's companion need a level. The guest levels it up
# through the same screens its HUD button opens; the host applies the change.
$null = Wait-Gameplay 30
$hj = Get-DumpState $h
$hostTrask = $hj.party | Where-Object { $_.tag -eq 'end_trask' } | Select-Object -First 1
$null = Ctrl-Send $h 'selectobjectbyid 2'
$r = Ctrl-Send $h 'givexp 600000'
Check 'G12 host player given experience' ($r.ok) "$($r.output -join ';')"
for ($i = 0; $i -lt 12; $i++) { if (-not (Ctrl-Send $h 'levelup').ok) { break } }
$hj = Get-DumpState $h
$hostPlayer = $hj.party | Where-Object { $_.id -eq 2 } | Select-Object -First 1
Check 'G12 host player raised above the companion' ($hostPlayer.level -gt $hostTrask.level) "playerLevel=$($hostPlayer.level) companionLevel=$($hostTrask.level)"
$null = Ctrl-Send $h ("selectobjectbyid " + $hostTrask.id)
$r = Ctrl-Send $h 'givexp 600000'
Check 'G12 companion given experience' ($r.ok) "$($r.output -join ';')"
$pending = Wait-ClientNet { param($n) $n.sheet.valid -and $n.sheet.levelUp } 30
$sheet = $pending.ns.sheet
Check 'G12 client sees the companion may level up' $pending.hit "sheet.creature=$($sheet.creature) level=$($sheet.level) levelUp=$($sheet.levelUp) xp=$($sheet.xp)"
$before = [int]$sheet.level
$levelRejectsBefore = (Get-NetStatus $h).clients[0].rejects
$r = Ctrl-Send $c 'cooplevelup'
Check 'G12 client cooplevelup sent' ($r.ok) "$($r.output -join ';')"
$leveled = Wait-ClientNet { param($n) $n.sheet.level -gt $before } 30
Check 'G12 guest level-up reached the host' $leveled.hit "level $before -> $($leveled.ns.sheet.level)"
$hr = (Get-NetStatus $h).clients[0]
$newLevelRejects = $false
foreach ($prop in $hr.rejects.PSObject.Properties) {
    $was = 0
    if ($levelRejectsBefore.PSObject.Properties.Name -contains $prop.Name) { $was = [int]$levelRejectsBefore.$($prop.Name) }
    if ([int]$prop.Value -gt $was) { $newLevelRejects = $true }
}
Check 'G12 host applied the level-up with no new rejects' ((-not $newLevelRejects) -and ($leveled.hit)) "accepted=$($hr.commandsAccepted) rejects=$($hr.rejects | ConvertTo-Json -Compress)"

# --- gate 13: combat (the guest attacks a hostile creature) --------------------
# Spawn a creature, stand it on the guest's companion, make it hostile, then have
# the guest attack it. The host must start a combat round for the companion.
$null = Wait-Gameplay 30
$cd = Get-CoopObj $c
$ctrlId = $cd.controlled
$comp = Get-ClientReplica $cd $ctrlId
$px = $comp.pos[0]; $py = $comp.pos[1]
$null = Ctrl-Send $h 'spawncreature crsghost10'
Start-Sleep -Seconds 2
$hj = Get-DumpState $h 60
$ghost = $hj.nearby | Where-Object { $_.tag -eq 'crossroads_ghost' } | Select-Object -First 1
Check 'G13 hostile creature spawned' ($null -ne $ghost) "id=$(if($ghost){$ghost.id}else{'none'})"
if ($ghost) {
    $null = Ctrl-Send $h ("selectobjectbyid " + $ghost.id)
    $null = Ctrl-Send $h ("setposition $px $py 0")
    $null = Ctrl-Send $h 'setfaction 1'
    Start-Sleep -Seconds 2
    $hj = Get-DumpState $h 60
    $ghost = $hj.nearby | Where-Object { $_.id -eq $ghost.id } | Select-Object -First 1
    Check 'G13 creature is hostile and in reach' ($ghost.hostile -and $ghost.dist -lt 6) "hostile=$($ghost.hostile) dist=$([math]::Round($ghost.dist,2))"
    $cd = Get-CoopObj $c
    $cg = Get-ClientReplica $cd $ghost.id
    Check 'G13 client replicates the creature' ($null -ne $cg) "hostId=$($ghost.id) clientHp=$(if($cg){$cg.hp}else{'none'})"
    $r = Ctrl-Send $c ("coopattack " + $ghost.id)
    Check 'G13 client coopattack sent' ($r.ok) "$($r.output -join ';')"
    # The host's combat rounds must contain the companion attacking this creature.
    $engaged = $false
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $hj = Get-DumpState $h 60
        foreach ($round in $hj.combat) {
            foreach ($act in $round.actions) {
                if ($act.attacker -eq $ctrlId -and $act.target -eq $ghost.id) { $engaged = $true }
            }
        }
        if ($engaged) { break }
        Start-Sleep -Milliseconds 500
    }
    Check 'G13 host started the combat round' $engaged "attacker=$ctrlId target=$($ghost.id)"
    $actor = $hj.party | Where-Object { $_.id -eq $ctrlId } | Select-Object -First 1
    Check 'G13 companion is in combat with the target' ($actor.inCombat -and $actor.attackTarget -eq $ghost.id) "inCombat=$($actor.inCombat) target=$($actor.attackTarget)"
}

# --- gate 14: area transition (the host warps, the guest follows) --------------
# A real module load: the host warps to the second Endar Spire deck. The client
# must receive the new ModuleInfo, load the module and rebuild its world.
$null = Wait-Gameplay 30
$cjBefore = Get-NetStatus $c
$r = Ctrl-Send $h 'warp end_m01ab'
Check 'G14 host warp sent' ($r.ok) "$($r.output -join ';')"
$followed = Wait-ClientNet { param($n) $n.module -eq 'end_m01ab' -and $n.replicas -gt 0 } 60 700
$cjAfter = $followed.ns
Check 'G14 client followed to the new module' $followed.hit "from=$($cjBefore.module) to=$($cjAfter.module) loaded=$($cjAfter.loaded)"
Check 'G14 client generation advanced' ($cjAfter.generation -gt $cjBefore.generation) "generation $($cjBefore.generation) -> $($cjAfter.generation)"
Check 'G14 client rebuilt the new module world' ($cjAfter.replicas -gt 0) "replicas=$($cjAfter.replicas) bound=$($cjAfter.bound) spawned=$($cjAfter.spawned)"
$hjAfter = Get-NetStatus $h
Check 'G14 both sides agree on the module' ($hjAfter.module -eq $cjAfter.module) "host=$($hjAfter.module) client=$($cjAfter.module)"

# --- gate 16: pause propagates both ways ---------------------------------------
# Anyone may pause the shared world. The guest asks; the host owns the pause and
# applies it to both seats, naming who asked for it.
$null = Wait-Gameplay 30
$hn = Get-NetStatus $h; $cn = Get-NetStatus $c
Check 'G16 neither side starts paused' ((-not $hn.paused) -and (-not $cn.paused)) "host=$($hn.paused) client=$($cn.paused)"
$r = Ctrl-Send $c 'cooppause 1'
Check 'G16 client cooppause sent' ($r.ok) "$($r.output -join ';')"
$paused = Wait-ClientNet { param($n) $n.paused -and $n.pausedBy -eq 'GuestPlayer' } 30
Check 'G16 client sees the world paused by the guest' $paused.hit "client paused=$($paused.ns.paused) by=$($paused.ns.pausedBy)"
$hp = Get-NetStatus $h
Check 'G16 host applied the guest pause and names the asker' ($hp.paused -and $hp.pausedBy -eq 'GuestPlayer') "host paused=$($hp.paused) by=$($hp.pausedBy)"
$r = Ctrl-Send $c 'cooppause 0'
Check 'G16 client cooppause resume sent' ($r.ok) "$($r.output -join ';')"
$resumed = Wait-ClientNet { param($n) -not $n.paused } 30
Check 'G16 both sides resumed' ($resumed.hit -and -not (Get-NetStatus $h).paused) "client paused=$($resumed.ns.paused) host paused=$((Get-NetStatus $h).paused)"

# --- gate 17: the guest opens a door (in reach) --------------------------------
# A door is replicated: the client's copy stands where the host's does. Clicking
# it walks the companion there and sends the open request, and the host opens the
# real door. The companion must be within reach of the door, so walk it onto the
# door first (as a player walking up and clicking would).
$null = Wait-Gameplay 30
$null = Ctrl-Send $h 'warp tar_m02ab'
$followedDoor = Wait-ClientNet { param($n) $n.module -eq 'tar_m02ab' -and $n.replicas -gt 0 } 60 700
Check 'G17 client reached the door module' $followedDoor.hit "module=$($followedDoor.ns.module)"
$hj = Get-DumpState $h 400
$door = $hj.nearby | Where-Object { $_.tag -eq 'droidshop' -and -not $_.locked } | Select-Object -First 1
Check 'G17 an unlocked door droidshop is present' ($null -ne $door) "id=$(if($door){$door.id}else{'none'}) locked=$(if($door){$door.locked}else{'?'})"
if ($door -and -not $door.open) {
    $cd = Get-CoopObj $c
    $ctrlDoor = $cd.controlled
    # Stand the companion on the door so the open is in reach, then wait for it to arrive.
    $null = Ctrl-Send $c ("coopmove {0} {1} 1" -f $door.pos[0], $door.pos[1])
    $arrived = $false
    $deadline = (Get-Date).AddSeconds(40)
    while ((Get-Date) -lt $deadline) {
        $cd = Get-CoopObj $c
        $me = $cd.objects | Where-Object { $_.id -eq $cd.controlled } | Select-Object -First 1
        if ($me) {
            $dd = [math]::Sqrt([math]::Pow($me.localPos[0]-$door.pos[0],2) + [math]::Pow($me.localPos[1]-$door.pos[1],2))
            if ($dd -lt 2.5) { $arrived = $true; break }
        }
        Start-Sleep -Milliseconds 500
    }
    Check 'G17 guest companion reached the door' $arrived "reachDist<2.5"
    $acceptedBefore = [int](Get-NetStatus $h).clients[0].commandsAccepted
    $r = Ctrl-Send $c ("coopdoor " + $door.id)
    Check 'G17 client coopdoor sent' ($r.ok) "$($r.output -join ';')"
    # The host opens the real door, and the client's replica must show it open.
    $openedHost = Wait-For $h "dumpstate 400" { param($x) try { $o = ((($x.output -join '') | ConvertFrom-Json).nearby | Where-Object { $_.id -eq $door.id } | Select-Object -First 1); $o.open } catch { $false } } 30
    Check 'G17 host opened the real door' $openedHost.hit "doorId=$($door.id)"
    $cOpened = $false
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $cd = Get-CoopObj $c
        $cdoor = Get-ClientReplica $cd $door.id
        if ($cdoor -and $cdoor.open) { $cOpened = $true; break }
        Start-Sleep -Milliseconds 500
    }
    Check 'G17 client replica shows the door open' $cOpened "hostId=$($door.id)"
    $hr = (Get-NetStatus $h).clients[0]
    Check 'G17 host accepted the open' ($hr.commandsAccepted -gt $acceptedBefore) "accepted $acceptedBefore -> $($hr.commandsAccepted)"
}

# --- gate 18: area-exit ready check (the guest walks into an exit) -------------
# A guest companion that walks into a module-linked exit trigger asks everyone to
# leave. The host starts a ready check naming the destination and the initiator;
# both answer, and both seats move to the new module.
$null = Wait-Gameplay 30
$cjBefore = Get-NetStatus $c
$null = Ctrl-Send $h 'warp tar_m03aa'
$followedExit = Wait-ClientNet { param($n) $n.module -eq 'tar_m03aa' -and $n.replicas -gt 0 } 60 700
Check 'G18 client reached the exit module' $followedExit.hit "module=$($followedExit.ns.module)"
$hj = Get-DumpState $h 600
$trig = $hj.nearby | Where-Object { $_.tag -eq 'tar03_npcext' } | Select-Object -First 1
Check 'G18 exit trigger tar03_npcext is present' ($null -ne $trig) "id=$(if($trig){$trig.id}else{'none'})"
if ($trig) {
    # Walk the companion into the trigger and wait for the ready check. A scripted
    # conversation can pull the host out of gameplay mid-walk, so re-assert it and
    # retry the walk a few times.
    $fired = $false
    for ($attempt = 0; $attempt -lt 4 -and -not $fired; $attempt++) {
        $null = Ctrl-Send $c ("coopmove {0} {1} 1" -f $trig.pos[0], $trig.pos[1])
        $deadline = (Get-Date).AddSeconds(30)
        while ((Get-Date) -lt $deadline) {
            $hn = Get-NetStatus $h
            if ($hn.readyCheck.active) { $fired = $true; break }
            $null = Wait-Gameplay 5
            Start-Sleep -Milliseconds 500
        }
    }
    $hn = Get-NetStatus $h
    Check 'G18 host started a ready check' $fired "active=$($hn.readyCheck.active) initiator=$($hn.readyCheck.initiator)"
    if ($fired) {
        Check 'G18 ready check names the destination module' ($hn.readyCheck.destination -eq 'tar_m03ab') "destination=$($hn.readyCheck.destination)"
        Check 'G18 host sees both players in the check' (@($hn.readyCheck.players).Count -eq 2) "players=$(@($hn.readyCheck.players).Count)"
        $r = Ctrl-Send $h 'coopready 1'
        Check 'G18 host answered the ready check' ($r.ok) "$($r.output -join ';')"
        $r = Ctrl-Send $c 'coopready 1'
        Check 'G18 guest answered the ready check' ($r.ok) "$($r.output -join ';')"
        $moved = Wait-ClientNet { param($n) $n.module -eq 'tar_m03ab' } 45 700
        Check 'G18 client moved to the destination module' $moved.hit "client=$($moved.ns.module) gen=$($moved.ns.generation) (was $($cjBefore.module))"
        $hjAfter = Get-NetStatus $h
        Check 'G18 both sides agree on the destination' ($hjAfter.module -eq $moved.ns.module) "host=$($hjAfter.module) client=$($moved.ns.module)"
    }
}

# --- gate 19: clean shutdown ---------------------------------------------------
$null = Ctrl-Send $c 'quit'
$null = Ctrl-Send $h 'quit'
Start-Sleep -Seconds 3
$hc = $hostEngine.HasExited; $cc = $clientEngine.HasExited
Check 'G19 host exited on quit'   $hc "HasExited=$hc"
Check 'G19 client exited on quit' $cc "HasExited=$cc"

foreach ($e in @($hostEngine, $clientEngine)) { if ($e -and -not $e.HasExited) { try { $e.Kill() } catch {} } }

# --- scorer --------------------------------------------------------------------
Write-Host ''
Write-Host '===== RESULTS ====='
$pass = 0; $fail = 0
foreach ($r in $results) { $t = if ($r.pass) {'PASS'; $pass++} else {'FAIL'; $fail++}; Write-Host "[$t] $($r.name) :: $($r.detail)" }
Write-Host "SUMMARY: $pass passed, $fail failed, $($results.Count) total"
if ($fail -eq 0) { Write-Host 'RESULT: PASS' } else { Write-Host "RESULT: FAIL ($fail)" }
