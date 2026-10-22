@echo off
rem build.bat           -> build\winmm.dll (release)
rem build.bat research  -> build\winmm.dll with the research commands (/DCOOP_RESEARCH)
rem Needs the MSVC x64 build tools (Visual Studio 2022 or its Build Tools).
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -property installationPath`) do set "VS=%%i"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
if not exist "%~dp0build" mkdir "%~dp0build"
cd /d "%~dp0build"
set DEFS=
if /i "%1"=="research" set DEFS=/DCOOP_RESEARCH
cl /nologo /LD /O2 /MT /EHsc /W3 /std:c++17 %DEFS% ..\src\coop.cpp /Fe:winmm.dll /link user32.lib
