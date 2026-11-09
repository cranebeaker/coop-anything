/* One-time SDL2 bootstrap. All ordinary SDL exports are PE forwarders; SDL_PollEvent loads the
 * co-op runtime payload from DarkestDungeon/mods/DDCoop and calls its game-thread pump. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int (*sdl_poll_event_fn)(void *event);
typedef void (*ddcoop_poll_fn)(void);
typedef void (*sdl_swap_fn)(void *);
typedef void (*ddcoop_draw_fn)(void *);
typedef int (*ddcoop_event_fn)(const void *);

static INIT_ONCE g_init_once = INIT_ONCE_STATIC_INIT;
static HMODULE g_sdl2_orig;
static HMODULE g_ddcoop;
static sdl_poll_event_fn g_real_poll_event;
static ddcoop_poll_fn g_ddcoop_poll;
static sdl_swap_fn g_real_swap;
static ddcoop_draw_fn g_ddcoop_draw;
static ddcoop_event_fn g_ddcoop_event;

static int path_from_exe(char *out, size_t cap, const char *relative) {
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
    if (!n || n >= sizeof exe) return 0;
    char *slash = strrchr(exe, '\\');
    if (!slash) return 0;
    slash[1] = 0;
    int written = snprintf(out, cap, "%s%s", exe, relative);
    return written > 0 && (size_t)written < cap;
}

static BOOL CALLBACK initialize_proxy(PINIT_ONCE once, PVOID parameter, PVOID *context) {
    (void)once; (void)parameter; (void)context;
    char path[MAX_PATH], absolute[MAX_PATH];
    g_sdl2_orig = GetModuleHandleA("SDL2_orig.dll");
    if (!g_sdl2_orig && path_from_exe(path, sizeof path, "SDL2_orig.dll"))
        g_sdl2_orig = LoadLibraryA(path);
    if (g_sdl2_orig)
        g_real_poll_event = (sdl_poll_event_fn)GetProcAddress(g_sdl2_orig, "SDL_PollEvent");
    if(g_sdl2_orig)g_real_swap=(sdl_swap_fn)GetProcAddress(g_sdl2_orig,"SDL_GL_SwapWindow");

    if (path_from_exe(path, sizeof path, "..\\..\\mods\\DDCoop\\ddcoop-runtime.dll")) {
        DWORD n = GetFullPathNameA(path, sizeof absolute, absolute, NULL);
        if (n > 0 && n < sizeof absolute) g_ddcoop = LoadLibraryA(absolute);
    }
    if (g_ddcoop)
        g_ddcoop_poll = (ddcoop_poll_fn)GetProcAddress(g_ddcoop, "ddcoop_poll");
    if(g_ddcoop) {
        g_ddcoop_draw=(ddcoop_draw_fn)GetProcAddress(g_ddcoop,"ddcoop_draw");
        g_ddcoop_event=(ddcoop_event_fn)GetProcAddress(g_ddcoop,"ddcoop_event");
    }
    if (!g_ddcoop_poll) OutputDebugStringA("DDCoop runtime payload was not loaded from mods\\DDCoop");
    return TRUE; /* The game must still work normally if the optional co-op payload is absent. */
}

int SDL_PollEvent(void *event) {
    InitOnceExecuteOnce(&g_init_once, initialize_proxy, NULL, NULL);
    if (g_ddcoop_poll) g_ddcoop_poll();
    int result;
    do {result=g_real_poll_event?g_real_poll_event(event):0;}
    while(result && g_ddcoop_event && g_ddcoop_event(event));
    return result;
}

void SDL_GL_SwapWindow(void *window) {
    InitOnceExecuteOnce(&g_init_once,initialize_proxy,NULL,NULL);
    if(g_ddcoop_draw)g_ddcoop_draw(window);
    if(g_real_swap)g_real_swap(window);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}
