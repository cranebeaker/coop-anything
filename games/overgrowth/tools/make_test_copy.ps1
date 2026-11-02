# Make an isolated test copy of an Overgrowth install for co-op testing.
#
# Data\ (about 20 GB) is hard-linked file by file (same bytes on disk, no copy).
# Everything at the install root (exe, dlls, steam_appid.txt) is a real copy, so the
# fork's installer can replace Overgrowth.exe in the copy without touching the
# original through a link. Never write into a hard-linked file: the game writes only
# to its write dir (--write-dir), which this script creates next to the copy.
#
# Usage:  make_test_copy.ps1 -Source "<steam>\steamapps\common\Overgrowth" -Dest "<lab>\host"
# Result: <Dest>\game  (the copy)   <Dest>\write  (its user-data dir, empty)
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Dest
)
$ErrorActionPreference = "Stop"
$Source = (Resolve-Path $Source).Path.TrimEnd('\')
if (-not (Test-Path (Join-Path $Source "Overgrowth.exe"))) { throw "No Overgrowth.exe in $Source" }
if ((Split-Path -Qualifier $Source) -ne (Split-Path -Qualifier $Dest)) { throw "Hard links need the same drive: $Source vs $Dest" }

$game = Join-Path $Dest "game"
$write = Join-Path $Dest "write"
if (Test-Path $game) { throw "$game exists; remove it first (it holds only links and small copies)" }
New-Item -ItemType Directory -Force $game | Out-Null
New-Item -ItemType Directory -Force $write | Out-Null

# Root files: real copies
Get-ChildItem $Source -File | ForEach-Object { Copy-Item $_.FullName (Join-Path $game $_.Name) }

# Data\: directories, then one hard link per file
$srcData = Join-Path $Source "Data"
$dstData = Join-Path $game "Data"
$dirs = Get-ChildItem $srcData -Directory -Recurse
New-Item -ItemType Directory -Force $dstData | Out-Null
foreach ($d in $dirs) {
    New-Item -ItemType Directory -Force ($d.FullName.Replace($srcData, $dstData)) | Out-Null
}
$files = Get-ChildItem $srcData -File -Recurse
$n = 0
foreach ($f in $files) {
    $target = $f.FullName.Replace($srcData, $dstData)
    New-Item -ItemType HardLink -Path $target -Target $f.FullName | Out-Null
    $n++
    if ($n % 5000 -eq 0) { Write-Host "  linked $n / $($files.Count)" }
}

# Manifest of the source (size + mtime) so a write through a link can be detected later
$manifest = Join-Path $Dest "source_manifest.txt"
$files | ForEach-Object { "{0}`t{1}`t{2}" -f $_.FullName.Substring($Source.Length + 1), $_.Length, $_.LastWriteTimeUtc.Ticks } | Set-Content $manifest -Encoding utf8

Write-Host "Test copy: $game  ($n Data files linked, $((Get-ChildItem $Source -File).Count) root files copied)"
Write-Host "Write dir: $write"
Write-Host "Manifest:  $manifest"
