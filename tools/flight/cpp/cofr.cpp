#include "cofr.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace cofr {
namespace {

// ---- little-endian byte writer / reader ----
struct W {
    std::vector<uint8_t>& v;
    void u8(uint32_t x) { v.push_back(static_cast<uint8_t>(x)); }
    void u16(uint32_t x) { u8(x & 0xFF); u8((x >> 8) & 0xFF); }
    void u32(uint32_t x) { u16(x & 0xFFFF); u16(x >> 16); }
    void u64(uint64_t x) { u32(static_cast<uint32_t>(x)); u32(static_cast<uint32_t>(x >> 32)); }
    void str(const std::string& s) { size_t n = s.size() > 0xFFFF ? 0xFFFF : s.size(); u16(static_cast<uint32_t>(n)); v.insert(v.end(), s.begin(), s.begin() + n); }
    void raw(const uint8_t* p, size_t n) { if (n) v.insert(v.end(), p, p + n); }
};
struct R {
    const uint8_t* p; size_t n; size_t i = 0; bool ok = true;
    R(const uint8_t* d, size_t sz) : p(d), n(sz) {}
    bool need(size_t k) { if (!ok || n - i < k) { ok = false; return false; } return true; }
    uint8_t u8() { if (!need(1)) return 0; return p[i++]; }
    uint16_t u16() { uint16_t a = u8(); uint16_t b = u8(); return static_cast<uint16_t>(a | (b << 8)); }
    uint32_t u32() { uint32_t a = u16(); uint32_t b = u16(); return a | (b << 16); }
    uint64_t u64() { uint64_t a = u32(); uint64_t b = u32(); return a | (b << 32); }
};

void putLE32(uint8_t* d, uint32_t x) { for (int k = 0; k < 4; ++k) d[k] = static_cast<uint8_t>(x >> (8 * k)); }
uint32_t getLE32(const uint8_t* d) { return d[0] | (d[1] << 8) | (d[2] << 16) | (static_cast<uint32_t>(d[3]) << 24); }

constexpr size_t kEntryHeader = 20;   // ring entry: u32 total, u8 type, u8 flags, u32 frame, u64 tUs, u16 payload length
constexpr size_t kMaxPayload = 65535;
constexpr size_t kMaxRawCap = kMaxPayload - kNetPrefix;
constexpr uint8_t kDecodeAtDump = 255;   // internal marker in a ring entry: the decoder has not run yet

uint64_t steadyUs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
uint64_t unixMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// SplitMix64 finalizer (Sebastiano Vigna, public domain).
uint64_t splitmix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

uint64_t digestHash(const std::vector<uint32_t>& shards, size_t entries) {
    std::vector<uint8_t> b;
    b.reserve(shards.size() * 4);
    W w{b};
    for (uint32_t s : shards) w.u32(s);
    return hash64(b.data(), b.size(), kHashSeed + entries);
}

void encodeDigest(std::vector<uint8_t>& v, const DigestEvent& d) {
    W w{v};
    size_t nShards = d.shards.size() > 4096 ? 4096 : d.shards.size();
    size_t nKeys = d.keys.size() > kMaxDigestKeys ? kMaxDigestKeys : d.keys.size();
    w.u8(d.category); w.u32(d.epoch); w.u64(d.seq); w.u64(d.hash); w.u32(d.itemCount);
    w.u16(static_cast<uint32_t>(nShards));
    for (size_t i = 0; i < nShards; ++i) w.u32(d.shards[i]);
    w.u32(static_cast<uint32_t>(nKeys));
    for (size_t i = 0; i < nKeys; ++i) { w.u64(d.keys[i].first); w.u32(d.keys[i].second); }
}

const char kDefaultSchema[] =
    R"({"game":"example",)"
    R"("digest_categories":{"0":"story","1":"actors","2":"containers","3":"objects","4":"other"},)"
    R"("marker_kinds":{"0":"note","1":"desync","2":"join","3":"leave","4":"test-step","5":"save","6":"load","7":"rebase"},)"
    R"("ownership_ops":{"0":"grant","1":"transfer","2":"release","3":"rejectedStale","4":"rejectedEcho","5":"rejectedNonOwner"},)"
    R"("ownership_owner_ops":[0,1],)"
    R"("apply_flags":{"1":"snapshot","2":"repair","4":"baseline","8":"catch-up"},)"
    R"("restate_mask":7})";

}  // namespace

const char* defaultSchemaJson() { return kDefaultSchema; }

uint64_t hash64(const void* data, size_t size, uint64_t seed) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = seed;
    for (size_t i = 0; i < size; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

uint32_t bucketMix(uint64_t key, uint32_t valueHash) {
    return static_cast<uint32_t>(splitmix(key ^ splitmix(valueHash)));
}

DigestEvent bucketedDigest(uint8_t category, uint32_t epoch, uint64_t seq, const std::vector<std::pair<uint64_t, uint32_t>>& entries, size_t buckets) {
    if (buckets == 0) buckets = 1;
    DigestEvent d;
    d.category = category; d.epoch = epoch; d.seq = seq; d.itemCount = static_cast<uint32_t>(entries.size());
    d.bucketed = true;
    d.shards.assign(buckets, 0);
    for (const auto& e : entries) d.shards[e.first % buckets] += bucketMix(e.first, mutations().bucketsIgnoreValue ? 0u : e.second);   // a sum: independent of the order of entries
    d.hash = digestHash(d.shards, entries.size());
    return d;
}

void BucketedState::set(uint64_t key, uint32_t valueHash) {
    size_t b = key % sums_.size();
    auto it = map_.find(key);
    if (it == map_.end()) {
        map_.emplace(key, valueHash);
    } else {
        if (!mutations().incrementalSkipsUpdate) sums_[b] -= bucketMix(key, it->second);
        it->second = valueHash;
    }
    sums_[b] += bucketMix(key, valueHash);
}

bool BucketedState::erase(uint64_t key) {
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    sums_[key % sums_.size()] -= bucketMix(key, it->second);
    map_.erase(it);
    return true;
}

DigestEvent BucketedState::digest(uint8_t category, uint32_t epoch, uint64_t seq) const {
    DigestEvent d;
    d.category = category; d.epoch = epoch; d.seq = seq; d.itemCount = static_cast<uint32_t>(map_.size());
    d.bucketed = true;
    d.shards = sums_;
    d.hash = digestHash(d.shards, map_.size());
    return d;
}

std::vector<std::pair<uint64_t, uint32_t>> BucketedState::entries() const {
    std::vector<std::pair<uint64_t, uint32_t>> v(map_.begin(), map_.end());
    std::sort(v.begin(), v.end());
    return v;
}

// ---- recorder ----

Recorder::Recorder(Config cfg) : cfg_(std::move(cfg)) {
    if (cfg_.ringBytes < 4096) cfg_.ringBytes = 4096;
    if (cfg_.maxRawBytes > kMaxRawCap) cfg_.maxRawBytes = kMaxRawCap;
    if (cfg_.schemaJson.empty()) cfg_.schemaJson = kDefaultSchema;
    ring_.assign(cfg_.ringBytes, 0);
    clock_ = steadyUs;
    clock0_ = clock_();
    wallStartMs_ = unixMs();
}

void Recorder::setClockSource(std::function<uint64_t()> nowUs) {
    std::lock_guard<std::mutex> g(mu_);
    clock_ = nowUs ? std::move(nowUs) : std::function<uint64_t()>(steadyUs);
    clock0_ = clock_();
}
void Recorder::setIdentity(uint8_t slot, uint8_t role) { std::lock_guard<std::mutex> g(mu_); cfg_.slot = slot; cfg_.role = role; }
void Recorder::setSession(uint32_t session, uint32_t epoch) { std::lock_guard<std::mutex> g(mu_); session_ = session; epoch_ = epoch; }
void Recorder::meta(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> g(mu_);
    for (auto& kv : meta_) if (kv.first == key) { kv.second = value; return; }
    meta_.emplace_back(key, value);
}

uint64_t Recorder::nowUs() const {
    uint64_t t = clock_();
    return t >= clock0_ ? t - clock0_ : 0;
}

// Appends one entry. `a` and `b` are two payload parts (avoids a temp buffer for Net records).
void Recorder::push(uint8_t type, uint8_t flags, const uint8_t* a, size_t an, const uint8_t* b, size_t bn) {
    size_t plen = an + bn;
    if (plen > kMaxPayload) return;
    size_t total = kEntryHeader + plen;
    std::lock_guard<std::mutex> g(mu_);
    if (total > ring_.size()) return;
    if (mutations().dropNewest && ring_.size() - used_ < total) { ++dropped_; return; }
    while (ring_.size() - used_ < total) {   // drop the oldest entry
        uint8_t hb[4];
        for (int k = 0; k < 4; ++k) hb[k] = ring_[(head_ + k) % ring_.size()];
        size_t old = getLE32(hb);
        if (old < kEntryHeader || old > used_) { head_ = 0; used_ = 0; break; }   // corrupt: reset
        head_ = (head_ + old) % ring_.size();
        used_ -= old;
        ++dropped_;
    }
    uint8_t hdr[kEntryHeader];
    putLE32(hdr, static_cast<uint32_t>(total));
    hdr[4] = type; hdr[5] = flags;
    putLE32(hdr + 6, frame_.load(std::memory_order_relaxed));
    uint64_t t = nowUs();
    putLE32(hdr + 10, static_cast<uint32_t>(t)); putLE32(hdr + 14, static_cast<uint32_t>(t >> 32));
    hdr[18] = static_cast<uint8_t>(plen & 0xFF); hdr[19] = static_cast<uint8_t>(plen >> 8);
    size_t pos = (head_ + used_) % ring_.size();
    auto put = [&](const uint8_t* s, size_t n) {
        if (!n) return;
        size_t first = ring_.size() - pos < n ? ring_.size() - pos : n;
        std::memcpy(ring_.data() + pos, s, first);
        if (first < n) std::memcpy(ring_.data(), s + first, n - first);
        pos = (pos + n) % ring_.size();
    };
    put(hdr, kEntryHeader); put(a, an); put(b, bn);
    used_ += total;
    ++count_;
}

void Recorder::netOut(uint32_t to, const uint8_t* data, size_t size, uint8_t channel) {
    if (!enabled()) return;
    uint8_t pre[kNetPrefix];
    size_t keep = size < cfg_.maxRawBytes ? size : cfg_.maxRawBytes;
    std::memset(pre, 0, sizeof pre);
    putLE32(pre, to);
    pre[4] = channel;
    pre[5] = size ? data[0] : 0;
    pre[7] = cfg_.decoder ? kDecodeAtDump : 2;
    putLE32(pre + 32, static_cast<uint32_t>(size));
    push(kNetOut, (keep < size && !mutations().noTruncFlag) ? kFlagTruncated : 0, pre, sizeof pre, data, keep);
}
void Recorder::netIn(uint32_t from, const uint8_t* data, size_t size) {
    if (!enabled() || mutations().noNetIn) return;
    uint8_t pre[kNetPrefix];
    size_t keep = size < cfg_.maxRawBytes ? size : cfg_.maxRawBytes;
    std::memset(pre, 0, sizeof pre);
    putLE32(pre, from);
    pre[4] = 0xFF;
    pre[5] = size ? data[0] : 0;
    pre[7] = cfg_.decoder ? kDecodeAtDump : 2;
    putLE32(pre + 32, static_cast<uint32_t>(size));
    push(kNetIn, (keep < size && !mutations().noTruncFlag) ? kFlagTruncated : 0, pre, sizeof pre, data, keep);
}

void Recorder::captured(const CapturedEvent& e) {
    if (!enabled()) return;
    std::vector<uint8_t> v; v.reserve(32);
    W w{v};
    w.u8(e.kind); w.u32(e.intentId); w.u64(e.seq); w.u32(e.effectCount); w.u64(e.hash); w.u8(e.cause); w.u32(e.epoch);
    push(kTxnCaptured, 0, v.data(), v.size());
}

void Recorder::applied(const AppliedEvent& e) {
    if (!enabled()) return;
    std::vector<uint8_t> v; v.reserve(40);
    W w{v};
    w.u64(e.seq); w.u32(e.epoch); w.u8(e.origin); w.u32(e.index);
    w.u8(mutations().noAppliedFlags ? 0 : e.flags);
    w.u8(e.cause); w.u8(e.effectTag); w.u64(e.hash);
    push(kTxnApplied, 0, v.data(), v.size());
}

void Recorder::ownership(const OwnershipEvent& e) {
    if (!enabled()) return;
    std::vector<uint8_t> v; v.reserve(32);
    W w{v};
    w.u8(e.op); w.u64(e.actor); w.u32(e.epoch); w.u8(e.fromOwner); w.u8(e.toOwner); w.u8(e.reason); w.u64(e.txnId);
    push(kOwnership, 0, v.data(), v.size());
}

void Recorder::digest(const DigestEvent& d, uint8_t flags) {
    if (!enabled()) return;
    std::vector<uint8_t> v; v.reserve(40 + d.shards.size() * 4 + d.keys.size() * 12);
    encodeDigest(v, d);
    push(kDigest, static_cast<uint8_t>(flags | (d.bucketed ? kFlagBucketed : 0)), v.data(), v.size());
}

void Recorder::digestDetail(uint8_t category, uint32_t epoch, uint64_t seq, const std::vector<std::pair<uint64_t, uint32_t>>& entries) {
    if (!enabled()) return;
    size_t n = entries.size();
    if (mutations().detailDropsKey && n) --n;
    for (size_t i = 0; i < n; i += kMaxDigestKeys) {
        DigestEvent d;
        d.category = category; d.epoch = epoch; d.seq = seq; d.itemCount = static_cast<uint32_t>(entries.size());
        d.keys.assign(entries.begin() + i, entries.begin() + std::min(n, i + kMaxDigestKeys));
        digest(d, kFlagDetail);
    }
}

void Recorder::entryText(uint64_t key, uint32_t valueHash, const std::string& text) {
    char head[64];
    std::snprintf(head, sizeof head, "entry %016llx %u ", static_cast<unsigned long long>(key), valueHash);
    marker(kMarkNote, std::string(head) + text);
}

void Recorder::marker(uint16_t kind, const std::string& text) {
    if (!enabled()) return;
    std::vector<uint8_t> v; v.reserve(4 + text.size());
    W w{v};
    w.u16(kind); w.str(text);
    push(kMarker, 0, v.data(), v.size());
}

void Recorder::custom(uint8_t type, const uint8_t* payload, size_t size) {
    if (!enabled() || type < kFirstCustom) return;
    if (mutations().customCutsPayload && size) --size;
    push(type, 0, payload, size);
}

Recorder::Stats Recorder::stats() const {
    std::lock_guard<std::mutex> g(mu_);
    Stats s;
    s.records = count_; s.dropped = dropped_; s.bytesRing = used_; s.dumps = dumps_; s.dumpFailures = dumpFailures_; s.autoDumpsSkipped = autoSkipped_;
    return s;
}

// ---- dump ----

bool Recorder::dump(const std::string& path, uint8_t reason) { return dumpImpl(path, reason, true); }

bool Recorder::dumpImpl(const std::string& path, uint8_t reason, bool lock) {
    try {
        std::vector<uint8_t> lin;
        uint64_t dropped, tNow;
        uint32_t session, epoch;
        uint8_t slot, role;
        std::vector<std::pair<std::string, std::string>> meta;
        {
            std::unique_lock<std::mutex> g(mu_, std::defer_lock);
            if (lock) g.lock(); else (void)g.try_lock();   // crash path: proceed without the lock if it is held
            lin.resize(used_);
            size_t first = ring_.size() - head_ < used_ ? ring_.size() - head_ : used_;
            if (used_) std::memcpy(lin.data(), ring_.data() + head_, first);
            if (first < used_) std::memcpy(lin.data() + first, ring_.data(), used_ - first);
            dropped = dropped_; tNow = nowUs(); session = session_; epoch = epoch_; slot = cfg_.slot; role = cfg_.role;
            meta = meta_;
        }
        meta.insert(meta.begin(), {"label", cfg_.label});
        std::vector<uint8_t> out;
        out.reserve(lin.size() + 4096);
        W w{out};
        // header (recordCount is patched below)
        w.raw(reinterpret_cast<const uint8_t*>("COFR"), 4);
        w.u16(kFormatVersion); w.u16(kHeaderSize);
        w.u8(slot); w.u8(role); w.u8(reason); w.u8(0);
        w.u32(cfg_.protoVersion);
        w.u64(wallStartMs_); w.u64(tNow); w.u64(mutations().noDroppedCount ? 0 : dropped);
        w.u64(0);   // recordCount
        w.u32(session); w.u32(epoch);
        w.u64(0);
        uint64_t nrec = 0;
        auto emit = [&](uint8_t type, uint8_t flags, uint32_t frame, uint64_t t, const uint8_t* p, size_t n) {
            w.u8(type); w.u8(flags); w.u16(static_cast<uint32_t>(n)); w.u32(frame); w.u64(t); w.raw(p, n);
            ++nrec;
        };
        {
            std::vector<uint8_t> m;
            W mw{m};
            for (auto& kv : meta) { mw.str(kv.first); mw.str(kv.second); }
            emit(kMeta, 0, 0, 0, m.data(), m.size());
        }
        if (!mutations().noSchemaRecord && !cfg_.schemaJson.empty() && cfg_.schemaJson.size() <= kMaxPayload)
            emit(kSchema, 0, 0, 0, reinterpret_cast<const uint8_t*>(cfg_.schemaJson.data()), cfg_.schemaJson.size());
        size_t pos = 0;
        std::vector<uint8_t> tmp;
        while (pos + kEntryHeader <= lin.size()) {
            const uint8_t* e = lin.data() + pos;
            size_t total = getLE32(e);
            if (total < kEntryHeader || pos + total > lin.size()) break;
            uint8_t type = e[4], flags = e[5];
            uint32_t frame = getLE32(e + 6);
            uint64_t t = getLE32(e + 10) | (static_cast<uint64_t>(getLE32(e + 14)) << 32);
            size_t plen = e[18] | (e[19] << 8);
            const uint8_t* payload = e + kEntryHeader;
            pos += total;
            if ((type == kNetOut || type == kNetIn) && plen >= kNetPrefix) {
                tmp.assign(payload, payload + plen);
                size_t rawStored = plen - kNetPrefix;
                uint32_t rawLen = getLE32(tmp.data() + 32);
                if (tmp[7] == kDecodeAtDump) {
                    tmp[7] = 1;
                    NetMeta m;
                    bool ok = false;
                    if (!(flags & kFlagTruncated) && rawStored == rawLen && cfg_.decoder) {
                        try { ok = cfg_.decoder(tmp.data() + kNetPrefix, rawStored, m); } catch (...) { ok = false; }
                    }
                    if (ok) {
                        tmp[7] = 0;
                        if (mutations().zeroSeq) m = NetMeta();
                        tmp[6] = m.msgType;
                        putLE32(tmp.data() + 8, m.session); putLE32(tmp.data() + 12, m.epoch);
                        putLE32(tmp.data() + 16, static_cast<uint32_t>(m.seqLo)); putLE32(tmp.data() + 20, static_cast<uint32_t>(m.seqLo >> 32));
                        putLE32(tmp.data() + 24, static_cast<uint32_t>(m.seqHi)); putLE32(tmp.data() + 28, static_cast<uint32_t>(m.seqHi >> 32));
                        emit(type, flags, frame, t, tmp.data(), tmp.size());
                        if (m.hasDigest && !mutations().noDerivedDigest) {   // a derived digest record next to the message
                            std::vector<uint8_t> dv;
                            encodeDigest(dv, m.digest);
                            emit(kDigest, static_cast<uint8_t>(kFlagDerived | (m.digest.bucketed ? kFlagBucketed : 0)), frame, t, dv.data(), dv.size());
                        }
                        continue;
                    }
                }
                emit(type, flags, frame, t, tmp.data(), tmp.size());
                continue;
            }
            emit(type, flags, frame, t, payload, plen);
        }
        for (int k = 0; k < 8; ++k) out[40 + k] = static_cast<uint8_t>(nrec >> (8 * k));

        std::error_code ec;
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) { ++dumpFailures_; return false; }
        bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
        ok = (std::fclose(f) == 0) && ok;
        if (ok) ++dumps_; else ++dumpFailures_;
        return ok;
    } catch (...) {
        ++dumpFailures_;
        return false;
    }
}

std::string Recorder::dumpToDir(uint8_t reason) {
    if (cfg_.dir.empty()) return {};
    static const char* names[] = {"manual", "desync", "crash", "shutdown", "test"};
    std::string name = cfg_.label + "-" + names[reason < 5 ? reason : 0] + "-" + std::to_string(unixMs()) + "-" + std::to_string(++fileSeq_) + ".cofr";   // seq: two dumps in one millisecond must not collide
    std::string path = (std::filesystem::path(cfg_.dir) / name).string();
    return dumpImpl(path, reason, true) ? path : std::string();
}

std::string Recorder::onDesync(const std::string& what) {
    marker(kMarkDesync, what);
    uint64_t now = unixMs();
    {
        std::lock_guard<std::mutex> g(mu_);
        if (autoDumps_ >= cfg_.autoDumpMax || (autoDumps_ > 0 && now - lastAutoDumpMs_ < cfg_.autoDumpMinIntervalMs)) { ++autoSkipped_; return {}; }
        ++autoDumps_; lastAutoDumpMs_ = now;
    }
    return dumpToDir(kDumpDesync);
}

std::string Recorder::dumpCrash() {
    if (cfg_.dir.empty()) return {};
    std::string path = (std::filesystem::path(cfg_.dir) / (cfg_.label + "-crash-" + std::to_string(unixMs()) + "-" + std::to_string(++fileSeq_) + ".cofr")).string();
    return dumpImpl(path, kDumpCrash, false) ? path : std::string();
}

// ---- reader ----

bool parseTrace(const std::vector<uint8_t>& b, Trace& out, std::string* err) {
    auto fail = [&](const char* m) { if (err) *err = m; return false; };
    out = Trace();
    if (b.size() < kHeaderSize || std::memcmp(b.data(), "COFR", 4) != 0) return fail("bad magic");
    R r(b.data(), b.size());
    r.i = 4;
    TraceHeader& h = out.header;
    h.version = r.u16(); h.headerSize = r.u16();
    h.slot = r.u8(); h.role = r.u8(); h.reason = r.u8(); r.u8();
    h.protoVersion = r.u32(); h.wallStartMs = r.u64(); h.dumpTUs = r.u64(); h.dropped = r.u64(); h.recordCount = r.u64();
    h.sessionId = r.u32(); h.epoch = r.u32();
    if (h.version != kFormatVersion) return fail("unsupported version");
    if (h.headerSize < kHeaderSize || h.headerSize > b.size()) return fail("bad header size");
    r.i = h.headerSize;
    while (b.size() - r.i >= kRecordHeaderSize) {
        TraceRecord rec;
        size_t start = r.i;
        rec.type = r.u8(); rec.flags = r.u8();
        size_t len = r.u16();
        rec.frame = r.u32(); rec.tUs = r.u64();
        if (b.size() - r.i < len) { r.i = start; break; }   // cut short: valid up to the last complete record
        rec.payload.assign(b.begin() + r.i, b.begin() + r.i + len);
        r.i += len;
        if (rec.type == kSchema && out.schemaJson.empty()) out.schemaJson.assign(rec.payload.begin(), rec.payload.end());
        out.records.push_back(std::move(rec));
    }
    return true;
}

bool readTrace(const std::string& path, Trace& out, std::string* err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { if (err) *err = "cannot open " + path; return false; }
    std::vector<uint8_t> b;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
    std::fclose(f);
    return parseTrace(b, out, err);
}

bool parseNet(const TraceRecord& rec, NetInfo& o) {
    if ((rec.type != kNetOut && rec.type != kNetIn) || rec.payload.size() < kNetPrefix) return false;
    R r(rec.payload.data(), rec.payload.size());
    o.peer = r.u32(); o.channel = r.u8(); o.mux = r.u8(); o.msgType = r.u8(); o.decode = r.u8();
    o.session = r.u32(); o.epoch = r.u32(); o.seqLo = r.u64(); o.seqHi = r.u64(); o.rawLen = r.u32();
    o.raw.assign(rec.payload.begin() + kNetPrefix, rec.payload.end());
    return r.ok;
}

bool parseDigest(const TraceRecord& rec, DigestEvent& d) {
    if (rec.type != kDigest) return false;
    R r(rec.payload.data(), rec.payload.size());
    d = DigestEvent();
    d.category = r.u8(); d.epoch = r.u32(); d.seq = r.u64(); d.hash = r.u64(); d.itemCount = r.u32();
    d.bucketed = (rec.flags & kFlagBucketed) != 0;
    size_t ns = r.u16();
    for (size_t i = 0; i < ns && r.ok; ++i) d.shards.push_back(r.u32());
    size_t nk = r.u32();
    for (size_t i = 0; i < nk && r.ok; ++i) { uint64_t k = r.u64(); uint32_t hsh = r.u32(); d.keys.emplace_back(k, hsh); }
    return r.ok;
}

bool parseOwnership(const TraceRecord& rec, OwnershipEvent& e) {
    if (rec.type != kOwnership) return false;
    R r(rec.payload.data(), rec.payload.size());
    e.op = r.u8(); e.actor = r.u64(); e.epoch = r.u32(); e.fromOwner = r.u8(); e.toOwner = r.u8(); e.reason = r.u8(); e.txnId = r.u64();
    return r.ok;
}

bool parseCaptured(const TraceRecord& rec, CapturedEvent& e) {
    if (rec.type != kTxnCaptured) return false;
    R r(rec.payload.data(), rec.payload.size());
    e.kind = r.u8(); e.intentId = r.u32(); e.seq = r.u64(); e.effectCount = r.u32(); e.hash = r.u64(); e.cause = r.u8(); e.epoch = r.u32();
    return r.ok;
}

bool parseApplied(const TraceRecord& rec, AppliedEvent& e) {
    if (rec.type != kTxnApplied) return false;
    R r(rec.payload.data(), rec.payload.size());
    e.seq = r.u64(); e.epoch = r.u32(); e.origin = r.u8(); e.index = r.u32(); e.flags = r.u8(); e.cause = r.u8(); e.effectTag = r.u8(); e.hash = r.u64();
    return r.ok;
}

}  // namespace cofr
