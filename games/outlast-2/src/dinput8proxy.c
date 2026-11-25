#include "win_min.h"
#include "coop_protocol.h"

#pragma function(memcpy, memset)
void* memcpy(void* dest, const void* src, size_t count)
{
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (count--) *d++ = *s++;
    return dest;
}

void* memset(void* dest, int c, size_t count)
{
    char* d = (char*)dest;
    while (count--) *d++ = (char)c;
    return dest;
}

// Forward declaration of DirectInput8Create
typedef HRESULT (WINAPI *fn_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, void*);
static fn_DirectInput8Create g_pRealDirectInput8Create = NULL;

static char g_logPath[260] = "outlast2_coop.log";

static size_t str_len(const char* s)
{
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void str_cat(char* dst, const char* src, size_t max_len)
{
    size_t dlen = str_len(dst);
    size_t slen = str_len(src);
    if (dlen + slen + 1 > max_len) return;
    for (size_t i = 0; i <= slen; i++) {
        dst[dlen + i] = src[i];
    }
}

static void log_write(const char* msg)
{
    HANDLE hFile = CreateFileA(g_logPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    SetFilePointer(hFile, 0, NULL, FILE_END);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char header[64];
    // Simple manual formatting: [HH:MM:SS] 
    header[0] = '[';
    header[1] = '0' + (st.wHour / 10); header[2] = '0' + (st.wHour % 10); header[3] = ':';
    header[4] = '0' + (st.wMinute / 10); header[5] = '0' + (st.wMinute % 10); header[6] = ':';
    header[7] = '0' + (st.wSecond / 10); header[8] = '0' + (st.wSecond % 10); header[9] = ']';
    header[10] = ' '; header[11] = '\0';

    DWORD written = 0;
    WriteFile(hFile, header, (DWORD)str_len(header), &written, NULL);
    WriteFile(hFile, msg, (DWORD)str_len(msg), &written, NULL);
    WriteFile(hFile, "\r\n", 2, &written, NULL);
    CloseHandle(hFile);
}

static void log_hex(const char* prefix, uintptr_t val)
{
    char buf[128];
    size_t i = 0;
    while (prefix && prefix[i] && i < 64) {
        buf[i] = prefix[i];
        i++;
    }
    buf[i++] = '0'; buf[i++] = 'x';
    for (int shift = 60; shift >= 0; shift -= 4) {
        int nibble = (int)((val >> shift) & 0xF);
        buf[i++] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    buf[i] = '\0';
    log_write(buf);
}

static void log_hex_bytes(const char* prefix, const void* p, int len)
{
    char buf[1024];
    size_t i = 0;
    while (prefix && prefix[i] && i < 64) {
        buf[i] = prefix[i];
        i++;
    }
    buf[i++] = '0'; buf[i++] = 'x';
    const unsigned char* b = (const unsigned char*)p;
    for (int j = 0; j < len; j++) {
        unsigned int byte = b[j];
        buf[i++] = (char)(byte < 10) ? ('0' + byte) : ('A' + (byte - 10));
        if (j < len - 1) {
            buf[i++] = ' ';
        }
    }
    buf[i] = '\0';
    log_write(buf);
}

// Global hook variables for ProcessEvent
// Target RVA: 0x4DA80
typedef void (__fastcall *fn_ProcessEvent)(void* pObject, void* pFunction, void* pParms, void* pResult);
static fn_ProcessEvent g_originalProcessEvent = NULL;
static volatile unsigned long g_processEventCount = 0;

// Minimal 64-bit trampoline buffer (allocated in 2GB range of target)
static BYTE* g_trampoline = NULL;

// ================= Phase 3 prep: world actor dump & puppet spawn scaffolding =================
// NOTE: The global UObject array accessor (GObject/GNames) for Outlast 2's UE3 build is
// not yet resolved at runtime (see NOTES.md §6). Phase 3 scaffolding consumes actor
// pointers surfaced through the ProcessEvent hook and is wired to the first captured
// ProcessEvent context. Do not hardcode global addresses before runtime verification.

static int g_coopPhase = 0; // 0 = solo testing, 1 = host, 2 = client
static void* g_coopHeroObject = NULL; // local hero (OLHero) object, host-side
static void* g_coopRemotePuppets[8] = { NULL };
static int g_coopPuppetCount = 0;

// Phase 3: local player state buffer, filled from the local pawn once identified
// via captured ProcessEvent context; broadcast at COOP_TICK_RATE to the relay.
static CoopPlayerState g_localPlayerState = {0};
static unsigned int g_localStateSeq = 0;

static CoopPlayerState* NextLocalState(void)
{
    g_localStateSeq++;
    g_localPlayerState.hdr.magic = COOP_MAGIC;
    g_localPlayerState.hdr.version = COOP_PROTO_VERSION;
    g_localPlayerState.hdr.type = COOP_PKT_PLAYER_STATE;
    g_localPlayerState.hdr.seq = g_localStateSeq;
    return &g_localPlayerState;
}

static void LogActorContext(const char* tag, void* pActor)
{
    if (!pActor) return;
    log_hex(tag, (uintptr_t)pActor);
    log_hex_bytes(tag, pActor, 32);
}

// Phase 3: world actor dump. Entry requires the resolved UObject array accessor:
//   - UObject** GObject array + count (name/address TBD - verify at runtime)
//   - For UE3-style builds the array may also be reachable via UClass children/outer.
// void DumpWorldActors(void* pArray, int nCount) { ... }
static void DumpWorldActorsNow(void)
{
    log_write("[PHASE3] DumpWorldActors invoked (scaffolding only - array accessor unresolved).");
}

// Phase 3: remote puppet spawn. Host spawns a puppet OLHero for a remote slot;
// client feeds input. Class spawning goes through the engine (World->SpawnActor),
// address to be resolved once captured.
static void RequestRemotePuppetSpawn(int slot, void* pSpawnClass)
{
    if (g_coopPhase < 1) return; // solo phase only
    if (slot < 0 || slot >= 8) return;
    log_write("[PHASE3] Remote puppet spawn requested (host authoritative).");
    g_coopRemotePuppets[slot] = pSpawnClass; // placeholder until real spawn path is wired
    g_coopPuppetCount++;
}

static void __fastcall Hook_ProcessEvent(void* pObject, void* pFunction, void* pParms, void* pResult)
{
    g_processEventCount++;
    if (g_processEventCount == 1) {
        log_write("[HOOK] *** FIRST ProcessEvent call intercepted successfully! ***");
        log_hex("[HOOK] Target Object: ", (uintptr_t)pObject);
        log_hex("[HOOK] Target Function: ", (uintptr_t)pFunction);
        log_hex_bytes("[HOOK] Object header (first 32 bytes): ", pObject, 32);
        log_hex_bytes("[HOOK] Function header (first 32 bytes): ", pFunction, 32);
    } else if (g_processEventCount == 100 || g_processEventCount == 1000 || (g_processEventCount % 10000) == 0) {
        log_hex("[HOOK] ProcessEvent tick count: ", g_processEventCount);
    }

    if (g_originalProcessEvent) {
        g_originalProcessEvent(pObject, pFunction, pParms, pResult);
    }
}

// Function to install hook at target RVA 0x4DA80
static void InstallProcessEventHook(HMODULE hExe)
{
    BYTE* target = (BYTE*)hExe + 0x4DA80;
    log_hex("[HOOK] Target function address: ", (uintptr_t)target);

    // Verify first 5 prologue bytes: 48 8b c4 48 89
    if (target[0] != 0x48 || target[1] != 0x8B || target[2] != 0xC4 || target[3] != 0x48 || target[4] != 0x89) {
        log_write("[HOOK ERROR] Target prologue bytes mismatch! Refusing to hook.");
        return;
    }
    log_write("[HOOK] Prologue bytes verified: 48 8B C4 48 89.");

    // Allocate trampoline buffer within 2GB of target (or 64-bit absolute)
    // 64-bit absolute jmp pattern:
    // FF 25 00 00 00 00 [8-byte target address]
    // Total: 14 bytes
    g_trampoline = (BYTE*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampoline) {
        log_write("[HOOK ERROR] VirtualAlloc for trampoline failed.");
        return;
    }

    // Target overwritten instructions:
    // 48 8b c4          mov rax, rsp (3 bytes)
    // 48 89 50 10       mov [rax+10h], rdx (4 bytes)
    // 56                push rsi (1 byte)
    // 57                push rdi (1 byte)
    // 41 54             push r12 (2 bytes)
    // 41 56             push r14 (2 bytes)
    // 41 57             push r15 (2 bytes)
    // Total stolen: 15 bytes! (prologue clean boundary)

    // Copy the 15 stolen bytes into trampoline
    for (int i = 0; i < 15; i++) {
        g_trampoline[i] = target[i];
    }
    // Trampoline then jumps back to target + 15
    uintptr_t resumeAddr = (uintptr_t)(target + 15);
    g_trampoline[15] = 0xFF; // jmp [rip+0]
    g_trampoline[16] = 0x25;
    g_trampoline[17] = 0x00;
    g_trampoline[18] = 0x00;
    g_trampoline[19] = 0x00;
    g_trampoline[20] = 0x00;
    *(uintptr_t*)(&g_trampoline[21]) = resumeAddr;

    g_originalProcessEvent = (fn_ProcessEvent)g_trampoline;

    // Patch target with jump to Hook_ProcessEvent (14 bytes)
    DWORD oldProtect = 0;
    if (VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        target[0] = 0xFF; // jmp [rip+0]
        target[1] = 0x25;
        target[2] = 0x00;
        target[3] = 0x00;
        target[4] = 0x00;
        target[5] = 0x00;
        *(uintptr_t*)(&target[6]) = (uintptr_t)&Hook_ProcessEvent;
        target[14] = 0x90; // NOP the 15th byte

        VirtualProtect(target, 16, oldProtect, &oldProtect);
        log_write("[HOOK SUCCESS] ProcessEvent hooked with 14-byte 64-bit absolute jump!");
    } else {
        log_write("[HOOK ERROR] VirtualProtect on target failed.");
    }
}

static DWORD WINAPI ModWorkerThread(LPVOID param)
{
    Sleep(800); // Wait for modules to settle

    HMODULE hExe = GetModuleHandleA(NULL);
    log_write("=================================================");
    log_write("Outlast 2 Co-op Mod - Stage 1 Proxy DLL Active");
    log_hex("Module Base Address: ", (uintptr_t)hExe);
    log_hex("Process ID: ", GetCurrentProcessId());
    log_write("=================================================");

    InstallProcessEventHook(hExe);
    return 0;
}

static BOOL LoadRealDirectInput8(void)
{
    if (g_pRealDirectInput8Create)
        return TRUE;

    char sysDir[260];
    UINT len = GetSystemDirectoryA(sysDir, 260);
    if (len == 0 || len >= 260)
        return FALSE;

    str_cat(sysDir, "\\dinput8.dll", 260);
    HMODULE hReal = LoadLibraryA(sysDir);
    if (!hReal)
        return FALSE;

    g_pRealDirectInput8Create = (fn_DirectInput8Create)GetProcAddress(hReal, "DirectInput8Create");
    return (g_pRealDirectInput8Create != NULL);
}

// Exported DirectInput8Create
__declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
    LPVOID *ppvOut, void* punkOuter)
{
    if (!LoadRealDirectInput8())
        return E_FAIL;
    return g_pRealDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);

        char modPath[260];
        if (GetModuleFileNameA(hinstDLL, modPath, 260) > 0) {
            char* p = NULL;
            for (int i = 0; modPath[i]; i++) {
                if (modPath[i] == '\\' || modPath[i] == '/') p = &modPath[i];
            }
            if (p) {
                *(p + 1) = '\0';
                g_logPath[0] = '\0';
                str_cat(g_logPath, modPath, 260);
                str_cat(g_logPath, "outlast2_coop.log", 260);
            }
        }

        log_write("[INIT] Outlast 2 Co-op Proxy DLL (dinput8.dll) attached.");
        CreateThread(NULL, 0, ModWorkerThread, NULL, 0, NULL);
    }
    return TRUE;
}
