# Kill (crash) or start only the client copy B inside the Sandboxie box, for leave/rejoin tests.
#   client-restart.ps1 -Kill            kill copy B only (like a crash)
#   client-restart.ps1 -Start           start copy B; it joins the lobby id in qa\lobby.txt
param(
    [string]$Root = 'D:\coop-test\thronefall',
    [string]$Box = 'Steam2',
    [switch]$Kill,
    [switch]$Start,
    [switch]$Unlock
)
$ErrorActionPreference = 'Stop'
$B = Join-Path $Root 'B'
if ($Kill) {
    $p = Get-Process thronefall -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$B\*" }
    $p | ForEach-Object { Stop-Process -Id $_.Id -Force -Confirm:$false }
    "killed client: $(@($p).Count) process(es)"
}
if ($Start) {
    Remove-Item "$B\coopqa-status.txt" -ErrorAction SilentlyContinue
    # Keep the old log for evidence, the game overwrites it
    if (Test-Path "$B\BepInEx\LogOutput.log") { Copy-Item "$B\BepInEx\LogOutput.log" "$B\BepInEx\LogOutput.before-restart.log" -Force }
    $lobbyFile = Join-Path $Root 'qa\lobby.txt'
    # The driver only joins from a lobby file newer than 120 s before its own start: refresh the timestamp.
    (Get-Item $lobbyFile).LastWriteTime = Get-Date
    $args = @('-screen-fullscreen', '0', '-screen-width', '800', '-screen-height', '450', '-logFile', "$B\Player.log",
              '-coopqa', 'join', '-coopqa-lobbyfile', $lobbyFile, '-coopqa-port', '47802')
    if ($Unlock) { $args += '-coopqa-unlock' }
    & 'C:\Program Files\Sandboxie-Plus\Start.exe' "/box:$Box" /wait:0 "$B\thronefall.exe" @args
    Start-Process powershell -WindowStyle Hidden -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $PSScriptRoot 'keep-minimized.ps1'), '-Root', $Root) | Out-Null
    "started client"
}
