#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "minhook/include/MinHook.h"

// Forward declaration of DirectInput8Create
typedef HRESULT (WINAPI *fn_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
static fn_DirectInput8Create g_pRealDirectInput8Create = NULL;

static char g_logPath[MAX_PATH] = "outlast2_coop.log";
static FILE* g_logFile = NULL;
static CRITICAL_SECTION g_logLock;

void Log(const char* fmt, ...)
{
    EnterCriticalSection(&g_logLock);
    if (!g_logFile) {
        g_logFile = fopen(g_logPath, "a");
    }
    if (g_logFile) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(g_logFile, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        va_list args;
        va_start(args, fmt);
        vfprintf(g_logFile, fmt, args);
        va_end(args);
        fprintf(g_logFile, "\n");
        fflush(g_logFile);
    }
    LeaveCriticalSection(&g_logLock);
}

// Function signature for UE3 ProcessEvent / Script dispatch
// In 64-bit UE3:
// RCX = UObject* this
// RDX = UFunction* pFunction
// R8  = void* Parms
// R9  = void* Result
typedef void (__fastcall *fn_ProcessEvent)(void* pObject, void* pFunction, void* pParms, void* pResult);
static fn_ProcessEvent g_originalProcessEvent = NULL;

static volatile LONG g_eventCount = 0;

void __fastcall Hooked_ProcessEvent(void* pObject, void* pFunction, void* pParms, void* pResult)
{
    LONG count = InterlockedIncrement(&g_eventCount);
    if (count == 1 || count == 100 || count == 1000 || (count % 10000) == 0) {
        Log("[HOOK] ProcessEvent intercepted! Total calls: %ld, Object: %p, Function: %p", count, pObject, pFunction);
    }

    g_originalProcessEvent(pObject, pFunction, pParms, pResult);
}

static DWORD WINAPI ModInitThread(LPVOID param)
{
    Sleep(1000); // Allow PE loader and CRT to finish initializing

    HMODULE hExe = GetModuleHandleA(NULL);
    Log("==================================================");
    Log("Outlast 2 Co-op Mod - Injector & Hook Initialized");
    Log("Base Address: %p, Process ID: %lu", (void*)hExe, GetCurrentProcessId());
    Log("==================================================");

    // Initialize MinHook
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        Log("[ERROR] Failed to initialize MinHook: %d", (int)status);
        return 1;
    }
    Log("[INFO] MinHook initialized successfully.");

    // Target RVA 0x4DA80 (ProcessEvent / Script Execution Dispatcher)
    uintptr_t targetAddr = (uintptr_t)hExe + 0x4DA80;
    Log("[INFO] Hooking ProcessEvent at RVA 0x4DA80 (VA: %p)...", (void*)targetAddr);

    status = MH_CreateHook((LPVOID)targetAddr, (LPVOID)&Hooked_ProcessEvent, (LPVOID*)&g_originalProcessEvent);
    if (status != MH_OK) {
        Log("[ERROR] MH_CreateHook failed with status %d", (int)status);
    } else {
        status = MH_EnableHook((LPVOID)targetAddr);
        if (status != MH_OK) {
            Log("[ERROR] MH_EnableHook failed with status %d", (int)status);
        } else {
            Log("[SUCCESS] ProcessEvent hook enabled successfully at %p!", (void*)targetAddr);
        }
    }

    Log("[INFO] Mod thread active. Monitoring engine events...");
    return 0;
}

static BOOL LoadRealDirectInput8(void)
{
    if (g_pRealDirectInput8Create)
        return TRUE;

    char sysDir[MAX_PATH];
    UINT len = GetSystemDirectoryA(sysDir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return FALSE;

    strcat_s(sysDir, MAX_PATH, "\\dinput8.dll");
    HMODULE hReal = LoadLibraryA(sysDir);
    if (!hReal) {
        return FALSE;
    }

    g_pRealDirectInput8Create = (fn_DirectInput8Create)GetProcAddress(hReal, "DirectInput8Create");
    return (g_pRealDirectInput8Create != NULL);
}

// Exported DirectInput8Create
extern "C" HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
    LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
    if (!LoadRealDirectInput8())
        return E_FAIL;
    return g_pRealDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        InitializeCriticalSection(&g_logLock);

        char modPath[MAX_PATH];
        if (GetModuleFileNameA(hinstDLL, modPath, MAX_PATH) > 0) {
            char* p = strrchr(modPath, '\\');
            if (p) {
                *(p + 1) = '\0';
                strcpy_s(g_logPath, MAX_PATH, modPath);
                strcat_s(g_logPath, MAX_PATH, "outlast2_coop.log");
            }
        }

        Log("[INIT] Outlast 2 Co-op Proxy DLL (dinput8.dll) attached to PID %lu", GetCurrentProcessId());
        CreateThread(NULL, 0, ModInitThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        Log("[EXIT] Outlast 2 Co-op Proxy DLL detaching.");
        if (g_logFile) {
            fclose(g_logFile);
            g_logFile = NULL;
        }
        DeleteCriticalSection(&g_logLock);
    }
    return TRUE;
}
