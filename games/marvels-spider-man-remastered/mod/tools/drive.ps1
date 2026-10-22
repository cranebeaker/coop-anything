# Focus one game window (by PID) and send scan-code key taps. Run with Windows PowerShell 5.1.
# Usage: drive.ps1 -GamePid <pid> [-Keys "ENTER,ESC,SPACE,W:1500"] [-DelayMs 600]
#   KEY        tap
#   KEY:ms     hold for ms
param([int]$GamePid, [string]$Keys = "", [int]$DelayMs = 600)
Add-Type @'
using System; using System.Runtime.InteropServices; using System.Threading;
public static class Drv {
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] i, int size);
  [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort vk; public ushort scan; public uint flags; public uint time; public IntPtr extra; }
  [StructLayout(LayoutKind.Explicit, Size = 40)] struct INPUT { [FieldOffset(0)] public uint type; [FieldOffset(8)] public KEYBDINPUT ki; }
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
  [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  public static bool Focus(IntPtr h) {
    keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero);  // ALT tap unlocks SetForegroundWindow
    uint fgThread = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero), me = GetCurrentThreadId();
    AttachThreadInput(me, fgThread, true);  // share the foreground thread's input state so the switch is allowed
    ShowWindow(h, 9); BringWindowToTop(h); SetForegroundWindow(h);
    AttachThreadInput(me, fgThread, false);
    Thread.Sleep(300);
    return GetForegroundWindow() == h;
  }
  public static void Key(ushort scan, bool extended, bool up) {
    var i = new INPUT[1]; i[0].type = 1; i[0].ki.scan = scan;
    i[0].ki.flags = 8u | (extended ? 1u : 0u) | (up ? 2u : 0u);  // KEYEVENTF_SCANCODE
    SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
  }
}
'@
$scan = @{ ESC = 0x01; ENTER = 0x1C; SPACE = 0x39; W = 0x11; A = 0x1E; S = 0x1F; D = 0x20; E = 0x12; Q = 0x10; F = 0x21;
           TAB = 0x0F; SHIFT = 0x2A; UP = 0x48; DOWN = 0x50; LEFT = 0x4B; RIGHT = 0x4D; F6 = 0x40; F7 = 0x41 }
$ext = @('UP', 'DOWN', 'LEFT', 'RIGHT')
$h = (Get-Process -Id $GamePid).MainWindowHandle
"focused=$([Drv]::Focus($h))"
foreach ($k in ($Keys -split ',' | Where-Object { $_ })) {
    $name, $ms = $k.Trim().ToUpper() -split ':'
    if (-not $scan.ContainsKey($name)) { "unknown key $name"; continue }
    [Drv]::Key($scan[$name], $ext -contains $name, $false)
    Start-Sleep -Milliseconds ([int]($(if ($ms) { $ms } else { 60 })))
    [Drv]::Key($scan[$name], $ext -contains $name, $true)
    Start-Sleep -Milliseconds $DelayMs
}
