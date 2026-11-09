@echo off
setlocal
pushd "%~dp0.."
if not exist build\win\dungeon-tests mkdir build\win\dungeon-tests
set "CHECK_FLAGS=/O2"
if "%DNG_ASAN%"=="1" set "CHECK_FLAGS=/Zi /fsanitize=address"
cl /nologo /std:c11 %CHECK_FLAGS% /MT /W4 /WX /D_CRT_SECURE_NO_WARNINGS /Isrc /Fobuild\win\dungeon-tests\ /Fdbuild\win\dungeon-tests\compiler.pdb /Febuild\win\dungeon-tests\test_dungeon.exe tests\test_dungeon.c src\dd_dungeon.c src\dd_dungeon_owners.c src\dd_dungeon_wire.c src\dd_crypto_win.c /link bcrypt.lib
if errorlevel 1 goto failed
build\win\dungeon-tests\test_dungeon.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
