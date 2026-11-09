@echo off
rem Puts every DDCoop file back (for example after a Steam update or "verify integrity of game files").
cd /d "%~dp0"
if not exist "files\DDCoop.exe" goto not_unpacked
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0files' | Unblock-File" >nul 2>&1
start "" /wait "files\DDCoop.exe" --repair
exit /b %errorlevel%

:not_unpacked
echo.
echo  De map "files" ontbreekt naast dit bestand.
echo  Pak eerst de HELE zip uit: rechtsklik op DDCoop-install.zip ^> Alles uitpakken.
echo.
pause
exit /b 1
