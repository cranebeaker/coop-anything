/*
 * Divinity II: Developer's Cut - Stage 1: dinput8.dll proxy DLL.
 *
 * Purpose: load-injection without an external injector. The game statically
 * imports exactly one function from DINPUT8.dll (DirectInput8Create), so this
 * proxy sits in the game's bin\ folder, loads the real system dinput8.dll and
 * forwards the call. On PROCESS_ATTACH it logs a marker to dinput8proxy.log
 * next to the DLL - proving injection works before we add MinHook hooks.
 *
 * Build (32-bit target - the game is PE32/i386) with llvm-mingw i686 gcc:
 *   i686-w64-mingw32-gcc -shared -o dinput8.dll dinput8proxy.c dinput8proxy.def -luser32
 *
 * Export-name / calling-convention problem and solution:
 *   The game imports the UNDECORATED name "DirectInput8Create" and CALLS it
 *   stdcall (callee pops the 5x4=20 argument bytes). With TinyCC this was
 *   impossible: tcc decorates stdcall exports as "_DirectInput8Create@20", its
 *   linker rejects --kill-at, ignores .def aliases, and its inline-asm cannot
 *   emit a working stdcall trampoline (all verified; details in NOTES.md). So we
 *   switched to llvm-mingw (clang) i686 cross compiler. Here the fix is standard:
 *   the implementation is a normal WINAPI (stdcall) function named with a
 *   leading underscore in C (DefDirectInput8Create), and dinput8proxy.def maps
 *   the game-facing export name to it:
 *       EXPORTS DirectInput8Create = _DefDirectInput8Create@20
 *   giving a correctly-named, correctly-conventioned export. The stdcall worker
 *   "ret 20" is emitted natively by the compiler - no inline asm.
 *
 * NEVER place this in the live install; only in test copies (see NOTES.md).
 */
#include <windows.h>

/* Minimal DirectInput8Create types (avoid depending on dinput.h). */
typedef const IID *LPCGUID;

/* Real function pointer, resolved from the system dinput8.dll (stdcall). */
typedef HRESULT (WINAPI *fn_DirectInput8Create)(
    HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

static fn_DirectInput8Create pReal = NULL;
static char g_logPath[MAX_PATH] = "dinput8proxy.log";

static void log_line(const char *msg)
{
    /* Simple CREATE_ALWAYS-style append. FILE_APPEND_DATA-only handles can be
     * finicky about share mode; use generic write + existing-file append. */
    HANDLE f = CreateFileA(g_logPath, FILE_GENERIC_WRITE, 0,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD written = 0;
    WriteFile(f, msg, (DWORD)lstrlenA(msg), &written, NULL);
    CloseHandle(f);
}

static void resolve_log_path(HINSTANCE h)
{
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(h, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return; /* keep the default relative "dinput8proxy.log" */
    /* Trim everything from the last backslash onward, so buf ends with "...bin\". */
    while (n > 0 && buf[n - 1] != '\\' && buf[n - 1] != '/')
        n--;
    buf[n] = '\0';
    /* Ensure a trailing separator is present before appending the log name.
     * (Appending without one produced a bogus "dinput8.dlldinput8proxy.log".) */
    static const char name[] = "dinput8proxy.log";
    if (n + 1 + sizeof(name) < MAX_PATH) {
        if (n > 0 && buf[n - 1] != '\\' && buf[n - 1] != '/') {
            buf[n] = '\\';
            buf[n + 1] = '\0';
        }
        lstrcpyA(g_logPath, buf);
        lstrcatA(g_logPath, name);
    }
}

static int load_real_dinput8(void)
{
    if (pReal)
        return 1;
    char sysdir[MAX_PATH];
    UINT len = GetSystemDirectoryA(sysdir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return 0;
    lstrcatA(sysdir, "\\dinput8.dll");
    HMODULE real = LoadLibraryA(sysdir);
    if (!real)
        return 0;
    pReal = (fn_DirectInput8Create)
        GetProcAddress(real, "DirectInput8Create");
    return pReal != NULL;
}

/* The stdcall implementation the .def exports under the name "DirectInput8Create".
 * The leading underscore in the C name is what the .def's "= _DefDirectInput8Create@20"
 * refers to (i386 Windows C symbols are underscore-prefixed, stdcall adds @20). */
HRESULT WINAPI DefDirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
    LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
    if (!load_real_dinput8()) {
        log_line("dinput8proxy: real dinput8.dll NOT loaded\r\n");
        return E_FAIL;
    }
    return pReal(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        resolve_log_path(h);
        char pidbuf[64];
        wsprintfA(pidbuf, "dinput8proxy: PROCESS_ATTACH pid=%u\r\n",
                  GetCurrentProcessId());
        log_line(pidbuf);
        if (!load_real_dinput8())
            log_line("dinput8proxy: FAILED to load real dinput8.dll\r\n");
        else
            log_line("dinput8proxy: real dinput8.dll loaded, forwarded\r\n");
    } else if (reason == DLL_PROCESS_DETACH) {
        log_line("dinput8proxy: PROCESS_DETACH\r\n");
    }
    return TRUE;
}
