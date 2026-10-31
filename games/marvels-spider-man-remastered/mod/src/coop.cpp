// Spider-Man Remastered LAN co-op waker (winmm.dll proxy). Build 23986256 only.
// Turns on the shipped-but-disabled Net::SessionLan path and exposes host/browse/join.
// Commands: write one of  host | browse | join [n] | status | leave | clients | stacks  into coop_cmd_<pid>.txt
// next to the exe (log: coop_log_<pid>.txt). Keys: F7 host (at the title screen), F9 join (in the city),
// F8 warp to partner, F6 status.
#include <winsock2.h>  // before windows.h (which would pull in winsock 1)
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <share.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <math.h>
#pragma comment(lib, "ws2_32.lib")
#include "exports.h"  // forward every winmm export to winmm_sys.dll (a copy of the system DLL)

static const uint32_t kExeTimestamp = 0x6a43cde7;  // build 23986256
static const uint64_t kImageBase = 0x140000000ULL;

static uint8_t* g_base;
static char g_dir[MAX_PATH];
static FILE* g_log;
static CRITICAL_SECTION g_logLock;

#define VA(x) (g_base + ((uint64_t)(x) - kImageBase))

static void Log(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    size_t n = strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
    EnterCriticalSection(&g_logLock);
    if (g_log) {
        fprintf(g_log, "%8lu %s\n", GetTickCount(), buf);
        fflush(g_log);
    }
    LeaveCriticalSection(&g_logLock);
}

// ---- engine printf (stripped to a stub in retail) -> our log ----
static int FormatGuarded(char* buf, size_t n, const char* fmt, va_list ap) {
    __try {
        return vsnprintf(buf, n, fmt, ap);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        _snprintf_s(buf, n, _TRUNCATE, "<bad format> %.200s", fmt);
        return -1;
    }
}

static void EngineLog(const char* fmt, ...) {
    if (!fmt) return;
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    FormatGuarded(buf, sizeof buf, fmt, ap);
    va_end(ap);
    Log("[game] %s", buf);
}

// ---- patching ----
static bool Patch(uint64_t va, const void* expect, const void* repl, size_t n, const char* what) {
    uint8_t* p = VA(va);
    if (memcmp(p, expect, n) != 0) {
        Log("patch %s at %llx: bytes differ, skipped", what, va);
        return false;
    }
    DWORD old;
    VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(p, repl, n);
    VirtualProtect(p, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, n);
    Log("patch %s at %llx: ok", what, va);
    return true;
}

// WAM::HeroWAMTargetPicker activate (0x1408c7720) resolves its owner actor handle and reads two
// more handles off it without a null check. On a LAN client spawning into the host's world the
// owner is not there yet and the client crashed. Redo the tail of that function with checks.
typedef uint8_t*(__fastcall* HandleLookupFn)(void* handle);

static void __fastcall WamPickerTail(uint8_t* self, uint8_t* owner) {
    static volatile LONG skipped;
    if (!owner) {
        if (InterlockedIncrement(&skipped) <= 5) Log("wam-picker: owner actor missing, skipped");
        return;
    }
    HandleLookupFn lookup = (HandleLookupFn)VA(0x1416798f0);
    if (uint8_t* a = lookup(owner + 0x180)) *(uint32_t*)(self + 0x5c) = *(uint32_t*)(a + 0x14);
    if (uint8_t* b = lookup(owner + 0x184)) *(uint32_t*)(self + 0x60) = *(uint32_t*)(b + 0x14);
}

static void PatchWamPicker() {
    // mov rcx,rdi ; mov rdx,rax ; mov rax,WamPickerTail ; call rax ; mov rax,resume ; jmp rax
    uint8_t* stub = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) return;
    uint64_t fn = (uint64_t)&WamPickerTail, resume = (uint64_t)VA(0x1408c7879);
    uint8_t code[] = {0x48, 0x89, 0xf9, 0x48, 0x89, 0xc2, 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xd0,
                      0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xe0};
    memcpy(code + 8, &fn, 8);
    memcpy(code + 20, &resume, 8);
    memcpy(stub, code, sizeof code);
    // mov rbx,rax ; lea rcx,[rax+180h] ; call ... (first 12 bytes)  ->  mov rax,stub ; jmp rax
    static const uint8_t orig[] = {0x48, 0x8b, 0xd8, 0x48, 0x8d, 0x88, 0x80, 0x01, 0x00, 0x00, 0xe8, 0x8f};
    uint8_t jmp[12] = {0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xe0};
    uint64_t s = (uint64_t)stub;
    memcpy(jmp + 2, &s, 8);
    Patch(0x1408c7852, orig, jmp, 12, "wam-picker-null");
}

// Several hero components assume parts that a client-side hero does not have and read through
// null. While we find them all, selected call sites run their callee under SEH: log, return 0,
// carry on. Only for callees with up to four integer/pointer arguments.
static const char* RttiName(void* obj);
typedef uint64_t(__fastcall* Call4Fn)(void*, void*, void*, void*);
static uint64_t g_guardTarget[8];
static const char* g_guardName[8];

template <int I>
static uint64_t __fastcall GuardedCall(void* a, void* b, void* c, void* d) {
    __try {
        return ((Call4Fn)g_guardTarget[I])(a, b, c, d);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER
                                                                  : EXCEPTION_CONTINUE_SEARCH) {
        static volatile LONG n;
        if (InterlockedIncrement(&n) <= 20)
            Log("guard %s: access violation in %s (%p), skipped", g_guardName[I], RttiName(a), a);
        return 0;
    }
}

static const char* RttiName(void* obj) {
    __try {
        uint8_t* col = *(uint8_t**)(*(uint8_t**)obj - 8);
        uint8_t* td = g_base + *(uint32_t*)(col + 12);
        return (const char*)(td + 16);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return "?";
    }
}

// rel32 calls need a target within 2 GB of the exe: put a tiny absolute-jump stub there.
static uint8_t* NearStub(uint64_t target) {
    static uint8_t* page;
    static size_t used;
    if (!page) {
        for (uint8_t* p = g_base - 0x10000; !page && p > g_base - 0x70000000; p -= 0x10000)
            page = (uint8_t*)VirtualAlloc(p, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!page) return nullptr;
    }
    uint8_t* s = page + used;
    used += 16;
    s[0] = 0x48, s[1] = 0xb8;  // mov rax, target ; jmp rax
    memcpy(s + 2, &target, 8);
    s[10] = 0xff, s[11] = 0xe0;
    return s;
}

// Point the rel32 call at `site` (which must call `callee`) at our function `fn`.
static void RedirectCall(uint64_t site, uint64_t callee, void* fn, const char* name) {
    uint8_t* stub = NearStub((uint64_t)fn);
    if (!stub) return;
    uint8_t orig[5] = {0xe8}, call[5] = {0xe8};
    int32_t rel = (int32_t)(VA(callee) - (VA(site) + 5));
    memcpy(orig + 1, &rel, 4);
    rel = (int32_t)(stub - (VA(site) + 5));
    memcpy(call + 1, &rel, 4);
    Patch(site, orig, call, 5, name);
}

template <int I>
static void GuardCallSite(uint64_t site, uint64_t callee, const char* name) {
    g_guardTarget[I] = (uint64_t)VA(callee);
    g_guardName[I] = name;
    RedirectCall(site, callee, (void*)&GuardedCall<I>, name);
}

// WAM target query (0x1408c9f80) resolves the picker's +0x5c handle and passes the result to the
// nearby-targets scan (0x141f24ad0) without a check. Since a reboot, the host's first frame after
// its hero spawns runs this before the picker's activate has filled +0x5c and crashed every time.
typedef void(__fastcall* NearbyTargetsFn)(void* self, uint32_t* count, float radius, char a, char b,
                                          void* out, int max);

static void __fastcall NearbyTargetsSafe(void* self, uint32_t* count, float radius, char a, char b,
                                         void* out, int max) {
    if (!self) {
        static volatile LONG n;
        if (InterlockedIncrement(&n) <= 5) Log("wam-query: picker not set up yet, no targets");
        *count = 0;
        return;
    }
    ((NearbyTargetsFn)VA(0x141f24ad0))(self, count, radius, a, b, out, max);
}

// ---- client tables (probe) ----
// ClientManager (static object 0x1463c8f40): +0x110 -> client table (handle per client index at
// +0x18), +0x118 -> 4 local-player slots of 0x10c (wanted index +0x100, client handle +0x104,
// active +0x108). HeroSystem create (vtable 0x1438a9858 +0xc0 -> 0x14083f430) gives a hero the
// HeroLocal components only when the table handle for its client index equals a local slot handle.
static void DescribeHandle(char* out, size_t n, uint32_t h) {
    __try {
        uint8_t* obj = ((HandleLookupFn)VA(0x1416798f0))(&h);
        if (!obj) _snprintf_s(out, n, _TRUNCATE, "%08x (dead)", h);
        else _snprintf_s(out, n, _TRUNCATE, "%08x %s index %d", h, RttiName(obj), *(int*)(obj + 0x94));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        _snprintf_s(out, n, _TRUNCATE, "%08x (fault)", h);
    }
}

static void DumpClients(const char* tag) {
    uint8_t* mgr = VA(0x1463c8f40);
    uint8_t* tbl = *(uint8_t**)(mgr + 0x110);
    uint8_t* slots = *(uint8_t**)(mgr + 0x118);
    char d[160];
    Log("clients (%s): table %p slots %p created %d", tag, tbl, slots, mgr[0x134]);
    for (int i = 0; tbl && i < 16; i++) {
        uint32_t h = *(uint32_t*)(tbl + 0x18 + i * 4);
        if (!h) continue;
        DescribeHandle(d, sizeof d, h);
        Log("  table[%d] = %s", i, d);
    }
    for (int i = 0; slots && i < 4; i++) {
        uint8_t* s = slots + i * 0x10c;
        DescribeHandle(d, sizeof d, *(uint32_t*)(s + 0x104));
        Log("  local slot %d: active %d index %d handle %s", i, s[0x108], *(int*)(s + 0x100), d);
    }
}

// The is-local check inside HeroSystem create (call at 0x14083f481 -> 0x141efc100, returns the
// local player for a client handle or null). Hooking the create vtable slot instead crashed a
// freshly started host every time, so only this call is redirected, and it logs only on null.
typedef void*(__fastcall* LocalPlayerForFn)(void* mgr, uint32_t* handle);

static void* __fastcall LocalPlayerForProbe(void* mgr, uint32_t* handle) {
    void* r = ((LocalPlayerForFn)VA(0x141efc100))(mgr, handle);
    if (!r) {
        __try {
            char d[160];
            DescribeHandle(d, sizeof d, *handle);
            Log("hero-create: not local, client %s", d);
            DumpClients("hero-create");
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("hero-create: probe faulted");
        }
    }
    return r;
}

// Puppet partner, step 1: the engine's remote hero (HeroRemote) is largely unfinished and crashes the
// client, so never spawn one. Both spawn paths (all clients 0x141f22710, respawn 0x141f22510) queue a
// 0x60-byte request (client index at +0x4c) with 0x141f65320; drop it unless that client is local.
typedef uint64_t(__fastcall* QueueSpawnFn)(void* req, uint64_t size, void* cb, void* arg);
typedef void(__fastcall* ClientHandleFn)(void* mgr, uint32_t* out, int index);

static uint64_t __fastcall SpawnRequestFilter(uint8_t* req, uint64_t size, void* cb, void* arg) {
    uint8_t* mgr = VA(0x1463c8f40);
    int index = *(int*)(req + 0x4c);
    uint32_t h = 0;
    ((ClientHandleFn)VA(0x141efbe40))(mgr, &h, index);
    if (!((LocalPlayerForFn)VA(0x141efc100))(mgr, &h)) {
        Log("spawn: client %d is remote, no engine hero (puppet instead)", index);
        return 0;
    }
    return ((QueueSpawnFn)VA(0x141f65320))(req, size, cb, arg);
}



static uint8_t* FindComponent(uint8_t* actor, const char* rtti);

#ifdef COOP_RESEARCH
static void ResearchOnHeroCreated(uint8_t* hero);
#endif
typedef void*(__fastcall* HeroCreateFn)(void* sys, uint8_t* req, void* param);
static uint8_t* g_heroSpawnParam;
static uint32_t g_heroSpawnA8;
static uint64_t g_heroSpawnLevel;

static void* __fastcall HeroCreateGate(void* sys, uint8_t* req, void* param) {
    uint8_t* mgr = VA(0x1463c8f40);
    int index = *(int*)(req + 0x4c);
    uint32_t h = 0;
    ((ClientHandleFn)VA(0x141efbe40))(mgr, &h, index);
    if (!((LocalPlayerForFn)VA(0x141efc100))(mgr, &h)) {
        static void* seen[8];
        void* ret = _ReturnAddress();
        bool logged = false;
        for (void*& s : seen) {
            if (s == ret) { logged = true; break; }
            if (!s) { s = ret; break; }
        }
        if (!logged) Log("hero-create: client %d is remote, skipped (caller %llx)", index,
                         (uint64_t)((uint8_t*)ret - g_base) + kImageBase);
        return nullptr;
    }
    g_heroSpawnParam = (uint8_t*)param;  // spawn context, reused for the puppet
    g_heroSpawnA8 = *(uint32_t*)((uint8_t*)param + 4);
    g_heroSpawnLevel = ((uint64_t(__fastcall*)(void*))VA(0x141f7b6e0))(param);
    uint8_t* hero = (uint8_t*)((HeroCreateFn)VA(0x14083f430))(sys, req, param);
#ifdef COOP_RESEARCH
    if (hero) ResearchOnHeroCreated(hero);
#endif
    return hero;
}

static void PatchHeroCreate() {
    void** slot = (void**)VA(0x1438a9858 + 0xc0);
    if (*slot != VA(0x14083f430)) {
        Log("patch hero-create-gate: vtable slot differs, skipped");
        return;
    }
    DWORD old;
    VirtualProtect(slot, 8, PAGE_READWRITE, &old);
    *slot = (void*)&HeroCreateGate;
    VirtualProtect(slot, 8, old, &old);
    Log("patch hero-create-gate: ok");
}

// The spawn-request callback (0x141f23710) runs create, then looks up the local player's hero in
// HeroSystem's slot table (+0x18, 16 x 0x14: client handle, actor handle +4, state via 0x141f231d0)
// and, if create returned that hero, stores its handle at +0x168. On a client that just joined, the
// host's hero arrives before its own: both are null, they "match" and the store reads through null.
// Same logic here, with the null check.
typedef void(__fastcall* LocalClientFn)(void* mgr, uint32_t* out);
typedef int(__fastcall* SlotStateFn)(void* slot);
typedef uint8_t*(__fastcall* ActorFromHandleFn)(void* handle);

static uint8_t* __fastcall SpawnCallback(void* req, void* param) {
    uint8_t* sys = *(uint8_t**)VA(0x147c307d8);
    uint8_t* hero = (uint8_t*)HeroCreateGate(sys, (uint8_t*)req, param);
    if (!hero) return nullptr;
    uint32_t local = 0;
    ((LocalClientFn)VA(0x141efbe90))(VA(0x1463c8f40), &local);
    uint8_t* mine = nullptr;
    for (int i = 0; i < 16; i++) {
        uint8_t* slot = sys + 0x18 + i * 0x14;
        if (*(uint32_t*)slot == local && ((SlotStateFn)VA(0x141f231d0))(slot) != 3) {
            mine = ((ActorFromHandleFn)VA(0x1415a0560))(slot + 4);
            break;
        }
    }
    if (hero == mine)
        *(uint32_t*)(sys + 0x168) = (*(uint16_t*)(hero + 0x10) & 0x7ff) << 20 | (*(uint32_t*)(hero + 0xc) & 0xfffff);
    return hero;
}

static void PatchSpawnCallback() {
    static const uint8_t orig[] = {0x41, 0x56, 0x48, 0x83, 0xec, 0x40, 0x48, 0x89, 0x5c, 0x24, 0x50, 0x4c};
    uint8_t jmp[12] = {0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xe0};  // mov rax, fn ; jmp rax
    uint64_t fn = (uint64_t)&SpawnCallback;
    memcpy(jmp + 2, &fn, 8);
    Patch(0x141f23710, orig, jmp, 12, "spawn-callback");
}

// ---- puppet partner ----
// A plain actor from the hero's asset with no HeroLocal/HeroRemote component, moved by us each frame.
// Spawn mirrors HeroSystem create: roster (0x1408407b0) entry 0 (+0x80 asset name) -> 0x141bb25d0 ->
// 0x14159f3c0 (asset), 0x14159ec90 (0x28-byte spawn params), 0x14190cfa0 (ActorManager 0x147a42f80,
// out handle), 0x1415a0560 (handle -> actor), 0x14159db20 (activate). Actor +0 -> transform node: 4x4
// row-major matrix, position at +0x30; set with 0x14191c0e0(node, matrix, node + 0x70).
// Everything runs on the game thread from the per-frame hero update (call 0x140844d86 -> 0x1408445b0).
typedef void*(__fastcall* PtrFn)();
typedef uint8_t*(__fastcall* RosterEntryFn)(void* roster, int index);
typedef uint64_t*(__fastcall* AssetNameFn)(uint64_t* out, uint64_t name);
typedef uint64_t(__fastcall* AssetFn)(void* table, uint64_t name);
typedef void*(__fastcall* SpawnParamsFn)(uint64_t asset, void* params);
typedef uint32_t*(__fastcall* SpawnActorFn)(void* mgr, uint32_t* out, void* params);
typedef void(__fastcall* ActivateFn)(void* actor);
typedef void(__fastcall* SetTransformFn)(float* node, const float* m, void* extra);

static volatile LONG g_puppetRequest;
static uint32_t g_puppet;  // actor handle, 0 = none
static float g_puppetOffset[3] = {2.0f, 0.0f, 0.0f};

static uint8_t* LocalHero() {
    uint8_t* sys = *(uint8_t**)VA(0x147c307d8);
    if (!sys || !*(uint32_t*)(sys + 0x168)) return nullptr;
    return ((ActorFromHandleFn)VA(0x1415a0560))(sys + 0x168);
}

static void SpawnPuppet() {
    uint8_t* roster = (uint8_t*)((PtrFn)VA(0x1408407b0))();
    if (!roster || !*(int*)(roster + 0x30)) {
        Log("puppet: no hero roster");
        return;
    }
    uint8_t* entry = ((RosterEntryFn)VA(0x141183030))(roster, 0);
    uint64_t tmp[2] = {};
    uint64_t* name = ((AssetNameFn)VA(0x141bb25d0))(tmp, *(uint64_t*)(entry + 0x80));
    uint64_t asset = ((AssetFn)VA(0x14159f3c0))(VA(0x1460421e0), *name);
    alignas(16) uint8_t params[0x40] = {};
    ((SpawnParamsFn)VA(0x14159ec90))(asset, params);
    uint32_t h = 0;
    ((SpawnActorFn)VA(0x14190cfa0))(VA(0x147a42f80), &h, params);
    uint8_t* actor = h ? ((ActorFromHandleFn)VA(0x1415a0560))(&h) : nullptr;
    Log("puppet: roster '%s' asset %llx -> handle %08x actor %p", *(char**)(entry + 8), asset, h, actor);
    if (!actor) return;
    // as HeroSystem create does between spawn and activate, with the local hero's spawn context
    Log("puppet: spawn context %p a8 %08x level %llx", g_heroSpawnParam, g_heroSpawnA8, g_heroSpawnLevel);
    ((void(__fastcall*)(void*, uint32_t))VA(0x14159e4a0))(actor, g_heroSpawnA8);
    ((void(__fastcall*)(void*, const char*))VA(0x14159e3f0))(actor, "CoopPartner");
    if (g_heroSpawnLevel) ((void(__fastcall*)(void*, uint64_t))VA(0x14159e590))(actor, g_heroSpawnLevel);
    // Activate as the actor manager's batch pass does (0x1419194e0: begin activate, flags |= 0x2001,
    // pre-activate job, add node to the scene, activate components). Groups of {handles, count}.
    Log("puppet: flags before activate %04x", *(uint16_t*)(actor + 8));
    struct { uint32_t* handles; int64_t count; } group = {&h, 1};
    ((uint64_t(__fastcall*)(void*, void*, uint32_t))VA(0x1419194e0))(VA(0x147a42f80), &group, 1);
    g_puppet = h;
    Log("puppet: activated, flags %04x node %p", *(uint16_t*)(actor + 8), *(void**)actor);
}

static volatile LONG g_ticks, g_infoRequest;


// Find the AttachmentList component and log every actor handle it holds (scans the first 0x200 bytes
// and one level of pointed-to memory for dwords that resolve to live actors).
static uint8_t* FindComponent(uint8_t* actor, const char* rtti) {
    uint16_t n = *(uint16_t*)(actor + 0x70);
    uint8_t* list = *(uint8_t**)(actor + 0x68);
    for (int i = 0; list && i < n; i++) {
        uint8_t* c = *(uint8_t**)(list + i * 16 + 8);
        if (c && !strcmp(RttiName(c), rtti)) return c;
    }
    return nullptr;
}
















// Dress the puppet: Spider-Man's body is built from SkinManager parts (bitset +0x58, 0x6b8-byte entries
// at +0x50). Copy every part the hero has: rebuild the AddPart descriptor (0x1420c8d50) from the hero's
// entry, as the part-load path (0x1420c6890) builds it, with the puppet's node id; then vtable +0x58.
static void DressPuppet(uint8_t* puppet) {
    uint8_t* hero = LocalHero();
    uint8_t* hs = hero ? FindComponent(hero, ".?AVSkinManager@@") : nullptr;
    uint8_t* ps = FindComponent(puppet, ".?AVSkinManager@@");
    uint8_t* pnode = *(uint8_t**)puppet;
    if (!hs || !ps || !pnode) {
        Log("dress: missing skin manager (hero %p puppet %p)", hs, ps);
        return;
    }
    uint32_t count = *(uint32_t*)(hs + 0x4c);
    uint8_t* parts = *(uint8_t**)(hs + 0x50);
    for (uint32_t i = 0; i < count && i < 128; i++) {
        if (!((*(uint64_t*)(hs + 0x58 + (i >> 6) * 8) >> (i & 63)) & 1)) continue;
        uint8_t* e = parts + (uint64_t)i * 0x6b8;
        alignas(16) uint8_t d[0x40] = {};
        *(uint64_t*)(d + 0x00) = *(uint64_t*)(e + 0x3c);
        *(uint32_t*)(d + 0x08) = *(uint32_t*)(pnode + 0x64);
        *(uint64_t*)(d + 0x0c) = *(uint64_t*)(e + 0x44);
        *(uint64_t*)(d + 0x14) = *(uint64_t*)(e + 0x4c);
        *(uint8_t**)(d + 0x20) = e + 0x70;
        d[0x30] = e[0x6b2];
        ((void(__fastcall*)(void*, uint32_t, void*))VA(0x1420c8d50))(ps, i, d);
        void* info = ((void*(__fastcall*)(void*, uint64_t))VA(0x140bcc0e0))(VA(0x146050d30), *(uint64_t*)d);
        if (info) (*(void(__fastcall**)(void*, void*))(*(uint8_t**)ps + 0x58))(ps, info);
        Log("dress: part %u asset %llx -> puppet parts now %llx", i, *(uint64_t*)d, *(uint64_t*)(ps + 0x58));
    }
}

static volatile LONG g_dressRequest, g_priusRequest;

static bool g_puppetPending, g_puppetHidden;

// Joint count of a scene node's skeleton: node +0x88 model -> +0x200 skeleton -> u16 at +2. The node's
// +0xd8 points to that many 4x4 joint matrices (the pose).
static uint32_t JointCount(uint8_t* node) {
    uint8_t* model = *(uint8_t**)(node + 0x88);
    uint8_t* skel = model ? *(uint8_t**)(model + 0x200) : nullptr;
    return skel ? *(uint16_t*)(skel + 2) : 0;
}

static LONG g_dressAt;

// ---- partner sync: our own UDP channel, independent of the game's netcode ----
// The host binds kSyncPort; the client binds any port and sends to host:kSyncPort. The host answers
// whoever sent last. Each side sends its hero's 4x4 matrix at up to 30 Hz.
static const uint16_t kSyncPort = 59010;
static const uint32_t kSyncMagic = 0x50435053;  // "SPCP"
static const int kMaxJoints = 256, kMaxParts = 6;
struct PartDesc {  // one enabled SkinManager part (see DressPuppet)
    uint8_t index, flag, pad[6];
    uint64_t asset, a44, a4c;
    uint64_t look, extra;  // SkinItemPrius (entry +0x70) +0x28 hash and +0x38, which differ per suit
};
struct SyncPacket {
    uint32_t magic, seq;
    uint16_t joints;
    uint8_t parts, pad[5];
    PartDesc part[kMaxParts];
    float m[16];
    float pose[kMaxJoints * 16];  // joint matrices (scene node +0xd8), only `joints` of them are sent
};
static const int kSyncHeader = offsetof(SyncPacket, pose);
static float g_remotePose[kMaxJoints * 16];
static uint16_t g_remoteJoints;
static PartDesc g_remoteParts[kMaxParts];
static uint8_t g_remotePartCount;
static uint64_t g_remotePartsSig, g_appliedPartsSig;

static uint64_t PartsSig(const PartDesc* p, int n) {
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < n; i++) h = (h ^ p[i].index ^ p[i].asset) * 1099511628211ULL;
    return h;
}

// Our hero's enabled skin parts, for the partner's puppet.
static int CollectParts(uint8_t* hero, PartDesc* out) {
    uint8_t* hs = FindComponent(hero, ".?AVSkinManager@@");
    if (!hs) return 0;
    uint32_t count = *(uint32_t*)(hs + 0x4c);
    uint8_t* parts = *(uint8_t**)(hs + 0x50);
    int n = 0;
    for (uint32_t i = 0; i < count && i < 128 && n < kMaxParts; i++) {
        if (!((*(uint64_t*)(hs + 0x58 + (i >> 6) * 8) >> (i & 63)) & 1)) continue;
        uint8_t* e = parts + (uint64_t)i * 0x6b8;
        PartDesc& d = out[n++];
        memset(&d, 0, sizeof d);
        d.index = (uint8_t)i, d.flag = e[0x6b2];
        d.asset = *(uint64_t*)(e + 0x3c), d.a44 = *(uint64_t*)(e + 0x44), d.a4c = *(uint64_t*)(e + 0x4c);
        d.look = *(uint64_t*)(e + 0x98), d.extra = *(uint64_t*)(e + 0xa8);
    }
    return n;
}

// Load one skin item onto a SkinManager the way the game does (suit menu paper doll 0x140822390, remote
// player 0x141f5e8c0): a default SkinItemPrius with only the item's asset at +0x30, handed to 0x1420bf2a0.
// That copies it into a load request, streams the asset and its sub-assets, then adds the part
// (0x1420c5db0 -> 0x1420c6890 -> AddPart 0x1420c8d50) in the slot the asset belongs to.
static void LoadSkinItem(uint8_t* skin, uint64_t asset, void* priusVtable) {
    using CtorFn = void*(__fastcall*)(void*);
    alignas(16) uint8_t p[0x520] = {};
    *(void**)p = priusVtable;
    ((CtorFn)VA(0x1407a6090))(p + 0x40);
    for (int i = 0; i < 4; i++) ((CtorFn)VA(0x1407a60f0))(p + 0x70 + i * 0x40);
    for (int i = 0; i < 6; i++) ((CtorFn)VA(0x1407a60b0))(p + 0x170 + i * 0x48);
    for (int i = 0; i < 4; i++) ((CtorFn)VA(0x1407a6060))(p + 0x320 + i * 0x70);
    ((CtorFn)VA(0x1407a60d0))(p + 0x4e0);
    ((CtorFn)VA(0x142273f20))(p);  // init
    *(uint64_t*)(p + 0x08) |= 2;
    *(uint64_t*)(p + 0x18) |= 2;
    *(uint64_t*)(p + 0x30) = asset;
    ((void(__fastcall*)(void*, void*))VA(0x1420bf2a0))(skin, p);
    ((void(__fastcall*)(void*))VA(0x142274040))(p);  // release
}

// Dress the puppet in the partner's suit: drop parts they don't wear, load the ones that differ.
static void ApplyPartnerSuit(uint8_t* puppet) {
    uint8_t* hero = LocalHero();
    uint8_t* hs = hero ? FindComponent(hero, ".?AVSkinManager@@") : nullptr;
    uint8_t* ps = FindComponent(puppet, ".?AVSkinManager@@");
    if (!hs || !ps || !*(uint8_t**)(hs + 0x50) || !*(uint8_t**)(ps + 0x50)) return;
    void* vt = *(void**)(*(uint8_t**)(hs + 0x50) + 0x70);  // SkinItemPrius vtable, from our part 0's item
    uint8_t* pparts = *(uint8_t**)(ps + 0x50);
    uint32_t count = *(uint32_t*)(ps + 0x4c);
    auto has = [&](uint32_t i) { return ((*(uint64_t*)(ps + 0x58 + (i >> 6) * 8) >> (i & 63)) & 1) != 0; };
    auto assetAt = [&](uint32_t i) { return *(uint64_t*)(pparts + (uint64_t)i * 0x6b8 + 0x3c); };
    for (uint32_t i = 0; i < count && i < 128; i++) {
        if (!has(i)) continue;
        bool keep = false;
        for (int k = 0; k < g_remotePartCount; k++) keep |= g_remoteParts[k].index == i;
        if (!keep) {
            (*(void(__fastcall**)(void*, uint32_t))(*(uint8_t**)ps + 0x88))(ps, i);  // vtable +0x88; did not clear a part in tests
            Log("suit: removed puppet part %u", i);
        }
    }
    for (int k = 0; k < g_remotePartCount; k++) {
        const PartDesc& r = g_remoteParts[k];
        if (r.index < count && has(r.index) && assetAt(r.index) == r.asset) continue;
        LoadSkinItem(ps, r.asset, vt);
        Log("suit: loading partner part %u asset %llx", r.index, r.asset);
    }
}
static SOCKET g_sock = INVALID_SOCKET;
static int g_syncMode;  // 0 off (local follow test), 1 host, 2 client
static sockaddr_in g_peer;
static bool g_havePeer, g_learnPeer;  // learnPeer: client broadcasting until the host's first reply
static float g_remote[16];
static DWORD g_remoteAt, g_lastSend;
static uint32_t g_sendSeq, g_recvSeq, g_recvCount;
static char g_syncArg[64];
static volatile LONG g_syncRequest;

static void SyncStart() {
    if (g_sock != INVALID_SOCKET) closesocket(g_sock), g_sock = INVALID_SOCKET;
    WSADATA wd;
    WSAStartup(MAKEWORD(2, 2), &wd);
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    u_long nb = 1;
    ioctlsocket(g_sock, FIONBIO, &nb);
    int bufsz = 1 << 20;  // pose packets are ~15 KB
    setsockopt(g_sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufsz, sizeof bufsz);
    setsockopt(g_sock, SOL_SOCKET, SO_SNDBUF, (char*)&bufsz, sizeof bufsz);
    sockaddr_in me = {};
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    g_havePeer = false;
    if (!strcmp(g_syncArg, "host") || !strcmp(g_syncArg, "loop")) {
        g_syncMode = 1;
        me.sin_port = htons(kSyncPort);
        if (g_syncArg[0] == 'l') {  // loopback test: receive our own packets, puppet keeps its offset
            g_peer = {};
            g_peer.sin_family = AF_INET;
            g_peer.sin_port = htons(kSyncPort);
            inet_pton(AF_INET, "127.0.0.1", &g_peer.sin_addr);
            g_havePeer = true;
        }
    } else {
        g_syncMode = 2;
        g_peer = {};
        g_peer.sin_family = AF_INET;
        g_peer.sin_port = htons(kSyncPort);
        g_havePeer = inet_pton(AF_INET, g_syncArg, &g_peer.sin_addr) == 1;
        if (!g_havePeer) {  // no address given: broadcast on the LAN until the host answers
            BOOL on = TRUE;
            setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char*)&on, sizeof on);
            g_peer.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            g_havePeer = g_learnPeer = true;
        }
    }
    int r = bind(g_sock, (sockaddr*)&me, sizeof me);
    Log("sync: %s, bind %s, peer %s", g_syncMode == 1 ? "host" : "client", r == 0 ? "ok" : "FAILED",
        g_learnPeer ? "(LAN broadcast until the host answers)" : g_havePeer ? g_syncArg : "(learned from first packet)");
}

static bool SyncTick(uint8_t* hero) {
    if (InterlockedExchange(&g_syncRequest, 0)) SyncStart();
    if (g_sock == INVALID_SOCKET) return false;
    static SyncPacket pk;
    sockaddr_in from;
    int fl = sizeof from, got;
    while ((got = recvfrom(g_sock, (char*)&pk, sizeof pk, 0, (sockaddr*)&from, &fl)) >= kSyncHeader) {
        fl = sizeof from;
        if (pk.magic != kSyncMagic || (int32_t)(pk.seq - g_recvSeq) <= 0 && g_recvCount) continue;
        g_recvSeq = pk.seq;
        memcpy(g_remote, pk.m, sizeof g_remote);
        if (pk.parts <= kMaxParts) {
            memcpy(g_remoteParts, pk.part, sizeof g_remoteParts);
            g_remotePartCount = pk.parts;
            g_remotePartsSig = PartsSig(pk.part, pk.parts);
        }
        if (pk.joints <= kMaxJoints && got >= kSyncHeader + pk.joints * 64) {
            memcpy(g_remotePose, pk.pose, pk.joints * 64);
            g_remoteJoints = pk.joints;
        }
        g_remoteAt = GetTickCount();
        if (++g_recvCount == 1) Log("sync: first packet from partner");
        if (g_syncMode == 2 && g_learnPeer) {
            g_peer = from, g_learnPeer = false;
            char ip[32];
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
            Log("sync: found host at %s", ip);
        }
        if (g_syncMode == 1 && (!g_havePeer || memcmp(&from, &g_peer, sizeof from))) {
            g_peer = from, g_havePeer = true;
            char ip[32];
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
            Log("sync: partner is %s:%u", ip, ntohs(from.sin_port));
        }
    }
    DWORD now = GetTickCount();
    if (g_havePeer && hero && *(float**)hero && now - g_lastSend >= 33) {
        g_lastSend = now;
        pk.magic = kSyncMagic;
        pk.seq = ++g_sendSeq;
        memcpy(pk.m, *(float**)hero, sizeof pk.m);
        pk.parts = (uint8_t)CollectParts(hero, pk.part);
        uint8_t* node = *(uint8_t**)hero;
        uint32_t n = JointCount(node);
        uint8_t* buf = *(uint8_t**)(node + 0xd8);
        pk.joints = (buf && n <= kMaxJoints) ? (uint16_t)n : 0;
        if (pk.joints) memcpy(pk.pose, buf, pk.joints * 64);
        sendto(g_sock, (char*)&pk, kSyncHeader + pk.joints * 64, 0, (sockaddr*)&g_peer, sizeof g_peer);
    }
    return g_recvCount && now - g_remoteAt < 2000;
}



// Warp the local hero, as the game's CoopWarpAction (0x1409ac210) does: a PerformWarpEvent (ctor 0x141b037f0,
// init 0x141b0aa00, flags +8/+0x18 |= 0x6c000, +0x22c/+0x22d = 1, target matrix via 0x141b57e40(ev + 0x138, m, 1))
// posted with 0x141676a50(component, ev), which stamps the component's actor as the event's actor.
#ifdef COOP_RESEARCH
#include "research.inc"
#endif

static volatile LONG g_warpRequest;
static bool g_autoWarp;
static int g_autoWarpTicks;
static float g_warpTarget[16];

static void WarpHero(const float* target) {
    uint8_t* hero = LocalHero();
    uint8_t* comp = hero ? FindComponent(hero, ".?AVHeroWarpProcessor@@") : nullptr;
    if (!comp) {
        Log("warp: no hero / HeroWarpProcessor");
        return;
    }
    alignas(16) static uint8_t ev[0x400];
    memset(ev, 0, sizeof ev);
    ((void*(__fastcall*)(void*))VA(0x141b037f0))(ev);
    ((uint8_t(__fastcall*)(void*))VA(0x141b0aa00))(ev);
    ev[0x22c] = 1;
    ev[0x22d] = 1;
    ev[0x22f] = 0;
    *(uint64_t*)(ev + 0x08) |= 0x6c000;
    *(uint64_t*)(ev + 0x18) |= 0x6c000;
    alignas(16) float m[16];
    memcpy(m, target, sizeof m);
    ((void(__fastcall*)(void*, const float*, int))VA(0x141b57e40))(ev + 0x138, m, 1);
    // Posting through the event system (0x141676a50) never reached the processor, so stamp the
    // sender like it does and call the PerformWarpEvent handler (0x140867520) directly.
    *(uint32_t*)(ev + 0x2c) = (*(uint16_t*)(hero + 0x10) & 0x7ff) << 20 | (*(uint32_t*)(hero + 0xc) & 0xfffff);
    *(uint64_t*)(ev + 0x08) |= 2;
    *(uint64_t*)(ev + 0x18) |= 2;
    ((void(__fastcall*)(void*, void*))VA(0x140867520))(comp, ev);
    ((void(__fastcall*)(void*))VA(0x141b08b00))(ev);
    ((void(__fastcall*)(void*))VA(0x141b03c40))(ev);
    Log("warp: requested to %.1f %.1f %.1f, processor %p pending %u", m[12], m[13], m[14], comp, comp[0xa8]);
#ifdef COOP_RESEARCH
    g_warpWatch = comp;
    g_warpWatchUntil = g_ticks + 600;
#endif
}

static void PuppetTick() {
    InterlockedIncrement(&g_ticks);
#ifdef COOP_RESEARCH
    ResearchTick();
#endif
    if (InterlockedExchange(&g_puppetRequest, 0)) SpawnPuppet();
    if (g_autoWarp && g_recvCount && LocalHero() && ++g_autoWarpTicks > 90) {  // joined: go to the host
        g_autoWarp = false;
        g_warpRequest = 2;
        Log("coop: warping to the host");
    }
    if (LONG w = InterlockedExchange(&g_warpRequest, 0)) {
        uint8_t* h = LocalHero();
        if (w == 1 && h && *(float**)h) {  // relative offset test
            alignas(16) float m[16];
            memcpy(m, *(float**)h, sizeof m);
            for (int i = 0; i < 3; i++) m[12 + i] += g_warpTarget[12 + i];
            WarpHero(m);
        } else if (w == 2 && g_recvCount) {  // next to the partner
            alignas(16) float m[16];
            memcpy(m, g_remote, sizeof m);
            m[12] += 1.5f;
            WarpHero(m);
        } else {
            Log("warp: nothing to warp to");
        }
    }
    if (g_puppet && InterlockedExchange(&g_dressRequest, 0)) {
        uint8_t* a = ((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet);
        if (a) DressPuppet(a);
    }
    uint8_t* hero = LocalHero();
    bool partner = SyncTick(hero);  // true while the other player's hero transform is fresh
    if (partner && !g_puppet && !g_puppetPending) {
        Log("sync: partner seen, spawning puppet");
        SpawnPuppet();
        g_puppetPending = true;
        g_dressAt = g_ticks + 30;
    }
    if (g_puppet && g_dressAt && g_ticks >= g_dressAt) {
        g_dressAt = 0;
        g_appliedPartsSig = 0;
        // Our suit first (visible at once), then the partner's parts are loaded over it below.
        if (uint8_t* a = ((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet)) DressPuppet(a);
        return;
    }
    if (g_puppet && !g_dressAt && g_remotePartCount && g_remotePartsSig != g_appliedPartsSig) {
        g_appliedPartsSig = g_remotePartsSig;
        if (uint8_t* a = ((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet)) ApplyPartnerSuit(a);
    }
    if (!g_puppet) return;
    uint8_t* actor = ((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet);
    if (!actor) {
        Log("puppet: actor gone");
        g_puppet = 0;
        g_puppetPending = false;
        return;
    }
    if (!*(float**)actor) return;
    alignas(16) float m[16];
    if (g_syncMode) {
        if (!partner) {  // partner gone or stalled: park the puppet far below the city until they're back
            if (!g_puppetHidden) {
                memcpy(m, g_remote, sizeof m);
                m[13] = -5000.0f;
                float* node = *(float**)actor;
                ((SetTransformFn)VA(0x14191c0e0))(node, m, node + 0x1c);
                g_puppetHidden = true;
                Log("sync: partner silent, puppet hidden");
            }
            return;
        }
        if (g_puppetHidden) {
            g_puppetHidden = false;
            Log("sync: partner back, puppet shown");
        }
        memcpy(m, g_remote, sizeof m);
        if (g_syncArg[0] == 'l')
            for (int i = 0; i < 3; i++) m[12 + i] += g_puppetOffset[i];
    } else {
        if (!hero || !*(float**)hero) return;
        memcpy(m, *(float**)hero, sizeof m);
        for (int i = 0; i < 3; i++) m[12 + i] += g_puppetOffset[i];
    }
    float* node = *(float**)actor;
    ((SetTransformFn)VA(0x14191c0e0))(node, m, node + 0x1c);  // node + 0x70 bytes
    if (g_syncMode) {
        uint8_t* tb = *(uint8_t**)((uint8_t*)node + 0xd8);
        if (tb && g_remoteJoints && g_remoteJoints == JointCount((uint8_t*)node)) memcpy(tb, g_remotePose, g_remoteJoints * 64);
    }
#ifdef COOP_RESEARCH
    else if (g_poseCopy && hero) {
        CopyPose(hero, actor);
    }
#endif
}

static uint64_t __fastcall HeroUpdateHook(void* a, void* b, void* c, void* d) {
    uint64_t r = ((Call4Fn)VA(0x1408445b0))(a, b, c, d);
    static volatile LONG faults;
    __try {
        PuppetTick();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (InterlockedIncrement(&faults) <= 5) Log("puppet: tick faulted %08lx, puppet dropped", GetExceptionCode());
        g_puppet = 0;
    }
    return r;
}

static void ApplyPatches() {
    RedirectCall(0x140844d86, 0x1408445b0, (void*)&HeroUpdateHook, "hero-update-tick");
    PatchHeroCreate();
    PatchSpawnCallback();
    RedirectCall(0x141f229ef, 0x141f65320, (void*)&SpawnRequestFilter, "spawn-filter-all");
    RedirectCall(0x141f226b9, 0x141f65320, (void*)&SpawnRequestFilter, "spawn-filter-respawn");
    RedirectCall(0x14083f481, 0x141efc100, (void*)&LocalPlayerForProbe, "hero-local-probe");
    RedirectCall(0x1408ca153, 0x141f24ad0, (void*)&NearbyTargetsSafe, "wam-query-null");
    PatchWamPicker();
    // Off: with these two guards a freshly started host crashed every time right after its own hero
    // spawned (WAM target picker update, null handle at +0x5c, from 0x1408ca158).
    // GuardCallSite<0>(0x14191989a, 0x141676010, "batch-activate");   // component activate
    // GuardCallSite<1>(0x140844d86, 0x1408445b0, "hero-state-update");  // per-frame, reads actor via handle +0x60
    // RoomList creation: cmp byte [LanEnable],0 / jz skip  -> fall through
    static const uint8_t jz1[] = {0x74, 0x26}, nop2[] = {0x90, 0x90};
    Patch(0x1417e557b, jz1, nop2, 2, "roomlist-gate");
    // SessionLan factory: cmp [LanEnable],bpl / jz fail -> fall through
    static const uint8_t jz2[] = {0x0f, 0x84, 0x1d, 0x01, 0x00, 0x00}, nop6[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    Patch(0x1417e5b4a, jz2, nop6, 6, "sessionlan-gate");
    // "<title>_GameInitMutex" already exists -> quit. Allow a second instance (two clients on one PC).
    static const uint8_t jeMutex[] = {0x0f, 0x84, 0x0b, 0x04, 0x00, 0x00};
    Patch(0x141d2ec4a, jeMutex, nop6, 6, "single-instance");
    // PSN account-linking HTTP worker thread: pops a queue entry without a null check and crashed
    // a second instance at startup. Optional feature; make the thread return at once.
    static const uint8_t psnProlog[] = {0x48, 0x89, 0x5c, 0x24, 0x08}, retZero[] = {0x31, 0xc0, 0xc3, 0x90, 0x90};
    Patch(0x142aa30e0, psnProlog, retZero, 5, "psn-linking-worker");
    // log stub: mov [rsp+10],rdx; xor eax,eax; mov [rsp+18],r8; mov [rsp+20],r9; ret
    static const uint8_t stub[] = {0x48, 0x89, 0x54, 0x24, 0x10, 0x33, 0xc0, 0x4c, 0x89,
                                   0x44, 0x24, 0x18, 0x4c, 0x89, 0x4c, 0x24, 0x20, 0xc3};
    uint8_t jmp[18] = {0xff, 0x25, 0, 0, 0, 0};
    uint64_t target = (uint64_t)&EngineLog;
    memcpy(jmp + 6, &target, 8);
    memset(jmp + 14, 0xcc, 4);
    Patch(0x141bbb4e0, stub, jmp, 18, "engine-log");
}

// ---- game functions (see games/marvels-spider-man-remastered/NOTES.md) ----
typedef void*(__fastcall* InitDescFn)(void* desc);
typedef void(__fastcall* SetDescB4Fn)(void* desc, int v);
typedef char(__fastcall* BrowseFn)(void* desc, void* cb, void* ctx);
typedef char(__fastcall* SessionFn)(void* desc);
typedef void(__fastcall* VoidFn)();

static uint8_t& LanEnable() { return *VA(0x147a88daa); }
static uint8_t* NetManager() { return *(uint8_t**)VA(0x147c2b1c0); }
static void* RoomList() { return *(void**)VA(0x1477fd5b8); }

static const size_t kDescSize = 0x108;
static const int kMaxRooms = 16;
alignas(16) static uint8_t g_rooms[kMaxRooms][kDescSize];
static volatile LONG g_roomCount;
static volatile LONG g_browseDone;

static void __fastcall BrowseCallback(void* entry, void* unused, void* ctx) {
    if (!entry) {
        g_browseDone = 1;
        Log("browse: done, %ld room(s)", g_roomCount);
        return;
    }
    LONG i = g_roomCount;
    if (i < kMaxRooms) {
        memcpy(g_rooms[i], entry, kDescSize);
        g_roomCount = i + 1;
        Log("browse: room %ld name='%.64s' type=%u players=%u/%u", i, (char*)g_rooms[i],
            g_rooms[i][0xbc], *(uint32_t*)(g_rooms[i] + 0xd4), *(uint32_t*)(g_rooms[i] + 0xdc));
    }
}

static void MakeDesc(uint8_t* desc, const char* name) {
    memset(desc, 0, kDescSize);
    ((InitDescFn)VA(0x1417e53a0))(desc);
    if (name) strncpy_s((char*)desc, 0x41, name, _TRUNCATE);
    desc[0xbc] = 1;  // LAN session type, required by the SessionLan factory
}

static void Status() {
    uint8_t* m = NetManager();
    Log("status: LanEnable=%u roomlist=%p netmgr=%p state=%d inner=%p session=%p", LanEnable(), RoomList(), m,
        m ? *(int*)(m + 0x10) : -1, m ? *(void**)(m + 8) : nullptr,
        (m && *(uint8_t**)(m + 8)) ? *(void**)(*(uint8_t**)(m + 8) + 8) : nullptr);
}

static int WalkFrames(CONTEXT ctx, uint64_t* frames, int max) {
    int n = 0;
    while (n < max && ctx.Rip) {
        frames[n++] = ctx.Rip;
        DWORD64 imageBase;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
        if (!fn) {  // leaf function: return address is at [rsp]
            ctx.Rip = *(DWORD64*)ctx.Rsp;
            ctx.Rsp += 8;
            continue;
        }
        void* handlerData;
        DWORD64 establisher;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData, &establisher, nullptr);
    }
    return n;
}

// Append frames to line as " module+offset", exe frames as Ghidra addresses.
static void FormatFrames(char* line, size_t size, int len, const uint64_t* frames, int n) {
    for (int i = 0; i < n && len > 0 && len < (int)size - 80; i++) {
        HMODULE mod = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)frames[i], &mod)) {
            GetModuleFileNameA(mod, name, MAX_PATH);
        }
        const char* base = strrchr(name, '\\');
        base = base ? base + 1 : name;
        if (mod == (HMODULE)g_base)
            len += _snprintf_s(line + len, size - len, _TRUNCATE, " %llx", frames[i] - (uint64_t)g_base + kImageBase);
        else
            len += _snprintf_s(line + len, size - len, _TRUNCATE, " %s+%llx", base, frames[i] - (uint64_t)mod);
    }
}

// First-chance hardware exceptions -> log with stack, so crashes that leave no dump still show up.
static LONG CALLBACK ExceptionLogger(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != 0xc0000409)
        return EXCEPTION_CONTINUE_SEARCH;
    static volatile LONG count;
    if (InterlockedIncrement(&count) > 40) return EXCEPTION_CONTINUE_SEARCH;
    uint64_t frames[20];
    int n = code == EXCEPTION_STACK_OVERFLOW ? 0 : WalkFrames(*ep->ContextRecord, frames, 20);
    char line[1600];
    int len = _snprintf_s(line, sizeof line, _TRUNCATE, "exception %08lx addr %llx thread %lu:", code,
                          ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[1] : 0ULL,
                          GetCurrentThreadId());
    if (n == 0) frames[n++] = ep->ContextRecord->Rip;
    FormatFrames(line, sizeof line, len, frames, n);
    Log("%s", line);
    return EXCEPTION_CONTINUE_SEARCH;
}

// Log every other thread's call stack as module+offset (for hangs). Frames are collected while
// the thread is suspended and only formatted after it resumes, so no CRT lock is taken meanwhile.
static void DumpStacks() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = {sizeof te};
    DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (!th) continue;
        uint64_t frames[24];
        int n = 0;
        if (SuspendThread(th) != (DWORD)-1) {
            CONTEXT ctx = {};
            ctx.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(th, &ctx)) n = WalkFrames(ctx, frames, 24);
            ResumeThread(th);
        }
        CloseHandle(th);
        char line[1600];
        int len = _snprintf_s(line, sizeof line, _TRUNCATE, "thread %lu:", te.th32ThreadID);
        FormatFrames(line, sizeof line, len, frames, n);
        Log("%s", line);
    }
    CloseHandle(snap);
}

static void DoCommand(char* cmd);

// F9: leave our own session, find the host's room on the LAN, join it, start partner sync (broadcast
// discovery) and warp to the host once their position arrives.
static DWORD WINAPI JoinThread(void*) {
    char c1[] = "setmax 2", c2[] = "leave", c3[] = "browse", c4[] = "join 0", c5[] = "sync ";
    DoCommand(c1);
    DoCommand(c2);
    Sleep(3000);
    DoCommand(c3);
    for (int t = 0; t < 80 && !g_roomCount; t++) Sleep(100);
    if (!g_roomCount) {
        Log("coop: no host found on the LAN (is the host's game hosting? same network? firewall?)");
        return 0;
    }
    DoCommand(c4);
    Sleep(8000);  // the joined world loads and our hero spawns
    DoCommand(c5);
    g_autoWarp = true;
    g_autoWarpTicks = 0;
    return 0;
}

static void DoCommand(char* cmd) {
    char* nl = strpbrk(cmd, "\r\n");
    if (nl) *nl = 0;
    Log("cmd: %s", cmd);
    if (!strcmp(cmd, "status")) {
        Status();
    } else if (!strcmp(cmd, "host")) {
        alignas(16) static uint8_t desc[kDescSize + 0x40];
        MakeDesc(desc, "SpideyCoop");
        char ok = ((SessionFn)VA(0x141f007b0))(desc);
        Log("host: %s", ok ? "started" : "refused");
        Status();
    } else if (!strcmp(cmd, "browse")) {
        alignas(16) static uint8_t desc[kDescSize + 0x40];
        MakeDesc(desc, nullptr);
        *(uint32_t*)(desc + 0xc8) = 200;
        ((SetDescB4Fn)VA(0x1417e5830))(desc, 0);
        g_roomCount = 0;
        g_browseDone = 0;
        char ok = ((BrowseFn)VA(0x1417e54f0))(desc, (void*)&BrowseCallback, nullptr);
        Log("browse: %s", ok ? "requested" : "refused (no RoomList?)");
    } else if (!strncmp(cmd, "join", 4)) {
        int n = atoi(cmd + 4);
        if (n < 0 || n >= g_roomCount) {
            Log("join: no room %d (have %ld; run browse first)", n, g_roomCount);
            return;
        }
        char ok = ((SessionFn)VA(0x141eff2a0))(g_rooms[n]);
        Log("join %d: %s", n, ok ? "started" : "refused");
        Status();
    } else if (!strcmp(cmd, "modes") || !strncmp(cmd, "setmax", 6)) {
        // Game-mode table (0x18-byte entries: id at +0, max players at +0x10). The host refuses a
        // joining client's slot request when used + requested > max players for the current mode.
        uint32_t count = *(uint32_t*)VA(0x147c2a4a4);
        uint8_t* table = VA(0x1463c8c40);
        int newMax = cmd[0] == 's' ? atoi(cmd + 6) : 0;
        for (uint32_t i = 0; i < count && i < 64; i++) {
            uint8_t* e = table + i * 0x18;
            uint32_t* maxp = (uint32_t*)(e + 0x10);
            if (newMax > 0 && *maxp < (uint32_t)newMax) *maxp = newMax;
            Log("mode[%u] id=%08x max=%u raw=%08x %08x %08x %08x", i, *(uint32_t*)e, *maxp, *(uint32_t*)(e + 4),
                *(uint32_t*)(e + 8), *(uint32_t*)(e + 0xc), *(uint32_t*)(e + 0x14));
        }
        if (!count) Log("mode table not initialised yet");
    } else if (!strcmp(cmd, "stacks")) {
        DumpStacks();
    } else if (!strcmp(cmd, "clients")) {
        DumpClients("cmd");
    } else if (!strcmp(cmd, "puppet")) {
        g_puppetRequest = 1;  // spawned on the game thread
    } else if (!strncmp(cmd, "sync ", 5) || !strcmp(cmd, "sync")) {
        strncpy_s(g_syncArg, cmd[4] ? cmd + 5 : "", _TRUNCATE);
        g_syncRequest = 1;  // opened on the game thread
    } else if (!strcmp(cmd, "coophost")) {  // F7, at the title screen
        char c1[] = "setmax 2", c2[] = "host", c3[] = "sync host";
        DoCommand(c1), DoCommand(c2), DoCommand(c3);
        Log("coop: hosting. Continue your game; your partner presses F9 once they are in their city.");
    } else if (!strcmp(cmd, "coopjoin")) {  // F9, once in the city
        CloseHandle(CreateThread(nullptr, 0, JoinThread, nullptr, 0, nullptr));
    } else if (!strcmp(cmd, "warpp")) {
        g_warpRequest = 2;
    } else if (!strncmp(cmd, "warp ", 5)) {
        sscanf_s(cmd + 5, "%f %f %f", &g_warpTarget[12], &g_warpTarget[13], &g_warpTarget[14]);
        g_warpRequest = 1;
    } else if (!strcmp(cmd, "pdress")) {
        g_dressRequest = 1;
    } else if (!strncmp(cmd, "poff", 4)) {
        sscanf_s(cmd + 4, "%f %f %f", &g_puppetOffset[0], &g_puppetOffset[1], &g_puppetOffset[2]);
        Log("puppet offset %.2f %.2f %.2f", g_puppetOffset[0], g_puppetOffset[1], g_puppetOffset[2]);
#ifdef COOP_RESEARCH
    } else if (ResearchCommand(cmd)) {
#endif
    } else if (!strcmp(cmd, "leave")) {
        ((VoidFn)VA(0x141eff510))();
    } else {
        Log("unknown command");
    }
}

static DWORD WINAPI Worker(void*) {
    char cmdPath[MAX_PATH];
    _snprintf_s(cmdPath, sizeof cmdPath, _TRUNCATE, "%s\\coop_cmd_%lu.txt", g_dir, GetCurrentProcessId());
    bool prev[4] = {};
    const int keys[4] = {VK_F6, VK_F7, VK_F8, VK_F9};
    const char* keyCmd[4] = {"status", "coophost", "warpp", "coopjoin"};
    int lastState = -2;
    for (;;) {
        Sleep(100);
        if (!LanEnable()) LanEnable() = 1;
        uint8_t* m = NetManager();
        int st = m ? *(int*)(m + 0x10) : -1;
        if (st != lastState) {
            Log("netmgr state %d -> %d", lastState, st);
            lastState = st;
        }
        FILE* f = nullptr;
        if (fopen_s(&f, cmdPath, "rb") == 0 && f) {
            char cmd[256] = {};
            fread(cmd, 1, sizeof cmd - 1, f);
            fclose(f);
            DeleteFileA(cmdPath);
            DoCommand(cmd);
        }
        DWORD fgPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &fgPid);
        bool focused = fgPid == GetCurrentProcessId();  // two instances on one PC: only the focused one
        for (int i = 0; i < 4; i++) {
            bool down = focused && (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
            if (down && !prev[i]) {
                char c[32];
                strcpy_s(c, keyCmd[i]);
                DoCommand(c);
            }
            prev[i] = down;
        }
    }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void*) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(inst);
    InitializeCriticalSection(&g_logLock);
    g_base = (uint8_t*)GetModuleHandleW(nullptr);
    GetModuleFileNameA(nullptr, g_dir, MAX_PATH);
    char* slash = strrchr(g_dir, '\\');
    if (!slash || _stricmp(slash + 1, "Spider-Man.exe") != 0) return TRUE;  // crs-handler etc. load us too
    *slash = 0;
    char logPath[MAX_PATH];
    _snprintf_s(logPath, sizeof logPath, _TRUNCATE, "%s\\coop_log_%lu.txt", g_dir, GetCurrentProcessId());
    g_log = _fsopen(logPath, "w", _SH_DENYNO);  // shared so it can be tailed while the game runs

    auto* dos = (IMAGE_DOS_HEADER*)g_base;
    auto* nt = (IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kExeTimestamp) {
        Log("coop: exe timestamp %08x is not build 23986256, doing nothing", nt->FileHeader.TimeDateStamp);
        return TRUE;
    }
    Log("coop: loaded, base %p", g_base);
    AddVectoredExceptionHandler(1, ExceptionLogger);
#ifdef COOP_RESEARCH
    AddVectoredExceptionHandler(1, WatchHandler);
#endif
    ApplyPatches();
    LanEnable() = 1;
    CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    return TRUE;
}
