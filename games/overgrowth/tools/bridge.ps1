# Helpers for the Overgrowth Online test bridge (--test-bridge <port>): JSON lines on 127.0.0.1.
# Dot-source this file:  . .\bridge.ps1
# Protocol (fork Source/Testing/test_bridge.cpp): request {"id":N,"cmd":"...",...} -> one JSON line back.
# Commands seen in build 0.40.0 source: ping, status, state, exec, input, load_level, host, connect,
# disconnect, start_session, menu, screenshot, quit, net_sim, net_nan, spawn, stick_item, forge, crash,
# level_messages, track, track_get, view, perf, shader_check, gl_problems, texture_*, rig_check, nan_scan.

$script:BridgeId = 0

function Send-Bridge {
    param([int]$Port, [hashtable]$Request, [int]$TimeoutMs = 10000)
    $script:BridgeId++
    $Request["id"] = $script:BridgeId
    $json = ($Request | ConvertTo-Json -Compress -Depth 6)
    $c = New-Object Net.Sockets.TcpClient
    try {
        $c.Connect("127.0.0.1", $Port)
        $s = $c.GetStream(); $s.ReadTimeout = $TimeoutMs
        $w = New-Object IO.StreamWriter($s); $w.NewLine = "`n"; $w.AutoFlush = $true
        $r = New-Object IO.StreamReader($s)
        $w.WriteLine($json)
        $line = $r.ReadLine()
        if ($null -eq $line) { throw "bridge $Port closed the connection" }
        return ($line | ConvertFrom-Json)
    } finally { $c.Close() }
}

function Get-Status([int]$Port) { Send-Bridge -Port $Port -Request @{cmd = "status"} }
function Get-GameState([int]$Port) { Send-Bridge -Port $Port -Request @{cmd = "state"} }

# Poll until $Check (a scriptblock over the status object) is true, or fail after $TimeoutS.
function Wait-Status {
    param([int]$Port, [scriptblock]$Check, [int]$TimeoutS = 120, [string]$What = "condition")
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutS) {
        try { $st = Get-Status $Port; if (& $Check $st) { return $st } } catch { }
        Start-Sleep -Milliseconds 500
    }
    throw "timeout after ${TimeoutS}s waiting for $What on bridge $Port"
}

function Format-Chars($state) {
    $state.characters | ForEach-Object {
        "{0,4} common={1,4} ctrl={2,-5} remote={3,-5} player={4,-5} proxy={5,-5} pos=({6:F1},{7:F1},{8:F1}) ko={9} hp={10}" -f $_.id, $_.common_id, $_.controlled, $_.remote, $_.is_player, $_.proxy, $_.pos[0], $_.pos[1], $_.pos[2], $_.knocked_out, $_.temp_health
    }
}
