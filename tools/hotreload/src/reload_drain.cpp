#include "hotreload/reload_drain.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "mut.h"

namespace hotreload {

CodeRange moduleRangeOf(const void* p) {
    CodeRange r;
    HMODULE m = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(p), &m) || !m)
        return r;
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(m);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const char*>(m) + dos->e_lfanew);
    r.lo = reinterpret_cast<uintptr_t>(m);
    r.hi = r.lo + nt->OptionalHeader.SizeOfImage;
    r.what = "core image";
    return r;
}

namespace {

constexpr size_t kMaxThreads = 4096;

typedef LONG(NTAPI* NtQueryInformationThreadFn)(HANDLE, ULONG, PVOID, ULONG, PULONG);
struct ThreadBasicInfo {
    LONG exitStatus;
    PVOID teb;
    PVOID clientIdProcess, clientIdThread;
    ULONG_PTR affinity;
    LONG priority, basePriority;
};

#if defined(_M_IX86)
#define HR_IP(c) ((c).Eip)
#define HR_SP(c) ((c).Esp)
#else
#define HR_IP(c) ((c).Rip)
#define HR_SP(c) ((c).Rsp)
#endif

// Why a thread counted as inside. Filled while the threads are suspended (plain stores only), read after they are resumed.
struct Hit {
    int kind = 0;            // 0 none, 1 instruction pointer, 2 stack word (return address), 3 context unreadable,
                             // 4 thread could not be opened, 5 thread could not be suspended, 6 too many threads to inspect
    uintptr_t value = 0;     // the address that is inside a range
    uintptr_t stackOff = 0;  // kind 2: distance of that word from the stack pointer, in bytes
    int rangeIdx = -1;
};

inline int rangeIndex(uintptr_t v, const CodeRange* r, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (v >= r[i].lo && v < r[i].hi) return static_cast<int>(i);
    return -1;
}

// Is `h` (a thread handle, already suspended) inside any range? No allocation, no logging: other threads are suspended while this runs.
bool isInside(HANDLE h, NtQueryInformationThreadFn ntq, const CodeRange* r, size_t n, Hit* hit) {
    CONTEXT ctx;
    std::memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (!GetThreadContext(h, &ctx)) {  // cannot prove it is outside: count it as busy
        hit->kind = 3;
        return true;
    }
    int ri = rangeIndex(HR_IP(ctx), r, n);
    if (ri >= 0) {
        hit->kind = 1;
        hit->value = HR_IP(ctx);
        hit->rangeIdx = ri;
        return true;
    }
    if (HR_MUT(17)) return false;
    ThreadBasicInfo tbi;
    std::memset(&tbi, 0, sizeof tbi);
    uintptr_t base = 0;
    if (ntq && ntq(h, 0 /*ThreadBasicInformation*/, &tbi, sizeof tbi, nullptr) >= 0 && tbi.teb) {
        __try {
            base = *reinterpret_cast<const uintptr_t*>(static_cast<const char*>(tbi.teb) + sizeof(void*));  // NT_TIB.StackBase
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            base = 0;
        }
    }
    uintptr_t sp = HR_SP(ctx) & ~(sizeof(void*) - 1);
    uintptr_t end = base ? base : sp + 65536;
    if (end > sp + (1u << 20)) end = sp + (1u << 20);
    for (uintptr_t a = sp; a + sizeof(void*) <= end; a += sizeof(void*)) {
        uintptr_t v;
        __try {
            v = *reinterpret_cast<const uintptr_t*>(a);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            break;
        }
        ri = rangeIndex(v, r, n);
        if (ri >= 0) {
            hit->kind = 2;
            hit->value = v;
            hit->stackOff = a - sp;
            hit->rangeIdx = ri;
            return true;
        }
    }
    return false;
}

std::string hex(uintptr_t v) {
    char b[32];
    std::snprintf(b, sizeof b, "0x%llX", static_cast<unsigned long long>(v));
    return b;
}

std::string jsonEsc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += static_cast<char>(c); }
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += static_cast<char>(c);
    }
    return o;
}

std::string describe(const Hit& h, const std::vector<CodeRange>& ranges) {
    auto label = [&](int i) {
        std::string s = (i >= 0 && i < static_cast<int>(ranges.size()) && ranges[i].what && *ranges[i].what) ? ranges[i].what : "watched code";
        if (i >= 0 && i < static_cast<int>(ranges.size())) s += " " + hex(ranges[i].lo) + "-" + hex(ranges[i].hi);
        return s;
    };
    switch (h.kind) {
        case 1: return "instruction pointer " + hex(h.value) + " is in the " + label(h.rangeIdx);
        case 2: return "return address " + hex(h.value) + " on its stack (sp+" + std::to_string(h.stackOff) + ") points into the " + label(h.rangeIdx);
        case 3: return "its register context could not be read, so it cannot be proven to be outside";
        case 4: return "the thread exists but could not be opened, so it cannot be proven to be outside";
        case 5: return "the thread could not be suspended, so it cannot be proven to be outside";
        case 6: return "more than " + std::to_string(kMaxThreads) + " threads, not all can be inspected";
        default: return "";
    }
}

}  // namespace

std::string DrainResult::toJson() const {
    return std::string("{\"drained\":") + (drained ? "true" : "false") + ",\"attempts\":" + std::to_string(attempts) + ",\"scanned\":" + std::to_string(scanned) +
           ",\"busyThread\":" + std::to_string(busyThread) + ",\"elapsedMs\":" + std::to_string(elapsedMs) + ",\"timeoutMs\":" + std::to_string(timeoutMs) +
           ",\"why\":\"" + jsonEsc(why) + "\",\"where\":\"" + jsonEsc(where) + "\"}";
}

DrainResult waitDrained(const std::vector<CodeRange>& ranges, const std::vector<unsigned long>& ignore, unsigned timeoutMs) {
    using Clock = std::chrono::steady_clock;
    DrainResult res;
    res.timeoutMs = timeoutMs;
    if (HR_MUT(1)) {
        res.drained = true;
        return res;
    }
    NtQueryInformationThreadFn ntq = nullptr;
    if (HMODULE nt = GetModuleHandleA("ntdll.dll")) ntq = reinterpret_cast<NtQueryInformationThreadFn>(GetProcAddress(nt, "NtQueryInformationThread"));
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    const auto t0 = Clock::now();
    const auto deadline = t0 + std::chrono::milliseconds(timeoutMs);
    auto elapsed = [&] { return static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count()); };
    std::vector<HANDLE> handles;  // everything below is reserved before any thread is suspended
    handles.reserve(kMaxThreads);
    std::vector<unsigned long> ids;
    ids.reserve(kMaxThreads);
    std::vector<unsigned char> suspendedOk;
    suspendedOk.reserve(kMaxThreads);
    for (;;) {
        ++res.attempts;
        ids.clear();
        handles.clear();
        suspendedOk.clear();
        bool busy = false;
        unsigned long busyTid = 0;
        Hit hit;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) {  // cannot even list the threads: cannot prove anything
            busy = true;
            hit.kind = 3;
        } else {
            THREADENTRY32 te;
            te.dwSize = sizeof te;
            if (Thread32First(snap, &te))
                do {
                    if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
                    if (std::find(ignore.begin(), ignore.end(), te.th32ThreadID) != ignore.end()) continue;
                    if (ids.size() < kMaxThreads) ids.push_back(te.th32ThreadID);
                    else if (!busy) { busy = true; busyTid = te.th32ThreadID; hit.kind = 6; }
                } while (Thread32Next(snap, &te));
            CloseHandle(snap);
        }
        for (unsigned long id : ids) {
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, id);
            if (h) handles.push_back(h);
            else if (GetLastError() != ERROR_INVALID_PARAMETER && !busy) { busy = true; busyTid = id; hit.kind = 4; }  // exists but cannot be inspected
        }
        // ---- from here until the resume loop: no allocation, no logging ----
        for (HANDLE h : handles) suspendedOk.push_back(SuspendThread(h) != static_cast<DWORD>(-1) ? 1 : 0);  // capacity reserved above
        for (size_t i = 0; i < handles.size(); ++i) {
            if (busy) break;
            if (!suspendedOk[i]) {  // a thread that is not stopped cannot be read reliably
                busy = true;
                busyTid = GetThreadId(handles[i]);
                hit.kind = 5;
            } else if (isInside(handles[i], ntq, ranges.data(), ranges.size(), &hit)) {
                busy = true;
                busyTid = GetThreadId(handles[i]);
            }
        }
        for (size_t i = 0; i < handles.size(); ++i)
            if (suspendedOk[i]) ResumeThread(handles[i]);
        res.scanned = static_cast<int>(handles.size());
        for (HANDLE h : handles) CloseHandle(h);
        res.elapsedMs = elapsed();
        if (!busy) {
            res.drained = true;
            res.busyThread = 0;
            res.why.clear();
            res.where.clear();
            return res;
        }
        res.busyThread = busyTid;
        res.where = describe(hit, ranges);
        // A scan can itself be slow (many threads, a loaded machine). A positive timeout therefore always gets one retry, so a
        // slow first scan cannot end the wait without the thread ever having had a chance to leave.
        const bool timedOut = Clock::now() >= deadline && (res.attempts >= 2 || timeoutMs == 0);
        if (timedOut) {
            if (HR_MUT(18)) {
                res.drained = true;
                return res;
            }
            res.why = "drain timed out after " + std::to_string(res.elapsedMs) + " ms (limit " + std::to_string(timeoutMs) + " ms, " + std::to_string(res.attempts) +
                      " attempt(s), " + std::to_string(res.scanned) + " thread(s) inspected): thread " + std::to_string(busyTid) +
                      " is still inside the code that would be unloaded: " + res.where;
            return res;
        }
        Sleep(1);
    }
}

}  // namespace hotreload
