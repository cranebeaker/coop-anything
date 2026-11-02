// Test host: a program that stands in for the game. It is how the swap tool is tested without any game.
//
//   hotreload_test_host --loader <hotreload_loader.dll> --core <core.dll> --pipe <name> [--cache DIR] [--frame-ms N]
//                       [--mode split|monolith|oldloader]
//
// split (default)  What a mod with a loader looks like: it loads the permanent loader, which maps the core from a cache
//                  copy. The main loop calls HotReload_OnFrame once per frame, and every 10th frame also passes a message.
// monolith         A build that has no loader: the control pipe answers `ping` and host.* only (no dev.status, no dev.reload).
// oldloader        A loader from before build ids: dev.status exists but has no buildId.
//
// Control methods of the host itself (all modes):
//   host.info   {"pid":..., "frames":..., "mode":"..."}   proves the process kept running across a swap
//   host.quit   ends the main loop and exits with code 0
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "hotreload/control_pipe.h"
#include "hotreload/loader_exports.h"

namespace {

std::atomic<bool> g_quit{false};
std::atomic<unsigned long long> g_frames{0};
std::string g_mode = "split";

int hostInfo(void*, const char*, HrOut* out) {
    char b[160];
    std::snprintf(b, sizeof b, "{\"pid\":%lu,\"frames\":%llu,\"mode\":\"%s\"}", GetCurrentProcessId(), g_frames.load(), g_mode.c_str());
    out->set(out->h, b);
    return 1;
}
int hostQuit(void*, const char*, HrOut* out) {
    g_quit = true;
    out->set(out->h, "true");
    return 1;
}

typedef int (*StartFn)(const HotReloadConfig*);
typedef void (*FrameFn)(void);
typedef void (*MessageFn)(uint32_t, void*, uint32_t);
typedef void (*RegisterFn)(const char*, HrMethodFn, void*);
typedef const char* (*ErrorFn)(void);

int runSplit(const std::string& loader, const std::string& core, const std::string& cache, const std::string& pipe, int frameMs) {
    HMODULE dll = LoadLibraryExA(loader.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) {
        std::fprintf(stderr, "cannot load %s (error %lu)\n", loader.c_str(), GetLastError());
        return 3;
    }
    auto start = reinterpret_cast<StartFn>(GetProcAddress(dll, "HotReload_Start"));
    auto frame = reinterpret_cast<FrameFn>(GetProcAddress(dll, "HotReload_OnFrame"));
    auto message = reinterpret_cast<MessageFn>(GetProcAddress(dll, "HotReload_OnMessage"));
    auto reg = reinterpret_cast<RegisterFn>(GetProcAddress(dll, "HotReload_RegisterMethod"));
    auto lastError = reinterpret_cast<ErrorFn>(GetProcAddress(dll, "HotReload_LastError"));
    auto shutdown = reinterpret_cast<FrameFn>(GetProcAddress(dll, "HotReload_Shutdown"));
    if (!start || !frame || !message || !reg || !lastError || !shutdown) {
        std::fprintf(stderr, "the loader DLL does not export the HotReload_* functions\n");
        return 3;
    }
    HotReloadConfig cfg{};
    cfg.size = sizeof cfg;
    cfg.core_path = core.c_str();
    cfg.cache_dir = cache.empty() ? nullptr : cache.c_str();
    cfg.pipe_name = pipe.c_str();
    if (!start(&cfg)) {
        std::fprintf(stderr, "HotReload_Start failed: %s\n", lastError());
        return 3;
    }
    reg("host.info", &hostInfo, nullptr);
    reg("host.quit", &hostQuit, nullptr);
    while (!g_quit) {
        frame();
        const unsigned long long n = ++g_frames;
        if (n % 10 == 0) {
            uint32_t v = static_cast<uint32_t>(n);
            message(7, &v, sizeof v);
        }
        Sleep(frameMs);
    }
    shutdown();
    return 0;
}

int runLoaderless(const std::string& mode, const std::string& core, const std::string& pipe, int frameMs) {
    hotreload::ControlServer server(pipe, [&](const std::string& method, const std::string&, std::string& out, int& code) -> bool {
        if (method == "host.info") {
            char b[160];
            std::snprintf(b, sizeof b, "{\"pid\":%lu,\"frames\":%llu,\"mode\":\"%s\"}", GetCurrentProcessId(), g_frames.load(), g_mode.c_str());
            out = b;
            return true;
        }
        if (method == "host.quit") {
            g_quit = true;
            out = "true";
            return true;
        }
        if (mode == "oldloader" && method == "dev.status") {
            out = "{\"loaded\":true,\"generation\":1,\"corePath\":\"" + hotreload::JsonEscape(core) + "\",\"hotReload\":true}";
            return true;
        }
        if (mode == "oldloader" && method == "dev.reload") {
            out = "{}";
            return true;
        }
        code = -32601;
        out = "unknown method: " + method;
        return false;
    });
    std::string err;
    if (!server.Start(err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 3;
    }
    while (!g_quit) {
        server.Pump();
        ++g_frames;
        Sleep(frameMs);
    }
    server.Stop();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string loader, core, cache, pipe;
    int frameMs = 5;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string k = argv[i], v = argv[i + 1];
        if (k == "--loader") loader = v;
        else if (k == "--core") core = v;
        else if (k == "--cache") cache = v;
        else if (k == "--pipe") pipe = v;
        else if (k == "--mode") g_mode = v;
        else if (k == "--frame-ms") frameMs = std::atoi(v.c_str());
        else {
            std::fprintf(stderr, "unknown option %s\n", k.c_str());
            return 2;
        }
    }
    if (pipe.empty() || core.empty() || (g_mode == "split" && loader.empty())) {
        std::fprintf(stderr, "usage: hotreload_test_host --loader DLL --core DLL --pipe NAME [--cache DIR] [--frame-ms N] [--mode split|monolith|oldloader]\n");
        return 2;
    }
    if (g_mode == "split") return runSplit(loader, core, cache, pipe, frameMs);
    if (g_mode == "monolith" || g_mode == "oldloader") return runLoaderless(g_mode, core, pipe, frameMs);
    std::fprintf(stderr, "unknown mode %s\n", g_mode.c_str());
    return 2;
}
