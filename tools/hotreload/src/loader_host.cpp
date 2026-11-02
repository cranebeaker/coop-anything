#include "hotreload/loader_host.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "crc32.h"
#include "mut.h"

namespace hotreload {
namespace {

constexpr int kOwnerCore = 1;   // owner tag of methods registered by the core: removed before every unload
constexpr int kOwnerDev = 2;    // dev.* methods of the loader itself
constexpr int kOwnerHost = 3;   // methods the host program registered

CoreHost* g_current = nullptr;
uint64_t g_seq = 0;   // cache file sequence, unique per process

using Clock = std::chrono::steady_clock;
double MsSince(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }

// Build id of a file: CRC-32 of its bytes (hex, 8 digits), a dash, and the size. "" = unreadable.
std::string BuildIdOfFile(const std::string& path) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::string();
    static unsigned char buf[1 << 16];   // main thread only
    uint32_t crc = 0xFFFFFFFFu;
    unsigned long long total = 0;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(h, buf, sizeof buf, &got, nullptr)) {
            CloseHandle(h);
            return std::string();
        }
        if (got == 0) break;
        crc = Crc32Update(crc, buf, got);
        total += got;
    }
    CloseHandle(h);
    char b[48];
    std::snprintf(b, sizeof b, "%08x-%llu", crc ^ 0xFFFFFFFFu, total);
    return b;
}

std::string DirOf(const std::string& path) {
    size_t p = path.find_last_of("\\/");
    return p == std::string::npos ? std::string() : path.substr(0, p + 1);
}
std::string StemOf(const std::string& path) {
    size_t p = path.find_last_of("\\/");
    std::string f = p == std::string::npos ? path : path.substr(p + 1);
    size_t d = f.find_last_of('.');
    return d == std::string::npos ? f : f.substr(0, d);
}
std::string FullPath(const std::string& p) {
    char b[1024];
    DWORD n = GetFullPathNameA(p.c_str(), sizeof b, b, nullptr);
    return (n > 0 && n < sizeof b) ? std::string(b, n) : p;
}
void WithSlash(std::string& dir) {
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '\\';
}

void SetStr(void* h, const char* text) { *static_cast<std::string*>(h) = text ? text : ""; }

// ---- loader -> core api callbacks (stateless C functions; they reach the host through Current()) ----
void ApiLog(const char* line) {
    if (CoreHost* h = Current()) h->Log(line ? line : "");
}
void ApiRegisterMethod(const char* name, HrMethodFn fn, void* user) {
    CoreHost* h = Current();
    if (!h || !name || !fn) return;
    h->RegisterMethod(name,
                      [fn, user](const std::string& params, std::string& out) -> bool {
                          std::string res;
                          HrOut o{&res, &SetStr};
                          int r = 0;
                          try {
                              r = fn(user, params.c_str(), &o);
                          } catch (...) {
                              res = "exception in core method";
                              r = 0;
                          }
                          out = res;
                          return r != 0;
                      },
                      kOwnerCore);
}
int ApiBlobPut(const char* key, const void* data, uint32_t len, uint32_t version) {
    CoreHost* h = Current();
    if (!h || !key) return 0;
    std::lock_guard<std::mutex> g(h->blobMu);
    auto& b = h->blobs[key];
    b.data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + (data ? len : 0));
    b.version = version;
    return 1;
}
int ApiBlobGet(const char* key, void* buf, uint32_t cap, uint32_t* len, uint32_t* version) {
    CoreHost* h = Current();
    if (!h || !key) return 0;
    std::lock_guard<std::mutex> g(h->blobMu);
    auto it = h->blobs.find(key);
    if (it == h->blobs.end()) return 0;
    if (len) *len = static_cast<uint32_t>(it->second.data.size());
    if (version) *version = it->second.version;
    if (buf && cap) std::memcpy(buf, it->second.data.data(), (std::min<size_t>)(cap, it->second.data.size()));
    return 1;
}
void ApiBlobErase(const char* key) {
    CoreHost* h = Current();
    if (!h || !key) return;
    std::lock_guard<std::mutex> g(h->blobMu);
    h->blobs.erase(key);
}
void ApiRequestReload(const char* reason) {
    if (CoreHost* h = Current()) h->RequestReload(reason);
}
void ApiVetoReason(const char* why) {
    if (CoreHost* h = Current()) h->vetoReason = why ? why : "";
}

}  // namespace

CoreHost* Current() { return g_current; }

CoreHost::CoreHost(HostEnv env) : env_(std::move(env)) {
    env_.corePath = FullPath(env_.corePath);
    coreDir_ = DirOf(env_.corePath);
    if (env_.cacheDir.empty()) env_.cacheDir = coreDir_ + "hr_cache\\";
    env_.cacheDir = FullPath(env_.cacheDir);
    WithSlash(env_.cacheDir);
    if (env_.logPath.empty()) env_.logPath = env_.cacheDir + "hotreload.log";
    CreateDirectoryA(env_.cacheDir.c_str(), nullptr);
    g_current = this;
    CleanStaleCache();
}

CoreHost::~CoreHost() {
    Shutdown();
    if (g_current == this) g_current = nullptr;
}

void CoreHost::Log(const char* line) {
    std::lock_guard<std::mutex> g(logMu_);
    FILE* f = std::fopen(env_.logPath.c_str(), "ab");
    if (!f) return;
    std::fprintf(f, "[%llu] %s\n", static_cast<unsigned long long>(GetTickCount64()), line);
    std::fclose(f);
}

void CoreHost::CleanStaleCache() {
    // Cache files of dead processes. DeleteFile fails for files still mapped by a live process: that is the filter.
    std::string pattern = env_.cacheDir + StemOf(env_.corePath) + ".*.dll";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        DeleteFileA((env_.cacheDir + fd.cFileName).c_str());
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

std::string CoreHost::MakeCachePath(uint32_t seq) const {
    char b[64];
    std::snprintf(b, sizeof b, ".%lu.%u.dll", GetCurrentProcessId(), seq);
    return env_.cacheDir + StemOf(env_.corePath) + b;
}

void CoreHost::FillApi(uint32_t generation, bool isReload) {
    api = HrLoaderApi{};
    api.abi_version = HR_ABI_VERSION;
    api.size = sizeof(HrLoaderApi);
    api.generation = generation;
    api.is_reload = isReload ? 1 : 0;
    api.game_dir = env_.gameDir.c_str();
    api.core_dir = coreDir_.c_str();
    api.user_context = env_.userContext;
    api.log = &ApiLog;
    api.register_method = &ApiRegisterMethod;
    api.blob_put = &ApiBlobPut;
    api.blob_get = &ApiBlobGet;
    api.blob_erase = &ApiBlobErase;
    api.request_reload = &ApiRequestReload;
    api.veto_reason = &ApiVetoReason;
}

void CoreHost::RegisterMethod(const std::string& name, std::function<bool(const std::string&, std::string&)> fn, int owner) {
    methods_[name] = MethodEntry{std::move(fn), owner};
}

void CoreHost::RegisterHostMethod(const std::string& name, HrMethodFn fn, void* user) {
    if (!fn) return;
    RegisterMethod(name,
                   [fn, user](const std::string& params, std::string& out) -> bool {
                       std::string res;
                       HrOut o{&res, &SetStr};
                       int r = fn(user, params.c_str(), &o);
                       out = res;
                       return r != 0;
                   },
                   kOwnerHost);
}

void CoreHost::UnregisterOwner(int owner) {
    for (auto it = methods_.begin(); it != methods_.end();) {
        if (it->second.owner == owner) it = methods_.erase(it);
        else ++it;
    }
}

bool CoreHost::Dispatch(const std::string& method, const std::string& params, std::string& out, int& code) {
    auto it = methods_.find(method);
    if (it == methods_.end()) {
        code = -32601;
        out = "unknown method: " + method;
        return false;
    }
    auto fn = it->second.fn;   // a copy: the entry may be removed while it runs (a reload purges the core's methods)
    const bool inCore = it->second.owner == kOwnerCore;
    if (inCore) ++depth_;
    bool ok = false;
    try {
        ok = fn(params, out);
    } catch (...) {
        ok = false;
        out = "exception in method";
    }
    if (inCore) --depth_;
    return ok;
}

bool CoreHost::LoadCoreFrom(const std::string& sourcePath, bool isReload, uint32_t generation, LoadedCore& out, std::string& err) {
    CreateDirectoryA(env_.cacheDir.c_str(), nullptr);
    std::string cache = sourcePath;
    if (!HR_MUT(4)) {
        cache = MakeCachePath(static_cast<uint32_t>(++g_seq));
        // The linker may still be finishing the file: retry the copy for up to about 2 s.
        bool copied = false;
        for (int i = 0; i < 20 && !copied; ++i) {
            copied = CopyFileA(sourcePath.c_str(), cache.c_str(), FALSE) != 0;
            if (!copied) Sleep(100);
        }
        if (!copied) {
            err = "cannot copy core '" + sourcePath + "' to the cache (error " + std::to_string(GetLastError()) + ")";
            return false;
        }
        SetFileAttributesA(cache.c_str(), FILE_ATTRIBUTE_NORMAL);   // a copy of a read-only file is read-only
    }
    HMODULE m = LoadLibraryExA(cache.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m) {
        err = "LoadLibrary failed for the core (error " + std::to_string(GetLastError()) + ")";
        if (!HR_MUT(4)) DeleteFileA(cache.c_str());
        return false;
    }
    auto fail = [&](const std::string& why) {
        err = why;
        FreeLibrary(m);
        if (!HR_MUT(4)) DeleteFileA(cache.c_str());
        return false;
    };
    auto init = reinterpret_cast<HrCoreInitFn>(GetProcAddress(m, HR_CORE_INIT_EXPORT));
    if (!init) return fail("the core has no export " HR_CORE_INIT_EXPORT);

    FillApi(generation, isReload);
    HrCoreApi core{};
    core.size = sizeof(HrCoreApi);
    int ok = 0;
    try {
        ok = init(&api, &core);
    } catch (...) {
        ok = 0;
    }
    if (!ok) {
        UnregisterOwner(kOwnerCore);
        return fail(HR_CORE_INIT_EXPORT " returned failure");
    }
    if (core.abi_version != HR_ABI_VERSION) {
        UnregisterOwner(kOwnerCore);
        return fail("core ABI version " + std::to_string(core.abi_version) + " != loader " + std::to_string(HR_ABI_VERSION));
    }
    if (core.size < offsetof(HrCoreApi, before_unload) + sizeof(void*) || !core.init || !core.before_unload) {
        UnregisterOwner(kOwnerCore);
        return fail("the core's HrCoreApi is incomplete");
    }
    out.module = m;
    out.cachePath = cache;
    out.buildId = BuildIdOfFile(cache);   // the exact bytes that were mapped (the cache copy, not the file the build may overwrite)
    out.core = core;
    // core.init() runs the module's own init and the state restore step. Calls into the core count as "inside core".
    int iok = 0;
    ++depth_;
    try {
        iok = core.init();
    } catch (...) {
        iok = 0;
    }
    --depth_;
    if (!iok) {
        ++depth_;
        try {
            core.before_unload();   // let a half-initialised core stop what it started
        } catch (...) {
        }
        --depth_;
        UnregisterOwner(kOwnerCore);
        return fail("the core's init() returned failure");
    }
    return true;
}

void CoreHost::UnloadCore(LoadedCore& c) {
    if (c.module) FreeLibrary(static_cast<HMODULE>(c.module));
    c.module = nullptr;
}

bool CoreHost::Start(std::string& err) {
    RegisterDevMethods();
    if (!env_.pipeName.empty()) {
        control_.reset(new ControlServer(env_.pipeName, [this](const std::string& m, const std::string& p, std::string& o, int& c) { return Dispatch(m, p, o, c); }));
        if (!control_->Start(err)) {
            startError_ = err;
            Log(err.c_str());
            control_.reset();
            return false;
        }
    }
    std::string e;
    Load(e);   // a failed load is reported by dev.status and in the log
    return true;
}

bool CoreHost::Load(std::string& err) {
    if (loaded_) return true;
    RegisterDevMethods();
    LoadedCore n;
    uint32_t gen = generation_ ? generation_ : 1;
    if (!LoadCoreFrom(env_.corePath, generation_ != 0, gen, n, err)) {
        Log(("loader: core load failed: " + err).c_str());
        last_ = ReloadResult{};
        last_.error = err;
        return false;
    }
    cur_ = n;
    cachePath_ = n.cachePath;
    generation_ = gen;
    loaded_ = true;
    last_ = ReloadResult{};
    last_.ok = true;
    last_.generation = gen;
    last_.buildId = n.buildId;
    Log(("loader: core generation " + std::to_string(generation_) + " loaded from " + cachePath_ + ", build " + cur_.buildId).c_str());
    return true;
}

ReloadResult CoreHost::Reload(const char* reason) {
    ReloadResult res;
    lastReason_ = reason ? reason : "";
    auto t0 = Clock::now();
    auto finish = [&](ReloadResult r) {
        last_ = r;
        Log(("loader: reload " + std::string(r.ok ? "ok" : "FAILED") + ", generation " + std::to_string(r.generation) + (r.error.empty() ? "" : ", " + r.error)).c_str());
        return r;
    };
    if (depth_ > 0) {
        res.error = "refused: reload requested from inside a core call (only allowed between frames)";
        res.generation = generation_;
        return finish(res);
    }
    if (!loaded_) {   // the first load failed earlier, or the core died: try again from the file
        std::string e;
        res.ok = Load(e);
        res.error = e;
        res.generation = generation_;
        res.buildId = cur_.buildId;
        res.msLoad = MsSince(t0);
        return finish(res);
    }

    // 1) the core quiesces: state saved, drain passed, threads stopped, hooks removed
    vetoReason.clear();
    int ready = 0;
    ++depth_;
    try {
        ready = cur_.core.before_unload();
    } catch (...) {
        ready = 0;
    }
    --depth_;
    if (HR_MUT(10)) ready = 1;
    if (!ready) {
        res.error = "the core refused the unload: " + (vetoReason.empty() ? std::string("no reason given, see ") + env_.logPath : vetoReason) + ". The old core stays loaded";
        res.generation = generation_;
        res.buildId = cur_.buildId;
        return finish(res);
    }
    res.msUnload = MsSince(t0);

    // 2) unload
    UnregisterOwner(kOwnerCore);
    std::map<std::string, Blob> blobSnapshot;
    {
        std::lock_guard<std::mutex> g(blobMu);
        blobSnapshot = blobs;
    }
    LoadedCore prev = cur_;
    UnloadCore(cur_);
    loaded_ = false;
    if (prev.cachePath != env_.corePath && GetModuleHandleA(prev.cachePath.c_str()) != nullptr) {
        // something else still holds the module (a leaked thread or hook): the old code is still mapped
        res.error = "the core DLL is still mapped after FreeLibrary (a leaked thread or hook?)";
        res.coreDead = true;
        cur_.buildId.clear();
        res.generation = 0;
        return finish(res);
    }

    // 3) load the new build
    auto t1 = Clock::now();
    LoadedCore n;
    std::string err;
    if (LoadCoreFrom(env_.corePath, true, generation_ + 1, n, err)) {
        if (HR_MUT(5)) n.buildId = prev.buildId;
        cur_ = n;
        cachePath_ = n.cachePath;
        ++generation_;
        ++reloadCount_;
        loaded_ = true;
        if (prev.cachePath != n.cachePath) DeleteFileA(prev.cachePath.c_str());
        res.ok = true;
        res.generation = generation_;
        res.buildId = cur_.buildId;
        res.msLoad = MsSince(t1);
        Log(("loader: reload ok, build " + cur_.buildId + " (" + lastReason_ + ")").c_str());
        return finish(res);
    }
    Log(("loader: reload failed: " + err).c_str());
    res.error = err;
    if (!HR_MUT(9)) {
        {
            std::lock_guard<std::mutex> g(blobMu);
            blobs = blobSnapshot;
        }
        LoadedCore back;
        std::string err2;
        if (LoadCoreFrom(prev.cachePath, true, generation_, back, err2)) {
            cur_ = back;
            cachePath_ = back.cachePath;
            loaded_ = true;
            if (prev.cachePath != back.cachePath) DeleteFileA(prev.cachePath.c_str());
            res.rolledBack = true;
            res.generation = generation_;
            res.buildId = cur_.buildId;
            res.msLoad = MsSince(t1);
            return finish(res);
        }
        res.error += "; rollback also failed: " + err2;
    }
    res.coreDead = true;
    res.generation = 0;
    cur_.buildId.clear();
    return finish(res);
}

bool CoreHost::Unload(std::string& err) {
    if (!loaded_) return true;
    if (depth_ > 0) {
        err = "refused: inside a core call";
        return false;
    }
    vetoReason.clear();
    int ready = 0;
    ++depth_;
    try {
        ready = cur_.core.before_unload();
    } catch (...) {
        ready = 0;
    }
    --depth_;
    if (!ready) {
        err = "the core refused the unload: " + vetoReason;
        return false;
    }
    UnregisterOwner(kOwnerCore);
    std::string path = cur_.cachePath;
    UnloadCore(cur_);
    loaded_ = false;
    if (path != env_.corePath) DeleteFileA(path.c_str());
    return true;
}

bool CoreHost::Service() {
    if (!reloadPending.exchange(false)) return false;
    std::string why;
    {
        std::lock_guard<std::mutex> g(pendingMu);
        why = pendingReason;
    }
    Reload(why.c_str());
    return true;
}

void CoreHost::OnFrame() {
    if (control_) control_->Pump();
    Service();
    if (!loaded_ || !cur_.core.on_frame) return;
    ++depth_;
    try {
        cur_.core.on_frame();
    } catch (...) {
        Log("loader: exception from core on_frame");
    }
    --depth_;
}

void CoreHost::OnMessage(uint32_t type, void* data, uint32_t len) {
    if (!loaded_ || !cur_.core.on_message) return;
    ++depth_;
    try {
        cur_.core.on_message(type, data, len);
    } catch (...) {
        Log("loader: exception from core on_message");
    }
    --depth_;
}

void CoreHost::Shutdown() {
    if (control_) {
        control_->Stop();
        control_.reset();
    }
}

void CoreHost::RequestReload(const char* reason) {
    {
        std::lock_guard<std::mutex> g(pendingMu);
        pendingReason = reason ? reason : "requested";
    }
    reloadPending = true;
}

std::string CoreHost::StatusJson() const {
    std::string s = "{\"loaded\":" + std::string(loaded_ ? "true" : "false") + ",\"generation\":" + std::to_string(generation_) +
                    ",\"reloadCount\":" + std::to_string(reloadCount_) + ",\"corePath\":\"" + JsonEscape(env_.corePath) + "\",\"cachePath\":\"" +
                    JsonEscape(cachePath_) + "\",\"buildId\":\"" + JsonEscape(cur_.buildId) + "\",\"hotReload\":true,\"abi\":" + std::to_string(HR_ABI_VERSION) +
                    ",\"pending\":" + (reloadPending.load() ? "true" : "false") + ",\"last\":{\"ok\":" + (last_.ok ? "true" : "false") +
                    ",\"rolledBack\":" + (last_.rolledBack ? "true" : "false") + ",\"coreDead\":" + (last_.coreDead ? "true" : "false") + ",\"error\":\"" +
                    JsonEscape(last_.error) + "\",\"msUnload\":" + std::to_string(last_.msUnload) + ",\"msLoad\":" + std::to_string(last_.msLoad) +
                    ",\"reason\":\"" + JsonEscape(lastReason_) + "\"}}";
    return s;
}

void CoreHost::RegisterDevMethods() {
    if (devRegistered_) return;
    devRegistered_ = true;
    RegisterMethod("dev.reload",
                   [this](const std::string& params, std::string& out) -> bool {
                       std::string reason = "bridge";
                       JsonGetString(params, "reason", reason);
                       ReloadResult r = Reload(reason.c_str());
                       if (!r.ok) {
                           out = r.error + (r.rolledBack ? " (rolled back to the previous core)" : "");
                           return false;
                       }
                       out = "{\"generation\":" + std::to_string(r.generation) + ",\"buildId\":\"" + JsonEscape(r.buildId) + "\",\"msUnload\":" + std::to_string(r.msUnload) +
                             ",\"msLoad\":" + std::to_string(r.msLoad) + "}";
                       return true;
                   },
                   kOwnerDev);
    RegisterMethod("dev.status", [this](const std::string&, std::string& out) -> bool { out = StatusJson(); return true; }, kOwnerDev);
}

}  // namespace hotreload
