#include "hotreload/core_glue.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "mut.h"

namespace hotreload {
namespace {

constexpr const char* kBlobKey = "coop.reload.state";
constexpr uint32_t kBlobVersion = 1;

Report g_last;
std::string g_lastPhase = "none";
std::string g_lastDrain = "null";
uint32_t g_lastBlobBytes = 0;
DrainHooks g_hooks;

std::string esc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += static_cast<char>(c); }
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += static_cast<char>(c);
    }
    return o;
}

std::string arr(const std::vector<std::string>& v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) o += (i ? ",\"" : "\"") + esc(v[i]) + "\"";
    return o + "]";
}

void logReport(const HrLoaderApi* api, const char* what, const Report& r) {
    std::string t = r.text();
    size_t pos = 0;
    while (pos < t.size()) {
        size_t nl = t.find('\n', pos);
        std::string line = t.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        if (!line.empty() && api && api->log) api->log((std::string("reload ") + what + ":" + line).c_str());
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
}

void veto(const HrLoaderApi* api, const std::string& why) {
    if (api && api->veto_reason && api->size >= offsetof(HrLoaderApi, veto_reason) + sizeof(void*)) api->veto_reason(why.c_str());
}

std::string joined(const std::vector<std::string>& v) {
    std::string s;
    for (auto& x : v) s += (s.empty() ? "" : "; ") + x;
    return s;
}

int MState(void*, const char*, HrOut* out) {
    std::string s = StatusJson();
    out->set(out->h, s.c_str());
    return 1;
}

}  // namespace

void SetDrainHooks(DrainHooks hooks) { g_hooks = std::move(hooks); }

bool BeforeUnload(const HrLoaderApi* api) {
    Report rep;
    Bytes blob;
    g_lastPhase = "before_unload";
    if (!global().saveAll(&blob, &rep)) {
        g_last = rep;
        logReport(api, "REFUSED (core keeps running)", rep);
        veto(api, "save or preflight refused: " + joined(rep.failures));
        return false;
    }
    // Drain: take hooks off first (if the mod has any) so nothing can ENTER the core any more, then wait until no other
    // thread is executing inside it. On a timeout the hooks go back and the reload is refused cleanly.
    {
        unsigned timeoutMs = 2000;
        if (const char* e = std::getenv("HOTRELOAD_DRAIN_MS")) timeoutMs = static_cast<unsigned>(std::atoi(e));
        int cannot = g_hooks.suspend ? g_hooks.suspend() : 0;
        DrainResult d;
        if (!cannot) {
            std::vector<CodeRange> ranges;
            ranges.push_back(moduleRangeOf(reinterpret_cast<const void*>(&BeforeUnload)));
            if (g_hooks.extraRanges) g_hooks.extraRanges(ranges);
            std::vector<unsigned long> ignore = HR_MUT(3) ? std::vector<unsigned long>() : managedThreadIds();
            d = waitDrained(ranges, ignore, timeoutMs);
        }
        g_lastDrain = cannot ? std::string("null") : d.toJson();
        if (cannot || !d.drained) {
            if (g_hooks.resume) g_hooks.resume();
            std::string why = cannot ? std::to_string(cannot) + " hook(s) could not be taken off" : "drain: " + d.why;
            rep.failures.push_back(why);
            g_last = rep;
            logReport(api, "REFUSED by the drain (hooks put back, core keeps running)", rep);
            veto(api, why);
            return false;
        }
        rep.notes.push_back("drained after " + std::to_string(d.attempts) + " attempt(s), " + std::to_string(d.scanned) + " threads inspected");
    }
    int leaks = global().shutdownAll(&rep);
    g_last = rep;
    g_lastBlobBytes = static_cast<uint32_t>(blob.size());
    if (leaks > 0 || !rep.failures.empty()) {
        logReport(api, "UNSAFE after shutdown (core degraded, do not unload)", rep);
        if (api && api->blob_erase) api->blob_erase(kBlobKey);
        std::vector<std::string> all = rep.failures;
        all.insert(all.end(), rep.leaks.begin(), rep.leaks.end());
        veto(api, "unsafe after shutdown: " + joined(all));
        return false;
    }
    if (!HR_MUT(7)) {
        if (!api || !api->blob_put || !api->blob_put(kBlobKey, blob.data(), static_cast<uint32_t>(blob.size()), kBlobVersion)) {
            if (api && api->log) api->log("reload: loader refused the state blob");
            veto(api, "the loader refused the state blob");
            return false;
        }
    }
    logReport(api, "ok", rep);
    return true;
}

bool AfterLoad(const HrLoaderApi* api) {
    if (api && api->register_method) api->register_method("reload.state", &MState, nullptr);
    Report rep;
    Bytes blob;
    g_lastPhase = "after_load";
    if (api && api->is_reload && api->blob_get) {
        uint32_t len = 0, ver = 0;
        if (api->blob_get(kBlobKey, nullptr, 0, &len, &ver) && ver == kBlobVersion && len > 0) {
            blob.resize(len);
            uint32_t got = 0;
            if (!api->blob_get(kBlobKey, blob.data(), len, &got, &ver) || got != len) blob.clear();
        } else {
            rep.notes.push_back("no usable state blob from the previous core");
        }
        if (api->blob_erase) api->blob_erase(kBlobKey);  // consumed: a second reload must not see a stale one
    }
    global().restoreAll(blob, &rep);
    g_last = rep;
    logReport(api, "after_load", rep);
    return true;
}

std::string StatusJson() {
    auto& reg = global();
    std::string s = "{\"phase\":\"" + esc(g_lastPhase) + "\",\"participants\":[";
    bool first = true;
    for (auto& n : reg.names()) {
        if (!first) s += ",";
        first = false;
        s += "{\"name\":\"" + esc(n) + "\",\"version\":" + std::to_string(reg.versionOf(n)) + "}";
    }
    s += "],\"liveThreads\":" + std::to_string(liveThreads()) + ",\"lastBlobBytes\":" + std::to_string(g_lastBlobBytes) +
         ",\"restored\":" + std::to_string(g_last.restored) + ",\"fresh\":" + std::to_string(g_last.freshStarts) +
         ",\"migrated\":" + std::to_string(g_last.migrated) + ",\"dropped\":" + std::to_string(g_last.dropped) +
         ",\"notes\":" + arr(g_last.notes) + ",\"failures\":" + arr(g_last.failures) + ",\"leaks\":" + arr(g_last.leaks) +
         ",\"drain\":" + g_lastDrain + "}";
    return s;
}

}  // namespace hotreload
