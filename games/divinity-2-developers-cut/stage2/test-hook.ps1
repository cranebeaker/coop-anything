# Stage 2 test: per-instance Profile/Savegames redirection via SHGetFolderPathA hook.
# Launches BOTH test copies simultaneously, each with its own DIV2_INSTANCE token:
#   divinity2-test     -> token 1
#   divinity2-test-b   -> token 2 (exe patched Div2Lock->Div2Loc2 so both can run)
# Verifies: proxy log (hook target MATCH + redirect lines), token dirs created,
# exit-write lands in the redirected profile, real %LOCALAPPDATA%\Divinity 2 untouched,
# no crash event. Live install (<game dir>) is NEVER touched.
#
# -StaggerSeconds N: wait N seconds between launching A and launching B (default 0 =
#   simultaneous). Used to test the startup-contention crash hypothesis: if a staggered
#   dual launch survives while a simultaneous one crashed, the crash is a timing/contention
#   trigger (the known main-menu movie NULL bug), not the hook.
[CmdletBinding()]
param(
    [int]$StaggerSeconds = 0
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Join-Path $env:LOCALAPPDATA 'Temp\opencode'
$binA = Join-Path $root 'divinity2-test\bin'
$binB = Join-Path $root 'divinity2-test-b\bin'
$dll  = Join-Path $here 'dinput8.dll'
if (-not (Test-Path $dll)) { throw 'build dinput8.dll first (build.ps1)' }
foreach ($b in @($binA, $binB)) {
    if (-not (Test-Path (Join-Path $b 'Divinity2.exe'))) { throw "missing exe: $b" }
}

# --- preflight state -----------------------------------------------------------
$profRoot = Join-Path $env:LOCALAPPDATA 'Divinity 2'
$tokA = Join-Path $env:LOCALAPPDATA '1'
$tokB = Join-Path $env:LOCALAPPDATA '2'
if (Test-Path $tokA) { throw "token dir A already exists: $tokA (refusing to reuse)" }
if (Test-Path $tokB) { throw "token dir B already exists: $tokB (refusing to reuse)" }

# baseline snapshot of the REAL profile: path|size|mtimeTicks|sha256 per file
function Snap($dir) {
    if (-not (Test-Path $dir)) { return ,@() }
    $out = @(Get-ChildItem $dir -Recurse -File | ForEach-Object {
        $h = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
        "$($_.FullName)|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)|$h"
    })
    return ,($out | Sort-Object)
}
$base = Snap $profRoot
Write-Output "real profile baseline: $($base.Count) files"

foreach ($d in @((Join-Path $env:PROGRAMDATA 'Divinity 2'),
                 (Join-Path $env:USERPROFILE 'Documents\Divinity 2'))) {
    Write-Output ("preexisting {0}: {1}" -f $d, (Test-Path $d))
}

# --- deploy --------------------------------------------------------------------
Copy-Item $dll (Join-Path $binA 'dinput8.dll') -Force
Copy-Item $dll (Join-Path $binB 'dinput8.dll') -Force
foreach ($b in @($binA, $binB)) {
    $lg = Join-Path $b 'dinput8proxy.log'
    if (Test-Path $lg) { Remove-Item $lg -Force }
}

# --- launch both instances, each with its own token ----------------------------
$before = Get-Date
$env:DIV2_INSTANCE = '1'
$pA = Start-Process -FilePath (Join-Path $binA 'Divinity2.exe') -WorkingDirectory $binA -PassThru
if ($StaggerSeconds -gt 0) {
    Write-Output "stagger: waiting $StaggerSeconds s before launching B..."
    Start-Sleep -Seconds $StaggerSeconds
}
$env:DIV2_INSTANCE = '2'
$pB = Start-Process -FilePath (Join-Path $binB 'Divinity2.exe') -WorkingDirectory $binB -PassThru
Write-Output "launched A pid=$($pA.Id) (token 1), B pid=$($pB.Id) (token 2); waiting 75s..."
Start-Sleep -Seconds 75

$aliveA = [bool](Get-Process -Id $pA.Id -ErrorAction SilentlyContinue)
$aliveB = [bool](Get-Process -Id $pB.Id -ErrorAction SilentlyContinue)
Write-Output "alive after 75s: A=$aliveA B=$aliveB"
# capture exit code for any instance that already died (classifies the death:
# 0 = clean self-quit / resource-refusal; NTSTATUS like 0xC0000005 = crash)
function ReapExit($proc, $label) {
    if (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue) { return }  # still alive
    try { $proc.WaitForExit(4000) | Out-Null } catch {}
    $ec = $proc.ExitCode
    Write-Output ("{0} EXIT CODE: {1} (0x{2:X8})" -f $label, $ec, $ec)
}
if (-not $aliveA) { ReapExit $pA 'A' }
if (-not $aliveB) { ReapExit $pB 'B' }
Write-Output ("token root A exists: {0}" -f (Test-Path $tokA))
Write-Output ("token root B exists: {0}" -f (Test-Path $tokB))
Write-Output ("token A\Divinity 2 exists: {0}" -f (Test-Path (Join-Path $tokA 'Divinity 2')))
Write-Output ("token B\Divinity 2 exists: {0}" -f (Test-Path (Join-Path $tokB 'Divinity 2')))

foreach ($p in @(@('A', $binA), @('B', $binB))) {
    $lg = Join-Path $p[1] 'dinput8proxy.log'
    Write-Output "--- log $($p[0]) ---"
    if (Test-Path $lg) { Get-Content $lg } else { Write-Output 'LOG MISSING' }
}

# --- graceful close so the game performs its exit-write of graphicoptions.xml ----
foreach ($proc in @($pA, $pB)) {
    $pr = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    if ($pr) { $null = $pr.CloseMainWindow() }
}
Start-Sleep -Seconds 15
$endA = [bool](Get-Process -Id $pA.Id -ErrorAction SilentlyContinue)
$endB = [bool](Get-Process -Id $pB.Id -ErrorAction SilentlyContinue)
if ($endA -or $endB) {
    Write-Output "still running after WM_CLOSE: A=$endA B=$endB - forcing stop"
    foreach ($id in @($pA.Id, $pB.Id)) {
        Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 3
}

# --- verdicts -------------------------------------------------------------------
$after = Snap $profRoot
$diff = Compare-Object $base $after
if ($null -eq $diff) { $diff = @() }
Write-Output ("REAL profile changed by the run: {0} files (expect 0)" -f $diff.Count)
if ($diff.Count) {
    $diff | ForEach-Object { Write-Output ("  " + $_.SideIndicator + " " + $_.InputObject) }
}

$gA = Join-Path $tokA 'Divinity 2\Profile\graphicoptions.xml'
$gB = Join-Path $tokB 'Divinity 2\Profile\graphicoptions.xml'
Write-Output ("redirected profile A exit-write: {0} (expect True)" -f (Test-Path $gA))
Write-Output ("redirected profile B exit-write: {0} (expect True)" -f (Test-Path $gB))
foreach ($t in @($tokA, $tokB)) {
    $sg = Join-Path $t 'Divinity 2\Savegames'
    if (Test-Path $sg) {
        $n = (Get-ChildItem $sg -File -ErrorAction SilentlyContinue).Count
        Write-Output ("{0}\Divinity 2\Savegames: {1} files" -f $t, $n)
    }
}

$crash = Get-WinEvent -FilterHashtable @{LogName='Application'; Id=1000; StartTime=$before} -ErrorAction SilentlyContinue |
    Where-Object { $_.Message -match 'Divinity' }
if ($crash) { Write-Output 'CRASH DETECTED (Application Error 1000 since launch)' }
else { Write-Output 'no crash event since launch' }
