@echo off
rem Puts every DDCoop file back (for example after a Steam update or "verify integrity of game files").
cd /d "%~dp0"
if not exist "files\DDCoop.exe" goto not_unpacked
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0files' | Unblock-File" >nul 2>&1
start "" /wait "files\DDCoop.exe" --repair
exit /b %errorlevel%

:not_unpacked
echo.
echo  The "files" folder is missing next to this file.
echo  Extract the ENTIRE zip first: right-click DDCoop-install.zip ^> Extract All.
echo.
pause
exit /b 1
