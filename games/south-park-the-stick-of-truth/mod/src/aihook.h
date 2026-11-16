// Signature-agnostic MinHook detours for the buddy AI's virtual functions.
//
// The game is CEG-packed: .text on disk is encrypted, so a function cannot be identified by
// disassembling the file. The live pages are decrypted, but the argument list of a vtable slot
// is still unknown. These stubs therefore do not declare a C signature for the target: they
// save every register and flags, call a bookkeeping helper, and tail-jump to MinHook's
// trampoline. The "hold" variant swaps the return address on the stack and jumps to the
// trampoline, so the original function cleans its own arguments and returns into our
// continuation, which restores the buddy's anchor and then resumes the caller. That keeps the
// caller's stack and all callee-saved registers correct without knowing whether the slot is
// __thiscall, __stdcall, __cdecl, or takes no arguments at all.
#pragma once

#include <stdint.h>

namespace aihook {

// Called on the first intercepted call of a hook (from game code, so keep it cheap).
typedef void (*CallNotify)(int id, uintptr_t self, unsigned long long count);

// Called after the hooked function returns, while the stack is still the caller's final stack.
typedef void (*HoldAfter)(int id, uintptr_t self);

bool initialize();
void set_notify(CallNotify notify);

// Install a counting hook on `target`. Returns a stable id, or -1. Repeating the same target
// returns the existing id (the caller can attach several vtable slots to one function).
int install(uintptr_t target, const char* label);

// Replace the counting detour of `id` with the hold detour. The target is unhooked and
// re-hooked while MinHook freezes the other threads, so no call executes a half-written stub.
bool switch_to_hold(int id, HoldAfter after);
bool revert_to_counting(int id);

void set_hold_id(int id);
int hold_id();
unsigned long long calls(int id);
uintptr_t last_self(int id);
int target(int id);
bool is_hold(int id);
int count();
const char* label(int id);
void reset_counts();

}  // namespace aihook
