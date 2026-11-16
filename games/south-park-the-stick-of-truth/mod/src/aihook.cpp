// See aihook.h. The x86 stubs are generated into one RWX page because the target functions'
// signatures are unknown; a C detour with the wrong signature would either read the arguments
// from the wrong offsets or leave the caller's stack unbalanced. A tail jump through MinHook's
// trampoline is signature-neutral. The hold path adds a return-address continuation, so the
// original function still performs its own `ret n` and the caller sees exactly the stack it
// would have seen without us.
#include "aihook.h"

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "MinHook.h"

namespace {

const int kMaxHooks = 64;
const int kMaxFrames = 16;

struct Hook {
    bool used;
    bool hold;
    int id;
    uintptr_t target;
    void* stub;
    void* trampoline;
    uintptr_t fallback_ret;
    volatile LONG calls;
    volatile uintptr_t last_self;
    char label[64];
};

Hook g_hooks[kMaxHooks];
int g_hook_count = 0;
int g_hold_id = -1;
aihook::HoldAfter g_hold_after = nullptr;
aihook::CallNotify g_notify = nullptr;
bool g_initialized = false;
DWORD g_tls = TLS_OUT_OF_INDEXES;

unsigned char* g_code = nullptr;
size_t g_code_used = 0;
size_t g_code_cap = 0;

struct Frame {
    uintptr_t ret;
    uintptr_t self;
    uintptr_t ebx;
    uintptr_t ebp;
    uintptr_t esi;
    uintptr_t edi;
};

struct ThreadFrames {
    Frame frames[kMaxFrames];
    int depth;
};

// A thread-local stack of pending return-address continuations. This is what makes the hold
// stub reentrant: each intercepted call keeps its own caller return address and callee-saved
// registers, so a nested call (or another game thread) cannot overwrite them.
ThreadFrames* thread_frames()
{
    if (g_tls == TLS_OUT_OF_INDEXES) {
        return nullptr;
    }
    auto* frames = static_cast<ThreadFrames*>(TlsGetValue(g_tls));
    if (frames == nullptr) {
        frames = static_cast<ThreadFrames*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadFrames)));
        if (frames != nullptr) {
            TlsSetValue(g_tls, frames);
        }
    }
    return frames;
}

void* alloc_code(size_t size)
{
    const size_t aligned = (size + 15u) & ~static_cast<size_t>(15u);
    if (g_code == nullptr || g_code_used + aligned > g_code_cap) {
        g_code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 64 * 1024,
                                                          MEM_COMMIT | MEM_RESERVE,
                                                          PAGE_EXECUTE_READWRITE));
        if (g_code == nullptr) {
            return nullptr;
        }
        g_code_cap = 64 * 1024;
        g_code_used = 0;
    }
    void* at = g_code + g_code_used;
    g_code_used += aligned;
    return at;
}

void put8(unsigned char*& p, unsigned char value)
{
    *p++ = value;
}

void put32(unsigned char*& p, unsigned int value)
{
    std::memcpy(p, &value, sizeof(value));
    p += sizeof(value);
}

// mov eax, imm32; call eax
void emit_call_abs(unsigned char*& p, void* function)
{
    put8(p, 0xB8);
    put32(p, reinterpret_cast<unsigned int>(function));
    put8(p, 0xFF);
    put8(p, 0xD0);
}

extern "C" void ai_entry_count(int id, void* ctx);
extern "C" void ai_entry_hold(int id, void* ctx);
extern "C" void ai_exit_hold(int id, void* ctx);

void count_call(int id, void* ctx)
{
    if (id < 0 || id >= kMaxHooks) {
        return;
    }
    Hook& hook = g_hooks[id];
    if (!hook.used) {
        return;
    }
    const LONG count = InterlockedIncrement(&hook.calls);
    auto* regs = static_cast<uintptr_t*>(ctx);
    // ctx is the pushad/pushfd frame: [0]=flags, [1]=EDI, [2]=ESI, [3]=EBP, [4]=saved ESP,
    // [5]=EBX, [6]=EDX, [7]=ECX (this), [8]=EAX.
    hook.last_self = regs[7];
    if (count == 1 && g_notify != nullptr) {
        g_notify(id, regs[7], static_cast<unsigned long long>(count));
    }
}

// Build the entry half of a stub. `hold` selects the version that also records a pending
// return frame before tail-jumping to the trampoline. Returns the offset of the trampoline
// immediate so the caller can patch it after MinHook has produced the trampoline.
size_t build_entry_stub(unsigned char* code, int id, bool hold, size_t* continuation_offset)
{
    unsigned char* p = code;
    put8(p, 0x60);  // pushad
    put8(p, 0x9C);  // pushfd
    put8(p, 0x89);  // mov eax, esp
    put8(p, 0xE0);
    put8(p, 0x50);  // push eax  (context pointer)
    put8(p, 0x68);  // push id
    put32(p, static_cast<unsigned int>(id));
    emit_call_abs(p, hold ? reinterpret_cast<void*>(&ai_entry_hold)
                          : reinterpret_cast<void*>(&ai_entry_count));
    put8(p, 0x83);  // add esp, 8
    put8(p, 0xC4);
    put8(p, 0x08);
    put8(p, 0x9D);  // popfd
    put8(p, 0x61);  // popad

    if (hold) {
        // The function has not been called yet when this is written, so a placeholder is
        // enough; the continuation is emitted right after the entry and patched in below.
        put8(p, 0xC7);  // mov dword ptr [esp], imm32
        put8(p, 0x04);
        put8(p, 0x24);
        size_t placeholder = static_cast<size_t>(p - code);
        put32(p, 0);
        // remember the placeholder through continuation_offset until the caller patches it
        *continuation_offset = placeholder;
    } else {
        *continuation_offset = 0;
    }

    put8(p, 0xB8);  // mov eax, trampoline
    const size_t trampoline_immediate = static_cast<size_t>(p - code);
    put32(p, 0);
    put8(p, 0xFF);  // jmp eax
    put8(p, 0xE0);
    return trampoline_immediate;
}

void* build_count_stub(int id, size_t* trampoline_immediate)
{
    unsigned char* code = static_cast<unsigned char*>(alloc_code(64));
    if (code == nullptr) {
        return nullptr;
    }
    size_t unused = 0;
    *trampoline_immediate = build_entry_stub(code, id, false, &unused);
    return code;
}

// The entry + continuation pair. `continuation_offset` is the stack slot that must hold the
// continuation's absolute address; `trampoline_immediate` is the trampoline operand.
void* build_hold_stub(int id, size_t* continuation_offset, size_t* trampoline_immediate)
{
    unsigned char* code = static_cast<unsigned char*>(alloc_code(128));
    if (code == nullptr) {
        return nullptr;
    }
    *trampoline_immediate = build_entry_stub(code, id, true, continuation_offset);

    unsigned char* cont = code + 64;
    unsigned char* p = cont;
    put8(p, 0x50);  // push eax   (original return value)
    put8(p, 0x52);  // push edx
    put8(p, 0x60);  // pushad
    put8(p, 0x9C);  // pushfd
    put8(p, 0x89);  // mov eax, esp
    put8(p, 0xE0);
    put8(p, 0x50);  // push eax
    put8(p, 0x68);  // push id
    put32(p, static_cast<unsigned int>(id));
    emit_call_abs(p, reinterpret_cast<void*>(&ai_exit_hold));
    put8(p, 0x83);  // add esp, 8
    put8(p, 0xC4);
    put8(p, 0x08);
    put8(p, 0x9D);  // popfd
    put8(p, 0x61);  // popad
    put8(p, 0x5A);  // pop edx    (restore original EDX)
    put8(p, 0x59);  // pop ecx    (restore original EAX)
    put8(p, 0x91);  // xchg eax, ecx
    put8(p, 0xFF);  // jmp ecx
    put8(p, 0xE1);

    // Patch the continuation address into the entry.
    const unsigned int cont_address = reinterpret_cast<unsigned int>(cont);
    std::memcpy(code + *continuation_offset, &cont_address, sizeof(cont_address));
    return code;
}

bool create_detour(Hook& hook, bool hold)
{
    size_t patch = 0;
    size_t cont_offset = 0;
    unsigned char* stub = hold ? static_cast<unsigned char*>(
                                     build_hold_stub(hook.id, &cont_offset, &patch))
                               : static_cast<unsigned char*>(
                                     build_count_stub(hook.id, &patch));
    if (stub == nullptr) {
        return false;
    }
    void* trampoline = nullptr;
    if (MH_CreateHook(reinterpret_cast<void*>(hook.target), stub, &trampoline) != MH_OK ||
        trampoline == nullptr) {
        return false;
    }
    const unsigned int trampoline_address = reinterpret_cast<unsigned int>(trampoline);
    std::memcpy(stub + patch, &trampoline_address, sizeof(trampoline_address));
    if (MH_EnableHook(reinterpret_cast<void*>(hook.target)) != MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(hook.target));
        return false;
    }
    hook.stub = stub;
    hook.trampoline = trampoline;
    hook.hold = hold;
    return true;
}

}  // namespace

extern "C" void ai_entry_count(int id, void* ctx)
{
    count_call(id, ctx);
}

extern "C" void ai_entry_hold(int id, void* ctx)
{
    count_call(id, ctx);
    if (id < 0 || id >= kMaxHooks) {
        return;
    }
    ThreadFrames* frames = thread_frames();
    if (frames == nullptr || frames->depth >= kMaxFrames) {
        // The entry stub still replaces the return address, so a failed frame push would
        // lose the caller. There is no safe way to cancel the swap here; this path only
        // triggers when the process cannot allocate 400 bytes.
        return;
    }
    Frame& frame = frames->frames[frames->depth++];
    auto* regs = static_cast<uintptr_t*>(ctx);
    frame.self = regs[7];
    frame.ret = *reinterpret_cast<uintptr_t*>(reinterpret_cast<unsigned char*>(ctx) + 36);
    frame.edi = regs[1];
    frame.esi = regs[2];
    frame.ebp = regs[3];
    frame.ebx = regs[5];
    g_hooks[id].fallback_ret = frame.ret;
}

extern "C" void ai_exit_hold(int id, void* ctx)
{
    auto* regs = static_cast<uintptr_t*>(ctx);
    ThreadFrames* frames = thread_frames();
    if (frames == nullptr || frames->depth <= 0) {
        // Unreachable unless the tiny TLS allocation failed at entry. Use the most recent
        // return address rather than jumping to zero; no other state is recoverable here.
        regs[8] = (id >= 0 && id < kMaxHooks) ? g_hooks[id].fallback_ret : 0;
        return;
    }
    const Frame frame = frames->frames[--frames->depth];
    regs[1] = frame.edi;
    regs[2] = frame.esi;
    regs[3] = frame.ebp;
    regs[5] = frame.ebx;
    regs[8] = frame.ret;
    if (id == g_hold_id && g_hold_after != nullptr) {
        g_hold_after(id, frame.self);
    }
}

namespace aihook {

bool initialize()
{
    if (g_initialized) {
        return true;
    }
    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        return false;
    }
    if (g_tls == TLS_OUT_OF_INDEXES) {
        g_tls = TlsAlloc();
    }
    g_initialized = g_tls != TLS_OUT_OF_INDEXES;
    return g_initialized;
}

void set_notify(CallNotify notify)
{
    g_notify = notify;
}

int install(uintptr_t target, const char* label)
{
    if (!g_initialized || target == 0) {
        return -1;
    }
    for (int index = 0; index < g_hook_count; ++index) {
        if (g_hooks[index].used && g_hooks[index].target == target) {
            return index;
        }
    }
    if (g_hook_count >= kMaxHooks) {
        return -1;
    }
    const int id = g_hook_count;
    Hook& hook = g_hooks[id];
    std::memset(&hook, 0, sizeof(hook));
    hook.id = id;
    hook.target = target;
    hook.used = true;
    if (label != nullptr) {
        std::strncpy(hook.label, label, sizeof(hook.label) - 1);
        hook.label[sizeof(hook.label) - 1] = '\0';
    }
    if (!create_detour(hook, false)) {
        hook.used = false;
        return -1;
    }
    g_hook_count = id + 1;
    return id;
}

bool switch_to_hold(int id, HoldAfter after)
{
    if (!g_initialized || id < 0 || id >= g_hook_count || !g_hooks[id].used) {
        return false;
    }
    Hook& hook = g_hooks[id];
    if (hook.hold) {
        g_hold_id = id;
        g_hold_after = after;
        return true;
    }
    MH_DisableHook(reinterpret_cast<void*>(hook.target));
    MH_RemoveHook(reinterpret_cast<void*>(hook.target));
    hook.stub = nullptr;
    hook.trampoline = nullptr;
    if (!create_detour(hook, true)) {
        // Put the count hook back so the target is not left unhooked.
        create_detour(hook, false);
        return false;
    }
    g_hold_id = id;
    g_hold_after = after;
    return true;
}

bool revert_to_counting(int id)
{
    if (!g_initialized || id < 0 || id >= g_hook_count || !g_hooks[id].used) {
        return false;
    }
    Hook& hook = g_hooks[id];
    if (!hook.hold) {
        return true;
    }
    MH_DisableHook(reinterpret_cast<void*>(hook.target));
    MH_RemoveHook(reinterpret_cast<void*>(hook.target));
    hook.stub = nullptr;
    hook.trampoline = nullptr;
    if (!create_detour(hook, false)) {
        return false;
    }
    if (g_hold_id == id) {
        g_hold_id = -1;
        g_hold_after = nullptr;
    }
    return true;
}

void set_hold_id(int id)
{
    g_hold_id = id;
    if (id < 0) {
        g_hold_after = nullptr;
    }
}

int hold_id()
{
    return g_hold_id;
}

unsigned long long calls(int id)
{
    if (id < 0 || id >= g_hook_count || !g_hooks[id].used) {
        return 0;
    }
    const LONG count = InterlockedCompareExchange(&g_hooks[id].calls, 0, 0);
    return static_cast<unsigned long long>(count < 0 ? 0 : count);
}

uintptr_t last_self(int id)
{
    if (id < 0 || id >= g_hook_count || !g_hooks[id].used) {
        return 0;
    }
    return g_hooks[id].last_self;
}

int target(int id)
{
    if (id < 0 || id >= g_hook_count || !g_hooks[id].used) {
        return 0;
    }
    return static_cast<int>(g_hooks[id].target);
}

bool is_hold(int id)
{
    return id >= 0 && id < g_hook_count && g_hooks[id].used && g_hooks[id].hold;
}

int count()
{
    return g_hook_count;
}

const char* label(int id)
{
    if (id < 0 || id >= g_hook_count || !g_hooks[id].used) {
        return "";
    }
    return g_hooks[id].label;
}

void reset_counts()
{
    for (int index = 0; index < g_hook_count; ++index) {
        if (g_hooks[index].used) {
            InterlockedExchange(&g_hooks[index].calls, 0);
            g_hooks[index].last_self = 0;
        }
    }
}

}  // namespace aihook
