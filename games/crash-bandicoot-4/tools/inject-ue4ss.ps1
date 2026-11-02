# inject-ue4ss.ps1 - loads UE4SS.dll into an already running Crash Bandicoot 4.
# Written for the Crash 4 co-op project. It only does this:
#   1. waits for CrashBandicoot4.exe to be running,
#   2. waits for you to press Enter (do it once the game is at the title screen),
#   3. asks Windows to load UE4SS.dll (from this same folder) inside the game.
# It changes no files, needs no admin rights, and touches nothing on the internet.

$ErrorActionPreference = 'Stop'
$dll = Join-Path $PSScriptRoot 'UE4SS.dll'
if (-not (Test-Path $dll)) { Write-Host "UE4SS.dll not found next to this script: $dll"; Read-Host 'Press Enter to close'; exit 1 }

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class Inj {
  [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr VirtualAllocEx(IntPtr h, IntPtr addr, UIntPtr size, uint type, uint protect);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool WriteProcessMemory(IntPtr h, IntPtr addr, byte[] buf, UIntPtr size, out UIntPtr written);
  [DllImport("kernel32.dll", CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr mod, string name);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr GetModuleHandle(string name);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr CreateRemoteThread(IntPtr h, IntPtr attr, UIntPtr stack, IntPtr start, IntPtr param, uint flags, IntPtr id);
  [DllImport("kernel32.dll")] public static extern uint WaitForSingleObject(IntPtr h, uint ms);
  [DllImport("kernel32.dll")] public static extern bool GetExitCodeThread(IntPtr h, out uint code);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
}
'@

function Has-UE4SS($proc) {
  try { return [bool]($proc.Modules | Where-Object { $_.ModuleName -eq 'UE4SS.dll' }) } catch { return $false }
}
Write-Host 'Waiting for a copy of CrashBandicoot4.exe without UE4SS... (start the game from the test copy now)'
$p = $null
while (-not $p) {
  $p = Get-Process -Name 'CrashBandicoot4' -ErrorAction SilentlyContinue | Where-Object { -not (Has-UE4SS $_) } | Select-Object -First 1
  Start-Sleep -Milliseconds 500
}
Write-Host "Found the game (process id $($p.Id)): $($p.Path)"
if ($p.Path -notlike '*Crash4-CoopTest*') { Write-Host 'This is NOT the test copy. Close it and start the game from your test copy (a folder whose path contains Crash4-CoopTest) instead.'; Read-Host 'Press Enter to close'; exit 1 }
Read-Host 'When THIS copy shows its title screen, press Enter here'

$h = [Inj]::OpenProcess(0x043A, $false, $p.Id)   # create thread + query + VM operation/read/write
if ($h -eq [IntPtr]::Zero) { throw "Could not open the game process (error $([Runtime.InteropServices.Marshal]::GetLastWin32Error()))." }
$bytes = [Text.Encoding]::Unicode.GetBytes($dll + [char]0)
$mem = [Inj]::VirtualAllocEx($h, [IntPtr]::Zero, [UIntPtr][uint64]$bytes.Length, 0x3000, 0x04)
$w = [UIntPtr]::Zero
if (-not [Inj]::WriteProcessMemory($h, $mem, $bytes, [UIntPtr][uint64]$bytes.Length, [ref]$w)) { throw 'Could not write the DLL path into the game.' }
$load = [Inj]::GetProcAddress([Inj]::GetModuleHandle('kernel32.dll'), 'LoadLibraryW')
$t = [Inj]::CreateRemoteThread($h, [IntPtr]::Zero, [UIntPtr]::Zero, $load, $mem, 0, [IntPtr]::Zero)
if ($t -eq [IntPtr]::Zero) { throw "Could not start the loader in the game (error $([Runtime.InteropServices.Marshal]::GetLastWin32Error()))." }
[void][Inj]::WaitForSingleObject($t, 15000)
$code = 0; [void][Inj]::GetExitCodeThread($t, [ref]$code)
[void][Inj]::CloseHandle($t); [void][Inj]::CloseHandle($h)
if ($code -eq 0) { Write-Host 'Windows reported that UE4SS.dll did NOT load.' } else { Write-Host 'UE4SS.dll loaded. A UE4SS console window should appear; UE4SS.log is being written.' }
Read-Host 'Press Enter to close this window'
