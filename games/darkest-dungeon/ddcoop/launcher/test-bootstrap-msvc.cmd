@echo off
setlocal
rem Run from a VS x64 Native Tools prompt. Three verified input files, read-only.
if "%~3"=="" (
  echo Usage: launcher\test-bootstrap-msvc.cmd new-bootstrap.dll previous-bootstrap.dll steam-original.dll
  exit /b 2
)
set "DDC_NEW_BOOT=%~f1"
set "DDC_OLD_BOOT=%~f2"
set "DDC_STEAM_SDL=%~f3"
pushd "%~dp0.."
if not exist build\win\bootstrap-tests mkdir build\win\bootstrap-tests
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Ilauncher\src /Fobuild\win\bootstrap-tests\ /Febuild\win\bootstrap-tests\test_lr_bootstrap.exe launcher\tests\test_lr_bootstrap.c launcher\src\lr_install.c launcher\src\lr_files.c launcher\src\lr_core.c launcher\src\lr_relay.c launcher\src\lr_sock.c launcher\src\lr_wire.c launcher\src\lr_owners.c launcher\src\lr_roster.c launcher\src\lr_snapshot.c /link ws2_32.lib bcrypt.lib user32.lib shell32.lib ole32.lib uuid.lib advapi32.lib
if errorlevel 1 goto failed
build\win\bootstrap-tests\test_lr_bootstrap.exe "%DDC_NEW_BOOT%" "%DDC_OLD_BOOT%" "%DDC_STEAM_SDL%"
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
