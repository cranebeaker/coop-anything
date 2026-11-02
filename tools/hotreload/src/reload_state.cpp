#include "hotreload/reload_state.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "crc32.h"
#include "mut.h"

namespace hotreload {

// ---- bytes -------------------------------------------------------------------------------------------------------
void Writer::f64(double v) {
    uint64_t u;
    std::memcpy(&u, &v, 8);
    put(u, 8);
}
void Writer::str(const std::string& s) {
    u32(static_cast<uint32_t>(s.size()));
    raw(s.data(), s.size());
}
void Writer::bytes(const Bytes& v) {
    u32(static_cast<uint32_t>(v.size()));
    raw(v.data(), v.size());
}
void Writer::raw(const void* p, size_t n) {
    const uint8_t* c = static_cast<const uint8_t*>(p);
    b_.insert(b_.end(), c, c + n);
}

uint64_t Reader::get(int n) {
    if (!ok_ || remaining() < static_cast<size_t>(n)) {
        ok_ = false;
        return 0;
    }
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v |= static_cast<uint64_t>(p_[pos_ + i]) << (8 * i);
    pos_ += n;
    return v;
}
double Reader::f64() {
    uint64_t u = get(8);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
std::string Reader::str() {
    uint32_t n = u32();
    if (!ok_ || remaining() < n) {
        ok_ = false;
        return {};
    }
    std::string s(reinterpret_cast<const char*>(p_ + pos_), n);
    pos_ += n;
    return s;
}
Bytes Reader::bytes() {
    uint32_t n = u32();
    if (!ok_ || remaining() < n) {
        ok_ = false;
        return {};
    }
    Bytes b(p_ + pos_, p_ + pos_ + n);
    pos_ += n;
    return b;
}

uint32_t crc32(const void* data, size_t n) { return Crc32(data, n); }

std::string Report::text() const {
    std::string s;
    for (auto& n : notes) s += "  " + n + "\n";
    for (auto& f : failures) s += "  FAIL " + f + "\n";
    for (auto& l : leaks) s += "  LEAK " + l + "\n";
    char buf[128];
    std::snprintf(buf, sizeof buf, "  restored=%d fresh=%d migrated=%d dropped=%d\n", restored, freshStarts, migrated, dropped);
    return s + buf;
}

// ---- registry ----------------------------------------------------------------------------------------------------
namespace {
constexpr uint32_t kMagic = 0x4C525248;  // "HRRL"
constexpr uint32_t kFormat = 1;

bool callBool(const std::function<bool()>& f, std::string* err) {
    try {
        return f();
    } catch (const std::exception& e) {
        *err = std::string("threw: ") + e.what();
    } catch (...) {
        *err = "threw an unknown exception";
    }
    return false;
}
}  // namespace

void Registry::add(const Participant& p) {
    for (auto& e : parts_)
        if (std::string(e.name) == p.name) {
            e = p;
            return;
        }
    parts_.push_back(p);
    std::stable_sort(parts_.begin(), parts_.end(), [](const Participant& a, const Participant& b) { return a.order < b.order; });
}
void Registry::addLeakProbe(const LeakProbe& p) {
    for (auto& e : probes_)
        if (e.name == p.name) {
            e = p;
            return;
        }
    probes_.push_back(p);
}
void Registry::clear() {
    parts_.clear();
    probes_.clear();
}
std::vector<std::string> Registry::names() const {
    std::vector<std::string> v;
    for (auto& p : parts_) v.push_back(p.name);
    return v;
}
uint32_t Registry::versionOf(const std::string& name) const {
    for (auto& p : parts_)
        if (name == p.name) return p.version;
    return 0;
}

bool Registry::saveAll(Bytes* blob, Report* rep) {
    Report local;
    Report& r = rep ? *rep : local;
    bool vetoed = false;
    for (auto& p : parts_) {
        if (!p.canUnload) continue;
        std::string why, err;
        if (!callBool([&] { why = p.canUnload(); return true; }, &err)) why = err;
        if (!why.empty()) {
            r.failures.push_back(std::string("preflight ") + p.name + ": " + why);
            if (!HR_MUT(19)) vetoed = true;
        }
    }
    if (vetoed) return false;
    Writer w;
    w.u32(kMagic);
    w.u32(kFormat);
    w.u32(static_cast<uint32_t>(parts_.size()));
    for (auto& p : parts_) {
        Writer pw;
        std::string err;
        bool ok = true;
        if (p.save) {
            ok = callBool([&] { return p.save(pw); }, &err);
            if (!ok && err.empty()) err = "returned false";
        }
        if (!ok) {
            r.failures.push_back(std::string("save ") + p.name + ": " + err);
            if (!HR_MUT(8)) {
                vetoed = true;
                continue;
            }
            pw = Writer();  // the twin writes an empty payload and carries on
        }
        w.str(p.name);
        w.u32(p.version);
        w.bytes(pw.data());
        w.u32(crc32(pw.data().data(), pw.data().size()));
        r.notes.push_back(std::string("saved ") + p.name + " v" + std::to_string(p.version) + " (" + std::to_string(pw.data().size()) + " bytes)");
    }
    if (vetoed) return false;
    uint32_t total = crc32(w.data().data(), w.data().size());
    w.u32(total);
    if (blob) *blob = w.take();
    return true;
}

int Registry::shutdownAll(Report* rep) {
    Report local;
    Report& r = rep ? *rep : local;
    std::vector<size_t> idx(parts_.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    if (!HR_MUT(15)) std::reverse(idx.begin(), idx.end());
    for (size_t i : idx) {
        Participant& p = parts_[i];
        if (!p.shutdown) continue;
        std::string err;
        bool ok = callBool([&] { p.shutdown(); return true; }, &err);
        if (!ok) r.failures.push_back(std::string("shutdown ") + p.name + ": " + err);  // the others still run
    }
    int leaks = 0;
    for (auto& pr : probes_) {
        int n = pr.count ? pr.count() : 0;
        if (n > pr.baseline) {
            leaks += n - pr.baseline;
            r.leaks.push_back(pr.name + ": " + std::to_string(n) + " alive (expected " + std::to_string(pr.baseline) + ")");
        }
    }
    return leaks;
}

namespace {
struct Entry {
    std::string name;
    uint32_t version = 0;
    Bytes payload;
    bool crcOk = false;
};
bool parseBlob(const Bytes& blob, std::vector<Entry>* out, std::string* why) {
    if (blob.size() < 16) {
        *why = "blob too short";
        return false;
    }
    uint32_t want = 0;
    std::memcpy(&want, blob.data() + blob.size() - 4, 4);
    if (crc32(blob.data(), blob.size() - 4) != want) {
        *why = "blob checksum mismatch";
        return false;
    }
    Reader r(blob.data(), blob.size() - 4);
    if (r.u32() != kMagic) {
        *why = "bad magic";
        return false;
    }
    if (r.u32() != kFormat) {
        *why = "unknown blob format";
        return false;
    }
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n && r.ok(); ++i) {
        Entry e;
        e.name = r.str();
        e.version = r.u32();
        e.payload = r.bytes();
        uint32_t c = r.u32();
        e.crcOk = r.ok() && crc32(e.payload.data(), e.payload.size()) == c;
        out->push_back(std::move(e));
    }
    if (!r.ok() || !r.atEnd()) {
        *why = "blob structure damaged";
        return false;
    }
    return true;
}
}  // namespace

bool Registry::inspect(const Bytes& blob, std::vector<std::pair<std::string, uint32_t>>* entries) {
    std::vector<Entry> es;
    std::string why;
    if (!parseBlob(blob, &es, &why)) return false;
    if (entries)
        for (auto& e : es) entries->push_back({e.name, e.version});
    return true;
}

bool Registry::restoreAll(const Bytes& blob, Report* rep) {
    Report local;
    Report& r = rep ? *rep : local;
    std::vector<Entry> es;
    std::string why;
    bool usable = !blob.empty() && parseBlob(blob, &es, &why);
    if (!blob.empty() && !usable) r.failures.push_back("restore: " + why + "; every participant starts fresh");
    if (blob.empty()) r.notes.push_back("restore: no blob, cold start");

    auto startFresh = [&](Participant& p, const std::string& reason) {
        ++r.freshStarts;
        r.notes.push_back(std::string("fresh ") + p.name + ": " + reason);
        std::string err;
        if (p.fresh && !callBool([&] { p.fresh(); return true; }, &err)) r.failures.push_back(std::string("fresh ") + p.name + ": " + err);
    };

    std::vector<bool> used(es.size(), false);
    for (auto& p : parts_) {
        size_t found = es.size();
        for (size_t i = 0; i < es.size(); ++i)
            if (es[i].name == p.name) found = i;
        if (!usable || found == es.size()) {
            startFresh(p, usable ? "no saved state" : "blob unusable");
            continue;
        }
        used[found] = true;
        Entry& e = es[found];
        if (!e.crcOk && !HR_MUT(14)) {
            r.failures.push_back(std::string("restore ") + p.name + ": payload checksum mismatch");
            startFresh(p, "payload corrupt");
            continue;
        }
        bool mismatch = e.version != p.version && !HR_MUT(6);
        if (mismatch && !p.migrate) {
            startFresh(p, "saved v" + std::to_string(e.version) + " but running v" + std::to_string(p.version) + " and no migrate()");
            continue;
        }
        Reader rd(e.payload);
        std::string err;
        const auto& fn = mismatch ? p.migrate : p.restore;
        bool ok = fn ? callBool([&] { return fn(e.version, rd); }, &err) : true;
        if (ok && !rd.ok()) {
            ok = false;
            err = "read past the end of its payload";
        }
        if (!ok) {
            r.failures.push_back(std::string("restore ") + p.name + ": " + (err.empty() ? "rejected" : err));
            startFresh(p, "restore failed");
            continue;
        }
        if (mismatch) ++r.migrated;
        ++r.restored;
        if (!rd.atEnd()) r.notes.push_back(std::string("restore ") + p.name + ": " + std::to_string(rd.remaining()) + " trailing bytes ignored");
        r.notes.push_back(std::string("restored ") + p.name + " v" + std::to_string(e.version));
    }
    for (size_t i = 0; i < es.size(); ++i)
        if (!used[i]) {
            ++r.dropped;
            r.notes.push_back("dropped saved state of removed participant " + es[i].name);
        }
    return usable || blob.empty();
}

Registry& global() {
    static Registry* r = [] {
        auto* g = new Registry();  // leaked on purpose: static destructors must not run while a core is being unmapped
        g->addLeakProbe(threadProbe());
        return g;
    }();
    return *r;
}

// ---- threads -----------------------------------------------------------------------------------------------------
namespace {
std::atomic<int> g_threads{0};
std::mutex g_tidMu;
std::vector<unsigned long>& tids() {
    static std::vector<unsigned long>* v = new std::vector<unsigned long>();
    return *v;
}
}  // namespace

std::vector<unsigned long> managedThreadIds() {
    std::lock_guard<std::mutex> g(g_tidMu);
    return tids();
}
void adoptThread(unsigned long tid) {
    if (!tid || HR_MUT(2)) return;
    std::lock_guard<std::mutex> g(g_tidMu);
    auto& v = tids();
    if (std::find(v.begin(), v.end(), tid) == v.end()) v.push_back(tid);
}
void releaseThread(unsigned long tid) {
    std::lock_guard<std::mutex> g(g_tidMu);
    auto& v = tids();
    v.erase(std::remove(v.begin(), v.end(), tid), v.end());
}
int liveThreads() { return g_threads.load(); }
LeakProbe threadProbe() { return LeakProbe{"threads", [] { return liveThreads(); }, 0}; }

struct ManagedThread::State {
    std::atomic<bool> stop{false};
    std::atomic<bool> done{false};
    std::thread th;
};

void ManagedThread::start(const char* name, std::function<void(const std::function<bool()>&)> body) {
    stopAndJoin();
    name_ = name ? name : "";
    auto st = std::make_shared<State>();
    st_ = st;
    ++g_threads;
    st->th = std::thread([st, body = std::move(body)] {
        std::function<bool()> stopped = [st] { return st->stop.load(); };
        const unsigned long me = GetCurrentThreadId();
        {
            std::lock_guard<std::mutex> g(g_tidMu);
            tids().push_back(me);
        }
        try {
            body(stopped);
        } catch (...) {
        }
        {
            std::lock_guard<std::mutex> g(g_tidMu);
            auto& v = tids();
            v.erase(std::remove(v.begin(), v.end(), me), v.end());
        }
        st->done = true;
        --g_threads;
    });
}

void ManagedThread::stopAndJoin() {
    if (!st_) return;
    st_->stop = true;
    if (st_->th.joinable()) {
        if (HR_MUT(16)) st_->th.detach();
        else st_->th.join();
    }
    st_.reset();
}

bool ManagedThread::running() const { return st_ && !st_->done.load(); }

}  // namespace hotreload
