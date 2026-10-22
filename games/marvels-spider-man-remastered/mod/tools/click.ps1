# Focus one game window (by PID) and left-click at client coordinates with real mouse input (the game
# reads raw input, so posted messages don't reach its menus). Run with Windows PowerShell 5.1.
# Usage: click.ps1 -GamePid <pid> -X <x> -Y <y>
param([int]$GamePid, [int]$X, [int]$Y)
Add-Type @'
using System; using System.Runtime.InteropServices; using System.Threading;
public static class Clk {
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  public struct POINT { public int X, Y; }
  public static bool Click(IntPtr h, int x, int y) {
    keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero);
    SetForegroundWindow(h); Thread.Sleep(300);
    if (GetForegroundWindow() != h) return false;
    var p = new POINT { X = x, Y = y }; ClientToScreen(h, ref p);
    SetCursorPos(p.X, p.Y); Thread.Sleep(150);
    mouse_event(1, 1, 0, 0, UIntPtr.Zero); Thread.Sleep(100);   // small relative move so the game sees the cursor
    mouse_event(2, 0, 0, 0, UIntPtr.Zero); Thread.Sleep(80);
    mouse_event(4, 0, 0, 0, UIntPtr.Zero);
    return true;
  }
}
'@
"clicked=$([Clk]::Click((Get-Process -Id $GamePid).MainWindowHandle, $X, $Y))"
