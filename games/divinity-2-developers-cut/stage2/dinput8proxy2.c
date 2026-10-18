/*
 * Divinity II: Developer's Cut - Stage 2: dinput8 proxy + per-instance path hook.
 *
 * Stage 1 (stage1/) proved dinput8.dll-proxy injection works: the game imports one
 * function (DirectInput8Create) from DINPUT8.dll, so a proxy in bin\ loads first,
 * forwards to the real system dinput8, and gives us code-in-process at startup.
 *
 * Stage 2 adds the first real hook: per-instance Profile+Savegames redirection so two
 * game copies on one PC don't clobber each other's settings and saves (needed for
 * split-screen). It uses MinHook (bundled under minhook/) to hook SHGetFolderPathA.
 *
 * WHY hook SHGetFolderPathA (not a hardcoded address):
 *   All three of the game's path builders do: SHGetFolderPathA(csidl) -> PathAppendA
 *   ("Divinity 2\") then an _access/_mkdir-style existence op. This exe build has exactly
 *   3 call sites:
 *     0x653a9a: csidl 0x1c = CSIDL_LOCAL_APPDATA   -> %LOCALAPPDATA%\Divinity 2\
 *                the ONLY one that matters: real Profile\ + Savegames\ live here.
 *                Creates the dir (ground truth: matches profile-backup/).
 *     0x653a3a: csidl 0x23 = CSIDL_COMMON_APPDATA  -> %PROGRAMDATA%\Divinity 2\
 *                observed to hold only Shaders\Generated\*.cache (shared, regenerable).
 *     0x653afa: csidl 0x05 = CSIDL_PERSONAL        -> %USERPROFILE%\Documents\Divinity 2\
 *                a pure existence probe (returns bool, NO mkdir); folder never created
 *                during normal runs, so nothing is written here.
 *   Redirecting 0x23 and 0x05 is therefore harmless but NOT required for isolation; 0x1c
 *   alone gives Profile+Savegames separation. Redirecting all three costs nothing and
 *   guards rare flows. Hardcoding a builder address would break on any other exe build
 *   (Steam/GOG friends); SHGetFolderPathA is a stable shell32 export, so hooking it by
 *   name is robust across builds.
 *
 * NOTE: the detour runs BEFORE the game appends its own folder name, so the token becomes
 * an instance ROOT segment: ...\<base>\<token>\Divinity 2\Profile\ and ...\Savegames\.
 *
 * ENABLE FLAG (keeps normal single-instance play intact):
 *   Reads env var DIV2_INSTANCE. Unset/empty/"0"/invalid -> no redirection (behaves
 *   exactly like Stage 1). Set to a short path-safe token (e.g. "1","2","host","join")
 *   -> every game-folder builder gets "\<token>" appended. Each instance uses its own
 *   DIV2_INSTANCE value, so two copies on one PC never share Profile/Savegames.
 *
 * KNOWN GAME BUG (unrelated to this DLL, found during dual-instance testing):
 *   Launching two instances at the SAME instant can crash one of them at startup with
 *   0xc0000005, fault RVA 0xB1024A. Cause: the main-menu movie player (FUN_00f07670,
 *   "MovieStreamLetter") passes a NULL movie object into the setter FUN_00f10220, which
 *   dereferences it unconditionally. Trigger is startup-timing/video-load contention, not
 *   profiles, shaders, or redirection (verified: both instances had identical profile and
 *   shader state at crash). This DLL's hook is not implicated. See NOTES.md Stage 2.
 *
 * Build (32-bit; the game is PE32/i386) with llvm-mingw i686 gcc - see build.ps1.
 *
 * NEVER place this in the live install; only in test copies (see NOTES.md).
 */
#include <windows.h>
#include <shlobj.h>
#include "minhook/include/MinHook.h"

/* ---------------------------------------------------------------- Forwarding */
/* Real DirectInput8Create (stdcall), resolved from the system dinput8.dll. */
typedef HRESULT (WINAPI *fn_DirectInput8Create)(
    HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

static fn_DirectInput8Create pReal_DI8Create = NULL;
static char g_logPath[MAX_PATH] = "dinput8proxy.log";
static char g_instanceToken[MAX_PATH] = "";   /* non-empty => redirection enabled */

static void log_line(const char *msg)
{
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
        return;
    while (n > 0 && buf[n - 1] != '\\' && buf[n - 1] != '/')
        n--;
    buf[n] = '\0';
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
    if (pReal_DI8Create)
        return 1;
    char sysdir[MAX_PATH];
    UINT len = GetSystemDirectoryA(sysdir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return 0;
    lstrcatA(sysdir, "\\dinput8.dll");
    HMODULE real = LoadLibraryA(sysdir);
    if (!real)
        return 0;
    pReal_DI8Create = (fn_DirectInput8Create)
        GetProcAddress(real, "DirectInput8Create");
    return pReal_DI8Create != NULL;
}

HRESULT WINAPI DefDirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
    LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
    if (!load_real_dinput8())
        return E_FAIL;
    return pReal_DI8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

/* ---------------------------------------------------------------- Path hook */
/* Real SHGetFolderPathA (stdcall): HRESULT(HWND,int,HANDLE,DWORD,LPSTR). */
typedef HRESULT (WINAPI *fn_SHGetFolderPathA)(HWND, int, HANDLE, DWORD, LPSTR);
static fn_SHGetFolderPathA pReal_SHGetFolderPathA = NULL;

/* CSIDL sanity: shlobj.h says CSIDL_LOCAL_APPDATA=0x001c, CSIDL_COMMON_APPDATA=0x0023,
 * CSIDL_PERSONAL=0x0005. I initially had 0x1c/0x23 backwards; the build's macro-
 * redefinition warning caught it. Values are masked with 0x00FF so caller OR-ed flag
 * bits (CSIDL_FLAG_CREATE 0x8000 etc.) still match; CSIDL ids all fit in the low byte.
 *
 * Detour: redirect the three game-folder builders (all of them PathAppend "Divinity 2\"
 * after SHGetFolderPathA returns). Every redirect is logged with its csidl so the test
 * run shows exactly which builders fire (expected: only 0x1c at startup - the other two
 * target folders that do not exist after normal runs). Unknown callers that ask for one
 * of these CSIDLs would also get the token dir; there are no such callers in the exe
 * (3 sites total) and dinput8/d3d9/PhysX do not use SHGetFolderPathA - and the token
 * dir is valid + writable, so worst case is benign and visible in the log. */
static HRESULT WINAPI Detour_SHGetFolderPathA(
    HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath)
{
    HRESULT hr = pReal_SHGetFolderPathA(hwnd, csidl, hToken, dwFlags, pszPath);
    int id = csidl & 0x00FF;
    if (SUCCEEDED(hr) && g_instanceToken[0] &&
        (id == 0x1c || id == 0x23 || id == 0x05)) {
        /* append "\<token>"; the caller then appends "Divinity 2\" itself, so the
         * final layout is <base>\<token>\Divinity 2\Profile. Create <base>\<token>
         * now (single level; the caller's _mkdir then succeeds under it). */
        char tmp[MAX_PATH];
        lstrcpynA(tmp, pszPath, MAX_PATH);
        if (lstrlenA(tmp) + 1 + lstrlenA(g_instanceToken) < MAX_PATH) {
            lstrcatA(tmp, "\\");
            lstrcatA(tmp, g_instanceToken);
            if (!CreateDirectoryA(tmp, NULL) &&
                GetLastError() != ERROR_ALREADY_EXISTS) {
                char m2[160];
                wsprintfA(m2, "dinput8proxy: CreateDirectory '%s' failed err=%u\r\n",
                          tmp, GetLastError());
                log_line(m2);
            }
            lstrcpynA(pszPath, tmp, MAX_PATH);
            char msg[300];
            wsprintfA(msg, "dinput8proxy: SHGetFolderPathA(csidl=0x%02x) -> '%s'\r\n",
                      id, pszPath);
            log_line(msg);
        }
    }
    return hr;
}

static void init_hook(void)
{
    if (MH_Initialize() != MH_OK) {
        log_line("dinput8proxy: MH_Initialize FAILED\r\n");
        return;
    }
    /* Resolve the hook target two ways and prefer GetProcAddress: &SHGetFolderPathA
     * could in principle be a DLL-local import thunk (in which case hooking it would
     * intercept only OUR calls, not the game exe's, which go straight to shell32).
     * Both addresses are logged so the test run can prove they are the same function. */
    LPVOID target = NULL;
    HMODULE s32 = GetModuleHandleA("shell32.dll");
    if (!s32)
        s32 = LoadLibraryA("shell32.dll");     /* our import keeps it loaded anyway */
    LPVOID byProc = s32 ? (LPVOID)GetProcAddress(s32, "SHGetFolderPathA") : NULL;
    LPVOID bySym  = (LPVOID)&SHGetFolderPathA;
    target = byProc ? byProc : bySym;
    char mb[224];
    wsprintfA(mb, "dinput8proxy: hook target shell32!SHGetFolderPathA byProc=%p "
                  "bySym=%p %s\r\n",
              byProc, bySym,
              (byProc && byProc == bySym) ? "MATCH" :
              (byProc ? "(differ - using byProc)" : "(bySym fallback)"));
    log_line(mb);
    if (!target) {
        log_line("dinput8proxy: no SHGetFolderPathA target, hook skipped\r\n");
        return;
    }
    if (MH_CreateHook(target, &Detour_SHGetFolderPathA,
                      (LPVOID*)&pReal_SHGetFolderPathA) != MH_OK) {
        log_line("dinput8proxy: MH_CreateHook(SHGetFolderPathA) FAILED\r\n");
        return;
    }
    if (MH_EnableHook(target) != MH_OK) {
        log_line("dinput8proxy: MH_EnableHook FAILED\r\n");
        return;
    }
    log_line("dinput8proxy: SHGetFolderPathA hook enabled\r\n");
}

static void read_instance_token(void)
{
    char buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("DIV2_INSTANCE", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH && lstrcmpA(buf, "0") != 0) {
        /* Validate: short (keeps MAX_PATH headroom for the game's own suffixes) and
         * path-safe - no separators/wildcards/traversal, so the token can never point
         * outside <base>. Invalid -> redirection OFF (normal single-instance play). */
        int ok = (n <= 48) && lstrcmpA(buf, ".") != 0 && lstrcmpA(buf, "..") != 0;
        for (DWORD i = 0; ok && i < n; i++) {
            char c = buf[i];
            if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|')
                ok = 0;
        }
        if (ok) {
            lstrcpynA(g_instanceToken, buf, MAX_PATH);
            char msg[128];
            wsprintfA(msg, "dinput8proxy: DIV2_INSTANCE='%s' (redirection ON)\r\n",
                      g_instanceToken);
            log_line(msg);
        } else {
            char msg[160];
            wsprintfA(msg, "dinput8proxy: DIV2_INSTANCE='%s' INVALID - redirection "
                      "OFF\r\n", buf);
            log_line(msg);
        }
    } else {
        log_line("dinput8proxy: DIV2_INSTANCE unset (redirection OFF)\r\n");
    }
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
        read_instance_token();
        init_hook();
    } else if (reason == DLL_PROCESS_DETACH) {
        log_line("dinput8proxy: PROCESS_DETACH\r\n");
    }
    return TRUE;
}
