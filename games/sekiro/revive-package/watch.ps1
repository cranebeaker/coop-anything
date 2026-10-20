<#
  Sekiro Online revive - live monitor
  Run this, then play. It records whether the game still tries the original
  matchmaking server and tails the mod's log, so a failed host/join leaves evidence.
  Stop with Ctrl+C.
#>
[CmdletBinding()]
param([string]$GameDir, [int]$Seconds = 900)

$ErrorActionPreference = 'Stop'
if (-not $GameDir) {
  $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  if ($steam) {
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    $libs = @((Join-Path $steam 'steamapps'))
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
}
if (-not $GameDir) { throw 'Sekiro not found; pass -GameDir.' }

$out = Join-Path $GameDir ("sekiro-online-watch-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
function Log($msg) {
  $line = "[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $msg
  Write-Host $line
  Add-Content -LiteralPath $out -Value $line
}
Log "watching for $Seconds s; log: $out"
Log "matchmaking endpoint under test: 74.91.125.31:54000"

$logFile = Join-Path $GameDir 'SekiroOnlineLog.txt'
$deadline = (Get-Date).AddSeconds($Seconds)
$lastSize = -1
$seen = @{}
while ((Get-Date) -lt $deadline) {
  $p = Get-Process sekiro -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $p) {
    if ($script:hadGame) { Log 'game exited'; break }
    Start-Sleep -Seconds 2
    continue
  }
  if (-not $script:hadGame) { $script:hadGame = $true; Log "game running (pid $($p.Id))" }

  foreach ($c in @(Get-NetTCPConnection -OwningProcess $p.Id -ErrorAction SilentlyContinue)) {
    $key = "$($c.State)|$($c.RemoteAddress)|$($c.RemotePort)"
    if (-not $seen.ContainsKey($key)) {
      $seen[$key] = $true
      $tag = if ($c.RemoteAddress -eq '74.91.125.31') { ' <-- original matchmaking server' } else { '' }
      Log "conn $($c.State) $($c.RemoteAddress):$($c.RemotePort)$tag"
    }
  }

  if (Test-Path $logFile) {
    $size = (Get-Item $logFile).Length
    if ($size -ne $lastSize) {
      $lastSize = $size
      Log "mod log grew to $size bytes"
      Get-Content -LiteralPath $logFile -Tail 10 | ForEach-Object { Log "  $_" }
    }
  }
  Start-Sleep -Seconds 2
}
Log 'monitor finished'
