@echo off
rem Opens the DDCoop window (the same as the DDCoop shortcut on the desktop).
cd /d "%~dp0"
if not exist "files\DDCoop.exe" goto not_unpacked
start "" "files\DDCoop.exe" --start
exit /b 0

:not_unpacked
echo.
echo  The "files" folder is missing next to this file.
echo  Extract the ENTIRE zip first: right-click DDCoop-install.zip ^> Extract All.
echo.
pause
exit /b 1
