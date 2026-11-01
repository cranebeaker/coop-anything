@echo off
rem Starts a SECOND copy of the test game for co-op testing, windowed, with its own settings/save folder
rem (a Player2 folder next to the exe) and without Unreal's Steam networking, so it does not fight the first copy.
if not exist "%~dp0Player2" mkdir "%~dp0Player2"
start "" "%~dp0CrashBandicoot4.exe" -windowed -ResX=1280 -ResY=720 -nosteam -userdir="%~dp0Player2" -log
