@echo off
rem Opens the DDCoop window (the same as the DDCoop shortcut on the desktop).
cd /d "%~dp0"
if not exist "files\DDCoop.exe" goto not_unpacked
start "" "files\DDCoop.exe" --start
exit /b 0

:not_unpacked
echo.
echo  De map "files" ontbreekt naast dit bestand.
echo  Pak eerst de HELE zip uit: rechtsklik op DDCoop-install.zip ^> Alles uitpakken.
echo.
pause
exit /b 1
