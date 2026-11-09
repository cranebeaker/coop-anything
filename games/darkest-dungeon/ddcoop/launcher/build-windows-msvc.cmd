@echo off
setlocal
rem Run in an x64 Native Tools Command Prompt for Visual Studio 2022.
where cl >nul 2>nul
if errorlevel 1 (
  echo Open an x64 Native Tools Command Prompt for Visual Studio first.
  exit /b 1
)
pushd "%~dp0.."
if not exist build\win mkdir build\win
if not defined DDC_VERSION set "DDC_VERSION=0.5.0-test58"
if not defined DDC_TEST_NOSAVE set "DDC_TEST_NOSAVE=0"
python -I tools\gen_mascot_resource.py "%DDC_MASCOT_GIF%" build\win\ddcoop-mascot.rc
if errorlevel 1 goto failed
rc /nologo /I build\win /I launcher\src /fo build\win\ddcoop.res launcher\src\ddcoop.rc
if errorlevel 1 goto failed
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /D_WIN32_WINNT=0x0601 /DDDC_VERSION=\"%DDC_VERSION%\" /DDDC_TEST_NOSAVE=%DDC_TEST_NOSAVE% /Ilauncher\src /Fobuild\win\ /Febuild\win\DDCoop.exe launcher\src\main.c launcher\src\lr_mascot.c launcher\src\lr_install.c launcher\src\lr_steam.c launcher\src\lr_core.c launcher\src\lr_relay.c launcher\src\lr_wire.c launcher\src\lr_files.c launcher\src\lr_sock.c launcher\src\lr_owners.c launcher\src\lr_roster.c launcher\src\lr_snapshot.c launcher\src\lr_prepare_win.c build\win\ddcoop.res /link /SUBSYSTEM:WINDOWS ws2_32.lib bcrypt.lib uuid.lib winmm.lib shell32.lib ole32.lib comctl32.lib gdiplus.lib user32.lib gdi32.lib advapi32.lib
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
