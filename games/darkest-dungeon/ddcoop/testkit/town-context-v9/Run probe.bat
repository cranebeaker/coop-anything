@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-town-context-probe.ps1"
if errorlevel 1 echo Probe stopped. Read the message above, then press any key.
pause
