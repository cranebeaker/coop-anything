@echo off
rem Removes DDCoop: the original SDL2.dll goes back and mods\DDCoop is set aside. Saves are not touched.
cd /d "%~dp0"
if not exist "files\DDCoop.exe" goto not_unpacked
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0files' | Unblock-File" >nul 2>&1
start "" /wait "files\DDCoop.exe" --uninstall
exit /b %errorlevel%

:not_unpacked
echo.
echo  De map "files" ontbreekt naast dit bestand.
echo  Pak eerst de HELE zip uit: rechtsklik op DDCoop-install.zip ^> Alles uitpakken.
echo.
pause
exit /b 1
