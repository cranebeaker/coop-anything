# Keeps every test copy under -Root minimized without activating it (SW_SHOWMINNOACTIVE), until none is left running.
param([string]$Root = 'D:\coop-test\thronefall')
Add-Type -Namespace CoopQa -Name Win2 -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool ShowWindowAsync(System.IntPtr h, int cmd);
[DllImport("user32.dll")] public static extern bool IsIconic(System.IntPtr h);
'@
$idle = 0
while ($idle -lt 100) {   # ~30 s with no test copy running
    $procs = @(Get-Process thronefall -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Root\*" })
    if ($procs.Count -eq 0) { $idle++ } else { $idle = 0 }
    foreach ($p in $procs) {
        $h = $p.MainWindowHandle
        if ($h -ne [IntPtr]::Zero -and -not [CoopQa.Win2]::IsIconic($h)) { [void][CoopQa.Win2]::ShowWindowAsync($h, 7) }
    }
    Start-Sleep -Milliseconds 300
}
