@echo off
setlocal
pushd "%~dp0.."
if not exist build\win\runtime-tests mkdir build\win\runtime-tests
for %%T in (proto town shop recruit hamlet provisions supplies saves addr session) do (
  call :test %%T
  if errorlevel 1 goto failed
)
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Isrc /Iruntime /Fobuild\win\runtime-tests\ /Febuild\win\runtime-tests\test_runtime_bridge.exe tests\test_runtime_bridge.c src\dd_shop.c src\dd_recruit.c src\dd_hamlet.c src\dd_dungeon_owners.c src\dd_provisions.c src\dd_supplies.c src\dd_addr.c src\dd_log.c src\dd_config.c src\dd_saves.c src\dd_proto.c src\dd_town.c src\dd_session.c src\dd_crypto_win.c src\dd_net.c /link bcrypt.lib ws2_32.lib user32.lib
if errorlevel 1 goto failed
build\win\runtime-tests\test_runtime_bridge.exe
if errorlevel 1 goto failed
popd
exit /b 0
:test
set "SOURCES=src\dd_%1.c"
set "LIBS="
if "%1"=="town" set "SOURCES=src\dd_town.c src\dd_proto.c"
if "%1"=="session" set "SOURCES=src\dd_session.c src\dd_crypto_win.c src\dd_proto.c"
if "%1"=="session" set "LIBS=bcrypt.lib"
if "%1"=="supplies" set "SOURCES=src\dd_supplies.c src\dd_proto.c src\dd_crypto_win.c"
if "%1"=="supplies" set "LIBS=bcrypt.lib"
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Isrc /Itests /Fobuild\win\runtime-tests\ /Febuild\win\runtime-tests\test_%1.exe tests\test_%1.c %SOURCES% /link %LIBS%
if errorlevel 1 exit /b 1
if "%1"=="addr" (
  if not defined DD_GAME_EXE (
    echo Set DD_GAME_EXE to the known Darkest.exe for address validation.
    exit /b 1
  )
  build\win\runtime-tests\test_%1.exe "%DD_GAME_EXE%"
) else (
  build\win\runtime-tests\test_%1.exe
)
exit /b %errorlevel%
:failed
popd
exit /b 1
