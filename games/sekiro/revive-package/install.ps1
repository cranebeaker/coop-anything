<#
  Sekiro Online revive - installer
  Installs the original Sekiro Online v2.05 files (from the player's own Nexus
  download) into a Sekiro install, after verifying the archive hash.
  It does not redistribute anything and never touches saves.
#>
[CmdletBinding()]
param(
  [string]$ZipPath,
  [string]$GameDir,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
$ExpectedZipSha256 = 'BE6EF9F61B73D7FAA8C2E78ECF35808FDE45EBAF1FC2A5240198DBF99AD311CC'
$ExpectedExeSha256 = '637ACA527538C0EC6E1F136C8ED66046E95DFBDBB1F51926E134D9916398B856'
$ExpectedDllSha256 = 'B40DD9C34B4192F546E65B2306959FFE276AF2991D551B2FABC46CC0BDA7DAD1'

function Write-Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "    $msg" -ForegroundColor Green }
function Write-Warn2($msg){ Write-Host "    $msg" -ForegroundColor Yellow }

function Get-Sha256($path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToUpperInvariant() }

# --- 1. locate the archive -------------------------------------------------
if (-not $ZipPath) {
  $candidates = @(Get-ChildItem -Path (Join-Path $env:USERPROFILE 'Downloads') -Filter 'Sekiro-Online*.zip' -ErrorAction SilentlyContinue |
                  Sort-Object LastWriteTime -Descending)
  if ($candidates.Count -eq 0) {
    throw "Sekiro Online v2.05 archive not found. Download it from https://www.nexusmods.com/sekiro/mods/577 (file 'Sekiro-Online-v2.05-577-2-05-1623599819.zip') and pass -ZipPath."
  }
  $ZipPath = $candidates[0].FullName
}
$ZipPath = (Resolve-Path -LiteralPath $ZipPath).Path
Write-Step "archive: $ZipPath"
$zipHash = Get-Sha256 $ZipPath
if ($zipHash -ne $ExpectedZipSha256) {
  if (-not $Force) {
    throw "Archive hash mismatch.`n  expected $ExpectedZipSha256`n  actual   $zipHash`nThis is not the v2.05 build this package was tested with. Re-download it, or pass -Force to install anyway."
  }
  Write-Warn2 "hash mismatch accepted with -Force (not the tested build)"
} else { Write-Ok "archive SHA-256 matches the tested v2.05 build" }

# --- 2. locate the game ----------------------------------------------------
if (-not $GameDir) {
  $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  if (-not $steam) { throw "Steam not found in the registry. Pass -GameDir 'X:\path\to\Sekiro'." }
  $libs = @((Join-Path $steam 'steamapps'))
  $vdf  = Join-Path $steam 'steamapps\libraryfolders.vdf'
  if (Test-Path $vdf) {
    foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) {
      $libs += Join-Path ($m.Groups[1].Value -replace '\\\\', '\') 'steamapps'
    }
  }
  foreach ($lib in $libs) {
    $acf = Join-Path $lib 'appmanifest_814380.acf'
    if (Test-Path $acf) {
      $name = [regex]::Match((Get-Content $acf -Raw), '"installdir"\s+"([^"]+)"').Groups[1].Value
      $candidate = Join-Path $lib ("common\" + $name)
      if (Test-Path (Join-Path $candidate 'sekiro.exe')) { $GameDir = $candidate; break }
    }
  }
  if (-not $GameDir) { throw "Sekiro (appid 814380) not found in any Steam library. Pass -GameDir." }
}
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$exe = Join-Path $GameDir 'sekiro.exe'
if (-not (Test-Path $exe)) { throw "sekiro.exe not found in $GameDir" }
Write-Step "game: $GameDir"

# --- 3. identity gate (AGENTS.md rule 5) ----------------------------------
$exeHash = Get-Sha256 $exe
if ($exeHash -ne $ExpectedExeSha256) {
  Write-Warn2 "sekiro.exe hash differs from the tested build"
  Write-Warn2 "  tested   $ExpectedExeSha256"
  Write-Warn2 "  this PC  $exeHash"
  Write-Warn2 "The mod may not work on this build. Continuing, but report the hash if you file a bug."
} else { Write-Ok "sekiro.exe matches the tested build (Steam buildid 5794815)" }

# --- 4. extract and verify payload ---------------------------------------
$tmp = Join-Path $env:TEMP ('sekiro-online-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
  Expand-Archive -LiteralPath $ZipPath -DestinationPath $tmp -Force
  $srcDll = Join-Path $tmp 'dinput8.dll'
  if (-not (Test-Path $srcDll)) { throw "dinput8.dll missing from the archive" }
  $dllHash = Get-Sha256 $srcDll
  if ($dllHash -ne $ExpectedDllSha256 -and -not $Force) {
    throw "dinput8.dll hash mismatch (expected $ExpectedDllSha256, got $dllHash)"
  }

  # --- 5. back up whatever is already loaded through dinput8 ---------------
  $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $destDll = Join-Path $GameDir 'dinput8.dll'
  $backups = @()
  foreach ($f in @('dinput8.dll', 'dinput8.dll.testoff', 'dinput8.dll.pretest.bak')) {
    $p = Join-Path $GameDir $f
    if (Test-Path $p) {
      $bak = "$p.bak-$stamp"
      Move-Item -LiteralPath $p -Destination $bak
      $backups += (Split-Path $bak -Leaf)
      Write-Warn2 "existing $f moved to $(Split-Path $bak -Leaf)"
    }
  }

  # --- 6. install ----------------------------------------------------------
  Copy-Item -LiteralPath $srcDll -Destination $destDll -Force
  $font = Join-Path $tmp 'SekiroOnlineFont.ttf'
  if (Test-Path $font) { Copy-Item -LiteralPath $font -Destination (Join-Path $GameDir 'SekiroOnlineFont.ttf') -Force }
  $readme = Join-Path $tmp 'readme.txt'
  if (Test-Path $readme) { Copy-Item -LiteralPath $readme -Destination (Join-Path $GameDir 'SekiroOnline-readme.txt') -Force }
  Write-Ok "installed dinput8.dll ($((Get-Item $destDll).Length) bytes)"

  # --- 7. manifest for uninstall ------------------------------------------
  $manifest = [ordered]@{
    package       = 'sekiro-online-revive'
    modVersion    = 'v2.05'
    installedUtc  = (Get-Date).ToUniversalTime().ToString('o')
    gameDir       = $GameDir
    sekiroSha256  = $exeHash
    dllSha256     = (Get-Sha256 $destDll)
    archiveSha256 = $zipHash
    backups       = $backups
  }
  $manifestPath = Join-Path $GameDir 'sekiro-online-revive.install.json'
  $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
  Write-Ok "manifest: $manifestPath"

  # --- 8. save safety ------------------------------------------------------
  $saveRoot = Join-Path $env:APPDATA 'Sekiro'
  if (Test-Path $saveRoot) {
    $saveBak = Join-Path $saveRoot ("save-backup-before-sekiro-online-" + $stamp)
    New-Item -ItemType Directory -Path $saveBak -Force | Out-Null
    $copied = 0
    foreach ($profile in @(Get-ChildItem -Path $saveRoot -Directory -ErrorAction SilentlyContinue)) {
      foreach ($name in @('S0000.sl2', 'S0000.sl2.bak')) {
        $src = Join-Path $profile.FullName $name
        if (-not (Test-Path $src)) { continue }
        $target = Join-Path (Join-Path $saveBak $profile.Name) $name
        New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $src -Destination $target -Force
        $copied++
      }
    }
    Write-Ok "$copied save file(s) backed up to $saveBak"
  }

  Write-Host ""
  Write-Host "Done. Next:" -ForegroundColor Cyan
  Write-Host " 1. Launch Sekiro through Steam (the mod loads as dinput8.dll)."
  Write-Host " 2. Finish the tutorial and reach the Dilapidated Temple once; online items are locked before that."
  Write-Host " 3. Host: use the Cursed Emblem, then invite your friend through the Steam friends list."
  Write-Host " 4. Friend: accept the Steam invite; co-op starts in the host's world."
  Write-Host ""
  Write-Warn2 "Public summon signs need the original matchmaking server, which is not answering (see README)."
}
finally {
  Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
