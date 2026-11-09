@echo off
setlocal
pushd "%~dp0.."
if not exist build\win\overlay-tests mkdir build\win\overlay-tests
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Isrc /Iruntime /Fobuild\win\overlay-tests\ /Febuild\win\overlay-tests\test_sdl_proxy.exe tests\test_sdl_proxy.c /link user32.lib
if errorlevel 1 goto failed
build\win\overlay-tests\test_sdl_proxy.exe
if errorlevel 1 goto failed
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Isrc /Iruntime /Fobuild\win\overlay-tests\ /Febuild\win\overlay-tests\test_overlay.exe tests\test_overlay.c src\dd_log.c /link opengl32.lib user32.lib gdi32.lib
if errorlevel 1 goto failed
build\win\overlay-tests\test_overlay.exe build\win\overlay-tests\heroes.bmp
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
