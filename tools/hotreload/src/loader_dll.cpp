// hotreload_loader.dll: the permanent loader as a DLL with a plain C API (see loader_exports.h).
#define HOTRELOAD_EXPORTS
#include "hotreload/loader_exports.h"

#include <memory>
#include <string>

#include "hotreload/loader_host.h"



namespace {
std::unique_ptr<hotreload::CoreHost> g_host;
std::string g_error;
std::string S(const char* p) { return p ? p : ""; }
}  // namespace

int HotReload_Start(const HotReloadConfig* c) {
    if (g_host) {
        g_error = "HotReload_Start was already called";
        return 0;
    }
    if (!c || c->size < sizeof(HotReloadConfig) || !c->core_path || !*c->core_path) {
        g_error = "bad HotReloadConfig (size or core_path)";
        return 0;
    }
    hotreload::HostEnv env;
    env.corePath = c->core_path;
    env.cacheDir = S(c->cache_dir);
    env.gameDir = S(c->game_dir);
    env.pipeName = S(c->pipe_name);
    env.logPath = S(c->log_path);
    env.userContext = c->user_context;
    g_host.reset(new hotreload::CoreHost(env));
    std::string err;
    if (!g_host->Start(err)) {
        g_error = err;
        g_host.reset();
        return 0;
    }
    g_error.clear();
    return 1;
}

void HotReload_OnFrame(void) {
    if (g_host) g_host->OnFrame();
}

void HotReload_OnMessage(uint32_t type, void* data, uint32_t len) {
    if (g_host) g_host->OnMessage(type, data, len);
}

void HotReload_RegisterMethod(const char* name, HrMethodFn fn, void* user) {
    if (g_host && name) g_host->RegisterHostMethod(name, fn, user);
}

const char* HotReload_LastError(void) { return g_error.c_str(); }

void HotReload_Shutdown(void) {
    if (g_host) g_host->Shutdown();
}
