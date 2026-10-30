// Research-only tools (memory diffs, dumps, watchpoint, partner-suit experiment). Built only with
// /DCOOP_RESEARCH; see games/marvels-spider-man-remastered/NOTES.md for what each one found.

// The filter above is not enough: a LAN client still creates the host's hero by another path (not
// through either spawn function). So also gate HeroSystem create (vtable 0x1438a9858 +0xc0 ->
// 0x14083f430, args: system, request, param). It already returns null when the client data is
// missing, so callers cope with null. Logs the caller once per return address.
// ---- hardware write watchpoint (research) ----
// Arms DR0 on every thread for 8-byte writes to one address; the VEH logs who wrote (RIP + stack).
static void* volatile g_watchAddr;

static bool g_watchEnabled;  // research only
static volatile LONG g_watchHits;
static int WalkFrames(CONTEXT ctx, uint64_t* frames, int max);
static void FormatFrames(char* line, size_t size, int len, const uint64_t* frames, int n);

static DWORD WINAPI ArmWatchThread(void*) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = {sizeof te};
    DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    int armed = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
        if (!th) continue;
        if (SuspendThread(th) != (DWORD)-1) {
            CONTEXT c = {};
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(th, &c)) {
                c.Dr0 = (DWORD64)g_watchAddr;
                c.Dr7 = (c.Dr7 & ~0xF0003ULL) | 1 | (1ULL << 16) | (3ULL << 18);  // L0, write, 8 bytes
                if (SetThreadContext(th, &c)) armed++;
            }
            ResumeThread(th);
        }
        CloseHandle(th);
    }
    CloseHandle(snap);
    Log("watch: armed on %d threads for %p", armed, g_watchAddr);
    return 0;
}


static LONG CALLBACK WatchHandler(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(ep->ContextRecord->Dr6 & 1))
        return EXCEPTION_CONTINUE_SEARCH;
    ep->ContextRecord->Dr6 = 0;
    LONG n = InterlockedIncrement(&g_watchHits);
    if (n <= 12) {
        uint64_t frames[16];
        int k = WalkFrames(*ep->ContextRecord, frames, 16);
        char line[1200];
        int len = _snprintf_s(line, sizeof line, _TRUNCATE, "watch hit %ld: value now %llx, thread %lu:", n,
                              *(uint64_t*)g_watchAddr, GetCurrentThreadId());
        FormatFrames(line, sizeof line, len, frames, k);
        Log("%s", line);
    }
    if (n >= 12) ep->ContextRecord->Dr7 &= ~3ULL;  // enough
    return EXCEPTION_CONTINUE_EXECUTION;
}

static volatile LONG g_pokeRequest, g_bboxRequest;

static uint32_t g_pokeOff, g_pokeVal;

static bool g_dumpComponents;

// Components: count u16 at +0x70, list at +0x68 of 16-byte entries {?, component*}.
static void DumpComponents(const char* tag, uint8_t* actor) {
    uint16_t n = *(uint16_t*)(actor + 0x70);
    uint8_t* list = *(uint8_t**)(actor + 0x68);
    Log("comp %s: actor %p flags %04x count %u", tag, actor, *(uint16_t*)(actor + 8), n);
    for (int i = 0; list && i < n && i < 200; i++) {
        uint8_t* c = *(uint8_t**)(list + i * 16 + 8);
        if (!c) continue;
        Log("comp %s %3d: %-50s %016llx %016llx %016llx", tag, i, RttiName(c), ((uint64_t*)c)[1], ((uint64_t*)c)[2],
            ((uint64_t*)c)[3]);
    }
}

static void LogActorBrief(const char* tag, uint32_t h) {
    uint8_t* a = ((ActorFromHandleFn)VA(0x1415a0560))(&h);
    if (!a) return;
    float* n = *(float**)a;
    const char* name = *(char**)(a + 0xb0);
    Log("%s: handle %08x actor %p name '%s' asset %llx flags %04x comps %u node-bbox-h %.2f", tag, h, a, name ? name : "",
        *(uint64_t*)(a + 0x30), *(uint16_t*)(a + 8), *(uint16_t*)(a + 0x70), n ? ((float*)((uint8_t*)n + 0x54))[0] - ((float*)((uint8_t*)n + 0x44))[0] : 0);
}

static void DumpAttachments(uint8_t* hero) {
    uint8_t* al = FindComponent(hero, ".?AVAttachmentList@@");
    Log("attach: AttachmentList %p", al);
    if (!al) return;
    char line[1200];
    int len = _snprintf_s(line, sizeof line, _TRUNCATE, "attach raw:");
    for (int i = 0; i < 0x100; i += 8) len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " %llx", *(uint64_t*)(al + i));
    Log("%s", line);
    int found = 0;
    for (int i = 8; i < 0x200 && found < 24; i += 4) {
        uint32_t v = *(uint32_t*)(al + i);
        if ((v >> 20) && (v >> 20) < 0x800 && ((ActorFromHandleFn)VA(0x1415a0560))(&v)) {
            char t[32];
            _snprintf_s(t, sizeof t, _TRUNCATE, "attach +%x", i);
            LogActorBrief(t, v), found++;
        }
        uint64_t q = (i % 8 == 0) ? *(uint64_t*)(al + i) : 0;
        if (q > 0x10000000000ULL && q < 0x7fffffffffffULL && !IsBadReadPtr((void*)q, 0x100)) {
            for (int j = 0; j < 0x100 && found < 24; j += 4) {
                uint32_t w = *(uint32_t*)(q + j);
                if ((w >> 20) && (w >> 20) < 0x800 && ((ActorFromHandleFn)VA(0x1415a0560))(&w)) {
                    char t[40];
                    _snprintf_s(t, sizeof t, _TRUNCATE, "attach +%x->+%x", i, j);
                    LogActorBrief(t, w), found++;
                }
            }
        }
    }
}

// Look for bone-pose buffers: pointers (one or two levels deep from the AnimControllerComponent) to memory
// holding many floats near the actor's position. Logs where they are and a sample.
static void ScanPose(const char* tag, uint8_t* actor) {
    uint8_t* ac = FindComponent(actor, ".?AVAnimControllerComponent@@");
    float* n = *(float**)actor;
    if (!ac || !n) return;
    float px = n[12], py = n[13], pz = n[14];
    Log("pose %s: AnimController %p, actor pos %.1f %.1f %.1f", tag, ac, px, py, pz);
    auto score = [&](uint8_t* m, int bytes) {
        int hits = 0;
        for (int k = 0; k + 12 <= bytes; k += 4) {
            float* f = (float*)(m + k);
            if (fabsf(f[0] - px) < 3 && fabsf(f[1] - py) < 3 && fabsf(f[2] - pz) < 3) hits++;
        }
        return hits;
    };
    for (int i = 0; i < 0x400; i += 8) {
        uint64_t q = *(uint64_t*)(ac + i);
        if (q < 0x10000000000ULL || q > 0x7fffffffffffULL || IsBadReadPtr((void*)q, 0x1000)) continue;
        int h = score((uint8_t*)q, 0x1000);
        if (h >= 4) Log("pose %s: ac+%x -> %p : %d position-like vec3s in 4 KB", tag, i, (void*)q, h);
        for (int j = 0; j < 0x200; j += 8) {
            uint64_t r = *(uint64_t*)(q + j);
            if (r < 0x10000000000ULL || r > 0x7fffffffffffULL || IsBadReadPtr((void*)r, 0x1000)) continue;
            int h2 = score((uint8_t*)r, 0x1000);
            if (h2 >= 4) Log("pose %s: ac+%x->+%x -> %p : %d position-like vec3s", tag, i, j, (void*)r, h2);
        }
    }
}

// Pose research: snapshot every buffer reachable from the AnimControllerComponent (3 pointer levels,
// 16 KB each), diff later and report buffers with many changed dwords (a joint pose changes every frame).
static const int kSnapBytes = 0x4000, kSnapMax = 2048;

struct SnapBuf { uint8_t* addr; uint16_t path[3]; uint8_t data[kSnapBytes]; };

static SnapBuf* g_snap;

static int g_snapCount;

static int UnitQuatRun(const uint8_t* m, int bytes, int stride) {
    int best = 0, run = 0;
    for (int k = 0; k + 16 <= bytes; k += stride) {
        const float* q = (const float*)(m + k);
        float n = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
        if (n > 0.98f && n < 1.02f) { run++; if (run > best) best = run; } else run = 0;
    }
    return best;
}

static bool Readable(const void* p, size_t n) {
    MEMORY_BASIC_INFORMATION mi;
    if (!VirtualQuery(p, &mi, sizeof mi) || mi.State != MEM_COMMIT || (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    return (uint8_t*)p + n <= (uint8_t*)mi.BaseAddress + mi.RegionSize;
}

static bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static uint8_t* PtrAt(uint8_t* base, int off) {
    uint64_t q = 0;
    if (!SafeCopy(&q, base + off, 8)) return nullptr;
    return (q > 0x10000000000ULL && q < 0x7fffffffffffULL && !(q & 7)) ? (uint8_t*)q : nullptr;
}

// root: 0 AnimControllerComponent, 1 actor node, 2 actor, 3 skin part 0 model node
static int g_diffRoot;

static uint8_t* DiffRoot(uint8_t* actor, int root) {
    if (root == 0) return FindComponent(actor, ".?AVAnimControllerComponent@@");
    if (root == 1) return *(uint8_t**)actor;
    if (root == 2) return actor;
    uint8_t* sk = FindComponent(actor, ".?AVSkinManager@@");
    if (!sk || !*(uint8_t**)(sk + 0x50)) return nullptr;
    uint32_t h = **(uint32_t**)(sk + 0x50);  // part 0 entry +0: model node handle
    return ((uint8_t*(__fastcall*)(void*, uint32_t))VA(0x14191ba30))(*(uint8_t**)(sk + 0x50), h & 0xffffff);
}

static void AnimDiff(uint8_t* actor, const char* tag) {
    uint8_t* rootp = DiffRoot(actor, g_diffRoot);
    if (!rootp) {
        Log("adiff %s: root %d not found", tag, g_diffRoot);
        return;
    }
    if (!g_snap) {
        g_snap = (SnapBuf*)VirtualAlloc(nullptr, sizeof(SnapBuf) * kSnapMax, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        g_snapCount = 0;
        auto add = [&](uint8_t* a, int i, int j, int k) {
            if (g_snapCount >= kSnapMax) return;
            for (int n = 0; n < g_snapCount; n++) if (g_snap[n].addr == a) return;
            SnapBuf& sb = g_snap[g_snapCount];
            if (!SafeCopy(sb.data, a, kSnapBytes)) return;
            sb.addr = a, sb.path[0] = (uint16_t)i, sb.path[1] = (uint16_t)j, sb.path[2] = (uint16_t)k;
            g_snapCount++;
        };
        add(rootp, 0xffff, 0xffff, 0xffff);
        for (int i = 0x8; i < 0x1000; i += 8) {
            uint8_t* p1 = PtrAt(rootp, i);
            if (!p1 || p1 == actor) continue;
            add(p1, i, 0xffff, 0xffff);
            for (int j = 0; j < 0x400; j += 8) {
                uint8_t* p2 = PtrAt(p1, j);
                if (!p2 || p2 == actor) continue;
                add(p2, i, j, 0xffff);
                for (int k = 0; k < 0x200; k += 8) {
                    uint8_t* p3 = PtrAt(p2, k);
                    if (p3 && p3 != actor) add(p3, i, j, k);
                }
            }
        }
        Log("adiff %s: root %d %p (%s), snapshot of %d buffers", tag, g_diffRoot, rootp, RttiName(rootp), g_snapCount);
        return;
    }
    static uint8_t now[kSnapBytes];
    int shown = 0;
    for (int n = 0; n < g_snapCount && shown < 40; n++) {
        SnapBuf& sb = g_snap[n];
        if (!SafeCopy(now, sb.addr, kSnapBytes)) continue;
        int changed = 0, first = -1, last = 0;
        for (int o = 0; o < kSnapBytes; o += 4)
            if (*(uint32_t*)(now + o) != *(uint32_t*)(sb.data + o)) { changed++, last = o; if (first < 0) first = o; }
        if (changed < 150) continue;
        shown++;
        Log("adiff %s: +%x->+%x->+%x %p: %d dwords changed (+%x..+%x), unit-quat runs s16 %d s32 %d s48 %d s64 %d", tag,
            sb.path[0], sb.path[1], sb.path[2], sb.addr, changed, first, last, UnitQuatRun(now, kSnapBytes, 16),
            UnitQuatRun(now, kSnapBytes, 32), UnitQuatRun(now, kSnapBytes, 48), UnitQuatRun(now, kSnapBytes, 64));
    }
    Log("adiff %s: diff done, %d buffers with >=150 changes", tag, shown);
    VirtualFree(g_snap, 0, MEM_RELEASE);
    g_snap = nullptr;
}

static volatile LONG g_adiffRequest, g_ndiffRequest;

// Diff the node's instance block (node +0x98 base + node +0xa4 offset) between two calls.
static uint8_t* g_nsnap;

static uint8_t* g_nsnapAddr;

static const int kNodeSnap = 0x10000;

static void NodeDiff(uint8_t* actor, const char* tag) {
    uint8_t* node = *(uint8_t**)actor;
    uint8_t* blk = *(uint8_t**)(node + 0x98) + *(int32_t*)(node + 0xa4);
    if (!g_nsnap) {
        Log("ndiff %s: node %p block %p readable %d", tag, node, blk, Readable(blk, 0x100));
        if (!Readable(blk, 0x100)) return;
        char line[700];
        int len = _snprintf_s(line, sizeof line, _TRUNCATE, "ndiff %s head:", tag);
        for (int i = 0; i < 0x80; i += 8) len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " %llx", *(uint64_t*)(blk + i));
        Log("%s", line);
        g_nsnap = (uint8_t*)VirtualAlloc(nullptr, kNodeSnap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        g_nsnapAddr = blk;
        int n = 0;
        while (n < kNodeSnap && Readable(blk + n, 0x1000)) n += 0x1000;
        memcpy(g_nsnap, blk, n);
        Log("ndiff %s: snapshot %x bytes", tag, n);
        return;
    }
    int runStart = -1, prev = -64;
    for (int o = 0; o < kNodeSnap && Readable(g_nsnapAddr + o, 4); o += 4) {
        bool ch = *(uint32_t*)(g_nsnapAddr + o) != *(uint32_t*)(g_nsnap + o);
        if (ch) {
            if (o - prev > 64) {
                if (runStart >= 0) Log("ndiff %s: changed +%x..+%x", tag, runStart, prev);
                runStart = o;
            }
            prev = o;
        }
    }
    if (runStart >= 0) Log("ndiff %s: changed +%x..+%x", tag, runStart, prev);
    Log("ndiff %s: unit-quat runs s16 %d s32 %d s48 %d s64 %d", tag, UnitQuatRun(g_nsnapAddr, 0x8000, 16),
        UnitQuatRun(g_nsnapAddr, 0x8000, 32), UnitQuatRun(g_nsnapAddr, 0x8000, 48), UnitQuatRun(g_nsnapAddr, 0x8000, 64));
    VirtualFree(g_nsnap, 0, MEM_RELEASE);
    g_nsnap = nullptr;
}

static void PuppetInfo() {
    uint8_t* sys = *(uint8_t**)VA(0x147c307d8);
    uint8_t* hero = LocalHero();
    uint8_t* actor = g_puppet ? ((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet) : nullptr;
    float* hn = hero ? *(float**)hero : nullptr;
    float* pn = actor ? *(float**)actor : nullptr;
    Log("pinfo: ticks %ld sys %p heroHandle %08x hero %p pos %.1f %.1f %.1f", g_ticks, sys,
        sys ? *(uint32_t*)(sys + 0x168) : 0, hero, hn ? hn[12] : 0, hn ? hn[13] : 0, hn ? hn[14] : 0);
    Log("pinfo: puppet %08x actor %p flags %04x node %p pos %.1f %.1f %.1f", g_puppet, actor,
        actor ? *(uint16_t*)(actor + 8) : 0, pn, pn ? pn[12] : 0, pn ? pn[13] : 0, pn ? pn[14] : 0);
    for (int k = 0; k < 2; k++) {
        uint8_t* a = k ? actor : hero;
        uint8_t* node = a ? *(uint8_t**)a : nullptr;
        if (!node) continue;
        char line[1024];
        int len = _snprintf_s(line, sizeof line, _TRUNCATE, "node %s %p:", k ? "puppet" : "hero", node);
        for (int i = 0x40; i < 0xc0; i += 4) len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " %x=%08x", i, *(uint32_t*)(node + i));
        Log("%s", line);
        len = _snprintf_s(line, sizeof line, _TRUNCATE, "actor %s:", k ? "puppet" : "hero");
        for (int i = 0; i < 0xc0; i += 8) len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " %x=%llx", i, *(uint64_t*)(a + i));
        Log("%s", line);
    }
    if (hero && *(uint8_t**)(hero + 0x20)) {
        uint64_t* x = *(uint64_t**)(hero + 0x20);
        Log("hero +0x20 -> %p rtti %s : %llx %llx %llx %llx %llx %llx %llx %llx", x, RttiName(x), x[0], x[1], x[2], x[3], x[4], x[5],
            x[6], x[7]);
        int n = *(uint16_t*)(hero + 0x28);
        for (int i = 0; i < n && i < 8; i++) {
            uint32_t ch = ((uint32_t*)x)[i];
            uint8_t* c = ((ActorFromHandleFn)VA(0x1415a0560))(&ch);
            if (!c) continue;
            float* cn = *(float**)c;
            Log("child %d: handle %08x actor %p name '%s' asset %llx flags %04x comps %u parent %08x pos %.1f %.1f %.1f",
                i, ch, c, *(char**)(c + 0xb0) ? *(char**)(c + 0xb0) : "", *(uint64_t*)(c + 0x30), *(uint16_t*)(c + 8),
                *(uint16_t*)(c + 0x70), *(uint32_t*)(c + 0x18), cn ? cn[12] : 0, cn ? cn[13] : 0, cn ? cn[14] : 0);
            if (g_dumpComponents) DumpComponents("child", c);
        }
    }
    for (int k = 0; k < 2; k++) {
        uint8_t* a = k ? actor : hero;
        uint8_t* sk = a ? FindComponent(a, ".?AVSkinManager@@") : nullptr;
        if (!sk) continue;
        for (int row = 0; row < 0x180; row += 0x40) {
            char line[700];
            int len = _snprintf_s(line, sizeof line, _TRUNCATE, "skin %s +%03x:", k ? "puppet" : "hero  ", row);
            for (int i = row; i < row + 0x40; i += 8) len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " %016llx", *(uint64_t*)(sk + i));
            Log("%s", line);
        }
    }
    if (hero && g_dumpComponents) DumpComponents("hero", hero);
    if (actor && g_dumpComponents) DumpComponents("puppet", actor);
    if (actor) {
        char line[512];
        int len = _snprintf_s(line, sizeof line, _TRUNCATE, "pinfo: actor qwords");
        for (int i = 0; i < 18; i++) len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " %llx", ((uint64_t*)actor)[i]);
        Log("%s", line);
    }
}

static bool g_partsFull;

// One line per enabled part of an actor (asset, entry flags at +0x6b4, item look hash), the full entry with "pparts full".
static void DumpParts(uint8_t* actor, const char* who) {
    uint8_t* hs = actor ? FindComponent(actor, ".?AVSkinManager@@") : nullptr;
    if (!hs) return;
    uint8_t* parts = *(uint8_t**)(hs + 0x50);
    Log("parts %s: bits %llx", who, *(uint64_t*)(hs + 0x58));
    for (uint32_t i = 0; i < *(uint32_t*)(hs + 0x4c) && i < 128; i++) {
        uint8_t* e = parts + (uint64_t)i * 0x6b8;
        bool on = (*(uint64_t*)(hs + 0x58 + (i >> 6) * 8) >> (i & 63)) & 1;
        if (!on && !*(uint64_t*)(e + 0x3c)) continue;
        Log("parts %s %2u %s asset %llx flags %04x look %llx", who, i, on ? "on " : "off", *(uint64_t*)(e + 0x3c),
            *(uint16_t*)(e + 0x6b4), *(uint64_t*)(e + 0x98));
        if (!on || !g_partsFull) continue;
        for (int o = 0; o < 0x6b8; o += 32) {
            char line[200];
            int len = _snprintf_s(line, sizeof line, _TRUNCATE, "part %2u +%03x:", i, o);
            for (int k = 0; k < 32 && o + k < 0x6b8; k += 8) len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " %016llx", *(uint64_t*)(e + o + k));
            Log("%s", line);
        }
    }
}


// Pose: the scene node's +0xd8 points to the instance's joint buffer, 32-byte entries (unit quaternion
// first)... actually 64-byte 4x4 matrices, one per skeleton joint (model node +0x88 -> +0x200 skeleton, joint count u16 at +2).
static bool g_poseCopy;

static void CopyPose(uint8_t* fromActor, uint8_t* toActor) {
    uint8_t* fn = *(uint8_t**)fromActor;
    uint8_t* tn = *(uint8_t**)toActor;
    if (!fn || !tn) return;
    uint8_t* fb = *(uint8_t**)(fn + 0xd8);
    uint8_t* tb = *(uint8_t**)(tn + 0xd8);
    uint32_t n = JointCount(fn);
    static bool logged;
    if (!logged) {
        logged = true;
        Log("pose: hero joints %u buf %p, puppet joints %u buf %p", n, fb, JointCount(tn), tb);
        float* q = (float*)fb;
        for (int j = 0; j < 4 && fb; j++, q += 8)
            Log("pose: joint %d: %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f", j, q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]);
    }
    if (fb && tb && fb != tb && n && n == JointCount(tn)) memcpy(tb, fb, n * 64);  // 4x4 matrices
}

// Strip the puppet and dress it again through the game's item-load path (LoadSkinItem) with our own
// parts: tests that path on one instance.
static volatile LONG g_reloadRequest;

static void ReloadPuppetSuit(uint8_t* puppet, bool stripOnly) {
    uint8_t* hero = LocalHero();
    PartDesc parts[kMaxParts];
    int n = hero ? CollectParts(hero, parts) : 0;
    uint8_t* hs = hero ? FindComponent(hero, ".?AVSkinManager@@") : nullptr;
    uint8_t* ps = FindComponent(puppet, ".?AVSkinManager@@");
    if (!n || !ps || !hs) return;
    uint32_t count = *(uint32_t*)(ps + 0x4c);
    for (uint32_t i = 0; i < count && i < 128; i++)
        if ((*(uint64_t*)(ps + 0x58 + (i >> 6) * 8) >> (i & 63)) & 1)
            (*(void(__fastcall**)(void*, uint32_t))(*(uint8_t**)ps + 0x88))(ps, i);
    Log("preload: puppet stripped, parts %llx", *(uint64_t*)(ps + 0x58));
    if (stripOnly) return;
    void* vt = *(void**)(*(uint8_t**)(hs + 0x50) + 0x70);
    for (int k = 0; k < n; k++) {
        uint8_t* e = *(uint8_t**)(hs + 0x50) + (uint64_t)parts[k].index * 0x6b8;
        Log("preload: part %u asset %llx (item asset %llx)", parts[k].index, parts[k].asset, *(uint64_t*)(e + 0xa0));
        LoadSkinItem(ps, parts[k].asset, vt);
    }
}

static uint8_t* g_warpWatch;

static LONG g_warpWatchUntil;

static uint8_t g_warpWatchLast = 0xff;

// Called at the start of every game-thread tick (PuppetTick).
static void ResearchTick() {
    if (!g_watchAddr && g_watchEnabled) {
        uint8_t* h = LocalHero();
        uint8_t* sk = h ? FindComponent(h, ".?AVSkinManager@@") : nullptr;
        if (sk) {
            Log("watch (tick): hero %p SkinManager %p parts %llx", h, sk, *(uint64_t*)(sk + 0x58));
            g_watchAddr = sk + 0x58;
            CloseHandle(CreateThread(nullptr, 0, ArmWatchThread, nullptr, 0, nullptr));
        }
    }
    if (InterlockedExchange(&g_infoRequest, 0)) {
        __try {
            PuppetInfo();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("pinfo: faulted");
        }
    }
    if (InterlockedExchange(&g_ndiffRequest, 0)) {
        if (uint8_t* h = LocalHero()) NodeDiff(h, "hero");
    }
    if (InterlockedExchange(&g_adiffRequest, 0)) {
        if (uint8_t* h = LocalHero()) AnimDiff(h, "hero");
    }
    if (InterlockedExchange(&g_priusRequest, 0)) {
        DumpParts(LocalHero(), "hero");
        if (g_puppet) DumpParts(((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet), "puppet");
    }
    if (g_warpWatch && g_ticks < g_warpWatchUntil && g_warpWatch[0xa8] != g_warpWatchLast) {
        g_warpWatchLast = g_warpWatch[0xa8];
        uint8_t* h = LocalHero();
        float* n = h ? *(float**)h : nullptr;
        Log("warp: tick %ld pending %u hero pos %.1f %.1f %.1f", g_ticks, g_warpWatchLast, n ? n[12] : 0, n ? n[13] : 0, n ? n[14] : 0);
    }
    if (g_puppet && (g_pokeRequest || g_bboxRequest)) {
        uint8_t* a = ((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet);
        uint8_t* pn = a ? *(uint8_t**)a : nullptr;
        uint8_t* h = LocalHero();
        if (pn && InterlockedExchange(&g_pokeRequest, 0)) {
            Log("ppoke: node+%x %08x -> %08x", g_pokeOff, *(uint32_t*)(pn + g_pokeOff), g_pokeVal);
            *(uint32_t*)(pn + g_pokeOff) = g_pokeVal;
        }
        if (pn && h && *(uint8_t**)h && InterlockedExchange(&g_bboxRequest, 0)) {
            memcpy(pn + 0x40, *(uint8_t**)h + 0x40, 0x1c);
            Log("pbbox: copied hero bounds to puppet node");
        }
    }
    if (LONG r = g_puppet ? InterlockedExchange(&g_reloadRequest, 0) : 0) {
        if (uint8_t* a = ((ActorFromHandleFn)VA(0x1415a0560))(&g_puppet)) ReloadPuppetSuit(a, r == 2);
    }
}

// Research commands (see DoCommand). Returns false if the command isn't one of them.
static bool ResearchCommand(const char* cmd) {
    if (!strncmp(cmd, "ppoke", 5)) {
        sscanf_s(cmd + 5, "%x %x", &g_pokeOff, &g_pokeVal);
        if (g_pokeOff < 0x100) g_pokeRequest = 1;
    } else if (!strcmp(cmd, "ndiff")) {
        g_ndiffRequest = 1;
    } else if (!strncmp(cmd, "adiff", 5)) {
        if (cmd[5] == ' ') g_diffRoot = atoi(cmd + 6);
        g_adiffRequest = 1;
    } else if (!strcmp(cmd, "pcopy")) {
        g_poseCopy = !g_poseCopy;
        Log("pose copy %s", g_poseCopy ? "on" : "off");
    } else if (!strcmp(cmd, "preload")) {
        g_reloadRequest = 1;
    } else if (!strcmp(cmd, "pstrip")) {
        g_reloadRequest = 2;
    } else if (!strncmp(cmd, "pparts", 6)) {
        g_partsFull = !strcmp(cmd + 6, " full");
        g_priusRequest = 1;
    } else if (!strcmp(cmd, "pbbox")) {
        g_bboxRequest = 1;
    } else if (!strcmp(cmd, "pinfo")) {
        Log("pinfo: game-thread ticks so far %ld", g_ticks);
        g_infoRequest = 1;
    } else {
        return false;
    }
    return true;
}

// Called from HeroCreateGate with each local hero it creates.
static void ResearchOnHeroCreated(uint8_t* hero) {
    if (!g_watchAddr && g_watchEnabled) {
        __try {
            uint8_t* sk = FindComponent(hero, ".?AVSkinManager@@");
            Log("watch: hero %p SkinManager %p parts %llx", hero, sk, sk ? *(uint64_t*)(sk + 0x58) : 0);
            if (sk) {
                g_watchAddr = sk + 0x58;
                CloseHandle(CreateThread(nullptr, 0, ArmWatchThread, nullptr, 0, nullptr));
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("watch: setup faulted");
        }
    }
}
