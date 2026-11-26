# Firewatch Co-op installer (BepInEx 5 + Shoshone Co-op patched)
# Usage:  powershell -ExecutionPolicy Bypass -File install.ps1
#         powershell -ExecutionPolicy Bypass -File install.ps1 -GameDir "D:\Games\Firewatch"
#         powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
#         powershell -ExecutionPolicy Bypass -File install.ps1 -OpenFirewall
param(
    [string]$GameDir = "",
    [switch]$Uninstall,
    [switch]$Force,
    [switch]$OpenFirewall
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

$expectedAssemblyHash = "145B56AEBE921C0FDC660A4E64008272E09A283332146958CE2F356C7490522A"
$bepInExVersion = "5.4.23.6"
$bepInExHash = "6c57a714a892efb4e9e1fef63d608a3391f30ec984031c465404e4ee381a0755"
$bepInExUrl = "https://github.com/BepInEx/BepInEx/releases/download/v$bepInExVersion/BepInEx_win_x64_$bepInExVersion.zip"

function Say($m) { Write-Host $m }
function Fail($m) { Write-Host ""; Write-Host "ERROR: $m" -ForegroundColor Red; Write-Host "Log: nothing was changed. Nothing was deleted."; exit 1 }

function Find-Firewatch {
    if ($GameDir -ne "") {
        if (Test-Path (Join-Path $GameDir "Firewatch.exe")) { return (Resolve-Path $GameDir).Path }
        Fail "Firewatch.exe not found in $GameDir"
    }
    $steam = $null
    try { $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction Stop).SteamPath } catch { }
    if (-not $steam) {
        try { $steam = (Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -ErrorAction Stop).InstallPath } catch { }
    }
    $roots = @()
    if ($steam) {
        $roots += $steam
        $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
        if (Test-Path $vdf) {
            foreach ($line in Get-Content $vdf) {
                if ($line -match '"path"\s+"([^"]+)"') { $roots += ($matches[1] -replace '\\\\', '\') }
            }
        }
    }
    foreach ($root in ($roots | Select-Object -Unique)) {
        $candidate = Join-Path $root "steamapps\common\Firewatch"
        if (Test-Path (Join-Path $candidate "Firewatch.exe")) { return $candidate }
    }
    Fail "Could not find Firewatch. Pass -GameDir ""<your Firewatch folder>""."
}

function Copy-BepInEx($target) {
    $zip = Join-Path $env:TEMP "BepInEx_win_x64_$bepInExVersion.zip"
    if (-not (Test-Path $zip)) {
        Say "Downloading BepInEx $bepInExVersion from the official GitHub release..."
        try { Invoke-WebRequest -Uri $bepInExUrl -OutFile $zip -UseBasicParsing }
        catch { Fail "Download failed: $($_.Exception.Message)" }
    }
    $hash = (Get-FileHash $zip -Algorithm SHA256).Hash
    if ($hash -ne $bepInExHash) { Fail "BepInEx archive hash mismatch (got $hash). Delete $zip and retry." }
    Say "BepInEx archive verified (SHA-256)."
    Expand-Archive -Path $zip -DestinationPath $target -Force
}

$dir = Find-Firewatch
Say "Firewatch: $dir"

$running = Get-Process Firewatch -ErrorAction SilentlyContinue
if ($running) { Fail "Firewatch is running (pid $($running.Id -join ',')). Close the game and run the installer again." }

$managed = Join-Path $dir "Firewatch_Data\Managed\Assembly-CSharp.dll"
if (-not (Test-Path $managed)) { Fail "This does not look like a Firewatch install (no Assembly-CSharp.dll)." }
$gameHash = (Get-FileHash $managed -Algorithm SHA256).Hash
if ($gameHash -ne $expectedAssemblyHash) {
    $msg = "Assembly-CSharp.dll is not the build this mod was tested on.`n  expected $expectedAssemblyHash`n  found    $gameHash`nRun Steam > Firewatch > Properties > Installed Files > Verify integrity, or re-run with -Force."
    if (-not $Force) { Fail $msg }
    Say "WARNING: game build differs from the tested one."
}

if ($Uninstall) {
    $plugin = Join-Path $dir "BepInEx\plugins\StoryCoop.dll"
    if (Test-Path $plugin) { Remove-Item $plugin -Force; Say "Removed StoryCoop.dll" }
    Say "BepInEx and its config were left in place (other mods may use them)."
    Say "To remove BepInEx too: delete '$dir\BepInEx', winhttp.dll and doorstop_config.ini."
    exit 0
}

if (-not (Test-Path (Join-Path $dir "winhttp.dll"))) {
    Copy-BepInEx $dir
} else {
    $verFile = Join-Path $dir "BepInEx\core\BepInEx.dll"
    if (Test-Path $verFile) {
        $v = (Get-Item $verFile).VersionInfo.FileVersion
        Say "BepInEx already installed ($v); leaving it in place."
    }
}

$plugins = Join-Path $dir "BepInEx\plugins"
New-Item -ItemType Directory -Force -Path $plugins | Out-Null
Copy-Item (Join-Path $here "StoryCoop.dll") (Join-Path $plugins "StoryCoop.dll") -Force
Say "Installed StoryCoop.dll"

$configDir = Join-Path $dir "BepInEx\config"
New-Item -ItemType Directory -Force -Path $configDir | Out-Null
$cfg = Join-Path $configDir "zwtzyy.firewatch.storycoop.cfg"
if (-not (Test-Path $cfg)) {
    @"
[Network]
Ip = 127.0.0.1
Port = 7778
PlayerName = Henry

[Sync]
PlayerSendRate = 20
SyncStoryFacts = true
ClientSendsFacts = false
"@ | Set-Content $cfg -Encoding UTF8
    Say "Wrote default config (edit PlayerName if you like)."
}

if ($OpenFirewall) {
    try {
        New-NetFirewallRule -DisplayName "Firewatch Co-op (TCP 7778)" -Direction Inbound -Protocol TCP -LocalPort 7778 -Action Allow -ErrorAction Stop | Out-Null
        Say "Firewall rule added for TCP 7778."
    } catch {
        Say "Could not add the firewall rule (needs an elevated PowerShell). Hosts can allow the game when Windows asks."
    }
}

Say ""
Say "Done. How to play on a LAN:"
Say "  1. Host: start Firewatch, press F8 or click SHOSHONE CO-OP, press Host."
Say "  2. Client: edit BepInEx\config\zwtzyy.firewatch.storycoop.cfg -> Ip = <host LAN IP>"
Say "     start the game, open SHOSHONE CO-OP, press Join."
Say "  3. Host starts a new game or loads a save; the client follows automatically."
Say ""
Say "Internet play needs TCP 7778 forwarded and is NOT recommended with this build"
Say "(the mod has no authentication)."
