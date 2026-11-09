@echo off
setlocal
rem Run in a Visual Studio 2022 x64 Native Tools Command Prompt.
pushd "%~dp0.."
if not exist build\win\runtime mkdir build\win\runtime
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Isrc /Iruntime /LD runtime\plugin.c runtime\dd_runtime.c runtime\dd_overlay.c src\dd_shop.c src\dd_recruit.c src\dd_hamlet.c src\dd_dungeon_owners.c src\dd_provisions.c src\dd_supplies.c src\dd_addr.c src\dd_log.c src\dd_config.c src\dd_saves.c src\dd_proto.c src\dd_town.c src\dd_session.c src\dd_crypto_win.c src\dd_net.c /Fobuild\win\runtime\ /Fe:build\win\ddcoop-runtime.dll /link bcrypt.lib ws2_32.lib user32.lib gdi32.lib opengl32.lib
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
