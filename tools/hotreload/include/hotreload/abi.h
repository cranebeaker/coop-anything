/* Stable C ABI between the permanent loader and the reloadable core DLL.
 *
 * The loader stays in the game for the whole session. The core is copied to a private cache file, mapped from there,
 * and can be unloaded and replaced while the game runs.
 *
 * Rules:
 *  - Plain C types only across this boundary: no std::string, no exceptions, no CRT objects. The loader and the core
 *    may be built with different runtimes.
 *  - Both structs start with {abi_version, size}. New fields are only ever appended. Check `size` before you use a
 *    field that was added later.
 *  - Every core entry point runs on the thread that calls HotReload_OnFrame (your game's main loop), between frames,
 *    never from inside another core call.
 *  - Every pointer the core receives stays valid until the core returns from before_unload and is unloaded.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HR_ABI_VERSION 1u

/* Output sink for control methods: call out->set(out->h, text) once. The text is copied. On success it must be JSON
 * (a number, a string in quotes, an object ...). On failure it is a plain message. */
typedef struct HrOut {
    void* h;
    void (*set)(void* h, const char* text);
} HrOut;

/* A control method implemented by the core. Return 1 for ok, 0 for failure (message in out). `user` is the value
 * passed at registration. Runs on the main thread between frames. */
typedef int (*HrMethodFn)(void* user, const char* params_json, HrOut* out);

/* loader -> core */
typedef struct HrLoaderApi {
    uint32_t abi_version;        /* HR_ABI_VERSION */
    uint32_t size;              /* sizeof(HrLoaderApi) as the loader built it */
    uint32_t generation;        /* 1 for the first load, +1 per successful reload */
    uint32_t is_reload;         /* 0 = first load in this process, 1 = hot reload (a state blob may exist) */
    const char* game_dir;       /* the folder the host passed to HotReload_Start, may be "" */
    const char* core_dir;       /* folder of the ORIGINAL core file, with trailing backslash. The core runs from a cache
                                   copy, so files that sit beside the build output are found here, not beside the module. */
    void* user_context;         /* the pointer the host passed to HotReload_Start (an engine interface, a table ...) */

    void (*log)(const char* line);
    /* Register a control method (see HrMethodFn). The loader removes it again before the core is unloaded, so the core
     * does not need to unregister. */
    void (*register_method)(const char* name, HrMethodFn fn, void* user);

    /* Blob store owned by the loader. It survives reloads (not process restarts). `version` is opaque to the loader.
     * blob_get copies min(cap, len) bytes, always sets *len to the full length, and returns 1 if the key exists
     * (len and version may be NULL), else 0. */
    int (*blob_put)(const char* key, const void* data, uint32_t len, uint32_t version);
    int (*blob_get)(const char* key, void* buf, uint32_t cap, uint32_t* len, uint32_t* version);
    void (*blob_erase)(const char* key);

    /* Ask for a reload. The loader does it at the next frame boundary. Safe to call from any thread. */
    void (*request_reload)(const char* reason);
    /* Call from before_unload before returning 0, so the reload report says why the unload was refused. */
    void (*veto_reason)(const char* why);
    /* Appended fields go below this line. */
} HrLoaderApi;

/* core -> loader */
typedef struct HrCoreApi {
    uint32_t abi_version;
    uint32_t size;
    /* Called once after load, on the main thread. Return 1 for ok. On a hot reload (api->is_reload) restore the saved
     * state here. Returning 0 makes the loader unload this core again and, on a reload, roll back to the previous one. */
    int (*init)(void);
    /* A message from your game or script extender, passed through unchanged. Optional. */
    void (*on_message)(uint32_t type, void* data, uint32_t len);
    /* Once per frame. Optional. */
    void (*on_frame)(void);
    /* Called before the unload, main thread, between frames. The core must stop its threads, close or hand over
     * sockets, remove every hook it installed, and write its state with api->blob_put. Return 1 when it is ready to be
     * unloaded. Return 0 to refuse: the loader keeps the core loaded, and the core must still work fully. */
    int (*before_unload)(void);
    /* Appended fields go below this line. */
} HrCoreApi;

/* The one export every core DLL has (undecorated C name):
 *   int HotCore_Init(const HrLoaderApi* api, HrCoreApi* out);   // fill `out`, return 1 on success
 * The loader checks api->abi_version == HR_ABI_VERSION and that `out` is complete up to before_unload. */
typedef int (*HrCoreInitFn)(const HrLoaderApi* api, HrCoreApi* out);
#define HR_CORE_INIT_EXPORT "HotCore_Init"

#ifdef __cplusplus
}
#endif
