<#
  Sekiro Online revive - diagnostics
  Reports game identity, what is installed, whether the original services still
  answer, and the tail of the mod's log. Read-only.
#>
[CmdletBinding()]
param([string]$GameDir, [int]$TimeoutMs = 4000)

$ExpectedExeSha256 = '637ACA527538C0EC6E1F136C8ED66046E95DFBDBB1F51926E134D9916398B856'
$TestedDllSha256   = 'B40DD9C34B4192F546E65B2306959FFE276AF2991D551B2FABC46CC0BDA7DAD1'

function Get-Sha256($path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToUpperInvariant() }
function Line($k, $v) { "{0,-28} {1}" -f $k, $v }

Write-Host "Sekiro Online revive - diagnostics" -ForegroundColor Cyan
Write-Host ("-" * 60)

if (-not $GameDir) {
  $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  if ($steam) {
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
        if (Test-Path (Join-Path $candidate 'sekiro.exe')) {
          $GameDir = $candidate
          $build = [regex]::Match((Get-Content $acf -Raw), '"buildid"\s+"([^"]+)"').Groups[1].Value
          break
        }
      }
    }
  }
}

if (-not $GameDir) { Write-Host "Sekiro not found. Pass -GameDir." -ForegroundColor Red; exit 1 }
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
Write-Host (Line "Game directory" $GameDir)
if ($build) { Write-Host (Line "Steam buildid" $build) } else { Write-Host (Line "Steam buildid" 'unknown (not a Steam-discovered folder)') }

$exe = Join-Path $GameDir 'sekiro.exe'
if (Test-Path $exe) {
  $h = Get-Sha256 $exe
  Write-Host (Line "sekiro.exe SHA-256" $h)
  if ($h -ne $ExpectedExeSha256) { Write-Host "  (differs from the build this package was tested on)" -ForegroundColor Yellow }
} else { Write-Host "sekiro.exe missing!" -ForegroundColor Red }

$dll = Join-Path $GameDir 'dinput8.dll'
if (Test-Path $dll) {
  $dh = Get-Sha256 $dll
  $kind = if ($dh -eq $TestedDllSha256) { 'Sekiro Online v2.05 (tested build)' } else { 'some other dinput8 mod' }
  Write-Host (Line "dinput8.dll" "$kind")
  Write-Host (Line "  size" ((Get-Item $dll).Length))
} else { Write-Host (Line "dinput8.dll" 'not installed') }

$manifest = Join-Path $GameDir 'sekiro-online-revive.install.json'
if (Test-Path $manifest) {
  $m = Get-Content $manifest -Raw | ConvertFrom-Json
  Write-Host (Line "Installed by package" "$($m.modVersion) at $($m.installedUtc)")
}

# Original services
Write-Host ""
Write-Host "Original services" -ForegroundColor Cyan
try {
  $ver = (Invoke-WebRequest -UseBasicParsing -TimeoutSec 8 -Uri 'https://raw.githubusercontent.com/LukeYui/Sekiro-Online/master/VERSION').Content.Trim()
  Write-Host (Line "version gate" "answers: $ver")
} catch { Write-Host (Line "version gate" "unreachable: $($_.Exception.Message)") }
try {
  (Invoke-WebRequest -UseBasicParsing -TimeoutSec 8 -Uri 'https://raw.githubusercontent.com/LukeYui/Sekiro-Online/master/DATA') | Out-Null
  Write-Host (Line "item data" "answers")
} catch { Write-Host (Line "item data" "unreachable: $($_.Exception.Message)") }

$client = [System.Net.Sockets.TcpClient]::new()
try {
  $task = $client.ConnectAsync('74.91.125.31', 54000)
  if ($task.Wait($TimeoutMs) -and $client.Connected) {
    Write-Host (Line "summon-sign server" "74.91.125.31:54000 OPEN") -ForegroundColor Green
  } else {
    Write-Host (Line "summon-sign server" "74.91.125.31:54000 no answer in ${TimeoutMs}ms (public summon signs unavailable)") -ForegroundColor Yellow
  }
} catch {
  Write-Host (Line "summon-sign server" "74.91.125.31:54000 error: $($_.Exception.InnerException.Message)") -ForegroundColor Yellow
} finally { $client.Dispose() }

# Logs
Write-Host ""
foreach ($name in @('SekiroOnlineLog.txt', 'SekiroOnlineErrorLog.txt')) {
  $p = Join-Path $GameDir $name
  if (Test-Path $p) {
    Write-Host "$name (last 20 lines):" -ForegroundColor Cyan
    Get-Content -LiteralPath $p -Tail 20 | ForEach-Object { "  $_" }
  } else {
    Write-Host (Line $name "not present")
  }
}
