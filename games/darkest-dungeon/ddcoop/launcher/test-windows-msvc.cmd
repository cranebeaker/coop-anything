@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist build\win\tests\fake-steam mkdir build\win\tests\fake-steam
set "DDC_CC=cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Ilauncher\src /Fobuild\win\tests\"
%DDC_CC% /Febuild\win\tests\test_lr_wire.exe launcher\tests\test_lr_wire.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_roster.exe launcher\tests\test_lr_roster.c launcher\src\lr_roster.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_owners.exe launcher\tests\test_lr_owners.c launcher\src\lr_owners.c launcher\src\lr_roster.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_live_party_win.exe launcher\tests\test_lr_live_party_win.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_snapshot.exe launcher\tests\test_lr_snapshot.c launcher\src\lr_snapshot.c launcher\src\lr_files.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_core.exe launcher\tests\test_lr_core.c launcher\src\lr_wire.c launcher\src\lr_sock.c launcher\src\lr_relay.c launcher\src\lr_files.c launcher\src\lr_core.c launcher\src\lr_owners.c launcher\src\lr_roster.c launcher\src\lr_snapshot.c /link /STACK:8388608 ws2_32.lib
if errorlevel 1 goto failed
%DDC_CC% /LD /Febuild\win\tests\fake-steam\steam_api64.dll launcher\tests\fake_storage_api.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_prepare_win.exe launcher\tests\test_lr_prepare_win.c launcher\src\lr_snapshot.c launcher\src\lr_files.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
for %%t in (wire roster owners live_party_win core) do (
  build\win\tests\test_lr_%%t.exe
  if errorlevel 1 goto failed
)
pushd build\win\tests
test_lr_snapshot.exe
if errorlevel 1 (popd & goto failed)
popd
build\win\tests\test_lr_prepare_win.exe build\win\DDCoop.exe build\win\tests\fake-steam
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
