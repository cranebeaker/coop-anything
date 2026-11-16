// Offline test for the signature-neutral stubs in aihook.cpp. It runs without the game and
// covers the calling conventions the AI vtables can use: cdecl, stdcall, no arguments, and a
// callee-saved-register check. The hold path is exercised with a callback that records the
// call, and every target's return value is compared with the value the C++ compiler produced.
#include <windows.h>
#include <cstdio>
#include <cstdint>

#include "aihook.h"

namespace {

int g_after_calls = 0;
uintptr_t g_last_self = 0;
int g_failures = 0;

extern "C" __attribute__((noinline)) int target_cdecl(int a, int b, int c);
extern "C" __attribute__((noinline)) int __stdcall target_stdcall(int a, int b);
extern "C" __attribute__((noinline)) int target_noarg();

typedef int(__cdecl* CdeclFn)(int, int, int);
typedef int(__stdcall* StdcallFn)(int, int);
typedef int(__cdecl* NoargFn)();
volatile CdeclFn g_cdecl = &target_cdecl;
volatile StdcallFn g_stdcall = &target_stdcall;
volatile NoargFn g_noarg = &target_noarg;

void check(bool condition, const char* what)
{
    std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
}

extern "C" void hold_after(int id, uintptr_t self)
{
    (void)id;
    ++g_after_calls;
    g_last_self = self;
}

extern "C" __attribute__((noinline)) int target_cdecl(int a, int b, int c)
{
    return a * 100 + b * 10 + c;
}

extern "C" __attribute__((noinline)) int __stdcall target_stdcall(int a, int b)
{
    return a * 1000 + b;
}

extern "C" __attribute__((noinline)) int target_noarg()
{
    return 42;
}

extern "C" __attribute__((noinline)) int preserved_target()
{
    return 7;
}

// Calls preserved_target with non-default callee-saved registers and checks they survive the
// detour, the original and the continuation.
extern "C" __attribute__((naked)) int check_preserved_registers()
{
    __asm__ volatile(
        "push %ebx\n"
        "push %esi\n"
        "push %edi\n"
        "mov $0x11111111, %ebx\n"
        "mov $0x22222222, %esi\n"
        "mov $0x33333333, %edi\n"
        "call _preserved_target\n"
        "xor %eax, %eax\n"
        "cmp $0x11111111, %ebx\n"
        "jne 1f\n"
        "cmp $0x22222222, %esi\n"
        "jne 1f\n"
        "cmp $0x33333333, %edi\n"
        "jne 1f\n"
        "mov $1, %eax\n"
        "1:\n"
        "pop %edi\n"
        "pop %esi\n"
        "pop %ebx\n"
        "ret\n");
}

}  // namespace

int main()
{
    if (!aihook::initialize()) {
        std::printf("FAIL aihook::initialize\n");
        return 1;
    }

    const volatile unsigned char* before =
        reinterpret_cast<const volatile unsigned char*>(&target_cdecl);
    const int cdecl_id =
        aihook::install(reinterpret_cast<uintptr_t>(&target_cdecl), "cdecl");
    check(cdecl_id >= 0, "install cdecl counting hook");
    check(before[0] == 0xE9, "MinHook wrote the relative jump at the target");
    check(g_cdecl(1, 2, 3) == 123, "cdecl call passes through counting stub");
    check(aihook::calls(cdecl_id) >= 1, "cdecl counter increments");

    check(aihook::switch_to_hold(cdecl_id, &hold_after), "switch cdecl to hold");
    check(g_cdecl(4, 5, 6) == 456, "cdecl return value survives hold continuation");
    check(g_after_calls == 1, "cdecl hold callback runs after the original");
    for (int index = 0; index < 100; ++index) {
        if (g_cdecl(index, 1, 2) != index * 100 + 12) {
            check(false, "cdecl loop stack/return");
            break;
        }
    }
    check(g_cdecl(9, 9, 9) == 999, "cdecl stack is balanced after 102 calls");

    const int stdcall_id =
        aihook::install(reinterpret_cast<uintptr_t>(&target_stdcall), "stdcall");
    check(stdcall_id >= 0, "install stdcall counting hook");
    check(aihook::switch_to_hold(stdcall_id, &hold_after), "switch stdcall to hold");
    const int before_stdcall = g_after_calls;
    check(g_stdcall(7, 8) == 7008, "stdcall return value survives hold");
    check(g_stdcall(1, 2) == 1002, "second stdcall call keeps the stack");
    check(g_after_calls == before_stdcall + 2, "stdcall hold callback runs");

    const int noarg_id =
        aihook::install(reinterpret_cast<uintptr_t>(&target_noarg), "noarg");
    check(noarg_id >= 0, "install no-argument counting hook");
    check(aihook::switch_to_hold(noarg_id, &hold_after), "switch no-argument to hold");
    const int before_noarg = g_after_calls;
    check(g_noarg() == 42, "no-argument return value survives hold");
    check(g_after_calls == before_noarg + 1, "no-argument hold callback runs");

    const int preserved_id =
        aihook::install(reinterpret_cast<uintptr_t>(&preserved_target), "preserved");
    check(preserved_id >= 0, "install register-check hook");
    check(aihook::switch_to_hold(preserved_id, &hold_after), "switch register check to hold");
    check(check_preserved_registers() == 1, "EBX/ESI/EDI survive the hold detour");
    check(aihook::revert_to_counting(preserved_id), "revert register check to counting");
    check(check_preserved_registers() == 1, "EBX/ESI/EDI survive the counting detour");

    std::printf("%s (%d failure(s))\n", g_failures == 0 ? "ALL OK" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
