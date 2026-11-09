@echo off
rem Installs DDCoop into Darkest Dungeon (finds the game through Steam by itself). Safe to run twice.
cd /d "%~dp0"
if not exist "files\DDCoop.exe" goto not_unpacked
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0files' | Unblock-File" >nul 2>&1
start "" /wait "files\DDCoop.exe" --install
exit /b %errorlevel%

:not_unpacked
echo.
echo  The "files" folder is missing next to this file.
echo  Extract the ENTIRE zip first: right-click DDCoop-install.zip ^> Extract All.
echo  Then double-click "Install DDCoop.bat" in the extracted folder.
echo.
pause
exit /b 1
