<#
  coop-probe test kit for Darkest Dungeon (Steam, Windows x64).

  What it does (and nothing else):
    1. Finds your Darkest Dungeon install via Steam and checks that Darkest.exe is the build the
       probe was made for. If not, it stops.
    2. Zips a BACKUP of your Darkest Dungeon saves (it only reads them).
    3. Copies the game to %USERPROFILE%\DD-coop-test\DarkestDungeon.
    4. In that COPY only: renames SDL2.dll to SDL2_orig.dll and puts the probe SDL2.dll next to it.
  Your real install and your saves are never changed.

  Run from the folder that holds this script and SDL2.dll:
      powershell -ExecutionPolicy Bypass -File .\install-testkit.ps1
  Remove the test copy again:
      powershell -ExecutionPolicy Bypass -File .\install-testkit.ps1 -Uninstall
  If Steam is not found automatically, add -GameDir "D:\path\to\DarkestDungeon".
#>
param(
    [string]$GameDir = "",
    [switch]$Uninstall
)
$ErrorActionPreference = "Stop"

$ExpectedExe   = "4d78fbfaa65b7d002d2208d02da809200f4a5723119521180e30100d6f1a75f2"
$ExpectedSdl   = "7ba3aa8b03e43690e2b1d4696a3beb90611224a9fee77ca2d4a19295bb4961dd"
$ExpectedProbe = "8ecac5bc817942e7b5cc36a7a1c589c73f16075b8fcc8feb2d19488dca375e3c"
$TestRoot = Join-Path $env:USERPROFILE "DD-coop-test"
$TestGame = Join-Path $TestRoot "DarkestDungeon"
$KitDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Hash($path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLower() }

if ($Uninstall) {
    if (Test-Path -LiteralPath $TestGame) {
        Remove-Item -LiteralPath $TestGame -Recurse -Force
        Write-Host "Removed the test copy $TestGame. Backups in $TestRoot are kept."
    } else {
        Write-Host "No test copy found at $TestGame."
    }
    exit 0
}

# --- 1. Find the game -------------------------------------------------------------------------
$SteamPath = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -Name SteamPath -ErrorAction SilentlyContinue).SteamPath
if (-not $GameDir) {
    if (-not $SteamPath) { throw "Steam not found in the registry. Run again with -GameDir `"<path to DarkestDungeon>`"." }
    $libraries = @($SteamPath)
    $vdf = Join-Path $SteamPath "steamapps\libraryfolders.vdf"
    if (Test-Path -LiteralPath $vdf) {
        foreach ($m in [regex]::Matches((Get-Content -Raw -LiteralPath $vdf), '"path"\s+"([^"]+)"')) {
            $libraries += $m.Groups[1].Value -replace '\\\\', '\'
        }
    }
    foreach ($lib in ($libraries | Select-Object -Unique)) {
        $candidate = Join-Path $lib "steamapps\common\DarkestDungeon"
        if (Test-Path -LiteralPath (Join-Path $candidate "_windows\win64\Darkest.exe")) { $GameDir = $candidate; break }
    }
    if (-not $GameDir) { throw "Darkest Dungeon not found in your Steam libraries. Run again with -GameDir." }
}
$exe = Join-Path $GameDir "_windows\win64\Darkest.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "No _windows\win64\Darkest.exe in $GameDir." }
Write-Host "Game found: $GameDir"

$exeHash = Hash $exe
if ($exeHash -ne $ExpectedExe) {
    throw "Darkest.exe is a different build ($exeHash). The probe only fits build $ExpectedExe. Nothing was changed."
}
if ((Hash (Join-Path $GameDir "_windows\win64\SDL2.dll")) -ne $ExpectedSdl) {
    throw "SDL2.dll in the game differs from the expected one. Nothing was changed."
}
if ((Hash (Join-Path $KitDir "SDL2.dll")) -ne $ExpectedProbe) {
    throw "The probe SDL2.dll in this kit does not match its expected hash. Nothing was changed."
}
Write-Host "Build check OK."

# --- 2. Back up the saves (read-only) ----------------------------------------------------------
New-Item -ItemType Directory -Force -Path $TestRoot | Out-Null
$saveDirs = @()
if ($SteamPath) {
    $saveDirs = Get-ChildItem -Directory -Path (Join-Path $SteamPath "userdata") -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName "262060\remote" } |
        Where-Object { Test-Path -LiteralPath $_ }
}
if ($saveDirs) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    foreach ($dir in $saveDirs) {
        # one zip per Steam account: ...\userdata\<account>\262060\remote
        $account = Split-Path -Leaf (Split-Path -Parent (Split-Path -Parent $dir))
        $zip = Join-Path $TestRoot "save-backup-$account-$stamp.zip"
        Compress-Archive -Path $dir -DestinationPath $zip
        Write-Host "Saves backed up to $zip"
    }
} else {
    Write-Host "No Steam save folder found to back up (looked in Steam\userdata\*\262060\remote)."
    $answer = Read-Host "Continue without a backup? (y/n)"
    if ($answer -ne "y") { exit 1 }
}

# --- 3. Copy the game -------------------------------------------------------------------------
Write-Host "Copying the game to $TestGame (about 4 GB, a few minutes)..."
robocopy $GameDir $TestGame /MIR /NFL /NDL /NJH /NP /R:1 /W:1 | Out-Host
if ($LASTEXITCODE -ge 8) { throw "Copy failed (robocopy exit code $LASTEXITCODE)." }

# --- 4. Put the probe in the copy -------------------------------------------------------------
$bin = Join-Path $TestGame "_windows\win64"
if (-not (Test-Path -LiteralPath (Join-Path $bin "SDL2_orig.dll"))) {
    Rename-Item -LiteralPath (Join-Path $bin "SDL2.dll") -NewName "SDL2_orig.dll"
}
Copy-Item -LiteralPath (Join-Path $KitDir "SDL2.dll") -Destination (Join-Path $bin "SDL2.dll") -Force
Remove-Item -LiteralPath (Join-Path $bin "coop-probe.log") -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Done. Test copy: $TestGame"
Write-Host "Start it (with Steam running) by double-clicking:"
Write-Host "    $bin\Darkest.exe"
Write-Host "The probe writes:  $bin\coop-probe.log"
