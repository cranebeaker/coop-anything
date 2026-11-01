# Shared pieces for the Overgrowth Online scenarios: start a host and a client copy, join them
# in a story level, wait until both players stand in the scene, read both views, stop them.
# Dot-source after bridge.ps1:   . .\bridge.ps1; . .\coop_common.ps1
#
# A "pair" is a hashtable: Lab, HostBridge, ClientBridge, GamePort, Exe, WorkingDir, WriteDirName, Started (processes), T0.

function New-Pair {
    param([string]$Lab, [int]$HostBridge = 47001, [int]$ClientBridge = 47002, [int]$GamePort = 27960,
          [string]$Exe = "", [string]$WorkingDir = "", [string]$WriteDirName = "write")
    return @{Lab = $Lab; HostBridge = $HostBridge; ClientBridge = $ClientBridge; GamePort = $GamePort; Exe = $Exe
             WorkingDir = $WorkingDir; WriteDirName = $WriteDirName; Started = @(); T0 = (Get-Date)}
}

function Say($pair, [string]$m) { Write-Host ("[{0:F0}s] {1}" -f ((Get-Date) - $pair.T0).TotalSeconds, $m) }

function Start-PairCopy($pair, [string]$Name, [int]$Bridge) {
    $dir = Join-Path $pair.Lab $Name
    $exe = if ($pair.Exe) { $pair.Exe } else { Join-Path $dir "game\OvergrowthOnline\OvergrowthOnline.exe" }
    $write = Join-Path $dir $pair.WriteDirName
    $log = Join-Path $write "logfile.txt"
    if (Test-Path $log) { Remove-Item $log -Force }
    $argv = @("--write-dir", "`"$write`"", "--test-bridge", "$Bridge", "--clear-log")
    if ($pair.WorkingDir) { $argv += @("--working-dir", "`"$($pair.WorkingDir.TrimEnd('\'))/`"", "--game-data", "`"$dir\game`"") }
    $p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru -WindowStyle Minimized -ArgumentList $argv
    $pair.Started += $p
    Say $pair "$Name started, pid $($p.Id)"
}

# Both copies up and joined in $Level of $Campaign, session started. Throws on timeout.
function Start-Session($pair, [string]$Campaign, [string]$Level) {
    foreach ($b in $pair.HostBridge, $pair.ClientBridge) {
        if (Get-NetTCPConnection -LocalPort $b -State Listen -ErrorAction SilentlyContinue) { throw "bridge port $b is already in use: a copy is still running" }
    }
    Start-PairCopy $pair "host" $pair.HostBridge
    Start-PairCopy $pair "client" $pair.ClientBridge
    Wait-Status $pair.HostBridge { param($s) $s.ok } 90 "host bridge" | Out-Null
    Wait-Status $pair.ClientBridge { param($s) $s.ok } 90 "client bridge" | Out-Null
    Send-Bridge -Port $pair.HostBridge -Request @{cmd = "host"; port = $pair.GamePort; campaign = $Campaign; level = $Level} | Out-Null
    Wait-Status $pair.HostBridge { param($s) $s.level_loaded -and $s.online.hosting } 180 "host level loaded" | Out-Null
    Send-Bridge -Port $pair.ClientBridge -Request @{cmd = "connect"; ip = "127.0.0.1"; port = $pair.GamePort} | Out-Null
    Wait-Status $pair.ClientBridge { param($s) $s.level_loaded -and $s.online.client } 180 "client joined and loaded" | Out-Null
    Send-Bridge -Port $pair.HostBridge -Request @{cmd = "start_session"} | Out-Null
    Wait-Status $pair.ClientBridge { param($s) -not $s.waiting_for_input -and $s.online.players.Count -ge 2 } 60 "session started" | Out-Null
    Say $pair "session up in $Level"
}

# Each machine's own avatar and its view of the other's, by common id. $null when not both in the scene.
function Get-Avatars($pair) {
    $hs = Get-GameState $pair.HostBridge; $cs = Get-GameState $pair.ClientBridge
    $hOwn = @($hs.characters | Where-Object { $_.id -eq $hs.my_avatar -and $_.controlled -and -not $_.remote })
    $cOwn = @($cs.characters | Where-Object { $_.id -eq $cs.my_avatar -and $_.controlled -and -not $_.remote })
    if ($hOwn.Count -ne 1 -or $cOwn.Count -ne 1 -or $hOwn[0].common_id -eq $cOwn[0].common_id) { return $null }
    $hSeesC = @($hs.characters | Where-Object { $_.common_id -eq $cOwn[0].common_id -and $_.remote })
    $cSeesH = @($cs.characters | Where-Object { $_.common_id -eq $hOwn[0].common_id -and $_.remote })
    if ($hSeesC.Count -ne 1 -or $cSeesH.Count -ne 1) { return $null }
    return @{HostOwn = $hOwn[0]; ClientOwn = $cOwn[0]; HostSeesClient = $hSeesC[0]; ClientSeesHost = $cSeesH[0]; HostState = $hs; ClientState = $cs}
}

# Skip the host's dialogue until both players stand in the scene (cutscenes hide guests). Returns Get-Avatars.
function Wait-BothInScene($pair, [int]$TimeoutS = 120, [switch]$NoSkip) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutS) {
        if (-not $NoSkip) { Send-Bridge -Port $pair.HostBridge -Request @{cmd = "input"; hold = @{skip_dialogue = 1.0}; steps = 10} | Out-Null }
        Start-Sleep -Seconds 2
        $a = Get-Avatars $pair
        if ($a -and $a.ClientOwn.visible -and $a.HostOwn.visible) { return $a }
    }
    throw "both players not in the scene within ${TimeoutS}s"
}

function Get-Distance($a, $b) { [math]::Sqrt(($a[0]-$b[0])*($a[0]-$b[0]) + ($a[1]-$b[1])*($a[1]-$b[1]) + ($a[2]-$b[2])*($a[2]-$b[2])) }

# Run script in a machine's level context (target "level") or a character's (target id)
function Invoke-Script($pair, [string]$Side, [string]$Code, $Target = "level") {
    $port = if ($Side -eq "host") { $pair.HostBridge } else { $pair.ClientBridge }
    return Send-Bridge -Port $port -Request @{cmd = "exec"; target = $Target; code = $Code}
}

function Get-LogLines($pair, [string]$Side, [string]$Pattern, [int]$Last = 10) {
    $log = Join-Path $pair.Lab "$Side\$($pair.WriteDirName)\logfile.txt"
    if (-not (Test-Path $log)) { return @() }
    return @(Get-Content $log | Select-String -Pattern $Pattern | Select-Object -Last $Last -ExpandProperty Line)
}

# One line per machine: its own avatar and its view of the other player's (for failure messages)
function Describe-Avatars($pair) {
    $out = @()
    foreach ($side in @(@("host", $pair.HostBridge), @("client", $pair.ClientBridge))) {
        try {
            $gs = Get-GameState $side[1]
            $own = $gs.characters | Where-Object { $_.id -eq $gs.my_avatar } | Select-Object -First 1
            $players = @($gs.characters | Where-Object { $_.is_player -or $_.remote -or $_.id -eq $gs.my_avatar })
            $desc = $players | ForEach-Object { "id=$($_.id)/c$($_.common_id) ctrl=$($_.controlled) remote=$($_.remote) proxy=$($_.proxy) vis=$($_.visible) ko=$($_.knocked_out) pos=$(($_.pos | ForEach-Object { [math]::Round($_,1) }) -join ',')" }
            $out += "$($side[0]) my_avatar=$($gs.my_avatar) own_present=$($null -ne $own): " + ($desc -join ' ; ')
        } catch { $out += "$($side[0]): no state ($($_.Exception.Message))" }
    }
    return ($out -join ' || ')
}

# Poll a scriptblock over Get-Avatars until it returns true. Throws on timeout, with both machines' avatars in the message.
function Wait-Avatars($pair, [scriptblock]$Check, [int]$TimeoutS, [string]$What) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutS) {
        try { $a = Get-Avatars $pair; if ($a -and (& $Check $a)) { return $a } } catch { }
        Start-Sleep -Milliseconds 700
    }
    throw "timeout after ${TimeoutS}s waiting for $What [$(Describe-Avatars $pair)]"
}

function Stop-Pair($pair) {
    foreach ($b in $pair.HostBridge, $pair.ClientBridge) { try { Send-Bridge -Port $b -Request @{cmd = "quit"} -TimeoutMs 3000 | Out-Null } catch {} }
    Start-Sleep -Seconds 3
    foreach ($p in $pair.Started) { if (-not $p.HasExited) { try { Stop-Process -Id $p.Id -Force } catch {} } }  # only processes this run started
}
