@echo off
setlocal
pushd "%~dp0.."
if not defined DD_SDL_ORIGINAL (
  echo Set DD_SDL_ORIGINAL to the verified Steam SDL2 original DLL first.
  goto failed
)
if not exist build\win\bootstrap mkdir build\win\bootstrap
python -I tools\gen_forward_def.py "%DD_SDL_ORIGINAL%" SDL2_orig build\win\bootstrap\sdl2_exports.c --msvc SDL_PollEvent SDL_GL_SwapWindow
if errorlevel 1 goto failed
cl /nologo /std:c11 /O2 /MT /W4 /LD runtime\sdl_proxy.c build\win\bootstrap\sdl2_exports.c /Fobuild\win\bootstrap\ /Febuild\win\bootstrap\SDL2.dll
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
