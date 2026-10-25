@echo off
setlocal
call "E:\Apps\Visual Studio IDE\VC\Auxiliary\Build\vcvarsall.bat" x64 || exit /b 1

set SRC_DIR=%~dp0
set OUT_DIR=%SRC_DIR%..\..\..\local\installs\instance1\Binaries\Win64

echo [1/2] Generating import libs (kernel32, ws2_32)...
lib /nologo /def:"%SRC_DIR%kernel32.def" /machine:x64 /out:"%SRC_DIR%kernel32.lib" || exit /b 1
lib /nologo /def:"%SRC_DIR%ws2_32.def" /machine:x64 /out:"%SRC_DIR%ws2_32.lib" || exit /b 1

echo [2/2] Compiling dinput8.dll proxy...
cl /nologo /O2 /LD /GS- /W3 ^
   "%SRC_DIR%dinput8proxy.c" ^
   "%SRC_DIR%coop_net.c" ^
   "%SRC_DIR%coop_session.c" ^
   "%SRC_DIR%coop_relay.c" ^
   "%SRC_DIR%coop_interp.c" ^
   "%SRC_DIR%kernel32.lib" ^
   "%SRC_DIR%ws2_32.lib" ^
   /link /DEF:"%SRC_DIR%dinput8proxy.def" ^
   /NODEFAULTLIB ^
   /ENTRY:DllMain ^
   /OUT:"%OUT_DIR%\dinput8.dll"

if %ERRORLEVEL% EQU 0 (
    echo [BUILD SUCCESS] Successfully compiled and deployed dinput8.dll to %OUT_DIR%
) else (
    echo [BUILD ERROR] Compilation failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
