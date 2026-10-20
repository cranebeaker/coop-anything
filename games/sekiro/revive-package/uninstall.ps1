<#
  Sekiro Online revive - uninstaller
  Restores the dinput8.dll that the installer moved aside and removes the files
  the installer added. Saves are never modified.
#>
[CmdletBinding()]
param([string]$GameDir)

$ErrorActionPreference = 'Stop'

if (-not $GameDir) {
  $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  if (-not $steam) { throw "Steam not found; pass -GameDir." }
  $libs = @((Join-Path $steam 'steamapps'))
  $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
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
}
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$manifestPath = Join-Path $GameDir 'sekiro-online-revive.install.json'
if (-not (Test-Path $manifestPath)) { throw "No install manifest in $GameDir - was Sekiro Online installed by this package?" }

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$installedDll = Join-Path $GameDir 'dinput8.dll'
if ((Test-Path $installedDll) -and (Get-FileHash -Algorithm SHA256 -LiteralPath $installedDll).Hash.ToString().ToUpperInvariant() -ne $manifest.dllSha256) {
  Write-Host "WARNING: game folder dinput8.dll is not the file this package installed. Leaving it alone." -ForegroundColor Yellow
} else {
  Remove-Item -LiteralPath $installedDll -Force -ErrorAction SilentlyContinue
}

$restored = $false
foreach ($bak in @($manifest.backups)) {
  if (-not $bak) { continue }
  $src = Join-Path $GameDir $bak
  if (-not (Test-Path $src)) { continue }
  if ($bak -like 'dinput8.dll.bak-*' -and -not $restored) {
    Move-Item -LiteralPath $src -Destination $installedDll
    Write-Host "restored dinput8.dll from $bak"
    $restored = $true
  }
}
Remove-Item -LiteralPath $manifestPath -Force
foreach ($extra in @('SekiroOnline-readme.txt', 'sekiro-online-revive.install.json')) {
  Remove-Item -LiteralPath (Join-Path $GameDir $extra) -Force -ErrorAction SilentlyContinue
}
Write-Host "Uninstalled. SekiroOnlineFont.ttf and SekiroOnlineLog.txt were left in place." -ForegroundColor Green
