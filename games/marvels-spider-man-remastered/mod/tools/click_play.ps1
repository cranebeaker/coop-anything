# Waits for the PC launcher window (790x445 client area) of a game process, then posts a click on Play.
# Usage: click_play.ps1 -GamePid <pid>
param([int]$GamePid)
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class LauncherWin {
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public struct RECT { public int L, T, R, B; } }
'@
for ($i = 0; $i -lt 120; $i++) {
    $p = Get-Process -Id $GamePid -ErrorAction SilentlyContinue
    if (-not $p) { 'exited'; exit 1 }
    $h = $p.MainWindowHandle
    $r = New-Object LauncherWin+RECT
    if ($h -ne 0 -and [LauncherWin]::GetClientRect($h, [ref]$r) -and $r.R -eq 790 -and $r.B -eq 445) {
        Start-Sleep -Milliseconds 1500
        $lp = [IntPtr]((335 -shl 16) -bor 50)
        [LauncherWin]::PostMessage($h, 0x200, [IntPtr]0, $lp) | Out-Null
        [LauncherWin]::PostMessage($h, 0x201, [IntPtr]1, $lp) | Out-Null
        [LauncherWin]::PostMessage($h, 0x202, [IntPtr]0, $lp) | Out-Null
        'clicked play'; exit 0
    }
    Start-Sleep -Milliseconds 500
}
'timeout'; exit 2
