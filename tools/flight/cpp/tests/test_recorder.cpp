// Tests of the co-op flight recorder. Each real test has a mutation twin (group "twin"): the twin re-introduces one bug
// and must make the same expectations FAIL. Run: cofr_tests [--group g] [--name n] [--list]
//
// The tests use a toy protocol of their own (see toyMsg), so the recorder is exercised without any game.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <thread>
#include "cofr.h"
#include "minitest.h"

using namespace cofr;

namespace {

// ---- helpers ----

struct Rng {   // SplitMix64 as a random number generator
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t next() { s += 0x9E3779B97F4A7C15ull; uint64_t z = s; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
};

void put16(Bytes& b, uint32_t x) { b.push_back(x & 0xFF); b.push_back((x >> 8) & 0xFF); }
void put32(Bytes& b, uint32_t x) { put16(b, x & 0xFFFF); put16(b, x >> 16); }
void put64(Bytes& b, uint64_t x) { put32(b, static_cast<uint32_t>(x)); put32(b, static_cast<uint32_t>(x >> 32)); }
uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint64_t le64(const uint8_t* p) { return le32(p) | (static_cast<uint64_t>(le32(p + 4)) << 32); }

static bool g_quiet = false;
static bool fail(int line) { if (!g_quiet) std::printf("    check failed at test_recorder.cpp:%d\n", line); return false; }
struct Mut {
    explicit Mut(void (*set)(Mutations&)) { mutations() = Mutations(); if (set) set(mutations()); g_quiet = true; }
    ~Mut() { mutations() = Mutations(); g_quiet = false; }
};

// ---- a toy protocol: byte 0 = 0xC0, byte 1 = type, u32 session, u32 epoch, u64 seqLo, u64 seqHi, then a body ----

enum : uint8_t { kHello = 1, kWelcome = 2, kBatch = 4, kAck = 5, kDigestMsg = 13 };
constexpr size_t kToyHead = 26;

Bytes toyMsg(uint8_t type, uint32_t session, uint32_t epoch, uint64_t seqLo, uint64_t seqHi, const Bytes& body = Bytes()) {
    Bytes b;
    b.push_back(0xC0); b.push_back(type);
    put32(b, session); put32(b, epoch); put64(b, seqLo); put64(b, seqHi);
    b.insert(b.end(), body.begin(), body.end());
    return b;
}

bool toyDecode(const uint8_t* d, size_t n, NetMeta& m) {
    if (n < kToyHead || d[0] != 0xC0) return false;
    m.msgType = d[1]; m.session = le32(d + 2); m.epoch = le32(d + 6); m.seqLo = le64(d + 10); m.seqHi = le64(d + 18);
    if (d[1] == kDigestMsg) {   // the body is the sender's bucket hashes
        m.hasDigest = true;
        m.digest.category = 0; m.digest.epoch = m.epoch; m.digest.seq = m.seqLo;
        for (size_t i = kToyHead; i + 4 <= n; i += 4) m.digest.shards.push_back(le32(d + i));
        m.digest.itemCount = static_cast<uint32_t>(m.digest.shards.size());
        m.digest.hash = hash64(d + kToyHead, n - kToyHead);
    }
    return true;
}

const char* toySchemaJson() {
    return R"({"game":"toy","digest_categories":{"0":"world","1":"npcs"},)"
           R"("message_types":{"1":"hello","2":"welcome","4":"batch","5":"ack","13":"digest"},"txn_messages":[4,5],)"
           R"("marker_kinds":{"0":"note","1":"desync","2":"join","3":"leave","4":"test-step","5":"save","6":"load","7":"rebase","9":"checkpoint"},)"
           R"("custom_records":{"40":{"name":"ScriptWrite","fields":[{"name":"seq","type":"u64"},{"name":"key","type":"str16"},{"name":"value","type":"i32"}]}}})";
}

Config cfgFor(uint8_t slot, uint8_t role, size_t ring = 1u << 20, const char* label = "t", const std::string& schema = std::string()) {
    Config c; c.slot = slot; c.role = role; c.ringBytes = ring; c.label = label; c.schemaJson = schema; c.decoder = toyDecode;
    return c;
}

Trace dumpAndParse(Recorder& r, uint8_t reason = kDumpTest) {
    static int n = 0;
    std::string path = "cofr_tmp_" + std::to_string(++n) + ".cofr";
    Trace t;
    bool ok = r.dump(path, reason);
    REQUIRE(ok);
    std::string err;
    REQUIRE(readTrace(path, t, &err));
    std::remove(path.c_str());
    return t;
}

size_t countType(const Trace& t, uint8_t type) { size_t n = 0; for (auto& r : t.records) if (r.type == type) ++n; return n; }

std::vector<uint8_t> readFile(const std::string& p, bool* ok) {
    FILE* f = std::fopen(p.c_str(), "rb");
    *ok = f != nullptr;
    std::vector<uint8_t> b;
    if (!f) return b;
    uint8_t buf[65536]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
    std::fclose(f);
    return b;
}

// ---- a host and a client over an in-memory network, every datagram seen by a recorder ----

struct Packet { uint64_t at; uint32_t to, from; Bytes data; };

struct Machine {
    Recorder rec;
    Machine(const Config& c, uint64_t* clock) : rec(c) {
        rec.setClockSource([clock] { return *clock * 1000; });   // a simulator injects its own clock
        rec.setWallStart(1700000000000ull);
    }
};

uint64_t keyOf(const std::string& name) { return hash64(name.data(), name.size()); }

struct Sim {
    uint64_t now = 1000;
    std::vector<Packet> q;
    Machine h, c;
    BucketedState hw, cw;
    std::map<uint64_t, std::string> names;
    bool bug;
    std::vector<uint64_t> clientApplied;

    explicit Sim(bool bugged = false, const std::string& schema = std::string())
        : h(cfgFor(0, kRoleHost, 1u << 20, "host", schema), &now), c(cfgFor(1, kRoleClient, 1u << 20, "client", schema), &now), bug(bugged) {
        h.rec.mapPeer(1, 1);   // on the host, transport peer 1 is machine slot 1
        c.rec.mapPeer(0, 0);   // on the client, transport peer 0 is the host
    }

    void send(Machine& from, uint32_t fromId, uint32_t toId, Bytes d) {
        from.rec.netOut(toId, d);
        q.push_back({now + 5, toId, fromId, std::move(d)});
    }

    void deliver(const Packet& p) {
        Machine& m = p.to == 0 ? h : c;
        m.rec.netIn(p.from, p.data);
        uint8_t type = p.data[1];
        if (p.to == 0 && type == kHello) send(h, 0, 1, toyMsg(kWelcome, 1, 1, 0, 0));
        if (p.to == 1 && type == kBatch) {
            uint64_t seq = le64(p.data.data() + 10), key = le64(p.data.data() + kToyHead);
            uint32_t value = le32(p.data.data() + kToyHead + 8);
            cw.set(key, value);
            clientApplied.push_back(seq);
            AppliedEvent a; a.seq = seq; a.epoch = 1; a.origin = 0; a.index = 0; a.effectTag = 1; a.hash = effectHash(seq, key, value);
            c.rec.applied(a);
            if (bug && seq == 4) {   // a local script on the client overwrites a shared value; nothing else notices
                uint64_t k = keyOf("stage.q1");
                cw.set(k, 99);
                Bytes cp; put64(cp, 4); put16(cp, 8); for (char ch : std::string("stage.q1")) cp.push_back(static_cast<uint8_t>(ch)); put32(cp, 99);
                c.rec.custom(40, cp);
            }
            send(c, 1, 0, toyMsg(kAck, 1, 1, seq, seq));
        }
    }

    static uint64_t effectHash(uint64_t seq, uint64_t key, uint32_t value) {
        Bytes b; put64(b, seq); put64(b, key); put32(b, value);
        return hash64(b.data(), b.size());
    }

    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 5) {
            now += 5;
            h.rec.setFrame(static_cast<uint32_t>(now / 16)); c.rec.setFrame(static_cast<uint32_t>(now / 16));
            for (size_t i = 0; i < q.size();) {
                if (q[i].at <= now) { Packet p = std::move(q[i]); q.erase(q.begin() + static_cast<long>(i)); deliver(p); }
                else ++i;
            }
        }
    }

    void commit(uint64_t seq, const std::string& name, uint32_t value) {
        uint64_t key = keyOf(name);
        names[key] = name;
        hw.set(key, value);
        CapturedEvent ce; ce.kind = 0; ce.seq = seq; ce.effectCount = 1; ce.hash = effectHash(seq, key, value); ce.epoch = 1;
        h.rec.captured(ce);
        Bytes body; put64(body, key); put32(body, value);
        send(h, 0, 1, toyMsg(kBatch, 1, 1, seq, seq, body));
    }

    void digests(uint64_t seq, bool detail) {
        Machine* ms[2] = {&h, &c};
        BucketedState* ws[2] = {&hw, &cw};
        for (int i = 0; i < 2; ++i) {
            ms[i]->rec.digest(ws[i]->digest(0, 1, seq));
            if (!detail) continue;
            auto es = ws[i]->entries();
            ms[i]->rec.digestDetail(0, 1, seq, es);
            for (auto& e : es) ms[i]->rec.entryText(e.first, e.second, names[e.first] + " = " + std::to_string(e.second));
        }
    }

    void scenario() {
        send(c, 1, 0, toyMsg(kHello, 1, 0, 0, 0));
        run(200);
        const char* nm[] = {"", "stage.q1", "stage.q2", "stage.q3", "stage.q2", "stage.q4"};
        const uint32_t val[] = {0, 10, 20, 30, 40, 50};
        for (uint64_t i = 1; i <= 5; ++i) {
            commit(i, nm[i], val[i]);
            run(30);
            if (i == 2) { run(30); digests(2, false); }
        }
        run(300);
        digests(5, true);
        if (bug) c.rec.marker(kMarkDesync, "world digest differs from the host at seq 5");
    }
};

// ---- the checks shared by a test and its twin: each returns true iff the trace is right ----

bool checkSessionTrace() {
    Sim s;
    s.scenario();
    if (!(s.cw.entries() == s.hw.entries())) return false;
    Trace ht = dumpAndParse(s.h.rec), ct = dumpAndParse(s.c.rec);
    if (ht.header.slot != 0 || ht.header.role != kRoleHost || ct.header.slot != 1 || ct.header.role != kRoleClient) return false;
    uint64_t hostMax = 0, cliMax = 0, lastAck = 0;
    size_t hostIn = 0, cliIn = 0;
    for (auto& r : ht.records) {
        NetInfo n;
        if (r.type == kNetOut && parseNet(r, n) && n.msgType == kBatch) hostMax = std::max(hostMax, n.seqHi);
        if (r.type == kNetIn) ++hostIn;
    }
    for (auto& r : ct.records) {
        NetInfo n;
        if (r.type == kNetIn && parseNet(r, n)) { ++cliIn; if (n.msgType == kBatch) cliMax = std::max(cliMax, n.seqHi); }
        if (r.type == kNetOut && parseNet(r, n) && n.msgType == kAck) lastAck = std::max(lastAck, n.seqLo);
    }
    if (hostMax != 5 || cliMax != 5 || lastAck != 5 || hostIn == 0 || cliIn == 0) return false;
    // the client's applied records are exactly seq 1..5 in order
    std::vector<uint64_t> applied;
    for (auto& r : ct.records) { AppliedEvent a; if (parseApplied(r, a)) applied.push_back(a.seq); }
    if (applied != std::vector<uint64_t>{1, 2, 3, 4, 5}) return false;
    // the session field is decoded and equal to the one in the Hello
    for (auto& r : ct.records) { NetInfo n; if (r.type == kNetOut && parseNet(r, n) && n.msgType == kHello && n.session != 1) return false; }
    return true;
}

bool checkRingDrop() {
    Recorder r(cfgFor(0, kRoleSim, 4096));
    for (uint32_t i = 0; i < 200; ++i) r.marker(kMarkNote, "m" + std::to_string(i));
    Trace t = dumpAndParse(r);
    if (t.header.dropped == 0) return false;
    // the newest marker must be there, the first one gone, the order kept
    std::vector<std::string> texts;
    for (auto& rec : t.records) if (rec.type == kMarker) texts.emplace_back(rec.payload.begin() + 4, rec.payload.end());
    if (texts.empty() || texts.back() != "m199" || texts.front() == "m0") return false;
    for (size_t i = 1; i < texts.size(); ++i) {
        int a = std::atoi(texts[i - 1].c_str() + 1), b = std::atoi(texts[i].c_str() + 1);
        if (b != a + 1) return false;
    }
    return t.header.dropped + texts.size() == 200;
}

bool checkTruncation() {
    Config c = cfgFor(0, kRoleSim, 1u << 20); c.maxRawBytes = 100;
    Recorder r(c);
    Bytes big(5000, 0x5A); big[0] = 0xD1;
    r.netOut(3, big, 1);
    Trace t = dumpAndParse(r);
    for (auto& rec : t.records) if (rec.type == kNetOut) {
        NetInfo n;
        if (!parseNet(rec, n)) return false;
        return (rec.flags & kFlagTruncated) && n.rawLen == 5000 && n.raw.size() == 100 && n.mux == 0xD1 && n.decode == 1 && n.channel == 1 && n.peer == 3;
    }
    return false;
}

bool checkRecords() {
    Recorder r(cfgFor(2, kRoleClient));
    r.setSession(9, 4);
    r.meta("build", "x");
    OwnershipEvent o; o.op = kOwnTransfer; o.actor = 0xABCDEF0123ull; o.epoch = 7; o.fromOwner = 0; o.toOwner = 2; o.reason = 2; o.txnId = 99;
    r.ownership(o);
    DigestEvent d; d.category = 1; d.epoch = 4; d.seq = 12; d.hash = 0x1122334455667788ull; d.itemCount = 3;
    d.shards = {1, 2, 3}; d.keys = {{10, 100}, {11, 101}};
    r.digest(d);
    AppliedEvent a; a.seq = 12; a.epoch = 4; a.origin = 1; a.index = 2; a.flags = kApplyRepair | kApplyCatchUp; a.cause = 3; a.effectTag = 5; a.hash = 0xFEEDull;
    r.applied(a);
    CapturedEvent ce; ce.kind = 1; ce.intentId = 5; ce.effectCount = 2; ce.cause = 6; ce.epoch = 4;
    r.captured(ce);
    r.marker(kMarkSave, "saved");
    Trace t = dumpAndParse(r);
    if (t.header.sessionId != 9 || t.header.epoch != 4 || t.header.slot != 2) return false;
    if (t.records.empty() || t.records[0].type != kMeta) return false;
    bool sawO = false, sawD = false, sawA = false, sawC = false, sawM = false;
    for (auto& rec : t.records) {
        OwnershipEvent o2; DigestEvent d2; AppliedEvent a2; CapturedEvent c2;
        if (parseOwnership(rec, o2)) sawO = o2.op == kOwnTransfer && o2.actor == o.actor && o2.epoch == 7 && o2.fromOwner == 0 && o2.toOwner == 2 && o2.reason == 2 && o2.txnId == 99;
        if (parseDigest(rec, d2)) sawD = d2.category == 1 && d2.seq == 12 && d2.hash == d.hash && d2.itemCount == 3 && d2.shards == d.shards && d2.keys == d.keys && !d2.bucketed;
        if (parseApplied(rec, a2)) sawA = rec.payload.size() == 28 && a2.seq == 12 && a2.index == 2 && a2.flags == (kApplyRepair | kApplyCatchUp) && a2.cause == 3 && a2.effectTag == 5 && a2.hash == 0xFEEDull;
        if (parseCaptured(rec, c2)) sawC = c2.kind == 1 && c2.intentId == 5 && c2.effectCount == 2 && c2.cause == 6 && c2.epoch == 4;
        if (rec.type == kMarker) sawM = rec.payload[0] == kMarkSave;
    }
    return sawO && sawD && sawA && sawC && sawM;
}

bool checkDerivedDigest() {
    Recorder r(cfgFor(0, kRoleHost));
    Bytes shards;
    for (int i = 0; i < 128; ++i) put32(shards, i == 5 ? 0xDEAD : 0);
    r.netIn(1, toyMsg(kDigestMsg, 4, 2, 77, 77, shards));
    Trace t = dumpAndParse(r);
    bool net = false, dig = false;
    for (auto& rec : t.records) {
        NetInfo n; DigestEvent d;
        if (rec.type == kNetIn && parseNet(rec, n)) net = n.msgType == kDigestMsg && n.seqLo == 77 && n.epoch == 2 && n.session == 4 && n.decode == 0;
        if (rec.type == kDigest && parseDigest(rec, d)) dig = (rec.flags & kFlagDerived) && d.seq == 77 && d.shards.size() == 128 && d.shards[5] == 0xDEAD && d.epoch == 2;
    }
    return net && dig;
}

bool checkDroppedHeader() {
    Recorder r(cfgFor(0, kRoleSim, 4096));
    for (int i = 0; i < 300; ++i) r.marker(kMarkNote, "xxxxxxxxxxxxxxxxxxxx");
    Trace t = dumpAndParse(r);
    return t.header.dropped > 0 && r.stats().dropped == t.header.dropped;
}

bool checkReceivedRecorded(bool wantNetIn) {
    Recorder r(cfgFor(0, kRoleSim));
    r.netIn(5, toyMsg(kAck, 1, 3, 42, 42));
    Trace t = dumpAndParse(r);
    return (countType(t, kNetIn) == 1) == wantNetIn;
}

// A schema the game writes is in the file, and so is a default one when the game gives none.
bool checkSchemaInFile() {
    Recorder withSchema(cfgFor(0, kRoleSim, 1u << 20, "t", toySchemaJson()));
    Trace a = dumpAndParse(withSchema);
    Recorder none(cfgFor(0, kRoleSim));
    Trace b = dumpAndParse(none);
    return a.schemaJson == toySchemaJson() && b.schemaJson == defaultSchemaJson() && countType(a, kSchema) == 1;
}

// A game-defined record survives with its payload.
bool checkCustomRecord() {
    Recorder r(cfgFor(0, kRoleSim, 1u << 20, "t", toySchemaJson()));
    Bytes p; put64(p, 4); put16(p, 3); p.push_back('a'); p.push_back('b'); p.push_back('c'); put32(p, 0xFFFFFFFFu);
    r.custom(40, p);
    r.custom(7, p);   // below 32: reserved for the core types, ignored
    Trace t = dumpAndParse(r);
    size_t n = 0;
    for (auto& rec : t.records) if (rec.type == 40) { ++n; if (rec.payload != p) return false; }
    return n == 1 && countType(t, kDigest) == 0;
}

}  // namespace

TEST(recorder, session_trace_matches_the_wire) { CHECK(checkSessionTrace()); }
TEST(recorder, ring_drops_oldest_and_counts) { CHECK(checkRingDrop()); }
TEST(recorder, oversize_datagram_is_cut_and_flagged) { CHECK(checkTruncation()); }
TEST(recorder, record_types_round_trip) { CHECK(checkRecords()); }
TEST(recorder, decoder_digest_yields_a_derived_digest_record) { CHECK(checkDerivedDigest()); }
TEST(recorder, header_reports_dropped) { CHECK(checkDroppedHeader()); }
TEST(recorder, received_datagrams_are_recorded) { CHECK(checkReceivedRecorded(true)); }
TEST(recorder, schema_is_written_into_the_file) { CHECK(checkSchemaInFile()); }
TEST(recorder, custom_records_keep_their_payload) { CHECK(checkCustomRecord()); }

TEST(recorder, without_a_decoder_datagrams_are_kept_raw) {
    Config c; c.label = "t";
    Recorder r(c);
    r.netOut(2, Bytes{0xC0, 1, 2, 3});
    Trace t = dumpAndParse(r);
    for (auto& rec : t.records) if (rec.type == kNetOut) {
        NetInfo n; REQUIRE(parseNet(rec, n));
        CHECK(n.decode == 2 && n.msgType == 0 && n.mux == 0xC0 && n.raw == (Bytes{0xC0, 1, 2, 3}) && n.rawLen == 4);
    }
}

TEST(recorder, meta_carries_label_and_peer_map) {
    Config c = cfgFor(1, kRoleClient); c.label = "my label";
    Recorder r(c);
    r.meta("build", "7"); r.meta("build", "8"); r.mapPeer(0, 2);
    Trace t = dumpAndParse(r);
    REQUIRE(!t.records.empty() && t.records[0].type == kMeta);
    const auto& p = t.records[0].payload;
    std::string all(p.begin(), p.end());
    CHECK(all.find("label") != std::string::npos && all.find("my label") != std::string::npos);
    CHECK(all.find("peer.0") != std::string::npos);
    CHECK(all.find("build") != std::string::npos && all.find(std::string("\x01\x00" "8", 3)) != std::string::npos && all.find(std::string("\x01\x00" "7", 3)) == std::string::npos);
}

TEST(recorder, disabled_recorder_records_nothing) {
    Recorder r(cfgFor(0, kRoleSim));
    r.setEnabled(false);
    r.marker(kMarkNote, "x"); r.netOut(1, Bytes{1, 2, 3}); r.netIn(1, Bytes{1}); r.custom(40, Bytes{1});
    Trace t = dumpAndParse(r);
    CHECK(t.records.size() == 2);   // only the Meta and Schema records
}

TEST(recorder, observer_base_class_is_a_null_observer) {
    IObserver nothing;
    nothing.netOut(1, nullptr, 0); nothing.netIn(1, nullptr, 0); nothing.marker(0, "x"); nothing.applied(AppliedEvent()); nothing.setFrame(3);
    Recorder r(cfgFor(0, kRoleSim));
    IObserver& o = r;   // hooks only know the interface
    o.marker(kMarkNote, "via interface"); o.setFrame(12); o.netOut(1, reinterpret_cast<const uint8_t*>("ab"), 2);
    Trace t = dumpAndParse(r);
    CHECK(countType(t, kMarker) == 1 && countType(t, kNetOut) == 1);
    CHECK(r.frame() == 12);
}

TEST(recorder, cut_short_file_parses_up_to_the_last_full_record) {
    Recorder r(cfgFor(0, kRoleSim));
    for (int i = 0; i < 10; ++i) r.marker(kMarkNote, "abc");
    r.dump("cofr_cut.cofr");
    Trace full; REQUIRE(readTrace("cofr_cut.cofr", full));
    bool ok; std::vector<uint8_t> b = readFile("cofr_cut.cofr", &ok);
    std::remove("cofr_cut.cofr");
    REQUIRE(ok);
    b.resize(b.size() - 3);
    Trace cut; REQUIRE(parseTrace(b, cut));
    CHECK(cut.records.size() + 1 == full.records.size());
    std::vector<uint8_t> bad(b.begin(), b.begin() + 10);
    Trace junk; CHECK(!parseTrace(bad, junk));
}

TEST(recorder, unknown_files_are_rejected) {
    std::vector<uint8_t> b(64, 0);
    Trace t; std::string err;
    CHECK(!parseTrace(b, t, &err));
    b[0] = 'C'; b[1] = 'O'; b[2] = 'F'; b[3] = 'R'; b[4] = 9;   // a future version
    CHECK(!parseTrace(b, t, &err));
    b[0] = 'A'; b[1] = 'B'; b[2] = 'C'; b[3] = 'D'; b[4] = 1;   // another magic
    CHECK(!parseTrace(b, t, &err));
}

TEST(recorder, dump_to_dir_and_desync_rate_limit) {
    Config c = cfgFor(1, kRoleClient); c.dir = "cofr_out_dir"; c.autoDumpMax = 2; c.autoDumpMinIntervalMs = 0;
    Recorder r(c);
    r.marker(kMarkNote, "hello");
    std::string p1 = r.onDesync("bucket 3"), p2 = r.onDesync("bucket 4"), p3 = r.onDesync("bucket 5");
    CHECK(!p1.empty()); CHECK(!p2.empty()); CHECK(p3.empty());
    CHECK(r.stats().autoDumpsSkipped == 1);
    Trace t; REQUIRE(readTrace(p1, t));
    CHECK(t.header.reason == kDumpDesync);
    CHECK(countType(t, kMarker) == 2);   // the note and the desync marker
    CHECK(p1.size() > 5 && p1.substr(p1.size() - 5) == ".cofr");
    std::string crash = r.dumpCrash();
    CHECK(!crash.empty());
    std::remove(p1.c_str()); std::remove(p2.c_str()); std::remove(crash.c_str());
    std::remove("cofr_out_dir");
}

TEST(recorder, threads_can_record_concurrently) {
    Recorder r(cfgFor(0, kRoleSim, 1u << 22));
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t) ts.emplace_back([&r, t] { for (int i = 0; i < 2000; ++i) r.marker(kMarkNote, "t" + std::to_string(t)); });
    for (auto& th : ts) th.join();
    Trace t = dumpAndParse(r);
    CHECK(countType(t, kMarker) == 8000);
}

TEST(recorder, wrap_around_keeps_big_records_intact) {
    Recorder r(cfgFor(0, kRoleSim, 10000));
    Bytes shards(128 * 4, 7);
    for (int i = 0; i < 100; ++i) r.netIn(1, toyMsg(kDigestMsg, 1, 1, static_cast<uint64_t>(i + 1), static_cast<uint64_t>(i + 1), shards));
    Trace t = dumpAndParse(r);
    size_t good = 0; uint64_t last = 0;
    for (auto& rec : t.records) { NetInfo n; if (rec.type == kNetIn && parseNet(rec, n)) { CHECK(n.decode == 0); CHECK(n.seqLo == last + 1 || last == 0); last = n.seqLo; ++good; } }
    CHECK(good >= 5);
    CHECK(last == 100);
}

// ---- bucketed digests: a changed value must be pinned to its bucket, then to its key ----

static std::vector<std::pair<uint64_t, uint32_t>> makeEntries(size_t n, uint64_t seed = 42) {
    Rng rng(seed);
    std::vector<std::pair<uint64_t, uint32_t>> v;
    for (size_t i = 0; i < n; ++i) { uint64_t k = rng.next(); v.emplace_back(k, rng.u32()); }
    return v;
}
static std::map<uint64_t, uint32_t> detailOf(const Trace& t) {
    std::map<uint64_t, uint32_t> m;
    for (auto& r : t.records) { DigestEvent d; if (r.type == kDigest && (r.flags & kFlagDetail) && parseDigest(r, d)) for (auto& kv : d.keys) m[kv.first] = kv.second; }
    return m;
}
bool checkPinpoint() {
    auto a = makeEntries(9000);
    auto b = a;
    b.back().second ^= 0x1234;   // one value differs on the other machine (the LAST entry: also catches a detail list that loses its tail)
    const uint64_t badKey = a.back().first;
    DigestEvent da = bucketedDigest(0, 3, 0, a), db = bucketedDigest(0, 3, 0, b);
    if (da.shards.size() != kDefaultBuckets || da.hash == db.hash || !da.bucketed) return fail(__LINE__);
    std::vector<size_t> diffB;
    for (size_t i = 0; i < kDefaultBuckets; ++i) if (da.shards[i] != db.shards[i]) diffB.push_back(i);
    if (diffB.size() != 1 || diffB[0] != badKey % kDefaultBuckets) return fail(__LINE__);
    Recorder ra(cfgFor(0, kRoleHost, 8u << 20)), rb(cfgFor(1, kRoleClient, 8u << 20));
    ra.digest(da); ra.digestDetail(0, 3, 0, a);
    rb.digest(db); rb.digestDetail(0, 3, 0, b);
    Trace ta = dumpAndParse(ra), tb = dumpAndParse(rb);
    std::map<uint64_t, uint32_t> ma = detailOf(ta), mb = detailOf(tb);
    if (ma.size() != 9000) return fail(__LINE__);   // all entries survived the 4000-per-record split
    std::vector<uint64_t> diffK;
    for (auto& kv : ma) { auto it = mb.find(kv.first); if (it == mb.end() || it->second != kv.second) diffK.push_back(kv.first); }
    for (auto& kv : mb) if (!ma.count(kv.first)) diffK.push_back(kv.first);
    return diffK.size() == 1 && diffK[0] == badKey ? true : fail(__LINE__);
}
TEST(recorder, one_changed_value_is_pinned_to_its_bucket_and_key) { CHECK(checkPinpoint()); }
TEST(recorder, bucket_digest_ignores_entry_order) {
    auto a = makeEntries(2000);
    auto b = a;
    std::reverse(b.begin(), b.end());
    CHECK(bucketedDigest(0, 1, 5, a).shards == bucketedDigest(0, 1, 5, b).shards);
    CHECK(bucketedDigest(0, 1, 5, a).hash == bucketedDigest(0, 1, 5, b).hash);
    b.pop_back();
    CHECK(bucketedDigest(0, 1, 5, a).hash != bucketedDigest(0, 1, 5, b).hash);
}
TEST(recorder, bucket_count_is_configurable) {
    auto a = makeEntries(500);
    DigestEvent d = bucketedDigest(0, 1, 5, a, 64);
    CHECK(d.shards.size() == 64);
    std::vector<uint32_t> sums(64, 0);
    for (auto& e : a) sums[e.first % 64] += bucketMix(e.first, e.second);
    CHECK(sums == d.shards);
    BucketedState st(64);
    for (auto& e : a) st.set(e.first, e.second);
    CHECK(st.digest(0, 1, 5).shards == d.shards);
}

// BucketedState keeps the digest up to date one entry at a time; after any sequence of sets and erases it must equal a full walk.
static bool checkIncremental() {
    BucketedState st;
    std::map<uint64_t, uint32_t> truth;
    Rng rng(77);
    for (int i = 0; i < 4000; ++i) {
        uint64_t key = rng.next() % 300;   // few keys, so most sets replace a value
        if (rng.next() % 4 == 0) { st.erase(key); truth.erase(key); }
        else { uint32_t v = rng.u32(); st.set(key, v); truth[key] = v; }
        if (rng.next() % 7 == 0) {
            std::vector<std::pair<uint64_t, uint32_t>> es(truth.begin(), truth.end());
            DigestEvent full = bucketedDigest(0, 1, i, es), inc = st.digest(0, 1, i);
            if (full.shards != inc.shards || full.hash != inc.hash || full.itemCount != inc.itemCount) return false;
        }
    }
    return st.size() == truth.size() && st.entries() == std::vector<std::pair<uint64_t, uint32_t>>(truth.begin(), truth.end());
}
TEST(recorder, incremental_digest_equals_full_walk_after_random_updates) { CHECK(checkIncremental()); }

TEST(perf, bucketed_digest_of_20000_entries) {
    auto a = makeEntries(20000);
    auto t0 = std::chrono::steady_clock::now();
    DigestEvent d;
    for (int i = 0; i < 20; ++i) d = bucketedDigest(0, 1, 5, a);
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 20;
    std::printf("    bucketedDigest(20000 entries): %.0f us (only at quiet points, never per frame)\n", us);
    CHECK(us < 5000);
}

TEST(perf, incremental_digest_after_one_change_in_a_big_state) {
    BucketedState st;
    auto a = makeEntries(200000);
    for (auto& e : a) st.set(e.first, e.second);
    const int reps = 200;
    DigestEvent d;
    for (int r = 0; r < 20; ++r) { st.set(a[static_cast<size_t>(r)].first, 7u); d = st.digest(0, 1, 7); }   // warm up the caches and the pages
    std::vector<double> times;
    for (int r = 0; r < reps; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        st.set(a[static_cast<size_t>(r)].first, 1000u + static_cast<uint32_t>(r));
        d = st.digest(0, 1, 7);
        times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
    }
    for (int r = 0; r < reps; ++r) a[static_cast<size_t>(r)].second = 1000u + static_cast<uint32_t>(r);
    std::vector<double> fullTimes;
    DigestEvent full;
    for (int i = 0; i < 5; ++i) {
        auto t1 = std::chrono::steady_clock::now();
        full = bucketedDigest(0, 1, 7, a);
        fullTimes.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count());
    }
    std::sort(times.begin(), times.end()); std::sort(fullTimes.begin(), fullTimes.end());
    double medianUs = times[times.size() / 2], fullMs = fullTimes[fullTimes.size() / 2];
    std::printf("    one change + digest in a 200000-entry state: median %.1f us, worst %.1f us; a full pass costs %.2f ms (median of 5)\n", medianUs, times.back(), fullMs);
    CHECK(full.shards == d.shards && full.hash == d.hash);
    CHECK(medianUs < 200.0);   // the median, because one scheduler hiccup on a busy PC must not fail the test
}

// ---- golden files: deterministic traces the Python tests read; also a drift detector for the format ----

static std::vector<uint8_t> goldenBytes(bool host, bool desync) {
    Sim s(desync, desync ? toySchemaJson() : std::string());
    s.scenario();
    Recorder& r = host ? s.h.rec : s.c.rec;
    if (!desync) {
        OwnershipEvent o; o.op = kOwnGrant; o.actor = 0x1001; o.epoch = 1; o.toOwner = 0; o.reason = 1; o.txnId = 3;
        r.ownership(o);
        r.marker(kMarkTestStep, "golden");
    }
    std::string p = "cofr_golden_tmp.cofr";
    REQUIRE(r.dump(p, kDumpTest));
    bool ok; std::vector<uint8_t> b = readFile(p, &ok);
    std::remove(p.c_str());
    REQUIRE(ok);
    return b;
}
static const char* kGolden[4] = {"sample_host.cofr", "sample_client.cofr", "desync_host.cofr", "desync_client.cofr"};
static bool goldenMatches() {
    const char* dir = std::getenv("COFR_GOLDEN_DIR");
    if (!dir) return true;   // not configured (run through ctest or run_tests.py)
    for (int i = 0; i < 4; ++i) {
        std::string p = std::string(dir) + "/" + kGolden[i];
        std::vector<uint8_t> a = goldenBytes(i % 2 == 0, i >= 2);
        if (std::getenv("COFR_WRITE_GOLDEN")) { FILE* f = std::fopen(p.c_str(), "wb"); if (f) { std::fwrite(a.data(), 1, a.size(), f); std::fclose(f); } continue; }
        bool ok; std::vector<uint8_t> b = readFile(p, &ok);
        if (!ok || a != b) return false;
    }
    return true;
}
TEST(golden, samples_are_deterministic_and_unchanged) {
    for (int i = 0; i < 4; ++i) CHECK(goldenBytes(i % 2 == 0, i >= 2) == goldenBytes(i % 2 == 0, i >= 2));
    CHECK(goldenMatches());
}

// A trace that uses every record type and option, for tools/flight/run_tests.py: the Python reader must read it field by field.
static bool buildEverything(const std::string& path) {
    Config c = cfgFor(2, kRoleClient, 4u << 20, "everything", toySchemaJson());
    c.maxRawBytes = 1000; c.protoVersion = 7;   // the 5000-byte datagram below is cut, the 538-byte digest message is not
    Recorder r(c);
    uint64_t clock = 5000;
    r.setClockSource([&clock] { return clock; });
    r.setWallStart(1700000000123ull);
    r.setSession(9, 4);
    r.meta("build", "x"); r.mapPeer(0, 0);
    r.setFrame(3); clock = 5100;
    Bytes big(5000, 0x5A); big[0] = 0xD1;
    r.netOut(3, big, 1);
    r.setFrame(4); clock = 5200;
    Bytes body; put64(body, 0x1111); put32(body, 5);
    r.netIn(0, toyMsg(kBatch, 4, 2, 7, 9, body));
    Bytes shards;
    for (int i = 0; i < 128; ++i) put32(shards, i == 5 ? 0xDEAD : 0);
    r.netIn(0, toyMsg(kDigestMsg, 4, 2, 77, 77, shards));
    clock = 5300;
    CapturedEvent ce; ce.kind = 1; ce.intentId = 5; ce.seq = 0; ce.effectCount = 2; ce.hash = 0xAABBCCDDEEFF0011ull; ce.cause = 3; ce.epoch = 4;
    r.captured(ce);
    AppliedEvent a; a.seq = 12; a.epoch = 4; a.origin = 1; a.index = 2; a.flags = kApplyRepair | kApplyCatchUp; a.cause = 3; a.effectTag = 12; a.hash = 0x1122334455667788ull;
    r.applied(a);
    OwnershipEvent o; o.op = kOwnTransfer; o.actor = 0xABCDEF0123ull; o.epoch = 7; o.fromOwner = 0; o.toOwner = 2; o.reason = 2; o.txnId = 99;
    r.ownership(o);
    DigestEvent d; d.category = 1; d.epoch = 4; d.seq = 12; d.hash = 0x1122334455667788ull; d.itemCount = 3; d.shards = {1, 2, 3}; d.keys = {{10, 100}, {11, 101}};
    r.digest(d);
    auto entries = makeEntries(5000);
    r.digest(bucketedDigest(0, 4, 13, entries));
    r.digestDetail(0, 4, 13, entries);
    r.entryText(entries[0].first, entries[0].second, "first entry");
    clock = 5400;
    Bytes cp; put64(cp, 4); put16(cp, 8); for (char ch : std::string("stage.q1")) cp.push_back(static_cast<uint8_t>(ch)); put32(cp, 0xFFFFFFFEu);   // value -2
    r.custom(40, cp);
    r.custom(99, Bytes{9, 9});
    r.marker(kMarkSave, std::string("saved \xC3\xA9"));
    return r.dump(path, kDumpDesync);
}
TEST(golden, emit_fresh_traces_for_the_python_reader) {
    const char* dir = std::getenv("COFR_EMIT_DIR");
    if (!dir) return;
    for (int i = 0; i < 4; ++i) {
        std::vector<uint8_t> b = goldenBytes(i % 2 == 0, i >= 2);
        std::string p = std::string(dir) + "/fresh_" + kGolden[i];
        FILE* f = std::fopen(p.c_str(), "wb");
        REQUIRE(f != nullptr);
        std::fwrite(b.data(), 1, b.size(), f);
        std::fclose(f);
    }
    CHECK(buildEverything(std::string(dir) + "/everything.cofr"));
}

// The other direction: a file written by the Python writer must parse here.
TEST(golden, reads_the_trace_written_by_python) {
    const char* dir = std::getenv("COFR_EMIT_DIR");
    if (!dir) return;
    Trace t; std::string err;
    REQUIRE(readTrace(std::string(dir) + "/from_python.cofr", t, &err));
    CHECK(t.header.slot == 3 && t.header.role == kRoleClient && t.header.reason == kDumpCrash && t.header.protoVersion == 5 && t.header.sessionId == 8 && t.header.epoch == 2);
    CHECK(t.header.recordCount == t.records.size());
    bool net = false, app = false, dig = false, mark = false;
    for (auto& rec : t.records) {
        NetInfo n; AppliedEvent a; DigestEvent d;
        if (parseNet(rec, n)) net = n.peer == 2 && n.msgType == 4 && n.seqLo == 5 && n.seqHi == 6 && n.raw == (Bytes{1, 2, 3}) && n.decode == 0;
        if (parseApplied(rec, a)) app = a.seq == 5 && a.index == 1 && a.hash == 0xFFFFFFFFFFFFFFFFull && a.flags == kApplyBaseline;
        if (parseDigest(rec, d)) dig = d.bucketed && d.shards == (std::vector<uint32_t>{7, 8}) && d.keys.size() == 1 && d.keys[0].first == 0x8000000000000000ull;
        if (rec.type == kMarker) mark = rec.payload.size() > 4;
    }
    CHECK(net && app && dig && mark);
    CHECK(t.schemaJson.find("\"game\"") != std::string::npos);
}

// ---- performance: recording overhead per frame must stay far below 0.1 ms ----

static double perFrameUs(int frames, bool busy) {
    Recorder r(cfgFor(0, kRoleHost, 16u << 20));
    Bytes ack = toyMsg(kAck, 1, 1, 5, 5);
    Bytes dgram(200, 0xFC);
    AppliedEvent e; e.seq = 1; e.effectTag = 3; e.hash = 99;
    auto t0 = std::chrono::steady_clock::now();
    for (int f = 0; f < frames; ++f) {
        r.setFrame(static_cast<uint32_t>(f));
        if (busy) {
            for (int i = 0; i < 20; ++i) { r.netOut(1, dgram); r.netIn(1, dgram); }
            for (int i = 0; i < 10; ++i) r.applied(e);
        } else if (f % 30 == 0) {   // idle: roughly an ack pair every half second at 60 fps
            r.netOut(1, ack); r.netIn(1, ack);
        }
        if (f % 120 == 0) { DigestEvent d; d.hash = static_cast<uint64_t>(f); r.digest(d); }
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / frames;
}
TEST(perf, idle_frame_overhead_is_far_below_0_1_ms) {
    double idle = perFrameUs(200000, false);
    double busy = perFrameUs(20000, true);
    std::printf("    recorder overhead: idle %.3f us/frame, busy (40 datagrams + 10 effects) %.3f us/frame (budget 100 us)\n", idle, busy);
    CHECK(idle < 100.0);
    CHECK(idle < 5.0);
    CHECK(busy < 100.0);
}

// ---- mutation twins: every injected bug must make the matching check fail ----

TEST(twin, drop_newest) { Mut m([](Mutations& f) { f.dropNewest = true; }); CHECK(!checkRingDrop()); }
TEST(twin, no_net_in) { Mut m([](Mutations& f) { f.noNetIn = true; }); CHECK(!checkReceivedRecorded(true)); CHECK(!checkSessionTrace()); }
TEST(twin, zero_seq) { Mut m([](Mutations& f) { f.zeroSeq = true; }); CHECK(!checkSessionTrace()); CHECK(!checkDerivedDigest()); }
TEST(twin, no_trunc_flag) { Mut m([](Mutations& f) { f.noTruncFlag = true; }); CHECK(!checkTruncation()); }
TEST(twin, no_dropped_count) { Mut m([](Mutations& f) { f.noDroppedCount = true; }); CHECK(!checkDroppedHeader()); CHECK(!checkRingDrop()); }
TEST(twin, no_applied_flags) { Mut m([](Mutations& f) { f.noAppliedFlags = true; }); CHECK(!checkRecords()); }
TEST(twin, incremental_skips_update) { Mut m([](Mutations& f) { f.incrementalSkipsUpdate = true; }); CHECK(!checkIncremental()); }
TEST(twin, buckets_ignore_value) { Mut m([](Mutations& f) { f.bucketsIgnoreValue = true; }); CHECK(!checkPinpoint()); CHECK(!checkIncremental()); }
TEST(twin, detail_drops_key) { Mut m([](Mutations& f) { f.detailDropsKey = true; }); CHECK(!checkPinpoint()); }
TEST(twin, no_derived_digest) { Mut m([](Mutations& f) { f.noDerivedDigest = true; }); CHECK(!checkDerivedDigest()); }
TEST(twin, no_schema_record) { Mut m([](Mutations& f) { f.noSchemaRecord = true; }); CHECK(!checkSchemaInFile()); }
TEST(twin, custom_cuts_payload) { Mut m([](Mutations& f) { f.customCutsPayload = true; }); CHECK(!checkCustomRecord()); }

MINITEST_MAIN()
