@echo off
setlocal
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
    ) else (
        echo Error: cl.exe not found and standard vcvarsall.bat paths not found. Run from Visual Studio x86 Command Prompt.
        exit /b 1
    )
)

cd /d "%~dp0"
if not exist bin mkdir bin
if not exist obj mkdir obj

cl /nologo /LD /O2 /W3 /MT /TP ^
   src\main.c ^
   minhook\src\buffer.c ^
   minhook\src\hook.c ^
   minhook\src\trampoline.c ^
   minhook\src\hde\hde32.c ^
   /Fo:obj\ ^
   /I dllproxy /I minhook\include ^
   /link ^
   user32.lib ^
   ws2_32.lib ^
   /DEF:dllproxy\version.def ^
   /OUT:bin\version.dll ^
   /NOIMPLIB /NOEXP

if %ERRORLEVEL% equ 0 (
    echo BUILD SUCCESS: bin\version.dll created successfully.
) else (
    echo BUILD FAILED with error %ERRORLEVEL%
)
