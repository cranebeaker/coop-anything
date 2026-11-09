@echo off
rem Installs DDCoop into Darkest Dungeon (finds the game through Steam by itself). Safe to run twice.
cd /d "%~dp0"
if not exist "files\DDCoop.exe" goto not_unpacked
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0files' | Unblock-File" >nul 2>&1
start "" /wait "files\DDCoop.exe" --install
exit /b %errorlevel%

:not_unpacked
echo.
echo  De map "files" ontbreekt naast dit bestand.
echo  Pak eerst de HELE zip uit: rechtsklik op DDCoop-install.zip ^> Alles uitpakken.
echo  Dubbelklik daarna in de uitgepakte map op "Install DDCoop.bat".
echo.
pause
exit /b 1
