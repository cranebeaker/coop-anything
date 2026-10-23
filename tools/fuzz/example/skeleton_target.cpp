// Skeleton target: the smallest complete example of wrapping a sync core in the ITarget interface.
// Read this first, then GUIDE.md. example_target.cpp is the full-featured one.
//
// Part 1 is a stand-in for YOUR sync core: a tiny host-authoritative position sync for 2 to 4 players. It knows nothing about the
// fuzzer. It talks to the world only through a send callback, onPacket(), tick() and a few accessors, which is the shape your real core
// needs to have (see GUIDE.md, "What your core must allow").
// Part 2 is the adapter: it owns N cores, connects them with the built-in LossyLink, advances a virtual clock, maps ops onto the core,
// reports what each machine applied, and answers the questions the oracles ask.
#include <algorithm>
#include <functional>
#include <set>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {
namespace {

// ====================================================================== part 1: the sync core being tested
uint32_t sum32(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}
void put32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
uint32_t get32(const Bytes& b, size_t at) { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(b[at + static_cast<size_t>(i)]) << (8 * i); return v; }

struct Pos { int x = 0, y = 0; };

// Applied-message report: the core calls this every time it applies or rejects a message. Real cores need one such hook.
using ReportFn = std::function<void(int machine, uint64_t idem, int player, uint32_t seq, int origin, bool accepted, const char* why)>;

class PosCore {
public:
    // Packets: 1 = client position (seq, x, y), 2 = ack of a position (seq), 3 = table (version, positions of all players), 4 = ack of a table.
    PosCore(int id, int players, bool dedupeTables, std::function<void(int, Bytes)> send, ReportFn report)
        : id_(id), n_(players), dedupe_(dedupeTables), send_(std::move(send)), report_(std::move(report)), pos_(static_cast<size_t>(players)),
          lastSeq_(static_cast<size_t>(players), 0), tableAcked_(static_cast<size_t>(players), 0) {}

    void setLocalPosition(int x, int y) {   // the game thread says: my player moved
        pos_[static_cast<size_t>(id_)] = {x, y};
        if (id_ == 0) { ++version_; return; }
        ++localSeq_;
    }
    void tick() {                           // called every 50 ms of virtual time
        if (id_ != 0) {
            if (ackedSeq_ < localSeq_) {
                Bytes b; b.push_back(1); put32(b, localSeq_); put32(b, static_cast<uint32_t>(pos_[static_cast<size_t>(id_)].x)); put32(b, static_cast<uint32_t>(pos_[static_cast<size_t>(id_)].y));
                send(0, b);
            }
            return;
        }
        for (int c = 1; c < n_; ++c) {
            if (tableAcked_[static_cast<size_t>(c)] >= version_) continue;
            Bytes b; b.push_back(3); put32(b, version_);
            for (auto& p : pos_) { put32(b, static_cast<uint32_t>(p.x)); put32(b, static_cast<uint32_t>(p.y)); }
            send(c, b);
        }
    }
    void onPacket(int from, const Bytes& d) {
        if (d.size() < 5 || sum32(d.data(), d.size() - 4) != get32(d, d.size() - 4)) return;   // corrupted: drop, the sender will resend
        size_t body = d.size() - 4;
        switch (d[0]) {
            case 1: if (id_ == 0 && body >= 13) onClientPos(from, get32(d, 1), static_cast<int>(get32(d, 5)), static_cast<int>(get32(d, 9))); break;
            case 2: if (id_ != 0 && body >= 5) ackedSeq_ = std::max(ackedSeq_, get32(d, 1)); break;
            case 3: if (id_ != 0 && body >= 5 + static_cast<size_t>(n_) * 8) onTable(d); break;
            case 4: if (id_ == 0 && body >= 5) tableAcked_[static_cast<size_t>(from)] = std::max(tableAcked_[static_cast<size_t>(from)], get32(d, 1)); break;
            default: break;
        }
    }
    bool settledWith(const PosCore& host) const { return id_ == 0 || (ackedSeq_ == localSeq_ && myVersion_ == host.version_); }
    uint32_t version() const { return version_; }
    uint32_t tableAcked(int c) const { return tableAcked_[static_cast<size_t>(c)]; }
    Pos position(int p) const { return pos_[static_cast<size_t>(p)]; }

private:
    void send(int to, Bytes b) { put32(b, sum32(b.data(), b.size())); send_(to, std::move(b)); }
    void onClientPos(int from, uint32_t seq, int x, int y) {
        uint64_t idem = (1ull << 62) | (static_cast<uint64_t>(from) << 32) | seq;
        if (seq <= lastSeq_[static_cast<size_t>(from)]) report_(0, idem, from, seq, from, false, "old sequence");
        else {
            lastSeq_[static_cast<size_t>(from)] = seq;
            pos_[static_cast<size_t>(from)] = {x, y};
            ++version_;
            report_(0, idem, from, seq, from, true, "");
        }
        Bytes a; a.push_back(2); put32(a, lastSeq_[static_cast<size_t>(from)]);   // always ack, so the client stops resending
        send(from, a);
    }
    void onTable(const Bytes& d) {
        uint32_t version = get32(d, 1);
        if (dedupe_ && version <= myVersion_) {
            report_(id_, (2ull << 62) | (static_cast<uint64_t>(version) << 8), -1, version, 0, false, "old table");
        } else {
            for (int p = 0; p < n_; ++p) {
                if (p == id_) continue;   // this machine owns its own position and never takes it back from the host
                pos_[static_cast<size_t>(p)] = {static_cast<int>(get32(d, 5 + static_cast<size_t>(p) * 8)), static_cast<int>(get32(d, 9 + static_cast<size_t>(p) * 8))};
                report_(id_, (2ull << 62) | (static_cast<uint64_t>(version) << 8) | static_cast<uint64_t>(p), p, version, 0, true, "");
            }
            myVersion_ = std::max(myVersion_, version);   // the bug twin keeps the dedupe check off, so an old table is applied again
        }
        Bytes a; a.push_back(4); put32(a, myVersion_);
        send(0, a);
    }

    int id_, n_;
    bool dedupe_;
    std::function<void(int, Bytes)> send_;
    ReportFn report_;
    std::vector<Pos> pos_;
    uint32_t localSeq_ = 0, ackedSeq_ = 0, myVersion_ = 0, version_ = 0;
    std::vector<uint32_t> lastSeq_, tableAcked_;
};

// ====================================================================== part 2: the adapter the fuzzer drives
class SkeletonTarget : public TargetBase {
public:
    SkeletonTarget(const Scenario& s, bool noDedupe) : net_(s.machines, s.seed * 77 + 3), n_(s.machines) {
        LinkParams link = s.link;
        if (!s.profile.empty()) linkProfile(s.profile, link);   // this skeleton ignores SetLink ops, so the start link is the whole chaos
        net_.setLink(link);
        for (int m = 0; m < n_; ++m) {
            cores_.emplace_back(new PosCore(
                m, n_, !noDedupe, [this, m](int to, Bytes b) { net_.send(m, to, std::move(b)); },
                [this](int machine, uint64_t idem, int player, uint32_t seq, int origin, bool ok, const char* why) {
                    // ONE report per applied or rejected message. The oracles read these.
                    logApply(machine, idem, player < 0 ? -1 : static_cast<int>(0x100 + static_cast<uint32_t>(player)), 0, seq, origin, ApplyKind::PlayerPos, ok, 0, why);
                }));
        }
    }

    uint64_t now() const override { return now_; }
    int machines() const override { return n_; }
    bool connected(int) const override { return true; }   // this skeleton has no join or leave
    uint64_t repairs(int) const override { return 0; }    // this skeleton has no repair path, so it cannot repair
    void apply(const Op& op) override {
        if (op.kind != OpKind::Move) return;               // everything else is outside this core: ignore it
        int m = std::max(0, std::min(n_ - 1, op.m));
        ++counters_["op_move"];
        cores_[static_cast<size_t>(m)]->setLocalPosition(std::max(0, std::min(1000, op.a)), std::max(0, std::min(1000, op.b)));
    }
    void advance(uint64_t toMs) override {
        while (now_ < toMs) {
            now_ = std::min<uint64_t>(now_ + 10, toMs);
            net_.advance(now_);
            for (int m = 0; m < n_; ++m) { int from = 0; Bytes d; while (net_.poll(m, from, d)) cores_[static_cast<size_t>(m)]->onPacket(from, d); }
            while (now_ >= next50_) { next50_ += 50; for (auto& c : cores_) c->tick(); }
        }
    }
    void heal() override { frozen_ = true; net_.setLink(LinkParams()); }   // perfect link; the fuzzer sends no more ops after this
    bool quiescent() const override {
        if (!frozen_ || !net_.idle()) return false;
        for (int m = 1; m < n_; ++m)
            if (!cores_[static_cast<size_t>(m)]->settledWith(*cores_[0]) || cores_[0]->tableAcked(m) != cores_[0]->version()) return false;
        return true;
    }
    std::vector<Thing> view(int m) const override {   // everything machine m believes about the world, sorted by net id
        std::vector<Thing> v;
        for (int p = 0; p < n_; ++p) {
            Thing t; t.netId = 0x100 + static_cast<uint32_t>(p); t.type = 0; t.hp = 100;
            Pos q = cores_[static_cast<size_t>(m)]->position(p);
            t.x = q.x; t.y = q.y;
            v.push_back(t);
        }
        return v;
    }
    uint64_t digest(int m, int cat) const override {   // equal on all machines when they agree
        uint64_t h = 1469598103934665603ull + static_cast<uint64_t>(cat);
        for (auto& t : view(m)) { h ^= static_cast<uint64_t>(t.x) * 1000003u + static_cast<uint64_t>(t.y); h *= 1099511628211ull; }
        return h;
    }

private:
    LossyLink net_;
    int n_;
    bool frozen_ = false;
    uint64_t now_ = 0, next50_ = 50;
    std::vector<std::unique_ptr<PosCore>> cores_;
};

std::unique_ptr<ITarget> makeSkeleton(const Scenario& s, std::string* err) {
    bool noDedupe = false;
    for (auto& b : s.bugs) {
        if (b == "no_dedupe") noDedupe = true;
        else { if (err) *err = "unknown bug class for target skeleton: " + b; return nullptr; }
    }
    return std::make_unique<SkeletonTarget>(s, noDedupe);
}

const TargetRegistrar kRegisterSkeleton(TargetInfo{"skeleton", "smallest complete target: host-authoritative player positions, 1 bug class", {"no_dedupe"}, makeSkeleton});

}  // namespace
}  // namespace coopfuzz
