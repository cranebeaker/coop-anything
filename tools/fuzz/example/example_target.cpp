// Example fuzz target: a small, self-contained host-authoritative world (players, actors with area ownership and epochs,
// ground items, doors, encounter sites, host save and load) over the built-in lossy link, with its own reliable streams,
// idempotency, digests and repair. It needs no game and no sync core. It shows how a target is built, and every bug
// class the oracles hunt can be switched on in it (the mutation twins, see BUG-CLASSES.md).
//
// Protocol (one host = machine 0, clients 1..N-1):
//   host -> client  Batch of events (reliable stream, seq 1.., go-back-N resend, cumulative Ack)
//   client -> host  Batch of intents (same stream scheme; idempotent by (client, seq))
//   owner -> host   Upd: actor position at (epoch, useq), unreliable, de-duplicated by packet id
//   host -> client  Digest (per category, only compared once the client is caught up to the digest's event seq)
//   client -> host  Repair request -> host appends a full Reset event
//
// A real target replaces all of this with calls into your own sync core. See GUIDE.md.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <set>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {
namespace {

// ---------------------------------------------------------------- the bug switches (mutation twins)
struct Bugs {
    bool staleEpoch = false;       // owner / epoch checks skipped when taking actor updates and events
    bool staleSeq = false;         // per-(actor, epoch) update sequence ignored
    bool echoAdopted = false;      // a machine applies the echo of its own update
    bool noIdempotency = false;    // duplicate packets / intents / events are applied again
    bool twoOwners = false;        // transfer does not bump the ownership epoch
    bool spawnDup = false;         // an encounter site can spawn more than once
    bool pickupDup = false;        // the loser of a loot race also gets a copy
    bool handoffRespawn = false;   // a transfer re-creates the actor under a new net id and leaves the old one
    bool digestUnstable = false;   // client digests fold positions differently than the host's (repairs never settle)
    bool noRetransmit = false;     // reliable streams send each item once
    bool noWorldEpoch = false;     // requests from before a host load are not rejected
    bool staleAssignmentEcho = false;   // an owner re-applies the state snapshot of an old assignment, resurrects an actor, then "rediscovers" it as new
};
const char* const kBugNames[] = {"stale_epoch_accepted", "stale_seq_accepted", "echo_adopted", "no_idempotency", "two_owners", "spawn_dup",
                                 "pickup_dup", "handoff_respawn", "digest_unstable", "no_retransmit", "no_world_epoch", "stale_assignment_echo"};
bool setBug(Bugs& b, const std::string& n) {
    bool* f[] = {&b.staleEpoch, &b.staleSeq, &b.echoAdopted, &b.noIdempotency, &b.twoOwners, &b.spawnDup, &b.pickupDup, &b.handoffRespawn,
                 &b.digestUnstable, &b.noRetransmit, &b.noWorldEpoch, &b.staleAssignmentEcho};
    for (size_t i = 0; i < sizeof kBugNames / sizeof *kBugNames; ++i)
        if (n == kBugNames[i]) { *f[i] = true; return true; }
    return false;
}

// ---------------------------------------------------------------- bytes
struct W {
    Bytes b;
    void u8(uint8_t v) { b.push_back(v); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void u64(uint64_t v) { u32(static_cast<uint32_t>(v)); u32(static_cast<uint32_t>(v >> 32)); }
};
struct R {
    const uint8_t* p; size_t n; size_t i = 0; bool ok = true;
    R(const uint8_t* d, size_t len) : p(d), n(len) {}
    uint8_t u8() { if (i + 1 > n) { ok = false; return 0; } return p[i++]; }
    uint32_t u32() { if (i + 4 > n) { ok = false; return 0; } uint32_t v = 0; for (int k = 0; k < 4; ++k) v |= static_cast<uint32_t>(p[i + static_cast<size_t>(k)]) << (8 * k); i += 4; return v; }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    uint64_t u64() { uint64_t lo = u32(); uint64_t hi = u32(); return lo | (hi << 32); }
};
uint32_t fnv32(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}
void putThing(W& w, const Thing& t) {
    w.u32(t.netId); w.u8(t.type); w.u32(t.ident); w.i32(t.x); w.i32(t.y); w.i32(t.cell); w.i32(t.hp); w.u8(t.dead); w.i32(t.owner);
    w.u32(t.epoch); w.i32(t.holder); w.u8(t.open);
}
Thing getThing(R& r) {
    Thing t;
    t.netId = r.u32(); t.type = r.u8(); t.ident = r.u32(); t.x = r.i32(); t.y = r.i32(); t.cell = r.i32(); t.hp = r.i32(); t.dead = r.u8() != 0;
    t.owner = r.i32(); t.epoch = r.u32(); t.holder = r.i32(); t.open = r.u8() != 0;
    return t;
}

enum EvType : uint8_t { E_Spawn = 1, E_Remove, E_Pos, E_Hp, E_Owner, E_Holder, E_Door, E_Reset, E_PlayerPos };
struct Ev {
    uint8_t type = 0;
    uint32_t id = 0;
    int32_t x = 0, y = 0, cell = 0, v = 0, owner = -1, origin = -1;
    uint8_t flag = 0;
    uint32_t epoch = 0, useq = 0, worldEpoch = 0;
    Thing t;
    std::vector<Thing> things;
};
Bytes encEv(const Ev& e) {
    W w;
    w.u8(e.type); w.u32(e.id); w.i32(e.x); w.i32(e.y); w.i32(e.cell); w.i32(e.v); w.i32(e.owner); w.i32(e.origin); w.u8(e.flag);
    w.u32(e.epoch); w.u32(e.useq); w.u32(e.worldEpoch);
    if (e.type == E_Spawn) putThing(w, e.t);
    if (e.type == E_Reset) { w.u32(static_cast<uint32_t>(e.things.size())); for (auto& t : e.things) putThing(w, t); }
    return w.b;
}
bool decEv(const Bytes& b, Ev& e) {
    R r(b.data(), b.size());
    e.type = r.u8(); e.id = r.u32(); e.x = r.i32(); e.y = r.i32(); e.cell = r.i32(); e.v = r.i32(); e.owner = r.i32(); e.origin = r.i32(); e.flag = r.u8();
    e.epoch = r.u32(); e.useq = r.u32(); e.worldEpoch = r.u32();
    if (e.type == E_Spawn) e.t = getThing(r);
    if (e.type == E_Reset) { uint32_t n = r.u32(); if (n > 4096) return false; for (uint32_t i = 0; i < n && r.ok; ++i) e.things.push_back(getThing(r)); }
    return r.ok;
}

enum IntentKind : uint8_t { I_Move = 1, I_Hit, I_Pickup, I_Drop, I_Spawn, I_Activate, I_Rediscover };
struct Intent {
    uint8_t kind = 0;
    uint32_t worldEpoch = 0;
    int32_t a = 0, b = 0, c = 0, d = 0;
};
Bytes encIntent(const Intent& i) { W w; w.u8(i.kind); w.u32(i.worldEpoch); w.i32(i.a); w.i32(i.b); w.i32(i.c); w.i32(i.d); return w.b; }
bool decIntent(const Bytes& b, Intent& i) {
    R r(b.data(), b.size());
    i.kind = r.u8(); i.worldEpoch = r.u32(); i.a = r.i32(); i.b = r.i32(); i.c = r.i32(); i.d = r.i32();
    return r.ok;
}

enum PktType : uint8_t { P_Batch = 1, P_Ack, P_Upd, P_Digest, P_Repair };
constexpr uint32_t kAvatarBase = 0x100, kItemBase = 0x200, kDoorBase = 0x300, kDynBase = 0x1000;

class Example : public TargetBase {
public:
    Example(const Scenario& s, const Bugs& b) : bugs_(b), net_(s.machines, s.seed * 31 + 7), n_(s.machines), link_(s.link) {
        if (!s.profile.empty()) { linkProfile(s.profile, link_); profileOwnsLink_ = true; }
        net_.setLink(link_);
        mach_.resize(static_cast<size_t>(n_));
        for (int m = 0; m < n_; ++m) {
            mach_[static_cast<size_t>(m)].id = m;
            mach_[static_cast<size_t>(m)].rng.reseed(s.seed * 1000003ull + static_cast<uint64_t>(m));
            up_.push_back(true);
            buildInitialWorld(mach_[static_cast<size_t>(m)].w);
            trimAvatars(mach_[static_cast<size_t>(m)].w);
            ack_[m] = sentTo_[m] = 0;
            intentExpected_[m] = 1;
        }
        for (int m = 0; m < n_; ++m) { lastPos_[m] = {100 * (m + 1), 100, 0}; mach_[static_cast<size_t>(m)].myPos = {100 * (m + 1), 100, 0}; }
    }

    // ------------------------------------------------------------ ITarget
    uint64_t now() const override { return now_; }
    int machines() const override { return n_; }
    bool connected(int m) const override { return m >= 0 && m < n_ && up_[static_cast<size_t>(m)]; }
    uint64_t repairs(int m) const override { return mach_[static_cast<size_t>(m)].repairs; }
    std::map<std::string, uint64_t> stats() const override {
        std::map<std::string, uint64_t> s = counters_;
        s["net_sent"] = net_.stats().sent; s["net_lost"] = net_.stats().lost; s["net_dup"] = net_.stats().duplicated; s["net_corrupt"] = net_.stats().corrupted;
        s["events"] = evLog_.size();
        return s;
    }
    std::vector<Thing> view(int m) const override {
        std::vector<Thing> v;
        for (auto& kv : mach_[static_cast<size_t>(m)].w) v.push_back(kv.second);
        return v;
    }
    uint64_t digest(int m, int cat) const override { return hashWorld(mach_[static_cast<size_t>(m)], cat, -1, false); }

    bool quiescent() const override {
        if (!frozen_ || !net_.idle()) return false;
        for (int c = 1; c < n_; ++c) {
            if (!up_[static_cast<size_t>(c)]) return false;
            const Mach& m = mach_[static_cast<size_t>(c)];
            if (ack_.at(c) != evLog_.size() || m.evExpected != evLog_.size() + 1) return false;
            if (m.intentAcked != m.intents.size() || intentExpected_.at(c) != m.intents.size() + 1) return false;
        }
        return true;
    }

    void heal() override {
        frozen_ = true;
        net_.setLink(LinkParams());
        for (int m = 1; m < n_; ++m) if (!up_[static_cast<size_t>(m)]) doJoin(m);
    }

    void advance(uint64_t toMs) override {
        while (now_ < toMs) {
            now_ = std::min<uint64_t>(now_ + 10, toMs);
            net_.advance(now_);
            pollAll();
            while (now_ >= next50_) { next50_ += 50; pumpStreams(); }
            while (now_ >= next200_) { next200_ += 200; ownershipTick(); aiTick(); }
            while (now_ >= next2000_) { next2000_ += 2000; sendDigests(); }
        }
    }

    void apply(const Op& op) override {
        ++counters_[std::string("op_") + opName(op.kind)];
        int m = op.m;
        if (m < 0 || m >= n_) m = 0;
        Mach& me = mach_[static_cast<size_t>(m)];
        switch (op.kind) {
            case OpKind::Join: if (m != 0) doJoin(m); return;
            case OpKind::Leave: if (m != 0) doLeave(m); return;
            case OpKind::Save: saved_ = hostWorldCopy(); hasSave_ = true; return;
            case OpKind::Load: if (hasSave_) doLoad(); return;
            case OpKind::SetLink: {
                if (profileOwnsLink_) return;   // a named profile owns the link
                link_.minLatencyMs = static_cast<uint32_t>(op.a); link_.maxLatencyMs = std::max<uint32_t>(static_cast<uint32_t>(op.b), link_.minLatencyMs);
                link_.loss = op.c / 1000.0; link_.duplicate = op.d / 1000.0;
                if (!frozen_) net_.setLink(link_);
                return;
            }
            case OpKind::Stale: doStale(op); return;
            default: break;
        }
        if (!up_[static_cast<size_t>(m)]) return;
        Intent in;
        in.worldEpoch = me.worldEpoch;
        switch (op.kind) {
            case OpKind::Move: {
                in.kind = I_Move; in.a = clampi(op.a, 0, 1000); in.b = clampi(op.b, 0, 1000); in.c = clampi(op.c, 0, 2);
                auto it = me.w.find(kAvatarBase + static_cast<uint32_t>(m));
                me.myPos = {in.a, in.b, in.c};
                if (it != me.w.end()) { it->second.x = in.a; it->second.y = in.b; it->second.cell = in.c; }   // local prediction
                break;
            }
            case OpKind::Hit: {
                std::vector<uint32_t> ids = idsOf(me, [](const Thing& t) { return t.type == 1 && !t.dead; });
                if (ids.empty()) return;
                in.kind = I_Hit; in.a = static_cast<int32_t>(ids[static_cast<size_t>(op.a) % ids.size()]); in.b = clampi(op.b, 1, 100);
                if (m != 0) {   // local prediction of the damage; the host's Hp event corrects it
                    Thing& t = me.w[static_cast<uint32_t>(in.a)];
                    t.hp -= in.b;
                    if (t.hp <= 0) { t.hp = 0; t.dead = true; }
                }
                break;
            }
            case OpKind::Pickup: {
                std::vector<uint32_t> ids = idsOf(me, [](const Thing& t) { return t.type == 2 && t.holder == -1; });
                if (ids.empty()) return;
                in.kind = I_Pickup; in.a = static_cast<int32_t>(ids[static_cast<size_t>(op.a) % ids.size()]);
                break;
            }
            case OpKind::Drop: {
                std::vector<uint32_t> ids = idsOf(me, [m](const Thing& t) { return t.type == 2 && t.holder == m; });
                if (ids.empty()) return;
                in.kind = I_Drop; in.a = static_cast<int32_t>(ids[static_cast<size_t>(op.a) % ids.size()]); in.b = clampi(op.b, 0, 1000); in.c = clampi(op.c, 0, 1000); in.d = clampi(op.d, 0, 2);
                break;
            }
            case OpKind::Spawn: in.kind = I_Spawn; in.a = ((op.a % 5) + 5) % 5; break;
            case OpKind::Activate: in.kind = I_Activate; in.a = static_cast<int32_t>(kDoorBase + static_cast<uint32_t>(((op.a % 4) + 4) % 4)); break;
            default: return;
        }
        submit(m, in);
    }

private:
    struct Mach {
        int id = 0;
        std::map<uint32_t, Thing> w;
        uint32_t worldEpoch = 0;
        std::vector<Bytes> intents;            // client intent stream (the host machine uses hostSeq_ and applies directly)
        uint32_t intentAcked = 0, intentSentTo = 0, evExpected = 1;
        std::map<uint32_t, std::pair<uint32_t, uint32_t>> useq;   // actor -> (epoch, last update seq)
        uint32_t pktCtr = 0;
        std::array<int, 3> myPos{{0, 0, 0}};   // where this player last put their own avatar (survives a Remove/Spawn replay)
        Rng rng;
        uint64_t repairs = 0;
        std::deque<std::array<uint32_t, 6>> updHist;   // netId, epoch, useq, x, y, cell
        std::map<int, std::deque<Bytes>> ring;         // last raw packets sent to a peer
    };

    static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
    template <class F> std::vector<uint32_t> idsOf(const Mach& m, F f) const {
        std::vector<uint32_t> ids;
        for (auto& kv : m.w) if (f(kv.second)) ids.push_back(kv.first);
        return ids;
    }
    static void buildInitialWorld(std::map<uint32_t, Thing>& w) {
        for (int m = 0; m < 4; ++m) {
            Thing t; t.netId = kAvatarBase + static_cast<uint32_t>(m); t.type = 0; t.x = 100 * (m + 1); t.y = 100; t.cell = 0; t.hp = 100;
            w[t.netId] = t;
        }
        for (int i = 0; i < 6; ++i) {
            Thing t; t.netId = kItemBase + static_cast<uint32_t>(i); t.type = 2; t.ident = 1000 + static_cast<uint32_t>(i); t.x = 100 + 90 * i; t.y = 500; t.cell = i % 3;
            w[t.netId] = t;
        }
        for (int i = 0; i < 4; ++i) {
            Thing t; t.netId = kDoorBase + static_cast<uint32_t>(i); t.type = 3; t.x = 50 * i; t.y = 900; t.cell = i % 3;
            w[t.netId] = t;
        }
    }
    // Only the first n_ avatars belong in a world with n_ machines.
    void trimAvatars(std::map<uint32_t, Thing>& w) const {
        for (int m = n_; m < 4; ++m) w.erase(kAvatarBase + static_cast<uint32_t>(m));
    }

    struct SavedWorld { std::map<uint32_t, Thing> w; std::set<int> sites; };
    SavedWorld hostWorldCopy() const { return {mach_[0].w, siteSpawned_}; }

    // ------------------------------------------------------------ packets
    void sendPkt(int from, int to, uint8_t type, const Bytes& body) {
        Bytes raw;
        raw.push_back(type);
        raw.insert(raw.end(), body.begin(), body.end());
        uint32_t h = fnv32(raw.data(), raw.size());
        for (int i = 0; i < 4; ++i) raw.push_back(static_cast<uint8_t>(h >> (8 * i)));
        auto& ring = mach_[static_cast<size_t>(from)].ring[to];
        ring.push_back(raw);
        if (ring.size() > 32) ring.pop_front();
        net_.send(from, to, raw);
    }
    void pollAll() {
        for (int m = 0; m < n_; ++m) {
            int from = 0; Bytes data;
            while (net_.poll(m, from, data)) handle(m, from, data);
        }
    }
    void handle(int m, int from, const Bytes& d) {
        if (d.size() < 5) return;
        uint32_t want = fnv32(d.data(), d.size() - 4);
        R tail(d.data() + d.size() - 4, 4);
        if (tail.u32() != want) { ++counters_["corrupt_dropped"]; return; }
        R r(d.data() + 1, d.size() - 5);
        uint8_t type = d[0];
        switch (type) {
            case P_Batch: onBatch(m, from, r); break;
            case P_Ack: {
                uint32_t cum = r.u32();
                if (!r.ok) return;
                if (m == 0) ack_[from] = std::max(ack_[from], std::min<uint32_t>(cum, static_cast<uint32_t>(evLog_.size())));
                else mach_[static_cast<size_t>(m)].intentAcked = std::max(mach_[static_cast<size_t>(m)].intentAcked, std::min<uint32_t>(cum, static_cast<uint32_t>(mach_[static_cast<size_t>(m)].intents.size())));
                break;
            }
            case P_Upd: {
                if (m != 0) return;
                uint32_t pkt = r.u32(), id = r.u32(), epoch = r.u32(), useq = r.u32();
                int32_t x = r.i32(), y = r.i32(), cell = r.i32();
                if (r.ok) hostUpdate(from, pkt, id, epoch, useq, x, y, cell);
                break;
            }
            case P_Digest: {
                if (m == 0) return;
                uint32_t evSeq = r.u32();
                uint64_t h[kDigestCats];
                for (auto& x : h) x = r.u64();
                if (r.ok) onDigest(m, evSeq, h);
                break;
            }
            case P_Repair: if (m == 0) hostRepair(); break;
            default: break;
        }
    }

    void onBatch(int m, int from, R& r) {
        uint32_t count = r.u32();
        if (!r.ok || count > 64) return;
        bool toHost = m == 0;
        uint32_t& expected = toHost ? intentExpected_[from] : mach_[static_cast<size_t>(m)].evExpected;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t seq = r.u32(), len = r.u32();
            if (!r.ok || r.i + len > r.n) return;
            Bytes item(r.p + r.i, r.p + r.i + len);
            r.i += len;
            bool fresh = seq == expected;
            bool dup = seq < expected;
            if (!fresh && !(dup && bugs_.noIdempotency)) continue;
            if (fresh) ++expected;
            if (toHost) { Intent in; if (decIntent(item, in)) hostIntent(from, seq, in); }
            else { Ev ev; if (decEv(item, ev)) clientApply(mach_[static_cast<size_t>(m)], ev, seq); }
        }
        W w; w.u32(expected - 1);
        sendPkt(m, from, P_Ack, w.b);
    }

    void pumpStreams() {
        for (int c = 1; c < n_; ++c) {
            if (!up_[static_cast<size_t>(c)]) continue;
            // host -> client events
            {
                uint32_t from = bugs_.noRetransmit ? sentTo_[c] : ack_[c];
                uint32_t total = static_cast<uint32_t>(evLog_.size());
                if (from < total) {
                    uint32_t cnt = std::min<uint32_t>(24, total - from);
                    W w; w.u32(cnt);
                    for (uint32_t i = 0; i < cnt; ++i) { const Bytes& e = evLog_[from + i]; w.u32(from + i + 1); w.u32(static_cast<uint32_t>(e.size())); w.b.insert(w.b.end(), e.begin(), e.end()); }
                    sendPkt(0, c, P_Batch, w.b);
                    sentTo_[c] = from + cnt;
                }
            }
            // client -> host intents
            {
                Mach& m = mach_[static_cast<size_t>(c)];
                uint32_t from = bugs_.noRetransmit ? m.intentSentTo : m.intentAcked;
                uint32_t total = static_cast<uint32_t>(m.intents.size());
                if (from < total) {
                    uint32_t cnt = std::min<uint32_t>(16, total - from);
                    W w; w.u32(cnt);
                    for (uint32_t i = 0; i < cnt; ++i) { const Bytes& e = m.intents[from + i]; w.u32(from + i + 1); w.u32(static_cast<uint32_t>(e.size())); w.b.insert(w.b.end(), e.begin(), e.end()); }
                    sendPkt(c, 0, P_Batch, w.b);
                    m.intentSentTo = from + cnt;
                }
            }
        }
    }

    // ------------------------------------------------------------ host side
    void emit(const Ev& e) {
        // Remember the last position the stream carried for each actor (the host's own world may already be ahead of it).
        if (e.type == E_Spawn) pub_[e.t.netId] = {e.t.x, e.t.y, e.t.cell};
        else if (e.type == E_Pos || e.type == E_Owner) pub_[e.id] = {e.x, e.y, e.cell};
        else if (e.type == E_Reset) for (auto& t : e.things) pub_[t.netId] = {t.x, t.y, t.cell};
        evLog_.push_back(encEv(e));
    }

    void submit(int m, const Intent& in) {
        if (m == 0) { hostIntent(0, ++hostSeq_, in); return; }
        mach_[static_cast<size_t>(m)].intents.push_back(encIntent(in));
    }

    void hostIntent(int c, uint32_t seq, const Intent& in) {
        Mach& H = mach_[0];
        uint64_t idem = (static_cast<uint64_t>(c) << 32) | seq;
        auto rec = [&](bool acc, int ref, const char* why) { logApply(0, idem, ref, 0, seq, c, ApplyKind::Intent, acc, in.worldEpoch, why); };
        if (!bugs_.noWorldEpoch && in.worldEpoch != H.worldEpoch) { rec(false, in.a, "stale world epoch"); return; }
        switch (in.kind) {
            case I_Move: {
                auto it = H.w.find(kAvatarBase + static_cast<uint32_t>(c));
                if (it == H.w.end() || !up_[static_cast<size_t>(c)]) { rec(false, in.a, "not joined"); return; }
                it->second.x = in.a; it->second.y = in.b; it->second.cell = in.c;
                lastPos_[c] = {in.a, in.b, in.c};
                Ev e; e.type = E_PlayerPos; e.id = it->first; e.x = in.a; e.y = in.b; e.cell = in.c; e.origin = c; emit(e);
                rec(true, static_cast<int>(it->first), "");
                return;
            }
            case I_Hit: {
                auto it = H.w.find(static_cast<uint32_t>(in.a));
                if (it == H.w.end() || it->second.type != 1 || it->second.dead) { rec(false, in.a, "gone"); return; }
                it->second.hp -= in.b;
                if (it->second.hp <= 0) { it->second.hp = 0; it->second.dead = true; ++counters_["deaths"]; }
                Ev e; e.type = E_Hp; e.id = it->first; e.v = it->second.hp; e.flag = it->second.dead; emit(e);
                rec(true, in.a, "");
                return;
            }
            case I_Pickup: {
                auto it = H.w.find(static_cast<uint32_t>(in.a));
                if (it == H.w.end() || it->second.type != 2) { rec(false, in.a, "gone"); return; }
                if (it->second.holder == -1) {
                    it->second.holder = c;
                    Ev e; e.type = E_Holder; e.id = it->first; e.v = c; e.x = it->second.x; e.y = it->second.y; e.cell = it->second.cell; emit(e);
                    rec(true, in.a, "");
                } else if (bugs_.pickupDup && it->second.holder != c) {
                    Thing cl = it->second; cl.netId = nextNet_++; cl.holder = c;
                    H.w[cl.netId] = cl;
                    Ev e; e.type = E_Spawn; e.id = cl.netId; e.t = cl; emit(e);
                    rec(true, in.a, "loot race loser also served");
                } else { ++counters_["loot_races_lost"]; rec(false, in.a, "contested"); }
                return;
            }
            case I_Drop: {
                auto it = H.w.find(static_cast<uint32_t>(in.a));
                if (it == H.w.end() || it->second.type != 2 || it->second.holder != c) { rec(false, in.a, "not held"); return; }
                it->second.holder = -1; it->second.x = in.b; it->second.y = in.c; it->second.cell = in.d;
                Ev e; e.type = E_Holder; e.id = it->first; e.v = -1; e.x = in.b; e.y = in.c; e.cell = in.d; emit(e);
                rec(true, in.a, "");
                return;
            }
            case I_Spawn: {
                int site = in.a;
                if (siteSpawned_.count(site) && !bugs_.spawnDup) { ++counters_["spawn_races_lost"]; rec(false, site, "already spawned"); return; }
                siteSpawned_.insert(site);
                ++counters_["spawns"];
                for (int k = 0; k < 3; ++k) {
                    Thing t; t.netId = nextNet_++; t.x = 200 * site + 100 + k * 20; t.y = 300; t.cell = site % 3;
                    if (k < 2) { t.type = 1; t.ident = static_cast<uint32_t>(site * 16 + 1 + k); t.hp = 100; }
                    else { t.type = 2; t.ident = static_cast<uint32_t>(5000 + site * 16); }
                    H.w[t.netId] = t;
                    Ev e; e.type = E_Spawn; e.id = t.netId; e.t = t; emit(e);
                }
                rec(true, site, "");
                return;
            }
            case I_Activate: {
                auto it = H.w.find(static_cast<uint32_t>(in.a));
                if (it == H.w.end() || it->second.type != 3) { rec(false, in.a, "gone"); return; }
                it->second.open = !it->second.open;
                Ev e; e.type = E_Door; e.id = it->first; e.flag = it->second.open; emit(e);
                rec(true, in.a, "");
                return;
            }
            case I_Rediscover: {
                // A client says it found this actor as a new one (the owner dropped its reference and met the actor again).
                auto it = H.w.find(static_cast<uint32_t>(in.a));
                if (it == H.w.end() || it->second.type != 1) { rec(false, in.a, "gone"); return; }
                Thing cl = it->second; cl.netId = nextNet_++; cl.owner = -1; cl.epoch = 0;
                H.w[cl.netId] = cl;
                ++counters_["rediscovered"];
                Ev e; e.type = E_Spawn; e.id = cl.netId; e.t = cl; emit(e);
                rec(true, in.a, "rediscovered as new");
                return;
            }
            default: rec(false, in.a, "bad intent"); return;
        }
    }

    void hostUpdate(int from, uint32_t pkt, uint32_t id, uint32_t epoch, uint32_t useq, int32_t x, int32_t y, int32_t cell) {
        Mach& H = mach_[0];
        uint64_t idem = (2ull << 62) | (static_cast<uint64_t>(from) << 32) | pkt;
        auto rec = [&](bool acc, const char* why) { logApply(0, idem, static_cast<int>(id), epoch, useq, from, ApplyKind::Pos, acc, H.worldEpoch, why); };
        if (!bugs_.noIdempotency) {
            if (!seenPkt_[from].insert(pkt).second) { ++counters_["dup_packets_dropped"]; return; }
        }
        auto it = H.w.find(id);
        if (it == H.w.end() || it->second.type != 1) { rec(false, "gone"); return; }
        Thing& t = it->second;
        if (t.dead) { rec(false, "dead"); return; }
        if (!bugs_.staleEpoch) {
            if (t.owner != from) { rec(false, "stale: not the owner"); return; }
            if (epoch != t.epoch) { rec(false, "stale epoch"); return; }
        }
        if (!bugs_.staleSeq) {
            auto ls = lastUseq_.find(id);
            if (ls != lastUseq_.end() && ls->second.first == epoch && useq <= ls->second.second) { rec(false, "stale update sequence"); return; }
        }
        auto& slot = lastUseq_[id];
        if (slot.first != epoch) slot = {epoch, 0};
        slot.second = std::max(slot.second, useq);
        auto pb = pub_.find(id);
        bool changed = pb == pub_.end() || pb->second[0] != x || pb->second[1] != y || pb->second[2] != cell;
        t.x = x; t.y = y; t.cell = cell;
        if (changed) { Ev e; e.type = E_Pos; e.id = id; e.x = x; e.y = y; e.cell = cell; e.epoch = epoch; e.useq = useq; e.origin = from; emit(e); }
        rec(true, "");
    }

    void setOwner(Thing& t, int newOwner, OwnEvent::Kind kind, bool bumpEpoch) {
        int prev = t.owner;
        if (bumpEpoch) t.epoch = ++epochCtr_;
        t.owner = newOwner;
        lastUseq_.erase(t.netId);
        lastXfer_[t.netId] = now_;
        logOwn(kind, static_cast<int>(t.netId), newOwner, prev, t.epoch);
        // The assignment carries a snapshot of the actor (v = hp, flag = dead). A correct client does not take state from it.
        Ev e; e.type = E_Owner; e.id = t.netId; e.owner = newOwner; e.epoch = t.epoch; e.x = t.x; e.y = t.y; e.cell = t.cell; e.v = t.hp; e.flag = t.dead; emit(e);
        ++counters_[kind == OwnEvent::Grant ? "grants" : kind == OwnEvent::Transfer ? "transfers" : "releases"];
    }

    void ownershipTick() {
        Mach& H = mach_[0];
        std::vector<uint32_t> ids = idsOf(H, [](const Thing& t) { return t.type == 1; });
        for (uint32_t id : ids) {
            Thing& t = H.w[id];
            auto dist = [&](int m) {
                auto a = H.w.find(kAvatarBase + static_cast<uint32_t>(m));
                int d = std::abs(a->second.x - t.x) + std::abs(a->second.y - t.y);
                return m == 0 ? d - 150 : d;
            };
            int best = -1;
            bool curOk = false;
            for (int m = 0; m < n_; ++m) {
                if (!up_[static_cast<size_t>(m)]) continue;
                auto a = H.w.find(kAvatarBase + static_cast<uint32_t>(m));
                if (a == H.w.end() || a->second.cell != t.cell) continue;
                if (m == t.owner) curOk = true;
                if (best < 0 || dist(m) < dist(best)) best = m;
            }
            if (t.dead || best < 0) {
                if (t.owner != -1) setOwner(t, -1, OwnEvent::Release, true);
            } else if (t.owner == -1) {
                setOwner(t, best, OwnEvent::Grant, true);
            } else if (!curOk) {
                transfer(t, best);
            } else if (best != t.owner && dist(t.owner) - dist(best) > 300 && now_ - lastXfer_[id] > 1000) {
                transfer(t, best);
            }
        }
    }
    void transfer(Thing& t, int to) {
        if (bugs_.handoffRespawn) {
            Thing cl = t; cl.netId = nextNet_++; cl.owner = -1; cl.dead = true;   // a ghost copy of the actor is left behind
            mach_[0].w[cl.netId] = cl;
            Ev e; e.type = E_Spawn; e.id = cl.netId; e.t = cl; emit(e);
        }
        setOwner(t, to, OwnEvent::Transfer, !bugs_.twoOwners);
    }

    void sendDigests() {
        for (int c = 1; c < n_; ++c) {
            if (!up_[static_cast<size_t>(c)]) continue;
            W w; w.u32(static_cast<uint32_t>(evLog_.size()));
            for (int cat = 0; cat < kDigestCats; ++cat) w.u64(hashWorld(mach_[0], cat, c, false));
            sendPkt(0, c, P_Digest, w.b);
        }
    }
    void onDigest(int m, uint32_t evSeq, const uint64_t* h) {
        Mach& me = mach_[static_cast<size_t>(m)];
        if (me.evExpected - 1 != evSeq) return;   // not at the same point of the event stream: nothing to compare
        for (int cat = 0; cat < kDigestCats; ++cat) {
            if (hashWorld(me, cat, m, true) == h[cat]) continue;
            ++me.repairs;
            sendPkt(m, 0, P_Repair, Bytes());
            return;
        }
    }
    void hostRepair() {
        if (lastReset_ != 0 && now_ - lastReset_ < 1000) return;
        lastReset_ = now_;
        ++counters_["repairs_served"];
        emitReset();
    }
    void emitReset() {
        Ev e; e.type = E_Reset; e.worldEpoch = mach_[0].worldEpoch;
        for (auto& kv : mach_[0].w) e.things.push_back(kv.second);
        emit(e);
    }

    uint64_t hashWorld(const Mach& m, int cat, int skipMachine, bool clientSide) const {
        uint64_t h = 1469598103934665603ull;
        auto mix = [&h](uint64_t v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); h *= 1099511628211ull; };
        if (cat == 0) { mix(m.worldEpoch); return h; }
        for (auto& kv : m.w) {
            const Thing& t = kv.second;
            bool inCat = (cat == 1 && (t.type == 0 || t.type == 1)) || (cat == 2 && t.type == 2) || (cat == 3 && t.type == 3);
            if (!inCat) continue;
            // the digest skips positions the machine owns itself: those are the machine's own truth, not the host's
            bool skipPos = skipMachine >= 0 && ((t.type == 1 && t.owner == skipMachine) || (t.type == 0 && t.netId == kAvatarBase + static_cast<uint32_t>(skipMachine)));
            mix(t.netId); mix(t.type); mix(t.ident);
            if (!skipPos) { mix(static_cast<uint64_t>(clientSide && bugs_.digestUnstable ? (t.x >> 1) : t.x)); mix(static_cast<uint64_t>(t.y)); mix(static_cast<uint64_t>(t.cell)); }
            mix(static_cast<uint64_t>(t.hp)); mix(t.dead); mix(static_cast<uint64_t>(t.owner + 1)); mix(t.epoch); mix(static_cast<uint64_t>(t.holder + 1)); mix(t.open);
        }
        return h;
    }

    // ------------------------------------------------------------ join / leave / save / load
    void doLeave(int m) {
        if (!up_[static_cast<size_t>(m)]) return;
        up_[static_cast<size_t>(m)] = false;
        net_.setConnected(m, false);
        ++counters_["leaves"];
        auto it = mach_[0].w.find(kAvatarBase + static_cast<uint32_t>(m));
        if (it != mach_[0].w.end()) {
            lastPos_[m] = {it->second.x, it->second.y, it->second.cell};
            mach_[0].w.erase(it);
            Ev e; e.type = E_Remove; e.id = kAvatarBase + static_cast<uint32_t>(m); emit(e);
        }
        // Items the leaver carried stay with them (the example has no inventory transfer); actors are released by the next ownership tick.
    }
    void doJoin(int m) {
        if (up_[static_cast<size_t>(m)]) return;
        up_[static_cast<size_t>(m)] = true;
        net_.setConnected(m, true);
        ++counters_["joins"];
        Thing t; t.netId = kAvatarBase + static_cast<uint32_t>(m); t.type = 0; t.hp = 100;
        t.x = lastPos_[m][0]; t.y = lastPos_[m][1]; t.cell = lastPos_[m][2];
        mach_[0].w[t.netId] = t;
        Ev e; e.type = E_Spawn; e.id = t.netId; e.t = t; emit(e);
    }
    void doLoad() {
        Mach& H = mach_[0];
        ++counters_["loads"];
        std::map<uint32_t, Thing> nw = saved_.w;
        for (auto it = nw.begin(); it != nw.end();) it = it->second.type == 0 ? nw.erase(it) : std::next(it);
        for (auto& kv : H.w) if (kv.second.type == 0) nw[kv.first] = kv.second;     // the people connected now stay connected
        for (auto& kv : nw)
            if (kv.second.type == 1) { kv.second.owner = -1; kv.second.epoch = ++epochCtr_; logOwn(OwnEvent::Release, static_cast<int>(kv.first), -1, -1, kv.second.epoch); }
        H.w = nw;
        siteSpawned_ = saved_.sites;
        ++H.worldEpoch;
        logOwn(OwnEvent::WorldEpoch, -1, -1, -1, H.worldEpoch);
        lastUseq_.clear();
        emitReset();
    }

    // ------------------------------------------------------------ client side
    void clientApply(Mach& m, const Ev& ev, uint32_t evSeq) {
        uint64_t idem = (1ull << 62) | evSeq;
        auto rec = [&](ApplyKind k, bool acc, const char* why) { logApply(m.id, idem, static_cast<int>(ev.id), ev.epoch, ev.useq, ev.origin, k, acc, ev.worldEpoch, why); };
        auto find = [&](uint32_t id) -> Thing* { auto it = m.w.find(id); return it == m.w.end() ? nullptr : &it->second; };
        switch (ev.type) {
            case E_Spawn: {
                uint32_t self = kAvatarBase + static_cast<uint32_t>(m.id);
                Thing t = ev.t;
                bool mine = t.netId == self;
                if (mine) { t.x = m.myPos[0]; t.y = m.myPos[1]; t.cell = m.myPos[2]; }   // our own avatar keeps our position
                m.w[t.netId] = t;
                rec(ApplyKind::Spawn, true, "");
                if (mine) reassert(m);
                break;
            }
            case E_Remove: m.w.erase(ev.id); rec(ApplyKind::Remove, true, ""); break;
            case E_PlayerPos: {
                Thing* t = find(ev.id);
                if (!t) { rec(ApplyKind::PlayerPos, false, "gone"); break; }
                if (ev.origin == m.id && !bugs_.echoAdopted) { rec(ApplyKind::PlayerPos, false, "echo of own update"); break; }
                t->x = ev.x; t->y = ev.y; t->cell = ev.cell;
                rec(ApplyKind::PlayerPos, true, "");
                break;
            }
            case E_Pos: {
                Thing* t = find(ev.id);
                if (!t) { rec(ApplyKind::Pos, false, "gone"); break; }
                if (ev.origin == m.id && !bugs_.echoAdopted) { rec(ApplyKind::Pos, false, "echo of own update"); break; }
                if (!bugs_.staleEpoch && ev.epoch < t->epoch) { rec(ApplyKind::Pos, false, "stale epoch"); break; }
                t->x = ev.x; t->y = ev.y; t->cell = ev.cell;
                rec(ApplyKind::Pos, true, "");
                break;
            }
            case E_Hp: { Thing* t = find(ev.id); if (t) { t->hp = ev.v; t->dead = ev.flag != 0; } rec(ApplyKind::Hp, t != nullptr, t ? "" : "gone"); break; }
            case E_Holder: {
                Thing* t = find(ev.id);
                if (t) { t->holder = ev.v; t->x = ev.x; t->y = ev.y; t->cell = ev.cell; }
                rec(ApplyKind::Holder, t != nullptr, t ? "" : "gone");
                break;
            }
            case E_Door: { Thing* t = find(ev.id); if (t) t->open = ev.flag != 0; rec(ApplyKind::Door, t != nullptr, t ? "" : "gone"); break; }
            case E_Owner: {
                Thing* t = find(ev.id);
                if (!t) { rec(ApplyKind::Owner, false, "gone"); break; }
                if (!bugs_.staleEpoch && ev.epoch < t->epoch) { rec(ApplyKind::Owner, false, "stale epoch"); break; }
                t->owner = ev.owner; t->epoch = ev.epoch; t->x = ev.x; t->y = ev.y; t->cell = ev.cell;   // the new owner starts from the authoritative position
                if (bugs_.staleAssignmentEcho && ev.owner == m.id) {
                    // The bug: the owner takes the state snapshot out of its own assignment. The snapshot is only an echo of what the
                    // host knew when it made the assignment, so it can be older than what this machine already did (a predicted kill).
                    bool resurrected = t->dead && ev.flag == 0;
                    t->hp = ev.v; t->dead = ev.flag != 0;
                    if (resurrected) {   // the actor came back to life under the owner: it drops it and "rediscovers" it as a new actor
                        Intent in; in.kind = I_Rediscover; in.worldEpoch = m.worldEpoch; in.a = static_cast<int32_t>(ev.id);
                        m.intents.push_back(encIntent(in));
                        ++counters_["resurrections"];
                    }
                }
                rec(ApplyKind::Owner, true, "");
                break;
            }
            case E_Reset: {
                // A player owns their own avatar, so a Reset never moves it: the client keeps its position and, when the world epoch
                // changed (requests made before the load were rejected), re-asserts it to the host. A repair Reset (same world epoch)
                // also must not roll back actors this machine owns and has already moved.
                uint32_t self = kAvatarBase + static_cast<uint32_t>(m.id);
                std::map<uint32_t, Thing> keep;
                for (auto& kv : m.w)
                    if (kv.first == self || (ev.worldEpoch == m.worldEpoch && kv.second.type == 1 && kv.second.owner == m.id)) keep[kv.first] = kv.second;
                bool newer = ev.worldEpoch > m.worldEpoch;
                keep.erase(self);
                m.w.clear();
                for (auto& t : ev.things) {
                    m.w[t.netId] = t;
                    auto k = keep.find(t.netId);
                    if (k != keep.end() && (t.netId == self || (k->second.owner == t.owner && k->second.epoch == t.epoch))) { m.w[t.netId].x = k->second.x; m.w[t.netId].y = k->second.y; m.w[t.netId].cell = k->second.cell; }
                }
                auto me = m.w.find(self);
                if (me != m.w.end()) { me->second.x = m.myPos[0]; me->second.y = m.myPos[1]; me->second.cell = m.myPos[2]; }
                m.worldEpoch = std::max(m.worldEpoch, ev.worldEpoch);
                m.useq.clear();
                rec(ApplyKind::Reset, true, "");
                if (newer) reassert(m);
                break;
            }
            default: break;
        }
    }

    void reassert(Mach& m) {
        if (m.id == 0 || !m.w.count(kAvatarBase + static_cast<uint32_t>(m.id))) return;
        Intent in; in.kind = I_Move; in.worldEpoch = m.worldEpoch; in.a = m.myPos[0]; in.b = m.myPos[1]; in.c = m.myPos[2];
        m.intents.push_back(encIntent(in));
        ++counters_["reasserts"];
    }

    void aiTick() {
        for (int m = 0; m < n_; ++m) {
            Mach& me = mach_[static_cast<size_t>(m)];
            bool hb = now_ >= nextHeartbeat_[m];
            if (hb) nextHeartbeat_[m] = now_ + 1000;
            std::vector<uint32_t> ids = idsOf(me, [m](const Thing& t) { return t.type == 1 && t.owner == m && !t.dead; });
            for (uint32_t id : ids) {
                Thing& t = me.w[id];
                bool moved = false;
                if (!frozen_ && me.rng.chance(0.7)) {
                    t.x = clampi(t.x + static_cast<int>(me.rng.range(0, 60)) - 30, 0, 1000);
                    t.y = clampi(t.y + static_cast<int>(me.rng.range(0, 60)) - 30, 0, 1000);
                    moved = true;
                }
                if (!moved && !hb) continue;
                auto& us = me.useq[id];
                if (us.first != t.epoch) us = {t.epoch, 0};
                ++us.second;
                uint32_t pkt = ++me.pktCtr;
                me.updHist.push_back({id, t.epoch, us.second, static_cast<uint32_t>(t.x), static_cast<uint32_t>(t.y), static_cast<uint32_t>(t.cell)});
                if (me.updHist.size() > 32) me.updHist.pop_front();
                sendUpd(m, pkt, id, t.epoch, us.second, t.x, t.y, t.cell);
            }
        }
    }
    void sendUpd(int from, uint32_t pkt, uint32_t id, uint32_t epoch, uint32_t useq, int32_t x, int32_t y, int32_t cell) {
        if (from == 0) { hostUpdate(0, pkt, id, epoch, useq, x, y, cell); return; }
        W w; w.u32(pkt); w.u32(id); w.u32(epoch); w.u32(useq); w.i32(x); w.i32(y); w.i32(cell);
        sendPkt(from, 0, P_Upd, w.b);
    }

    // Fault injection from the scenario: replay an old message. c=1 re-sends a raw packet from the ring, c=0 sends old content in a fresh packet.
    void doStale(const Op& op) {
        int from = ((op.a % n_) + n_) % n_, to = ((op.m % n_) + n_) % n_;
        Mach& s = mach_[static_cast<size_t>(from)];
        if (op.c == 1) {
            if (from == to || !up_[static_cast<size_t>(from)]) return;
            auto it = s.ring.find(to);
            if (it == s.ring.end() || it->second.empty()) return;
            const Bytes& raw = it->second[static_cast<size_t>(op.b) % it->second.size()];
            ++counters_["stale_raw_injected"];
            net_.send(from, to, raw);
        } else {
            if (s.updHist.empty() || !up_[static_cast<size_t>(from)]) return;
            auto h = s.updHist[static_cast<size_t>(op.b) % s.updHist.size()];
            ++counters_["stale_old_content_injected"];
            sendUpd(from, ++s.pktCtr, h[0], h[1], h[2], static_cast<int32_t>(h[3]), static_cast<int32_t>(h[4]), static_cast<int32_t>(h[5]));
        }
    }

    Bugs bugs_;
    LossyLink net_;
    int n_;
    LinkParams link_;
    bool profileOwnsLink_ = false;
    std::vector<Mach> mach_;
    std::vector<bool> up_;
    uint64_t now_ = 0;
    uint64_t next50_ = 50, next200_ = 200, next2000_ = 2000;
    std::map<int, uint64_t> nextHeartbeat_;
    bool frozen_ = false;

    // host state
    std::vector<Bytes> evLog_;
    std::map<int, uint32_t> ack_, sentTo_, intentExpected_;
    std::map<int, std::set<uint32_t>> seenPkt_;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> lastUseq_;
    std::map<uint32_t, uint64_t> lastXfer_;
    std::map<uint32_t, std::array<int32_t, 3>> pub_;
    std::set<int> siteSpawned_;
    std::map<int, std::array<int, 3>> lastPos_;
    SavedWorld saved_;
    bool hasSave_ = false;
    uint32_t epochCtr_ = 0, nextNet_ = kDynBase, hostSeq_ = 0;
    uint64_t lastReset_ = 0;
};

std::unique_ptr<ITarget> makeExample(const Scenario& s, std::string* err) {
    Bugs b;
    for (auto& n : s.bugs)
        if (!setBug(b, n)) { if (err) *err = "unknown bug class for target example: " + n; return nullptr; }
    return std::make_unique<Example>(s, b);
}

const TargetRegistrar kRegisterExample(TargetInfo{
    "example",
    "self-contained host-authoritative world with 12 injectable bug classes",
    std::vector<std::string>(std::begin(kBugNames), std::end(kBugNames)),
    makeExample});

}  // namespace
}  // namespace coopfuzz
