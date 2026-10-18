# Stage 1 build: 32-bit dinput8 proxy DLL via llvm-mingw (clang) i686 cross compiler.
# Toolchain history (all verified, see NOTES.md):
#   - Zig 0.17.0 abandoned: ziglang.org CDN resets connections from owner's network.
#   - TinyCC 0.9.27 tried next: it decorates stdcall exports ("_DirectInput8Create@20"),
#     its linker rejects --kill-at, ignores .def aliases, and its inline-asm cannot
#     emit a working stdcall trampoline (cdecl export crashed the game ~10-40s in with
#     0xc0000005, fault addrs ~0x800 = classic stdcall stack-misbalance signature).
#   - llvm-mingw (MartinStorsjo.LLVM-MinGW.MSVCRT, installed via winget) is the fix: a
#     normal WINAPI function plus a .def "EXPORTS DirectInput8Create = _Name@20" gives
#     a correctly-named, correctly-conventioned export with native "ret 20".
# The game is PE32/i386; i686-w64-mingw32-gcc targets i386 -> output is 32-bit.
#
# Export must be the UNDECORATED name "DirectInput8Create" (what the game's import
# table binds to) with stdcall convention (the game's caller pops nothing; callee
# "ret 20" pops the 20 arg bytes). The .def file + WINAPI impl provide both. Checks
# below confirm: PE32 machine, plain export name present, and a "ret 20" (C2 14 00)
# opcode in the binary proving correct stdcall cleanup.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$mingwBin = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.MSVCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261006-msvcrt-x86_64\bin'
$cc = Join-Path $mingwBin 'i686-w64-mingw32-gcc.exe'
if (-not (Test-Path $cc)) { throw "i686-w64-mingw32-gcc not found at $cc" }

Push-Location $here
try {
    & $cc -shared -o dinput8.dll dinput8proxy.c dinput8proxy.def -luser32
    if ($LASTEXITCODE -ne 0) { throw "mingw gcc failed: $LASTEXITCODE" }
    $h = (Get-FileHash .\dinput8.dll -Algorithm SHA256).Hash
    Write-Host "built dinput8.dll SHA-256: $h"
    # sanity: must be PE32 (i386), not PE32+
    $dllPath = Join-Path (Get-Location).Path 'dinput8.dll'
    $b = [System.IO.File]::ReadAllBytes($dllPath)
    $pe = [BitConverter]::ToInt32($b, 0x3C)
    $machine = [BitConverter]::ToUInt16($b, $pe + 4)
    Write-Host ("machine: 0x{0:X4} (expect 0x014C=i386)" -f $machine)
    if ($machine -ne 0x14C) { throw 'not a 32-bit DLL!' }
    # sanity: export must be the undecorated name the game imports
    $optSize = [BitConverter]::ToUInt16($b, $pe + 20)
    $exportRVA = [BitConverter]::ToUInt32($b, $pe + 24 + 96)
    $sectOff = $pe + 24 + $optSize
    $numSec = [BitConverter]::ToUInt16($b, $pe + 6)
    $map = @()
    for ($i = 0; $i -lt $numSec; $i++) {
        $so = $sectOff + 40 * $i
        $vaddr = [BitConverter]::ToUInt32($b, $so + 12)
        $vsize = [BitConverter]::ToUInt32($b, $so + 8)
        $rawsz = [BitConverter]::ToUInt32($b, $so + 16)
        $raw = [BitConverter]::ToUInt32($b, $so + 20)
        $map += ,@($vaddr, [Math]::Max($vsize, $rawsz), $raw)
    }
    $eo = $null
    foreach ($m in $map) { if ($exportRVA -ge $m[0] -and $exportRVA -lt ($m[0]+$m[1])) { $eo = $m[2] + ($exportRVA - $m[0]) } }
    if ($null -eq $eo) { throw 'no export directory!' }
    $numNames = [BitConverter]::ToUInt32($b, $eo + 24)
    $namePtrRVA = [BitConverter]::ToUInt32($b, $eo + 32)
    $npOff = $null
    foreach ($m in $map) { if ($namePtrRVA -ge $m[0] -and $namePtrRVA -lt ($m[0]+$m[1])) { $npOff = $m[2] + ($namePtrRVA - $m[0]) } }
    $names = @()
    for ($i = 0; $i -lt $numNames; $i++) {
        $nrva = [BitConverter]::ToUInt32($b, $npOff + 4 * $i)
        $no = $null
        foreach ($m in $map) { if ($nrva -ge $m[0] -and $nrva -lt ($m[0]+$m[1])) { $no = $m[2] + ($nrva - $m[0]) } }
        $end = $no
        while ($b[$end] -ne 0) { $end++ }
        $names += [System.Text.Encoding]::ASCII.GetString($b, $no, $end - $no)
    }
    Write-Host ("exports: " + ($names -join ', '))
    if ($names -notcontains 'DirectInput8Create') { throw 'undecorated DirectInput8Create export missing!' }
    Write-Host 'export OK: DirectInput8Create present (undecorated)'
    # sanity: the stdcall impl must emit "ret 20" (opcode C2 14 00). Its presence
    # proves correct stdcall cleanup for the game's caller (callee pops 20 arg bytes).
    $ret20 = 0
    for ($i = 0; $i -lt $b.Length - 2; $i++) {
        if ($b[$i] -eq 0xC2 -and $b[$i+1] -eq 0x14 -and $b[$i+2] -eq 0x00) { $ret20++ }
    }
    Write-Host ("stdcall 'ret 20' (C2 14 00) occurrences: $ret20 (expect >=1)")
    if ($ret20 -lt 1) { throw 'stdcall ret 20 missing - impl would not clean the stack!' }
}
finally { Pop-Location }
