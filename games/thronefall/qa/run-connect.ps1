# Two-account connect test: copy A hosts on the normal Steam, copy B joins from a Sandboxie box running a second Steam account.
# Prints both status files at the end and exits 0 only when both copies reached step=connected.
param(
    [string]$Root = 'D:\coop-test\thronefall',
    [string]$Box = 'Steam2',
    [string]$Driver = 'D:\coop-test\thronefall\qa-build\aaa.coopqa.dll',
    [int]$TimeoutSec = 240,
    [switch]$Kill,
    [switch]$Unlock,
    [switch]$UnlockHostOnly   # host has every level, the client only its own progress (fresh profile: tutorial only)
)
$ErrorActionPreference = 'Stop'
$start = 'C:\Program Files\Sandboxie-Plus\Start.exe'
$qa = Join-Path $Root 'qa'
New-Item -ItemType Directory -Force $qa | Out-Null
$lobbyFile = Join-Path $qa 'lobby.txt'
Remove-Item $lobbyFile -ErrorAction SilentlyContinue

foreach ($n in 'A', 'B') {
    $d = Join-Path $Root $n
    Copy-Item $Driver (Join-Path $d 'BepInEx\plugins\aaa.coopqa.dll') -Force
    Remove-Item "$d\BepInEx\LogOutput.log", "$d\Player.log", "$d\coopqa-status.txt" -ErrorAction SilentlyContinue
}
Start-Process powershell -WindowStyle Hidden -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $PSScriptRoot 'keep-minimized.ps1'), '-Root', $Root) | Out-Null
$common = @('-screen-fullscreen', '0', '-screen-width', '800', '-screen-height', '450')
if ($Unlock) { $common += '-coopqa-unlock' }
$A = Join-Path $Root 'A'; $B = Join-Path $Root 'B'
$pa = Start-Process "$A\thronefall.exe" -WorkingDirectory $A -PassThru -WindowStyle Minimized `
    -ArgumentList ($common + @('-logFile', "$A\Player.log", '-coopqa', 'host', '-coopqa-lobbyfile', $lobbyFile, '-coopqa-port', '47801') + $(if ($UnlockHostOnly) { @('-coopqa-unlock') } else { @() }))
"$($pa.Id) $($pa.StartTime.ToString('o')) A" | Add-Content (Join-Path $Root 'pids.txt')
& $start "/box:$Box" /wait:0 "$B\thronefall.exe" @common -logFile "$B\Player.log" -coopqa join -coopqa-lobbyfile $lobbyFile -coopqa-port 47802

Add-Type -Namespace CoopQa -Name Win -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool ShowWindowAsync(System.IntPtr h, int cmd);
[DllImport("user32.dll")] public static extern bool IsIconic(System.IntPtr h);
'@ -ErrorAction SilentlyContinue
# Keep the test copies minimized without activating them (SW_SHOWMINNOACTIVE = 7), so they never take the owner's screen.
function KeepMinimized {
    Get-Process thronefall -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Root\*" } | ForEach-Object {
        $h = $_.MainWindowHandle
        if ($h -ne [IntPtr]::Zero -and -not [CoopQa.Win]::IsIconic($h)) { [void][CoopQa.Win]::ShowWindowAsync($h, 7) }
    }
}

function Status($n) {
    $f = Join-Path $Root "$n\coopqa-status.txt"
    # The driver rewrites the file every second, so it can vanish between the check and the read.
    if (Test-Path $f) { Get-Content $f -ErrorAction SilentlyContinue } else { @() }
}
function StepOf($lines) { ($lines | Where-Object { $_ -like 'step=*' } | Select-Object -First 1) -replace '^step=', '' }

$deadline = (Get-Date).AddSeconds($TimeoutSec)
$last = ''
while ((Get-Date) -lt $deadline) {
    $sa = StepOf (Status 'A'); $sb = StepOf (Status 'B')
    $now = "A=$sa B=$sb"
    if ($now -ne $last) { "$(Get-Date -Format HH:mm:ss) $now"; $last = $now }
    if (($sa -eq 'connected' -and $sb -eq 'connected') -or $sa -like 'FAIL*' -or $sb -like 'FAIL*') { break }
    KeepMinimized
    Start-Sleep -Milliseconds 500
}
1..10 | ForEach-Object { KeepMinimized; Start-Sleep -Milliseconds 500 }
foreach ($n in 'A', 'B') { "===== $n"; Status $n }
$ok = (StepOf (Status 'A')) -eq 'connected' -and (StepOf (Status 'B')) -eq 'connected'
if ($ok) { 'RESULT: PASS connect' } else { 'RESULT: FAIL connect' }
if ($Kill) {
    Get-Process thronefall -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Root\*" } |
        ForEach-Object { Stop-Process -Id $_.Id -Force -Confirm:$false }
}
