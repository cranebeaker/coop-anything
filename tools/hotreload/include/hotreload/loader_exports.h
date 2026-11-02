/* The C API of hotreload_loader.dll, the permanent loader. Your plugin entry point (or a small host) loads the DLL,
 * calls HotReload_Start once, then HotReload_OnFrame once per frame on the main thread.
 *
 * If you would rather not ship a second DLL, link the static library `hotreload_host` into your own plugin and use
 * hotreload::CoreHost (loader_host.h) directly. The DLL is a thin wrapper around it. */
#pragma once
#include <stdint.h>

#include "hotreload/abi.h"

/* The loader DLL defines HOTRELOAD_EXPORTS when it is built. A host that loads the DLL with GetProcAddress does not
 * need these declarations at all. */
#ifdef HOTRELOAD_EXPORTS
#define HR_API __declspec(dllexport)
#else
#define HR_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HotReloadConfig {
    uint32_t size;               /* sizeof(HotReloadConfig) */
    const char* core_path;       /* the core DLL your build writes. The loader never maps this file itself. */
    const char* cache_dir;       /* where the private copies go. NULL or "" = "hr_cache" beside core_path. */
    const char* game_dir;        /* handed to the core as api->game_dir. May be NULL. */
    const char* pipe_name;       /* control pipe, short name ("mygame-1") or a full \\.\pipe\ path. NULL = no control pipe. */
    const char* log_path;        /* NULL or "" = hotreload.log inside cache_dir */
    void* user_context;          /* handed to the core as api->user_context. May be NULL. */
} HotReloadConfig;

/* Starts the control pipe, registers dev.status and dev.reload, and loads the core. Returns 1 when the loader is up.
 * A core that fails to load does not make Start fail: the loader keeps running, dev.status says why, and a later
 * dev.reload (after you fix the build) loads it. Returns 0 if the control pipe could not be created or Start was
 * already called; HotReload_LastError() then says why. */
HR_API int HotReload_Start(const HotReloadConfig* config);
/* Main thread, once per frame: serves control requests, performs a pending reload, then calls the core's on_frame. */
HR_API void HotReload_OnFrame(void);
HR_API void HotReload_OnMessage(uint32_t type, void* data, uint32_t len);
/* Register a control method that belongs to the host, not to the core. It survives reloads. Runs on the main thread. */
HR_API void HotReload_RegisterMethod(const char* name, HrMethodFn fn, void* user);
HR_API const char* HotReload_LastError(void);
/* Stops the control pipe. Does not unload the core (the process is exiting). */
HR_API void HotReload_Shutdown(void);

#ifdef __cplusplus
}
#endif
