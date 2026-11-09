// coop-probe v12: read-only test DLL for Darkest Dungeon (Steam, x64 build sha256 4d78fbfa...).
//
// Loaded as SDL2.dll (every export forwards to SDL2_orig.dll, see sdl2_forward.def). It changes no
// game state. It only:
//   1. logs that it loaded and checks the exe hash (on another build it stops right there);
//   2. finds the town building activities (Building::Activity::* objects) by their vftables and logs
//      every change of a slot's hero id (activity + 0x78: vector of 0x18-byte slots, first int = hero id);
//   3. puts hardware write breakpoints on the first tavern bar slot (+0x0, +0x8, +0x10) and the first
//      meditation slot and logs which code wrote them (instruction + unwound call stack, Darkest.exe offsets);
//   v8: logs the game's save storage commands - StorageMountDeviceCommand (0x140246000: device slot +
//   storage name) and StorageRemoveDirectoryCommand (0x140247860: name) - and Darkest::LoadCampaign
//   (0x1403d8590), to learn which save slot is mounted for which campaign. Nothing else is traced.
//   v12: directly watches TownUI::ProvisionSelect::PreChildrenShowInternal (0x140688a50), the
//   preparation panel's pre-show method. It also polls the TownController active state for comparison.
//   v7: snapshots the fight's units (hero/monster, class name, HP) at every AI decision
//   (GetMonsterAISelectSkill 0x14059fe80) to compare game state, not only random numbers; logs the call
//   stack of Party::OnRaidStart (0x1405f7170) to find who supplies the RaidStartParameters; keeps the
//   named-stream draw trace and the named reseed (fight start). Leaf callers fall back to [rsp].
//   v6: new threads are armed right away (DLL_THREAD_ATTACH wakes the worker), the logged caller is the
//   first frame outside the PRNG code (not the PRNG's own int/bool wrapper), named-stream reseeds are
//   caught (0x14028e030), raw draws are no longer traced (cosmetic, frame-dependent noise).
//   v5 (determinism test): execute breakpoints on the PRNG core steps (raw prng_minstd 0x14028dde0, named
//   prng_minstd_wrapped 0x14028e0c0) and on reseed (0x14028ddc0); every draw goes to coop-rng.log as
//   (sequence, kind, generator, state, caller). The fourth breakpoint first catches the raid load
//   (0x1403cf7f0) to learn the app object, then watches app+0x16a0 (RaidStartParameters*) for writes, so
//   the next departure shows which code fills the party/provisions/seed. Memory diffs are switched off.
//   v4: execute breakpoints on the wallet functions (deduct/add, with caller), the provisions item
//   transfer and the heirloom exchange; plus a byte diff of every roster hero record (0x1560 bytes)
//   and of the campaign object (0x2000 bytes) to find trinkets, skill selection and the party lineup.
//   v3: the four hardware breakpoints are now EXECUTE breakpoints on the town action functions
//   (buy upgrade, assign hero, dismiss, event dispatch filtered on HeroRecruitedEvent); each call is
//   logged with its arguments. The estate wallet (campaign+0xe70, 0x48-byte entries) is logged too.
//   4. (v2) logs every changed byte of every slot, and per roster hero: id (+0), state (+0x1540) and
//      assignment fields (+0x1550, +0x1554). Test kit 1 showed the hero is not in the slot's first int.
// Output: coop-probe.log next to Darkest.exe.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define EXPECTED_EXE_SHA256 "4d78fbfaa65b7d002d2208d02da809200f4a5723119521180e30100d6f1a75f2"
#define IMAGE_BASE 0x140000000ULL
#define SLOTS_OFFSET 0x78
#define SLOT_STRIDE 0x18
#define MAX_SLOTS 12
#define MAX_ACTIVITIES 64
#define MAX_HEROES 96
#define CAMPAIGN_CURRENT_VA 0x14117d9b8ULL /* g_pCurrent, read by Campaign::Current() 0x14054c950 */
#define WALLET_OFFSET 0xe70 /* vector of 0x48-byte currency entries: amount at +0, type at +0x44 */
#define MAX_CURRENCIES 16
#define FN_RNG_RAW 0x14028dde0ULL     /* prng_minstd::Uniform(this): state at this+8 */
#define FN_RNG_NAMED 0x14028e0c0ULL   /* prng_minstd_wrapped::Uniform(this): state this+8, name this+0x10 */
#define FN_RNG_RESEED 0x14028ddc0ULL  /* prng_minstd::Seed(this, seed) */
#define FN_RAID_LOAD 0x1403cf7f0ULL   /* (app): reads app+0x16a0 = RaidStartParameters* */
#define FN_RNG_NAMED_RESEED 0x14028e030ULL /* prng_minstd_wrapped::Seed(this, seed) */
#define FN_MONSTER_AI 0x14059fe80ULL  /* Battle::GetMonsterAISelectSkill(brain): brain+0x20 = round list */
#define FN_PARTY_ON_RAID_START 0x1405f7170ULL /* (party, restoring, RaidStartParameters*, quests, index) */
#define ACTOR_HP 0x1038               /* float, written by Actor::ApplyHealthDamageInternal 0x14047e570 */
#define HERO_CLASS 0x12b0             /* Hero vtable +0x30 returns *(this+0x12b0) */
#define MONSTER_CLASS 0x13f8          /* Monster vtable +0x30 returns *(this+0x13f8); name at class+8 */
#define VFT_HERO 0x140e7c638ULL
#define VFT_MONSTER 0x140e9b020ULL
#define FN_STORAGE_MOUNT 0x140246000ULL   /* (manager, u8 device slot, const char *storage name) */
#define FN_STORAGE_RMDIR 0x140247860ULL   /* (manager, const char *name, ...) */
#define FN_LOAD_CAMPAIGN 0x1403d8590ULL   /* Darkest::LoadCampaign(app, params) */
#define FN_PROVISION_PANEL_SHOW 0x140688a50ULL /* TownUI::ProvisionSelect::PreChildrenShowInternal(this) */
#define RNG_CODE_LO 0x14028d000ULL    /* prng_minstd + prng_minstd_wrapped methods live here */
#define RNG_CODE_HI 0x14028f000ULL
#define APP_RAID_PARAMS 0x16a0
#define ENABLE_DIFF 0
#define MAX_RNG_LINES 400000
#define FN_WALLET_DEDUCT 0x14055ea10ULL /* (wallet, type, amount) */
#define FN_WALLET_ADD 0x14055e910ULL    /* (wallet, type, amount) */
#define FN_ITEM_TRANSFER 0x1405d4910ULL /* (store, slot, count, destInventory): provisions buy */
#define FN_HEIRLOOM_EXCH 0x14067a550ULL /* (ctx*): ctx[1] wallet, ctx[2] {dstType, srcAmount}, ctx[3] dstAmount */
#define HERO_SNAPSHOT 0x1560
#define CAMPAIGN_SNAPSHOT 0x2000
#define MAX_DIFF_LINES 60
#define CAMPAIGN_ALT_FLAG_VA 0x142a5c08bULL /* when set, the roster lives at campaign + 0x728 */
/* TownController globals and its state-id -> state-name map were recovered from this exact build. */
#define TOWN_CONTROLLER_VA 0x14117d8e0ULL
#define TOWN_STATE_NAME_COUNT 17
static const char *const k_town_state_names[TOWN_STATE_NAME_COUNT] = {
    "HeirloomExchange", "ChooseItem", "ChooseHero", "InspectHero", "UpgradeFacility", "Statue",
    "Graveyard", "Stagecoach", "TownEventPurchaseItem", "Nomad", "ActivityFacility", "HeroFacility",
    "ActivityLog", "ChooseBuilding", "Embark", "Provisions", "Null",
};

typedef struct { const char *name; uint64_t vftable_va; } ActivityClass;

// From Ghidra RTTI (NOTES.md, "Stress buildings"). Order = breakpoint priority (tavern bar first).
static const ActivityClass k_classes[] = {
    {"Bar", 0x140e8ef70},          {"Meditation", 0x140e8c6f0}, {"Gambling", 0x140e8ee60},
    {"Brothel", 0x140e8eee8},      {"Prayer", 0x140e8c5f8},     {"Flagellation", 0x140e8c570},
    {"QuirkTreatment", 0x140e8cca0}, {"StressRelief", 0x140e8d068},
};
#define NUM_CLASSES (sizeof(k_classes) / sizeof(k_classes[0]))

typedef struct {
    uintptr_t obj;
    int cls;
    uintptr_t slots_begin;
    int slot_count;
    int ids[MAX_SLOTS];
    uint8_t raw[MAX_SLOTS][SLOT_STRIDE];
} Activity;

typedef struct {
    uintptr_t entry;
    int id, state, assign_a, assign_b;
} Hero;

static HANDLE g_log = INVALID_HANDLE_VALUE;
static HANDLE g_rng = INVALID_HANDLE_VALUE;
static volatile LONG g_rng_seq;
static volatile int g_bp_write[4]; /* 0 = execute breakpoint, 1 = 8-byte write breakpoint */
static volatile uintptr_t g_app;
static HANDLE g_new_thread; /* set by DLL_THREAD_ATTACH so the worker arms the new thread at once */
static CRITICAL_SECTION g_log_lock;
static uintptr_t g_base;
static uintptr_t g_vft[NUM_CLASSES];
static Activity g_acts[MAX_ACTIVITIES];
static int g_act_count;
// Hardware breakpoint targets: address of a slot's hero id, plus what it belongs to.
static volatile uintptr_t g_bp_addr[4];
static Hero g_heroes[MAX_HEROES];
static int g_hero_count = -1;
static int g_wallet[MAX_CURRENCIES][2], g_wallet_count = -1; /* {type, amount} */
static uint8_t g_hero_snap[MAX_HEROES][HERO_SNAPSHOT];
static uintptr_t g_hero_snap_ptr[MAX_HEROES];
static uint8_t g_campaign_snap[CAMPAIGN_SNAPSHOT];
static uintptr_t g_campaign_snap_ptr;
#define NOISY_AFTER 20 /* a byte that changed this often is a timer or counter: stop reporting it */
static uint8_t g_campaign_changes[CAMPAIGN_SNAPSHOT];
static uint8_t g_hero_changes[MAX_HEROES][HERO_SNAPSHOT];
static volatile LONG g_bp_hits;
static uintptr_t g_last_town_controller;
static int32_t g_last_town_state_id = -1;
static uintptr_t g_last_town_state_object;

static void plog(const char *fmt, ...) {
    char buf[1024];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = snprintf(buf, sizeof buf, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof buf - n, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 2) n = sizeof buf - 2;
    buf[n++] = '\r';
    buf[n++] = '\n';
    EnterCriticalSection(&g_log_lock);
    DWORD w;
    WriteFile(g_log, buf, n, &w, NULL);
    LeaveCriticalSection(&g_log_lock);
}

static uintptr_t rva(uintptr_t a) { return a - g_base + IMAGE_BASE; }

// Reads game memory without risking a crash: the game may free a block while we look at it.
static BOOL safe_read(uintptr_t addr, void *dst, size_t len) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (void *)addr, dst, len, &got) && got == len;
}

static BOOL exe_hash_ok(void) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE h;
    UCHAR digest[32], chunk[1 << 16];
    DWORD got;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0);
    while (ReadFile(f, chunk, sizeof chunk, &got, NULL) && got) BCryptHashData(h, chunk, got, 0);
    BCryptFinishHash(h, digest, sizeof digest, 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", digest[i]);
    plog("exe: %s", path);
    plog("exe sha256: %s (%s)", hex, strcmp(hex, EXPECTED_EXE_SHA256) ? "MISMATCH" : "expected build");
    return strcmp(hex, EXPECTED_EXE_SHA256) == 0;
}

// Reads the slot vector of an activity; returns FALSE if it does not look like one.
static BOOL read_slots(Activity *a) {
    uintptr_t v[2];
    uint8_t raw[MAX_SLOTS * SLOT_STRIDE];
    if (!safe_read(a->obj + SLOTS_OFFSET, v, sizeof v)) return FALSE;
    if (v[1] < v[0] || (v[1] - v[0]) % SLOT_STRIDE) return FALSE;
    int count = (int)((v[1] - v[0]) / SLOT_STRIDE);
    if (count > MAX_SLOTS || (count && !safe_read(v[0], raw, v[1] - v[0]))) return FALSE;
    a->slots_begin = v[0];
    a->slot_count = count;
    for (int i = 0; i < count; i++) {
        memcpy(&a->ids[i], raw + i * SLOT_STRIDE, sizeof(int));
        memcpy(a->raw[i], raw + i * SLOT_STRIDE, SLOT_STRIDE);
    }
    return TRUE;
}

// Scans private read/write memory for objects whose first qword is one of the activity vftables.
static int scan_activities(Activity *out) {
    int n = 0;
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t p = 0x10000;
    while (n < MAX_ACTIVITIES && VirtualQuery((void *)p, &mbi, sizeof mbi)) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress, end = base + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && mbi.Protect == PAGE_READWRITE) {
            static uintptr_t chunk[8192];
            for (uintptr_t c0 = base; c0 < end && n < MAX_ACTIVITIES; c0 += sizeof chunk) {
                size_t len = end - c0 < sizeof chunk ? end - c0 : sizeof chunk;
                if (!safe_read(c0, chunk, len)) continue;
                for (size_t k = 0; k < len / 8 && n < MAX_ACTIVITIES; k++)
                    for (int c = 0; c < (int)NUM_CLASSES; c++) {
                        if (chunk[k] != g_vft[c]) continue;
                        Activity a = {.obj = c0 + k * 8, .cls = c};
                        if (read_slots(&a)) out[n++] = a;
                        break;
                    }
            }
        }
        if (end <= p) break;
        p = end;
    }
    return n;
}

static void set_debug_regs(CONTEXT *ctx) {
    ctx->Dr7 = 0;
    for (int i = 0; i < 4; i++) {
        (&ctx->Dr0)[i] = g_bp_addr[i];
        // local enable; execute = RW 00 / LEN 00, write = RW 01 / LEN 10 (8 bytes)
        if (!g_bp_addr[i]) continue;
        ctx->Dr7 |= 1ULL << (2 * i);
        if (g_bp_write[i]) ctx->Dr7 |= (1ULL << (16 + 4 * i)) | (2ULL << (18 + 4 * i));
    }
}

// Applies the breakpoints to every thread of the game except this one. No logging while threads are
// suspended (a suspended thread may hold the log lock).
static int arm_all_threads(void) {
    int armed = 0;
    DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 te = {.dwSize = sizeof te};
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
        if (!t) continue;
        if (SuspendThread(t) != (DWORD)-1) {
            CONTEXT ctx = {0};
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(t, &ctx)) {
                set_debug_regs(&ctx);
                if (SetThreadContext(t, &ctx)) armed++;
            }
            ResumeThread(t);
        }
        CloseHandle(t);
    }
    CloseHandle(snap);
    return armed;
}

static void describe(uintptr_t addr, char *out, size_t len) {
    HMODULE m;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &m)) {
        GetModuleFileNameA(m, name, MAX_PATH);
        if ((uintptr_t)m == g_base) {
            snprintf(out, len, "Darkest.exe!0x%llx", (unsigned long long)rva(addr));
            return;
        }
        const char *s = strrchr(name, '\\');
        snprintf(out, len, "%s+0x%llx", s ? s + 1 : name, (unsigned long long)(addr - (uintptr_t)m));
        return;
    }
    snprintf(out, len, "0x%llx", (unsigned long long)addr);
}

// One line per draw: seq kind generator state caller [name]. Names are logged once per generator.
__attribute__((unused)) static void rng_line(char kind, uintptr_t gen, int state, uintptr_t caller, const char *name) {
    LONG seq = InterlockedIncrement(&g_rng_seq);
    if (seq > MAX_RNG_LINES) return;
    char buf[256];
    int n = snprintf(buf, sizeof buf, "%ld %c %p %d 0x%llx%s%s\r\n", seq, kind, (void *)gen, state,
                     (unsigned long long)(caller >= g_base && caller < g_base + 0x4000000 ? rva(caller) : caller),
                     name ? " " : "", name ? name : "");
    DWORD w;
    EnterCriticalSection(&g_log_lock);
    WriteFile(g_rng, buf, n, &w, NULL);
    LeaveCriticalSection(&g_log_lock);
}

// First return address outside the PRNG methods: the game code that asked for the number.
static uintptr_t real_caller(const CONTEXT *c) {
    CONTEXT u = *c;
    for (int f = 0; f < 6; f++) {
        DWORD64 img;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(u.Rip, &img, NULL);
        uintptr_t r;
        if (!fn) { /* leaf function: no unwind data, the return address is at [rsp] */
            r = 0;
            safe_read(u.Rsp, &r, sizeof r);
            u.Rsp += 8;
            u.Rip = r;
        } else {
            void *hd;
            DWORD64 est;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, img, u.Rip, fn, &u, &hd, &est, NULL);
            r = (uintptr_t)u.Rip;
        }
        if (!r) return 0;
        uint64_t va = rva(r);
        if (va < RNG_CODE_LO || va >= RNG_CODE_HI) return r;
    }
    return (uintptr_t)u.Rip;
}

// One line per unit in the fight's round list: side, class name, HP, the actor and whose turn it is.
__attribute__((unused)) static void snapshot_round(uintptr_t brain) {
    static LONG decision;
    LONG d = InterlockedIncrement(&decision);
    uintptr_t vec[2] = {0};
    uint32_t cur = 0;
    safe_read(brain + 0x20, vec, sizeof vec);
    safe_read(brain + 0xa0, &cur, 4);
    if (vec[1] < vec[0] || (vec[1] - vec[0]) % 0x18 || (vec[1] - vec[0]) / 0x18 > 16) {
        plog("FIGHT decision %ld: round list unreadable", d);
        return;
    }
    int n = (int)((vec[1] - vec[0]) / 0x18), heroes = 0, monsters = 0;
    plog("FIGHT decision %ld: %d units in the round list, current index %u (rng seq %ld)", d, n, cur, g_rng_seq);
    for (int i = 0; i < n; i++) {
        uintptr_t actor = 0, vft = 0, cls = 0;
        float hp = 0;
        char name[40] = {0};
        safe_read(vec[0] + (uintptr_t)i * 0x18, &actor, 8);
        if (!actor) continue;
        safe_read(actor, &vft, 8);
        int is_hero = vft == g_base + (uintptr_t)(VFT_HERO - IMAGE_BASE);
        int is_monster = vft == g_base + (uintptr_t)(VFT_MONSTER - IMAGE_BASE);
        safe_read(actor + (is_hero ? HERO_CLASS : MONSTER_CLASS), &cls, 8);
        if (cls) safe_read(cls + 8, name, sizeof name - 1);
        for (int k = 0; name[k]; k++)
            if ((unsigned char)name[k] < 32 || (unsigned char)name[k] > 126) name[k] = '?';
        safe_read(actor + ACTOR_HP, &hp, 4);
        heroes += is_hero;
        monsters += is_monster;
        plog("    [%d]%s %-8s %-28s hp %6.1f  actor %p", i, (uint32_t)i == cur ? "*" : " ",
             is_hero ? "hero" : is_monster ? "monster" : "other", name, hp, (void *)actor);
    }
    plog("    = %d heroes, %d monsters", heroes, monsters);
}

__attribute__((unused)) static void read_name(uintptr_t gen, char *name, int len) {
    memset(name, 0, len);
    safe_read(gen + 0x10, name, len - 1);
    for (int k = 0; name[k]; k++)
        if ((unsigned char)name[k] < 33) name[k] = '?';
}

static LONG CALLBACK on_exception(EXCEPTION_POINTERS *ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT *c = ep->ContextRecord;
    DWORD64 hit = c->Dr6 & 0xF;
    if (!hit) return EXCEPTION_CONTINUE_SEARCH;
    c->Dr6 = 0;
    /* Resume flag first: an execute breakpoint without it fires again on the same instruction forever. */
    c->EFlags |= 0x10000;
    InterlockedIncrement(&g_bp_hits);
    char where[160];
    for (int i = 0; i < 4; i++) {
        if (!(hit & (1ULL << i)) || !g_bp_addr[i]) continue;
        uint8_t buf[16] = {0};
        uintptr_t ret = 0;
        safe_read(c->Rsp, &ret, sizeof ret); /* at function entry [rsp] = return address */
        switch (i) {
        case 0: { /* StorageMountDeviceCommand */
            char name[96] = {0};
            safe_read(c->R8, name, sizeof name - 1);
            for (int k = 0; name[k]; k++)
                if ((unsigned char)name[k] < 32 || (unsigned char)name[k] > 126) name[k] = '?';
            describe(real_caller(c), where, sizeof where);
            plog("STORAGE mount device slot %d <- \"%s\" (manager %p, from %s)", (int)(uint8_t)c->Rdx, name,
                 (void *)c->Rcx, where);
            continue;
        }
        case 1: { /* StorageRemoveDirectoryCommand */
            char name[96] = {0};
            safe_read(c->Rdx, name, sizeof name - 1);
            for (int k = 0; name[k]; k++)
                if ((unsigned char)name[k] < 32 || (unsigned char)name[k] > 126) name[k] = '?';
            describe(real_caller(c), where, sizeof where);
            plog("STORAGE REMOVE DIRECTORY \"%s\" (from %s)", name, where);
            continue;
        }
        case 2: /* Darkest::LoadCampaign */
            describe(real_caller(c), where, sizeof where);
            plog("CALL Darkest::LoadCampaign (from %s)", where);
            continue;
        case 3: { /* ProvisionSelect::PreChildrenShowInternal(this). */
            uintptr_t panel = (uintptr_t)c->Rcx, panel_vtable = 0, controller = 0;
            int32_t controller_state_id = -1;
            safe_read(panel, &panel_vtable, sizeof panel_vtable);
            safe_read(g_base + (uintptr_t)(TOWN_CONTROLLER_VA - IMAGE_BASE), &controller, sizeof controller);
            if (controller) safe_read(controller + 0x18, &controller_state_id, sizeof controller_state_id);
            describe(real_caller(c), where, sizeof where);
            plog("PROVISION_SHOW panel=%p vtable=%p town_controller=%p state_id=%ld caller=%s",
                 (void *)panel, (void *)panel_vtable, (void *)controller, (long)controller_state_id, where);
            continue;
        }
        }
        (void)buf;
        describe((uintptr_t)c->Rip, where, sizeof where);
        CONTEXT u = *c;
        DWORD64 img;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(u.Rip, &img, NULL);
        if (fn) {
            void *hd;
            DWORD64 est;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, img, u.Rip, fn, &u, &hd, &est, NULL);
            char from[160];
            describe((uintptr_t)u.Rip, from, sizeof from);
            plog("    at %s, called from %s", where, from);
        }
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void choose_breakpoints(void) {
    const uint64_t fns[4] = {FN_STORAGE_MOUNT, FN_STORAGE_RMDIR, FN_LOAD_CAMPAIGN, FN_PROVISION_PANEL_SHOW};
    for (int i = 0; i < 4; i++) {
        g_bp_write[i] = 0;
        g_bp_addr[i] = fns[i] ? g_base + (uintptr_t)(fns[i] - IMAGE_BASE) : 0; /* 0 = slot not used */
    }
}

static void hex(const uint8_t *b, int n, char *out) {
    for (int i = 0; i < n; i++) sprintf(out + 3 * i, "%02x%s", b[i], (i % 8 == 7 && i + 1 < n) ? "|" : " ");
}

// Logs the changed byte ranges between two snapshots (merged when closer than 8 bytes).
static int log_diff(const char *what, int id, const uint8_t *old, const uint8_t *cur, int len, int budget,
                    uint8_t *changes) {
    for (int i = 0; i < len && budget > 0; i++) {
        if (old[i] == cur[i]) continue;
        if (changes[i] < 255) changes[i]++;
        if (changes[i] > NOISY_AFTER) continue;
        if (changes[i] == NOISY_AFTER) {
            plog("DIFF %s %d +0x%x is noisy (changed %d times), no longer reported", what, id, i, NOISY_AFTER);
            continue;
        }
        int j = i, last = i;
        while (j < len && j - last < 8) {
            if (old[j] != cur[j]) last = j;
            j++;
        }
        int n = last - i + 1 > 32 ? 32 : last - i + 1;
        char a[3 * 32 + 1], b[3 * 32 + 1];
        hex(old + i, n, a);
        hex(cur + i, n, b);
        if (id >= 0) plog("DIFF %s %d +0x%x (%d bytes): %s-> %s", what, id, i, last - i + 1, a, b);
        else plog("DIFF %s +0x%x (%d bytes): %s-> %s", what, i, last - i + 1, a, b);
        budget--;
        i = last;
    }
    return budget;
}

// Reads the roster (campaign + 0x20 .. + 0x28, a vector of entry pointers) and logs changes.
static void poll_roster(void) {
    uintptr_t campaign = 0, vec[2];
    uint8_t alt = 0;
    if (!safe_read(g_base + (uintptr_t)(CAMPAIGN_CURRENT_VA - IMAGE_BASE), &campaign, sizeof campaign) || !campaign) return;
    safe_read(g_base + (uintptr_t)(CAMPAIGN_ALT_FLAG_VA - IMAGE_BASE), &alt, 1);
    if (alt) campaign += 0x728;
    uintptr_t wv[2];
    if (safe_read(campaign + WALLET_OFFSET, wv, sizeof wv) && wv[1] >= wv[0] && (wv[1] - wv[0]) % 0x48 == 0 &&
        (wv[1] - wv[0]) / 0x48 <= MAX_CURRENCIES) {
        int wn = (int)((wv[1] - wv[0]) / 0x48), cur[MAX_CURRENCIES][2];
        for (int i = 0; i < wn; i++) {
            cur[i][1] = 0;
            cur[i][0] = -1;
            safe_read(wv[0] + (uintptr_t)i * 0x48, &cur[i][1], 4);
            safe_read(wv[0] + (uintptr_t)i * 0x48 + 0x44, &cur[i][0], 4);
        }
        if (wn != g_wallet_count) {
            plog("Wallet: %d currencies", wn);
            for (int i = 0; i < wn; i++) plog("  type %d amount %d", cur[i][0], cur[i][1]);
        } else {
            for (int i = 0; i < wn; i++)
                if (cur[i][1] != g_wallet[i][1])
                    plog("WALLET type %d: %d -> %d", cur[i][0], g_wallet[i][1], cur[i][1]);
        }
        memcpy(g_wallet, cur, sizeof(int) * 2 * wn);
        g_wallet_count = wn;
    }
    if (!safe_read(campaign + 0x20, vec, sizeof vec) || vec[1] < vec[0] || (vec[1] - vec[0]) % 8) return;
    int n = (int)((vec[1] - vec[0]) / 8);
    if (n > MAX_HEROES) return;
    uintptr_t ptrs[MAX_HEROES];
    if (n && !safe_read(vec[0], ptrs, (size_t)n * 8)) return;
    Hero now[MAX_HEROES];
    int got = 0;
    for (int i = 0; i < n; i++) {
        Hero h = {.entry = ptrs[i]};
        int a[2];
        if (!safe_read(h.entry, &h.id, 4) || !safe_read(h.entry + 0x1540, &h.state, 4) ||
            !safe_read(h.entry + 0x1550, a, sizeof a))
            continue;
        h.assign_a = a[0];
        h.assign_b = a[1];
        now[got++] = h;
    }
    if (g_hero_count < 0 || got != g_hero_count) {
        plog("Roster: %d heroes (campaign %p%s)", got, (void *)campaign, alt ? ", alt +0x728" : "");
        for (int i = 0; i < got; i++)
            plog("  hero id %d @%p state %d assign %d/%d", now[i].id, (void *)now[i].entry, now[i].state,
                 now[i].assign_a, now[i].assign_b);
    } else {
        for (int i = 0; i < got; i++) {
            const Hero *o = &g_heroes[i], *h = &now[i];
            if (o->entry != h->entry || o->state != h->state || o->assign_a != h->assign_a || o->assign_b != h->assign_b)
                plog("HERO id %d @%p: state %d -> %d, assign %d/%d -> %d/%d", h->id, (void *)h->entry, o->state, h->state,
                     o->assign_a, o->assign_b, h->assign_a, h->assign_b);
        }
    }
    memcpy(g_heroes, now, sizeof(Hero) * got);
    g_hero_count = got;

    if (!ENABLE_DIFF) return;
    int budget = MAX_DIFF_LINES;
    static uint8_t cur[CAMPAIGN_SNAPSHOT];
    if (safe_read(campaign, cur, CAMPAIGN_SNAPSHOT)) {
        if (g_campaign_snap_ptr == campaign) budget = log_diff("campaign", -1, g_campaign_snap, cur, CAMPAIGN_SNAPSHOT, budget, g_campaign_changes);
        memcpy(g_campaign_snap, cur, CAMPAIGN_SNAPSHOT);
        g_campaign_snap_ptr = campaign;
    }
    for (int i = 0; i < got && i < MAX_HEROES; i++) {
        if (!safe_read(now[i].entry, cur, HERO_SNAPSHOT)) continue;
        if (g_hero_snap_ptr[i] == now[i].entry) budget = log_diff("hero", now[i].id, g_hero_snap[i], cur, HERO_SNAPSHOT, budget, g_hero_changes[i]);
        if (g_hero_snap_ptr[i] != now[i].entry) memset(g_hero_changes[i], 0, HERO_SNAPSHOT);
        memcpy(g_hero_snap[i], cur, HERO_SNAPSHOT);
        g_hero_snap_ptr[i] = now[i].entry;
    }
    if (budget <= 0) plog("DIFF output capped this tick (%d lines)", MAX_DIFF_LINES);
}

/* Read the active state ID and selected state object from the game-owned TownController. */
static void poll_town_active_state(void) {
    uintptr_t controller = 0, states = 0, state_object = 0, state_vtable = 0;
    int32_t state_id = -1;
    if (!safe_read(g_base + (uintptr_t)(TOWN_CONTROLLER_VA - IMAGE_BASE), &controller, sizeof controller) ||
        !controller || !safe_read(controller + 0x18, &state_id, sizeof state_id) ||
        !safe_read(controller + 0x90, &states, sizeof states) || !states ||
        state_id < 0 || state_id >= TOWN_STATE_NAME_COUNT ||
        !safe_read(states + (uintptr_t)state_id * sizeof(uintptr_t), &state_object, sizeof state_object) ||
        !state_object || !safe_read(state_object, &state_vtable, sizeof state_vtable))
        return;

    if (controller != g_last_town_controller || state_id != g_last_town_state_id ||
        state_object != g_last_town_state_object) {
        plog("TOWNACTIVE id=%ld name=%s controller=%p state=%p vtable=%p",
             (long)state_id, k_town_state_names[state_id], (void *)controller,
             (void *)state_object, (void *)state_vtable);
        g_last_town_controller = controller;
        g_last_town_state_id = state_id;
        g_last_town_state_object = state_object;
    }
}

static BOOL same_layout(const Activity *a, int n) {
    if (n != g_act_count) return FALSE;
    for (int i = 0; i < n; i++)
        if (a[i].obj != g_acts[i].obj || a[i].slots_begin != g_acts[i].slots_begin ||
            a[i].slot_count != g_acts[i].slot_count)
            return FALSE;
    return TRUE;
}

static DWORD WINAPI worker(void *unused) {
    (void)unused;
    if (!exe_hash_ok()) {
        plog("Different game build: the probe stays idle (addresses would be wrong).");
        return 0;
    }
    for (int c = 0; c < (int)NUM_CLASSES; c++) g_vft[c] = (uintptr_t)(k_classes[c].vftable_va - IMAGE_BASE) + g_base;
    AddVectoredExceptionHandler(1, on_exception);
    choose_breakpoints();
    plog("Execute breakpoints: storage mount/remove, LoadCampaign, ProvisionSelect pre-show.");
    plog("Waiting for a campaign with town buildings; active TownController state is polled every 50 ms.");
    static Activity scan[MAX_ACTIVITIES];
    DWORD last_scan = 0, last_arm = 0;
    for (;;) {
        DWORD now = GetTickCount();
        if (now - last_scan >= 5000) {
            last_scan = now;
            int n = scan_activities(scan);
            if (!same_layout(scan, n)) {
                memcpy(g_acts, scan, sizeof(Activity) * n);
                g_act_count = n;
                plog("Found %d building activities:", n);
                for (int i = 0; i < n; i++) {
                    char ids[160] = "";
                    for (int s = 0; s < g_acts[i].slot_count; s++)
                        snprintf(ids + strlen(ids), sizeof ids - strlen(ids), "%s%d", s ? "," : "", g_acts[i].ids[s]);
                    plog("  %-14s @%p slots=%d hero ids=[%s]", k_classes[g_acts[i].cls].name, (void *)g_acts[i].obj,
                         g_acts[i].slot_count, ids);
                }
                last_arm = 0;
            }
        }
        poll_town_active_state();
        if (now - last_arm >= 2000) {
            last_arm = now;
            int armed = arm_all_threads();
            static int logged;
            if (!logged++) plog("Breakpoints applied to %d threads (re-applied every 2 s for new threads).", armed);
        }
        for (int i = 0; i < g_act_count; i++) {
            Activity cur = g_acts[i];
            if (!read_slots(&cur) || cur.slots_begin != g_acts[i].slots_begin) continue;
            for (int s = 0; s < cur.slot_count; s++)
                if (memcmp(cur.raw[s], g_acts[i].raw[s], SLOT_STRIDE)) {
                    char before[3 * SLOT_STRIDE + 1], after[3 * SLOT_STRIDE + 1];
                    hex(g_acts[i].raw[s], SLOT_STRIDE, before);
                    hex(cur.raw[s], SLOT_STRIDE, after);
                    plog("CHANGE %s@%p slot %d:", k_classes[cur.cls].name, (void *)cur.obj, s);
                    plog("    before %s", before);
                    plog("    after  %s", after);
                    memcpy(g_acts[i].raw[s], cur.raw[s], SLOT_STRIDE);
                    g_acts[i].ids[s] = cur.ids[s];
                }
        }
        poll_roster();
        if (WaitForSingleObject(g_new_thread, 50) == WAIT_OBJECT_0) arm_all_threads();
    }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    (void)inst;
    if (reason == DLL_THREAD_ATTACH) {
        if (g_new_thread) SetEvent(g_new_thread);
        return TRUE;
    }
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    InitializeCriticalSection(&g_log_lock);
    g_new_thread = CreateEventA(NULL, FALSE, FALSE, NULL);
    g_base = (uintptr_t)GetModuleHandleA(NULL);
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    strcpy(slash ? slash + 1 : path, "coop-probe.log");
    g_log = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    strcpy(slash ? slash + 1 : path, "coop-rng.log");
    g_rng = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    plog("coop-probe v12 loaded (read-only test DLL). Darkest.exe base %p", (void *)g_base);
    HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (t) CloseHandle(t);
    return TRUE;
}
