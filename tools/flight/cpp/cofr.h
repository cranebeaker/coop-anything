// Co-op flight recorder: a compact binary trace of everything one machine sent, received, decided and applied.
// Standalone C++17, no game or engine code. Format: tools/flight/FORMAT.md (.cofr, version 1).
//
// The ring lives in memory. It is written to a .cofr file on a desync, a crash, shutdown or an explicit dump.
// Hot-path cost: one mutex lock and one memcpy of the datagram (at most maxRawBytes); no allocation, no decoding.
// Decoding (message type, session, epoch, seq range) happens at dump time through an optional decoder you give it.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "observer.h"

namespace cofr {

using Bytes = std::vector<uint8_t>;

constexpr uint16_t kFormatVersion = 1;
constexpr size_t kHeaderSize = 64;
constexpr size_t kRecordHeaderSize = 16;
constexpr size_t kNetPrefix = 36;           // bytes of a Net payload before the raw datagram
constexpr size_t kMaxDigestKeys = 4000;     // per Digest record; digestDetail() splits bigger lists over several records
constexpr size_t kDefaultBuckets = 256;     // bucketed digests: bucket = key mod number of buckets
constexpr uint64_t kHashSeed = 14695981039346656037ull;   // FNV-1a 64 offset basis

// TEST ONLY: each flag re-introduces one bug that the recorder's tests must catch (mutation twins).
struct Mutations {
    bool dropNewest = false;           // a full ring drops the NEW record instead of the oldest
    bool noNetIn = false;              // received datagrams are not recorded
    bool zeroSeq = false;              // decoded session / epoch / seq fields are left zero
    bool noTruncFlag = false;          // a cut datagram is not flagged
    bool noDroppedCount = false;       // the header says nothing was dropped
    bool noAppliedFlags = false;       // the applied flags (repair / baseline / catch-up) are always 0
    bool noDerivedDigest = false;      // a decoder's digest does not yield a Digest record
    bool bucketsIgnoreValue = false;   // bucket hashes cover keys only: a changed value is invisible
    bool detailDropsKey = false;       // digestDetail loses the last entry
    bool incrementalSkipsUpdate = false;   // BucketedState::set forgets to remove the old value of a replaced key
    bool noSchemaRecord = false;       // dumps carry no schema
    bool customCutsPayload = false;    // custom records lose their last byte
};
inline Mutations& mutations() { static Mutations m; return m; }

// The schema is a JSON text the game owns: names for digest categories, marker kinds, message types and so on, and
// the layout of game-defined record types. See FORMAT.md section 5. An empty schemaJson means the default example set.
const char* defaultSchemaJson();

// Decoder for datagrams, called at dump time on each recorded datagram that was not cut. Return false when the bytes are not
// a message of your protocol. Fill only what your protocol has.
using NetDecoder = std::function<bool(const uint8_t* data, size_t size, NetMeta& out)>;

struct Config {
    size_t ringBytes = 16u << 20;     // oldest records are overwritten when full
    size_t maxRawBytes = 48000;       // datagram bytes kept per Net record (the rest is cut and flagged)
    uint8_t slot = kNoSlot;
    uint8_t role = kRoleSim;
    uint32_t protoVersion = 0;        // your protocol or schema version, goes into the file header
    std::string label = "cofr";       // goes into the Meta record and the dump file name
    std::string dir;                  // default folder for dumpToDir()
    uint32_t autoDumpMinIntervalMs = 30000;   // onDesync() dumps at most this often ...
    uint32_t autoDumpMax = 8;                 // ... and at most this many files per run
    std::string schemaJson;           // empty: defaultSchemaJson()
    NetDecoder decoder;               // optional
};

// ---- hashing helpers (the same algorithms are in tools/flight/cofr.py) ----

uint64_t hash64(const void* data, size_t size, uint64_t seed = kHashSeed);   // FNV-1a, 64 bit
uint32_t bucketMix(uint64_t key, uint32_t valueHash);                        // low 32 bits of a 64-bit mix of (key, value hash)

// A digest with order-independent bucket hashes over (key, value hash) entries: bucket = key mod buckets, bucket hash =
// sum (mod 2^32) of bucketMix over its entries, `hash` covers the buckets and the entry count. Two machines whose entry
// lists differ in one value get exactly one differing bucket.
DigestEvent bucketedDigest(uint8_t category, uint32_t epoch, uint64_t seq, const std::vector<std::pair<uint64_t, uint32_t>>& entries,
                           size_t buckets = kDefaultBuckets);

// The same digest kept up to date one entry at a time, so a game that changes few entries per frame does not walk its whole state.
class BucketedState {
public:
    explicit BucketedState(size_t buckets = kDefaultBuckets) : sums_(buckets ? buckets : 1, 0) {}
    void set(uint64_t key, uint32_t valueHash);   // insert or replace
    bool erase(uint64_t key);
    size_t size() const { return map_.size(); }
    DigestEvent digest(uint8_t category, uint32_t epoch, uint64_t seq) const;   // costs one pass over the buckets, not over the entries
    std::vector<std::pair<uint64_t, uint32_t>> entries() const;
private:
    std::unordered_map<uint64_t, uint32_t> map_;
    std::vector<uint32_t> sums_;
};

// ---- the recorder ----

class Recorder : public IObserver {
public:
    explicit Recorder(Config cfg = Config());

    // clock and frame: real by default; a simulator injects its virtual clock
    void setClockSource(std::function<uint64_t()> nowUs);    // microseconds, monotonic; recorder time = value - value at this call
    void setFrame(uint32_t frame) override { frame_.store(frame, std::memory_order_relaxed); }
    uint32_t frame() const { return frame_.load(std::memory_order_relaxed); }
    void setIdentity(uint8_t slot, uint8_t role);
    void setSession(uint32_t session, uint32_t epoch);       // for the file header
    void setWallStart(uint64_t unixMs) { wallStartMs_ = unixMs; }   // tests: makes the file deterministic
    void setEnabled(bool on) { enabled_.store(on, std::memory_order_relaxed); }
    bool enabled() const { return enabled_.load(std::memory_order_relaxed); }

    // Meta data, written into the Meta record of the next dump.
    void meta(const std::string& key, const std::string& value);
    // Tell the analyzer which machine slot a transport peer id means on this machine (stored as Meta "peer.<id>").
    void mapPeer(uint32_t peerId, uint8_t slot) { meta("peer." + std::to_string(peerId), std::to_string(slot)); }

    // recording (IObserver)
    void netOut(uint32_t to, const uint8_t* data, size_t size, uint8_t channel = 0) override;
    void netIn(uint32_t from, const uint8_t* data, size_t size) override;
    void netOut(uint32_t to, const Bytes& data, uint8_t channel = 0) { netOut(to, data.data(), data.size(), channel); }
    void netIn(uint32_t from, const Bytes& data) { netIn(from, data.data(), data.size()); }
    void captured(const CapturedEvent& e) override;
    void applied(const AppliedEvent& e) override;
    void ownership(const OwnershipEvent& e) override;
    void digest(const DigestEvent& d, uint8_t flags = 0) override;
    void marker(uint16_t kind, const std::string& text) override;
    void custom(uint8_t type, const uint8_t* payload, size_t size) override;
    void custom(uint8_t type, const Bytes& payload) { custom(type, payload.data(), payload.size()); }

    // Every entry of the state a digest was taken from, as Digest records flagged kFlagDetail (same category, epoch, seq).
    void digestDetail(uint8_t category, uint32_t epoch, uint64_t seq, const std::vector<std::pair<uint64_t, uint32_t>>& entries);
    // A readable line for one entry, which the analyzer shows next to the value hashes: "entry <key as 16 hex digits> <value hash> <text>".
    void entryText(uint64_t key, uint32_t valueHash, const std::string& text);

    // dumping
    // Writes the ring to `path`. Returns false on I/O failure. Never throws.
    bool dump(const std::string& path, uint8_t reason = kDumpManual);
    // Writes <cfg.dir>/<label>-<reason>-<unixms>-<n>.cofr; returns the path or "" on failure (or when dir is empty).
    std::string dumpToDir(uint8_t reason = kDumpManual);
    // A desync was detected: marks the ring and, if a dir is configured, dumps (rate limited). Returns the path or "".
    std::string onDesync(const std::string& what);
    // Crash path: does not wait for the lock if another thread holds it. Best effort.
    std::string dumpCrash();

    struct Stats {
        uint64_t records = 0, dropped = 0, bytesRing = 0, dumps = 0, dumpFailures = 0, autoDumpsSkipped = 0;
    };
    Stats stats() const;
    const Config& config() const { return cfg_; }

private:
    void push(uint8_t type, uint8_t flags, const uint8_t* a, size_t an, const uint8_t* b = nullptr, size_t bn = 0);
    uint64_t nowUs() const;
    bool dumpImpl(const std::string& path, uint8_t reason, bool lock);

    Config cfg_;
    std::function<uint64_t()> clock_;
    uint64_t clock0_ = 0;
    uint64_t wallStartMs_ = 0;
    std::atomic<uint32_t> frame_{0};
    std::atomic<bool> enabled_{true};
    uint32_t session_ = 0, epoch_ = 0;
    std::vector<std::pair<std::string, std::string>> meta_;

    mutable std::mutex mu_;
    std::vector<uint8_t> ring_;
    size_t head_ = 0, used_ = 0;          // oldest entry offset, bytes in use
    uint64_t count_ = 0, dropped_ = 0, dumps_ = 0, dumpFailures_ = 0, autoSkipped_ = 0;
    uint64_t lastAutoDumpMs_ = 0;
    uint32_t autoDumps_ = 0;
    std::atomic<uint32_t> fileSeq_{0};
};

// ---- reader (tests and your own tools; the Python reader is tools/flight/cofr.py) ----

struct TraceHeader {
    uint16_t version = 0, headerSize = 0;
    uint8_t slot = 0, role = 0, reason = 0;
    uint32_t protoVersion = 0;
    uint64_t wallStartMs = 0, dumpTUs = 0, dropped = 0, recordCount = 0;
    uint32_t sessionId = 0, epoch = 0;
};
struct TraceRecord {
    uint8_t type = 0, flags = 0;
    uint32_t frame = 0;
    uint64_t tUs = 0;
    std::vector<uint8_t> payload;
};
struct Trace {
    TraceHeader header;
    std::vector<TraceRecord> records;
    std::string schemaJson;                     // text of the Schema record, empty if the file has none
};
bool readTrace(const std::string& path, Trace& out, std::string* err = nullptr);
bool parseTrace(const std::vector<uint8_t>& bytes, Trace& out, std::string* err = nullptr);

struct NetInfo {
    uint32_t peer = 0; uint8_t channel = 0, mux = 0, msgType = 0, decode = 0;   // decode: 0 decoded, 1 decoder refused or datagram cut, 2 no decoder
    uint32_t session = 0, epoch = 0; uint64_t seqLo = 0, seqHi = 0; uint32_t rawLen = 0;
    std::vector<uint8_t> raw;
};
bool parseNet(const TraceRecord& r, NetInfo& out);
bool parseDigest(const TraceRecord& r, DigestEvent& out);
bool parseOwnership(const TraceRecord& r, OwnershipEvent& out);
bool parseCaptured(const TraceRecord& r, CapturedEvent& out);
bool parseApplied(const TraceRecord& r, AppliedEvent& out);

}  // namespace cofr
