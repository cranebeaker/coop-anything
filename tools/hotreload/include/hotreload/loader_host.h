// The permanent half of a hot-reload split, game-free: no engine headers. Use it through hotreload_loader.dll
// (loader_exports.h) or link the static library `hotreload_host` into your own plugin and use CoreHost directly.
//
// One CoreHost per process. It owns the private cache copy of the core DLL, the load, unload and reload sequence, the
// blob store that survives reloads, the control methods (dev.status, dev.reload and whatever the core registers) and
// the control pipe.
//
// Why a cache copy: Windows will not let a build overwrite a DLL that is loaded. The loader maps a private copy
// (<cache>\<core stem>.<pid>.<n>.dll) and leaves the original core file unlocked, so the build and the swap tool can
// replace it at any time.
//
// Build id: CRC-32 of the mapped copy and its size, "a1b2c3d4-1981440". tools/hotreload/hotreload.py computes the same
// text for the file it installs, so "the running core is the build I just copied" is a fact it can check.
//
// Threading: Start, Load, Reload, Service, OnFrame, OnMessage and Shutdown must be called on the main thread, between
// frames. RequestReload may be called from any thread.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hotreload/abi.h"
#include "hotreload/control_pipe.h"

namespace hotreload {

struct HostEnv {
    std::string corePath;   // the DLL the build writes
    std::string cacheDir;   // "" = <dir of corePath>\hr_cache
    std::string gameDir;    // handed to the core, may be ""
    std::string pipeName;   // "" = no control pipe
    std::string logPath;    // "" = <cacheDir>\hotreload.log
    void* userContext = nullptr;
};

struct ReloadResult {
    bool ok = false;
    bool rolledBack = false;   // the new core failed and the previous one is running again
    bool coreDead = false;     // nothing is running (rollback failed too)
    std::string error;
    uint32_t generation = 0;   // generation now running (0 if dead)
    std::string buildId;       // build id of the core now running; "" if dead
    double msUnload = 0, msLoad = 0;
};

class CoreHost {
public:
    explicit CoreHost(HostEnv env);
    ~CoreHost();
    CoreHost(const CoreHost&) = delete;
    CoreHost& operator=(const CoreHost&) = delete;

    // Registers dev.status and dev.reload, starts the control pipe (if HostEnv::pipeName is set) and loads the core.
    // False only if the pipe could not be created. A core that fails to load is not an error here (see status()).
    bool Start(std::string& err);
    // First load (generation 1). Safe to call again after a failure.
    bool Load(std::string& err);
    ReloadResult Reload(const char* reason);
    // Frame boundary: runs queued control requests, then a pending reload request, then the core's on_frame.
    void OnFrame();
    void OnMessage(uint32_t type, void* data, uint32_t len);
    // Runs a pending reload request (from RequestReload or the core). True if a reload ran.
    bool Service();
    // Orderly unload without a replacement (tests). Same quiesce sequence as Reload.
    bool Unload(std::string& err);
    // The process is exiting: stops the control pipe. Does not unload the core (the OS does).
    void Shutdown();

    void RequestReload(const char* reason);

    // A control method that belongs to the host. Runs on the main thread, survives reloads.
    void RegisterHostMethod(const std::string& name, HrMethodFn fn, void* user);

    bool loaded() const { return loaded_; }
    uint32_t generation() const { return generation_; }
    uint32_t reloadCount() const { return reloadCount_; }
    const std::string& coreBuildId() const { return cur_.buildId; }
    const std::string& lastError() const { return startError_; }
    std::string StatusJson() const;

    // ---- used by the api callbacks ----
    struct Blob {
        std::vector<uint8_t> data;
        uint32_t version = 0;
    };
    std::mutex blobMu;
    std::map<std::string, Blob> blobs;
    std::atomic<bool> reloadPending{false};
    std::string pendingReason;
    std::mutex pendingMu;
    std::string vetoReason;
    void Log(const char* line);
    void RegisterMethod(const std::string& name, std::function<bool(const std::string&, std::string&)> fn, int owner);
    HrLoaderApi api{};

private:
    struct LoadedCore {
        void* module = nullptr;   // HMODULE
        std::string cachePath;
        std::string buildId;      // CRC-32 and size of the cache copy that was mapped
        HrCoreApi core{};
    };
    bool LoadCoreFrom(const std::string& sourcePath, bool isReload, uint32_t generation, LoadedCore& out, std::string& err);
    void UnloadCore(LoadedCore& c);
    void CleanStaleCache();
    std::string MakeCachePath(uint32_t seq) const;
    void FillApi(uint32_t generation, bool isReload);
    void RegisterDevMethods();
    bool Dispatch(const std::string& method, const std::string& params, std::string& out, int& code);
    void UnregisterOwner(int owner);

    struct MethodEntry {
        std::function<bool(const std::string&, std::string&)> fn;
        int owner = 0;
    };

    HostEnv env_;
    LoadedCore cur_;
    std::string cachePath_;
    std::string coreDir_;
    std::string startError_;
    std::unique_ptr<ControlServer> control_;
    std::map<std::string, MethodEntry> methods_;
    bool loaded_ = false;
    bool devRegistered_ = false;
    uint32_t generation_ = 0;
    uint32_t reloadCount_ = 0;
    int depth_ = 0;   // > 0 while inside a core call
    ReloadResult last_;
    std::string lastReason_;
    std::mutex logMu_;
};

CoreHost* Current();

}  // namespace hotreload
