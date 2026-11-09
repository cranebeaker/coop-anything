@echo off
setlocal
pushd "%~dp0..\.."
if not exist build\win\provisions-watch mkdir build\win\provisions-watch
cl /nologo /std:c11 /O2 /MT /W4 /WX /D_CRT_SECURE_NO_WARNINGS /Isrc /Fobuild\win\provisions-watch\ /Fdbuild\win\provisions-watch\ /Febuild\win\provisions-watch\DDCoop-voorraadmeting.exe testkit\provisions-watch\watch.c src\dd_provisions.c /link bcrypt.lib shell32.lib ole32.lib uuid.lib
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
