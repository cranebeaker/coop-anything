# Runs two ptle_peer.py processes against the two local PCSX2 copies and walks each Harry.
# Usage: .\tools\peer_trial.ps1 -Name <run name> [-JoinTarget 127.0.0.1:47100] [-Seconds 18]
# JoinTarget lets the joiner go through a chaos relay instead of straight to the host.
param([string]$Name = "peer", [string]$JoinTarget = "127.0.0.1:47100", [int]$Seconds = 18)
$py = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
$ida = [int](Get-Content local\pcsx2-a.pid)
$idb = [int](Get-Content local\pcsx2-b.pid)
& $py -I tools\pine_state.py load 22 28011 | Out-Null
& $py -I tools\pine_state.py load 22 28012 | Out-Null
Start-Sleep 2
$host_ = Start-Process -FilePath $py -NoNewWindow -PassThru -RedirectStandardOutput "local\$Name-host.txt" -RedirectStandardError "local\$Name-host.err" -ArgumentList @("-I", "tools\ptle_peer.py", "--pid", "$idb", "--pine", "28012", "--code", "jungle", "--bind", "127.0.0.1", "--listen", "47100", "--seconds", "$Seconds", "--csv", "local\$Name-host.csv")
Start-Sleep 0.5
$join = Start-Process -FilePath $py -NoNewWindow -PassThru -RedirectStandardOutput "local\$Name-join.txt" -RedirectStandardError "local\$Name-join.err" -ArgumentList @("-I", "tools\ptle_peer.py", "--pid", "$ida", "--pine", "28011", "--code", "jungle", "--bind", "127.0.0.1", "--connect", $JoinTarget, "--seconds", "$Seconds", "--csv", "local\$Name-join.csv")
Start-Sleep 3
& $py -I tools\pcsx2_keys.py $idb d:1500 wait:3500
& $py -I tools\pcsx2_keys.py $ida s:1500 wait:3500
$host_.WaitForExit(60000) | Out-Null
$join.WaitForExit(60000) | Out-Null
"--- host (copy B) ---"; Get-Content "local\$Name-host.txt" | Select-Object -Last 4; Get-Content "local\$Name-host.err"
"--- join (copy A) ---"; Get-Content "local\$Name-join.txt" | Select-Object -Last 4; Get-Content "local\$Name-join.err"
"--- check ---"; & $py -I tools\peer_check.py "local\$Name-host.csv" "local\$Name-join.csv"
