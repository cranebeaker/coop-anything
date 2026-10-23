// RE2 (Resident Evil 2 2019 remake) online-co-op protocol prototype, wrapped as a coopfuzz target.
// The model of the protocol we want to implement in the mod (REFramework Lua + C++ transport):
//
//   - 2 to 4 machines, machine 0 = the host (P1, the one who runs the campaign). The rest are the
//     players with separate PCs.
//   - Pawns: each machine simulates its own pawn and sends its position to the host with a per-machine
//     increasing seq, retransmitted until the host acks (the newest position wins, older seqs rejected).
//   - The host replies with a versioned snapshot replicating pawns (with a presence flag, so leave/join
//     are visible), zombies (host-authoritative: alive, hp, position, nudged by the host AI tick),
//     doors and items (holder + ownership epoch). A snapshot applies only if version > myVersion
//     (per-thing idem), retransmitted until the client acks.
//   - Interactions travel as requests (door toggle, pickup, drop, spawn site, hit) in a second ordered
//     retransmit stream per machine: each request carries a request seq plus the world epoch it was
//     made under; the host applies requests strictly in seq order, rejects duplicates and pre-load
//     (stale world epoch) requests, and acks everything it handled.
//   - World epoch: Save freezes a host-side copy; Load restores it and bumps the world epoch. Pawn
//     moves and requests made before a load are rejected by epoch and acked (they may not touch the
//     reloaded world); clients learn the new epoch from the snapshot and re-assert their pawn position.
//   - Ownership: zombies are the host's (grant on spawn, release on death, reload re-grants with a
//     fresh ownership epoch); items transfer host <-> holder machine on pickup/drop/leave. Every
//     ownership decision is logged with a monotone epoch; a load logs a WorldEpoch decision first.
//   - Out of the MVP on purpose: host migration (that would be the real zombie-ownership transfer)
//     and story sync.
//
// Bug twins, each caught by the oracles: "no_snapshot_dedupe" (old snapshots re-applied -> double_apply),
// "no_request_dedupe" (duplicate requests re-executed -> double_apply), "no_world_epoch" (pre-load
// requests/moves applied to the reloaded world -> stale_applied).
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {
namespace {

// ============================================================ bytes
uint32_t sum32(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}
void put32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
void puti32(Bytes& b, int32_t v) { put32(b, static_cast<uint32_t>(v)); }
void put8v(Bytes& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v & 0xFF)); }
uint32_t get32(const Bytes& b, size_t at) { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(b[at + static_cast<size_t>(i)]) << (8 * i); return v; }
int32_t geti32(const Bytes& b, size_t at) { return static_cast<int32_t>(get32(b, at)); }
uint8_t get8(const Bytes& b, size_t at) { return b[at]; }

constexpr int kEnemies = 4;
constexpr int kDoors = 4;
constexpr int kItems = 4;
constexpr uint32_t kPawnBase = 0x100;
constexpr uint32_t kEnemyBase = 0x200;
constexpr uint32_t kDoorBase = 0x300;
constexpr uint32_t kItemBase = 0x400;

struct V3 { int x = 0, y = 0; };

// The report the adapter routes into TargetBase::logApply, in its parameter order.
using ReportFn = std::function<void(int machine, uint64_t idem, int ref, uint32_t epoch, uint32_t seq, int origin,
                                    ApplyKind kind, bool accepted, uint32_t worldEpoch, const char* why)>;
// Ownership decisions the adapter routes into TargetBase::logOwn (all of them come from the host).
using OwnFn = std::function<void(OwnEvent::Kind kind, int ref, int owner, int prev, uint32_t epoch)>;

// Bug twins.
struct Bugs {
    bool noSnapshotDedupe = false;   // client applies old snapshots again
    bool noRequestDedupe = false;    // host re-executes duplicate / out-of-order requests
    bool noWorldEpoch = false;       // host skips the pre-load epoch check
};

enum ReqKind : uint8_t { RDoor = 1, RPick, RDrop, RSpawn, RHit };
enum Pkt : uint8_t { kMove = 1, kMoveAck, kReq, kReqAck, kSnap, kSnapAck, kHello, kHelloAck };

struct ReqEntry {
    uint32_t seq = 0, wep = 0;
    ReqKind kind = RDoor;
    int32_t a = 0, b = 0, c = 0;
};

// ============================================================ one machine's sync core
class Re2Core {
public:
    Re2Core(int id, int players, const Bugs& bugs, std::function<void(int, Bytes)> send, ReportFn report, OwnFn own)
        : id_(id), n_(players), bugs_(bugs), send_(std::move(send)), report_(std::move(report)), own_(std::move(own)),
          zom_(kEnemies), door_(kDoors, 0), item_(kItems),
          pawnPos_(static_cast<size_t>(players)), presence_(static_cast<size_t>(players), true),
          lastMoveSeq_(static_cast<size_t>(players), 0), lastReqSeq_(static_cast<size_t>(players), 0),
          snapAcked_(static_cast<size_t>(players), 0) {
        for (int p = 0; p < players; ++p) pawnPos_[static_cast<size_t>(p)] = {100 * (p + 1), 100};
    }

    // ---- state the adapter reads
    const V3& pawnPos(int p) const { return pawnPos_[static_cast<size_t>(p)]; }
    bool pawnPresent(int p) const { return p == id_ ? connected_ : presence_[static_cast<size_t>(p)] != 0; }
    bool zombieAlive(int z) const { return zom_[static_cast<size_t>(z)].alive; }
    const V3& zombiePos(int z) const { return zom_[static_cast<size_t>(z)].pos; }
    int zombieHp(int z) const { return zom_[static_cast<size_t>(z)].hp; }
    uint32_t zombieEpoch(int z) const { return zom_[static_cast<size_t>(z)].epoch; }
    bool doorOpen(int d) const { return door_[static_cast<size_t>(d)] != 0; }
    int itemHolder(int it) const { return item_[static_cast<size_t>(it)].holder; }
    uint32_t itemEpoch(int it) const { return item_[static_cast<size_t>(it)].epoch; }
    uint32_t myVersion() const { return myVersion_; }
    uint32_t hostVersion() const { return version_; }
    uint32_t worldEpoch() const { return worldEpoch_; }
    uint32_t moveAcked() const { return ackedSeq_; }
    uint32_t localSeqTotal() const { return localSeq_; }
    uint32_t reqAcked() const { return reqAcked_; }
    uint32_t reqHigh() const { return reqHigh_; }
    bool helloSynced() const { return !helloPending_; }
    uint32_t snapAckedByHost(int c) const { return snapAcked_[static_cast<size_t>(c)]; }

    // ---- commands from the adapter
    void setConnected(bool up) {
        connected_ = up;
        if (!up) {
            // A leaver drops its pending traffic and catches its own streams up; sequence numbers are
            // never reused, so the host's per-peer counters (kept across the leave) stay meaningful.
            reqQueue_.clear();
            reqAcked_ = reqHigh_;
            ackedSeq_ = localSeq_;
            helloPending_ = false;
        } else {
            // On join the client resyncs the host's request gate: requests lost while it was away are
            // self-acked (dropped), so the host never saw them and its gate would lag forever. The
            // mark is frozen here so requests submitted after the join stay fresh for the host.
            joinReqHigh_ = reqHigh_;
            helloPending_ = true;
        }
    }
    void setHostPresence(int m, bool present) {   // host only, on a client join/leave
        if (id_ != 0 || m <= 0 || m >= n_ || presence_[static_cast<size_t>(m)] == (present ? 1 : 0)) return;
        presence_[static_cast<size_t>(m)] = present ? 1 : 0;
        ++version_;
    }
    // Host only: a leaver's carried items fall back to the ground.
    void hostReleaseHeld(int m) {
        if (id_ != 0 || m < 0 || m >= n_) return;
        for (int it = 0; it < kItems; ++it) {
            Item& i = item_[static_cast<size_t>(it)];
            if (i.holder != m) continue;
            int prev = i.holder;
            i.holder = -1;
            i.epoch = ++epochCtr_;
            logOwnE(OwnEvent::Transfer, static_cast<int>(kItemBase + it), 0, prev, i.epoch);
            ++version_;
        }
    }
    void setLocalPawnPosition(int x, int y) {
        pawnPos_[static_cast<size_t>(id_)] = {x, y};
        ++localSeq_;
        if (id_ == 0) ++version_;   // the host's own pawn has to re-replicate itself
    }
    void reassertPawn() { ++localSeq_; }   // resend our pawn after a join or a new world epoch

    void request(int door) { queueReq(RDoor, door, 0, 0); }
    void request(int a, ReqKind kind) { queueReq(kind, a, 0, 0); }    // pickup, spawn site
    void requestDrop(int item, int x, int y) { queueReq(RDrop, item, x, y); }
    void requestHit(int enemy, int dmg) { queueReq(RHit, enemy, dmg, 0); }

    void tick() {   // every 50 ms
        if (id_ != 0) {
            if (ackedSeq_ < localSeq_) sendMove();
            while (!reqQueue_.empty() && reqQueue_.front().seq <= reqAcked_) reqQueue_.pop_front();
            // Window 1: only the oldest unacked request flies (plus its retransmits), so the host can
            // keep applying strictly in seq order; a raw replay of a later request is a hard gap.
            if (!reqQueue_.empty()) {
                const ReqEntry& r = reqQueue_.front();
                Bytes b; b.push_back(kReq);
                put32(b, r.seq); put32(b, r.wep); put8v(b, r.kind);
                put32(b, asU32(r.a)); put32(b, asU32(r.b)); put32(b, asU32(r.c));
                post(0, b);
            }
            // Post-join gate resync, retransmitted until the host acks it.
            if (helloPending_) {
                Bytes b; b.push_back(kHello); put32(b, joinReqHigh_);
                post(0, b);
            }
            return;
        }
        for (int c = 1; c < n_; ++c) {
            if (!presence_[static_cast<size_t>(c)]) continue;
            if (snapAcked_[static_cast<size_t>(c)] >= version_) continue;
            post(c, snapshotPacket());
        }
    }

    void hostWander(Rng& rng) {   // host only: the zombies roam a little, replicated via the snapshot
        if (id_ != 0) return;
        for (int z = 0; z < kEnemies; ++z) {
            Zombie& zt = zom_[static_cast<size_t>(z)];
            if (!zt.alive || !rng.chance(0.8)) continue;
            V3 oldPos = zt.pos;
            zt.pos.x = clampi(zt.pos.x + static_cast<int>(rng.range(0, 41)) - 20, 0, 1000);
            zt.pos.y = clampi(zt.pos.y + static_cast<int>(rng.range(0, 41)) - 20, 0, 1000);
            if (zt.pos.x == oldPos.x && zt.pos.y == oldPos.y) continue;
            ++version_;
            report_(0, idemWander(version_, z), static_cast<int>(kEnemyBase + static_cast<uint32_t>(z)), zt.epoch,
                    version_, 0, ApplyKind::Pos, true, worldEpoch_, "");
        }
    }

    // Stale op: replay a raw packet this machine sent to `to` (the ring keeps the last 32).
    void replayStale(int to, uint32_t pick) {
        auto it = ring_.find(to);
        if (to == id_ || it == ring_.end() || it->second.empty()) return;
        const Bytes& raw = it->second[static_cast<size_t>(pick % it->second.size())];
        send_(to, raw);
    }

    void onPacket(int from, const Bytes& d) {
        if (d.size() < 5 || sum32(d.data(), d.size() - 4) != get32(d, d.size() - 4)) return;   // corrupted: dropped
        size_t body = d.size() - 4;
        switch (d[0]) {
            case kMove:    if (id_ == 0 && body >= 17) onPawnMove(from, get32(d, 1), get32(d, 5), geti32(d, 9), geti32(d, 13)); break;
            case kMoveAck: if (id_ != 0 && body >= 5) ackedSeq_ = std::max(ackedSeq_, get32(d, 1)); break;
            case kReq:     if (id_ == 0 && body >= 21) onRequest(from, get32(d, 1), get32(d, 5), get8(d, 9), get32(d, 10), geti32(d, 14), geti32(d, 18)); break;
            case kReqAck:  if (id_ != 0 && body >= 5) reqAcked_ = std::max(reqAcked_, get32(d, 1)); break;
            case kSnap:    if (id_ != 0 && body >= snapBodyLen(n_)) onSnapshot(d); break;
            case kSnapAck: if (id_ == 0 && body >= 5) snapAcked_[static_cast<size_t>(from)] = std::max(snapAcked_[static_cast<size_t>(from)], get32(d, 1)); break;
            case kHello: {
                // Host: adopt the client's request high-water mark so its gate catches up after the
                // client's rejoin; idempotent (max), acked with the adopted value.
                if (id_ != 0 || body < 5) break;
                uint32_t r = get32(d, 1);
                if (r > lastReqSeq_[static_cast<size_t>(from)]) lastReqSeq_[static_cast<size_t>(from)] = r;
                Bytes a; a.push_back(kHelloAck); put32(a, lastReqSeq_[static_cast<size_t>(from)]);
                post(from, a);
                break;
            }
            case kHelloAck:
                if (id_ != 0 && body >= 5 && get32(d, 1) >= joinReqHigh_) helloPending_ = false;
                break;
            default: break;
        }
    }

    // ---- host save / load
    struct SavedWorld {
        std::vector<uint8_t> zAlive, doorOpen;
        std::vector<int> zHp;
        std::vector<V3> zPos;
        std::vector<int> itHolder;
    };
    void hostSave(SavedWorld& out) const {
        if (id_ != 0) return;
        out.zAlive.clear(); out.zHp.clear(); out.zPos.clear(); out.doorOpen.clear(); out.itHolder.clear();
        for (int z = 0; z < kEnemies; ++z) {
            out.zAlive.push_back(zom_[static_cast<size_t>(z)].alive ? 1 : 0);
            out.zHp.push_back(zom_[static_cast<size_t>(z)].hp);
            out.zPos.push_back(zom_[static_cast<size_t>(z)].pos);
        }
        for (int d = 0; d < kDoors; ++d) out.doorOpen.push_back(door_[static_cast<size_t>(d)] ? 1 : 0);
        for (int it = 0; it < kItems; ++it) out.itHolder.push_back(item_[static_cast<size_t>(it)].holder);
    }
    void hostLoad(const SavedWorld& in) {
        if (id_ != 0 || in.zAlive.size() != static_cast<size_t>(kEnemies)) return;
        ++worldEpoch_;                                   // the world restarted; pre-load traffic is stale now
        logOwnE(OwnEvent::WorldEpoch, -1, -1, -1, worldEpoch_);
        for (int z = 0; z < kEnemies; ++z) {
            Zombie& zt = zom_[static_cast<size_t>(z)];
            zt.alive = in.zAlive[static_cast<size_t>(z)] != 0;
            zt.hp = in.zHp[static_cast<size_t>(z)];
            zt.pos = in.zPos[static_cast<size_t>(z)];
            zt.epoch = ++epochCtr_;                      // fresh epoch for the reloaded world, dead or alive
            if (zt.alive)
                logOwnE(OwnEvent::Transfer, static_cast<int>(kEnemyBase + z), 0, 0, zt.epoch);
            else
                logOwnE(OwnEvent::Release, static_cast<int>(kEnemyBase + z), -1, 0, zt.epoch);
        }
        for (int d = 0; d < kDoors; ++d) door_[static_cast<size_t>(d)] = in.doorOpen[static_cast<size_t>(d)] != 0 ? 1 : 0;
        for (int it = 0; it < kItems; ++it) {
            Item& i = item_[static_cast<size_t>(it)];
            int prev = i.holder;
            i.holder = in.itHolder[static_cast<size_t>(it)];
            i.epoch = ++epochCtr_;
            if (prev != i.holder)
                logOwnE(OwnEvent::Transfer, static_cast<int>(kItemBase + it), ownerOf(i.holder), ownerOf(prev), i.epoch);
        }
        ++version_;
    }

private:
    struct Zombie { bool alive = false; int hp = 100; uint32_t epoch = 0; V3 pos{}; };
    struct Item { int holder = -1; uint32_t epoch = 0; };
    static size_t snapBodyLen(int players) {
        return 8 + static_cast<size_t>(players) * 9 + static_cast<size_t>(kEnemies) * 17 + static_cast<size_t>(kDoors) + static_cast<size_t>(kItems) * 8;
    }

    static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
    static uint32_t asU32(int32_t v) { return static_cast<uint32_t>(v); }
    static int ownerOf(int holder) { return holder < 0 ? 0 : holder; }   // ground items belong to the host

    static uint64_t idemReq(int client, uint32_t seq) { return (1ull << 60) | (static_cast<uint64_t>(client) << 40) | seq; }
    static uint64_t idemSnap(uint32_t version, uint32_t slot) { return (2ull << 60) | (static_cast<uint64_t>(version) << 20) | slot; }
    static uint64_t idemWander(uint32_t version, int z) { return (3ull << 60) | (static_cast<uint64_t>(version) << 20) | static_cast<uint64_t>(z); }
    static uint64_t idemPawn(int client, uint32_t seq) { return (4ull << 60) | (static_cast<uint64_t>(client) << 40) | seq; }

    void logOwnE(OwnEvent::Kind k, int ref, int owner, int prev, uint32_t epoch) { own_(k, ref, owner, prev, epoch); }

    void post(int to, Bytes b) {   // checksum + remember for raw replay
        put32(b, sum32(b.data(), b.size()));
        auto& ring = ring_[to];
        ring.push_back(b);
        if (ring.size() > 32) ring.pop_front();
        send_(to, std::move(b));
    }

    void sendMove() {
        Bytes b; b.push_back(kMove);
        put32(b, localSeq_); put32(b, worldEpoch_);
        puti32(b, pawnPos_[static_cast<size_t>(id_)].x); puti32(b, pawnPos_[static_cast<size_t>(id_)].y);
        post(0, b);
    }

    void queueReq(ReqKind kind, int32_t a, int32_t b, int32_t c) {
        ReqEntry r;
        r.seq = ++reqHigh_; r.wep = worldEpoch_; r.kind = kind; r.a = a; r.b = b; r.c = c;
        reqQueue_.push_back(r);
        if (id_ == 0) {   // the host's own requests execute immediately on itself
            onRequest(0, r.seq, r.wep, static_cast<uint8_t>(kind), asU32(a), b, c);
            reqAcked_ = r.seq;
        }
    }

    Bytes snapshotPacket() {
        Bytes b; b.push_back(kSnap);
        put32(b, version_); put32(b, worldEpoch_);
        for (int p = 0; p < n_; ++p) {
            put8v(b, presence_[static_cast<size_t>(p)]);
            puti32(b, pawnPos_[static_cast<size_t>(p)].x); puti32(b, pawnPos_[static_cast<size_t>(p)].y);
        }
        for (int z = 0; z < kEnemies; ++z) {
            const Zombie& zt = zom_[static_cast<size_t>(z)];
            put8v(b, zt.alive ? 1u : 0u); puti32(b, zt.hp); put32(b, zt.epoch);
            puti32(b, zt.pos.x); puti32(b, zt.pos.y);
        }
        for (int d = 0; d < kDoors; ++d) put8v(b, door_[static_cast<size_t>(d)]);
        for (int it = 0; it < kItems; ++it) { puti32(b, item_[static_cast<size_t>(it)].holder); put32(b, item_[static_cast<size_t>(it)].epoch); }
        return b;
    }

    void onPawnMove(int from, uint32_t seq, uint32_t wep, int32_t x, int32_t y) {
        uint64_t idem = idemPawn(from, seq);
        auto recFail = [&](const char* why) {
            report_(0, idem, static_cast<int>(kPawnBase + static_cast<uint32_t>(from)), 0, seq, from, ApplyKind::Intent, false, wep, why);
        };
        if (seq <= lastMoveSeq_[static_cast<size_t>(from)]) {
            recFail("old pawn sequence");
        } else if (!bugs_.noWorldEpoch && wep != worldEpoch_) {
            lastMoveSeq_[static_cast<size_t>(from)] = seq;   // consume it (stale by epoch) so the stream unsticks
            recFail("stale world epoch");
        } else {
            lastMoveSeq_[static_cast<size_t>(from)] = seq;
            pawnPos_[static_cast<size_t>(from)] = {x, y};
            ++version_;
            report_(0, idem, static_cast<int>(kPawnBase + static_cast<uint32_t>(from)), 0, seq, from, ApplyKind::Intent, true, wep, "");
        }
        Bytes a; a.push_back(kMoveAck); put32(a, lastMoveSeq_[static_cast<size_t>(from)]);
        post(from, a);
    }

    void onRequest(int from, uint32_t seq, uint32_t wep, uint8_t kind, uint32_t a, int32_t b, int32_t c) {
        uint64_t idem = idemReq(from, seq);
        auto rec = [&](bool ok, int ref, const char* why) {
            report_(0, idem, ref, 0, seq, from, ApplyKind::Intent, ok, wep, why);
        };
        bool executed = false;
        if (!bugs_.noRequestDedupe) {
            if (seq <= lastReqSeq_[static_cast<size_t>(from)]) { rec(false, -1, "duplicate request"); }
            else if (seq > lastReqSeq_[static_cast<size_t>(from)] + 1) { rec(false, -1, "gap request"); }
            else executed = true;
        } else {
            executed = true;   // twin: no dedupe at all, a replayed request runs again
        }
        if (executed && from != 0 && !presence_[static_cast<size_t>(from)]) { rec(false, -1, "not joined"); executed = false; }
        if (executed && !bugs_.noWorldEpoch && wep != worldEpoch_) {
            lastReqSeq_[static_cast<size_t>(from)] = seq;   // stale by epoch: consumed, so the stream unsticks
            rec(false, -1, "stale world epoch");
        } else if (executed) {
            lastReqSeq_[static_cast<size_t>(from)] = seq;
            executeRequest(idem, from, wep, kind, a, b, c);
        }
        if (from != 0) {
            Bytes t; t.push_back(kReqAck); put32(t, lastReqSeq_[static_cast<size_t>(from)]);
            post(from, t);
        }
    }

    void executeRequest(uint64_t idem, int from, uint32_t wep, uint8_t kind, uint32_t a, int32_t b, int32_t c) {
        (void)c;   // only carried items exist in this model; a dropped item has no ground coordinates
        auto rec = [&](bool ok, int ref, const char* why) { report_(0, idem, ref, 0, 0, from, ApplyKind::Intent, ok, wep, why); };
        switch (kind) {
            case RDoor: {
                if (a >= static_cast<uint32_t>(kDoors)) { rec(false, -1, "bad door"); return; }
                door_[static_cast<size_t>(a)] = door_[static_cast<size_t>(a)] ? 0 : 1;
                ++version_;
                rec(true, static_cast<int>(kDoorBase + a), "");
                return;
            }
            case RPick: {
                if (a >= static_cast<uint32_t>(kItems)) { rec(false, -1, "bad item"); return; }
                Item& i = item_[static_cast<size_t>(a)];
                if (i.holder != -1) { rec(false, static_cast<int>(kItemBase + a), "already taken"); return; }
                int ref = static_cast<int>(kItemBase + a);
                i.holder = from; i.epoch = ++epochCtr_;
                logOwnE(OwnEvent::Transfer, ref, from, 0, i.epoch);
                ++version_;
                rec(true, ref, "");
                return;
            }
            case RDrop: {
                if (a >= static_cast<uint32_t>(kItems)) { rec(false, -1, "bad item"); return; }
                Item& i = item_[static_cast<size_t>(a)];
                if (i.holder != from) { rec(false, static_cast<int>(kItemBase + a), "not held"); return; }
                int ref = static_cast<int>(kItemBase + a);
                int prev = i.holder;
                i.holder = -1; i.epoch = ++epochCtr_;
                logOwnE(OwnEvent::Transfer, ref, 0, prev, i.epoch);
                ++version_;
                rec(true, ref, "");
                return;
            }
            case RSpawn: {
                int z = static_cast<int>(a % static_cast<uint32_t>(kEnemies));
                Zombie& zt = zom_[static_cast<size_t>(z)];
                if (zt.alive) { rec(false, static_cast<int>(kEnemyBase + z), "already spawned"); return; }
                zt.alive = true; zt.hp = 100;
                zt.pos = {25 + static_cast<int>(a) * 13, 25 + static_cast<int>(a) * 29};
                int ref = static_cast<int>(kEnemyBase + z);
                zt.epoch = ++epochCtr_;
                logOwnE(OwnEvent::Grant, ref, 0, -1, zt.epoch);
                ++version_;
                rec(true, ref, "");
                return;
            }
            case RHit: {
                if (a >= static_cast<uint32_t>(kEnemies)) { rec(false, -1, "bad enemy"); return; }
                Zombie& zt = zom_[static_cast<size_t>(a)];
                if (!zt.alive) { rec(false, static_cast<int>(kEnemyBase + a), "dead"); return; }
                zt.hp = std::max(0, zt.hp - std::max(1, b));
                if (zt.hp == 0) {
                    int ref = static_cast<int>(kEnemyBase + a);
                    zt.alive = false;
                    zt.epoch = ++epochCtr_;
                    logOwnE(OwnEvent::Release, ref, -1, 0, zt.epoch);
                }
                ++version_;
                rec(true, static_cast<int>(kEnemyBase + a), "");
                return;
            }
            default: rec(false, -1, "bad request kind"); return;
        }
    }

    void onSnapshot(const Bytes& d) {
        uint32_t version = get32(d, 1), wep = get32(d, 5);
        auto recSnap = [&](bool ok, uint32_t slot, int ref, ApplyKind k, uint32_t epoch, bool acceptedUnused, const char* why) {
            (void)acceptedUnused;
            report_(id_, idemSnap(version, slot), ref, epoch, version, 0, k, ok, wep, why);
        };
        if (!bugs_.noSnapshotDedupe && version <= myVersion_) {
            recSnap(false, 999, -1, ApplyKind::Reset, 0, false, "old snapshot");
        } else if (!bugs_.noSnapshotDedupe && version > myVersion_ && wep < worldEpoch_) {
            recSnap(false, 998, -1, ApplyKind::Reset, 0, false, "old world epoch");
        } else {
            bool newerEpoch = wep > worldEpoch_;
            worldEpoch_ = wep;
            size_t at = 9;
            for (int p = 0; p < n_; ++p) {
                bool present = get8(d, at) == 1;
                V3 q{geti32(d, at + 1), geti32(d, at + 5)}; at += 9;
                if (p == id_) continue;   // my pawn: the snapshot does not overwrite my own truth
                presence_[static_cast<size_t>(p)] = present ? 1 : 0;
                pawnPos_[static_cast<size_t>(p)] = q;
                recSnap(true, static_cast<uint32_t>(p), static_cast<int>(kPawnBase + static_cast<uint32_t>(p)), ApplyKind::PlayerPos, 0, true, "");
            }
            for (int z = 0; z < kEnemies; ++z) {
                bool alive = get8(d, at) == 1; int hp = geti32(d, at + 1); uint32_t ep = get32(d, at + 5);
                V3 q{geti32(d, at + 9), geti32(d, at + 13)}; at += 17;
                Zombie& zt = zom_[static_cast<size_t>(z)];
                bool changed = zt.alive != alive || zt.hp != hp || zt.pos.x != q.x || zt.pos.y != q.y;
                if (changed) {
                    uint32_t prevAlive = zt.alive ? 1u : 0u;
                    zt.alive = alive; zt.hp = hp; zt.pos = q; zt.epoch = ep;
                    int ref = static_cast<int>(kEnemyBase + static_cast<uint32_t>(z));
                    ApplyKind k = prevAlive == 0 && alive ? ApplyKind::Spawn : ApplyKind::Pos;
                    recSnap(true, 100u + static_cast<uint32_t>(z), ref, k, ep, true, "");
                } else {
                    zt.epoch = std::max(zt.epoch, ep);
                    recSnap(false, 100u + static_cast<uint32_t>(z), static_cast<int>(kEnemyBase + static_cast<uint32_t>(z)), ApplyKind::Pos, ep, false, "no change");
                }
            }
            for (int dc = 0; dc < kDoors; ++dc) {
                bool open = get8(d, at) == 1; at += 1;
                if ((door_[static_cast<size_t>(dc)] != 0) != open) {
                    door_[static_cast<size_t>(dc)] = open ? 1 : 0;
                    recSnap(true, 200u + static_cast<uint32_t>(dc), static_cast<int>(kDoorBase + static_cast<uint32_t>(dc)), ApplyKind::Door, 0, true, "");
                } else {
                    recSnap(false, 200u + static_cast<uint32_t>(dc), static_cast<int>(kDoorBase + static_cast<uint32_t>(dc)), ApplyKind::Door, 0, false, "no change");
                }
            }
            for (int it = 0; it < kItems; ++it) {
                int holder = geti32(d, at); uint32_t ep = get32(d, at + 4); at += 8;
                Item& i = item_[static_cast<size_t>(it)];
                if (i.holder != holder || i.epoch != ep) {
                    i.holder = holder; i.epoch = ep;
                    recSnap(true, 300u + static_cast<uint32_t>(it), static_cast<int>(kItemBase + static_cast<uint32_t>(it)), ApplyKind::Holder, ep, true, "");
                } else {
                    recSnap(false, 300u + static_cast<uint32_t>(it), static_cast<int>(kItemBase + static_cast<uint32_t>(it)), ApplyKind::Holder, ep, false, "no change");
                }
            }
            myVersion_ = std::max(myVersion_, version);
            if (newerEpoch && connected_) reassertPawn();   // resync our pawn into the reloaded world
        }
        Bytes a; a.push_back(kSnapAck); put32(a, myVersion_);
        post(0, a);   // ack whatever we saw so the host stops retransmitting
    }

    int id_ = 0, n_ = 0;
    Bugs bugs_;
    std::function<void(int, Bytes)> send_;
    ReportFn report_;
    OwnFn own_;

    std::vector<Zombie> zom_;
    std::vector<uint8_t> door_;
    std::vector<Item> item_;
    std::vector<V3> pawnPos_;
    std::vector<uint8_t> presence_;
    std::vector<uint32_t> lastMoveSeq_, lastReqSeq_, snapAcked_;
    std::deque<ReqEntry> reqQueue_;
    std::map<int, std::deque<Bytes>> ring_;
    bool connected_ = true;
    uint32_t localSeq_ = 0, ackedSeq_ = 0, myVersion_ = 0, version_ = 0, worldEpoch_ = 0;
    uint32_t reqAcked_ = 0, reqHigh_ = 0, epochCtr_ = 0;
    uint32_t joinReqHigh_ = 0;    // the request mark frozen at the last join (the hello's payload)
    bool helloPending_ = false;   // client: the post-join gate resync is not acked yet
};

// Everything below is the fuzz harness (an ITarget + its registration). The mod plugin reuses this
// file for Re2Core only, so with COOPNET_MOD_CORE_ONLY defined the harness does not compile there
// (TargetBase's implementations live in the coopfuzz lib, not in the mod).
#ifndef COOPNET_MOD_CORE_ONLY

// ============================================================ the adapter the fuzzer drives
class Re2Target : public TargetBase {
public:
    explicit Re2Target(const Scenario& s, Bugs bugs) : bugs_(bugs), net_(s.machines, s.seed * 77 + 3), n_(s.machines) {
        LinkParams link = s.link;
        if (!s.profile.empty()) linkProfile(s.profile, link);
        net_.setLink(link);
        wander_.reseed(s.seed * 1000003ull + 17);
        for (int m = 0; m < n_; ++m) {
            up_.push_back(true);
            cores_.emplace_back(new Re2Core(
                m, n_, bugs_, [this, m](int to, Bytes b) { net_.send(m, to, std::move(b)); },
                [this](int machine, uint64_t idem, int ref, uint32_t epoch, uint32_t seq, int origin, ApplyKind kind, bool ok, uint32_t wep, const char* why) {
                    logApply(machine, idem, ref, epoch, seq, origin, kind, ok, wep, why);
                },
                [this](OwnEvent::Kind k, int ref, int owner, int prev, uint32_t epoch) { logOwn(k, ref, owner, prev, epoch); }));
        }
    }

    uint64_t now() const override { return now_; }
    int machines() const override { return n_; }
    bool connected(int m) const override { return m >= 0 && m < n_ && up_[static_cast<size_t>(m)]; }
    uint64_t repairs(int) const override { return 0; }   // without resync/desync-repair path yet (like the MVP)

    void apply(const Op& op) override {
        int m = std::max(0, std::min(n_ - 1, op.m));
        Re2Core& c = *cores_[static_cast<size_t>(m)];
        switch (op.kind) {
            case OpKind::Move:
                if (!up_[static_cast<size_t>(m)]) return;
                ++counters_["op_move"];
                c.setLocalPawnPosition(clampi(op.a, 0, 1000), clampi(op.b, 0, 1000));
                break;
            case OpKind::Activate:
                if (!up_[static_cast<size_t>(m)]) return;
                ++counters_["op_door"];
                c.request(modSlot(op.a, kDoors));
                break;
            case OpKind::Pickup: {
                if (!up_[static_cast<size_t>(m)]) return;
                ++counters_["op_pickup"];
                std::vector<int> ground = itemsWhere(c, -1);
                if (ground.empty()) return;
                c.request(pickSlot(ground, op.a), RPick);
                break;
            }
            case OpKind::Drop:
                if (!up_[static_cast<size_t>(m)]) return;
                ++counters_["op_drop"];
                ++counters_["op_active"];
                if (itemsWhere(c, m).empty()) return;
                c.requestDrop(pickSlot(itemsWhere(c, m), op.a), clampi(op.b, 0, 1000), clampi(op.c, 0, 1000));
                break;
            case OpKind::Spawn:
                if (!up_[static_cast<size_t>(m)]) return;
                ++counters_["op_spawn"];
                ++counters_["op_active"];
                c.request(((op.a % 5) + 5) % 5, RSpawn);
                break;
            case OpKind::Hit:
                if (!up_[static_cast<size_t>(m)]) return;
                ++counters_["op_hit"];
                ++counters_["op_active"];
                {
                    std::vector<int> live;
                    for (int z = 0; z < kEnemies; ++z) if (c.zombieAlive(z)) live.push_back(z);
                    if (live.empty()) return;
                    int z = live[static_cast<size_t>(modSlot(op.a, static_cast<int>(live.size())))];
                    c.requestHit(z, clampi(op.b, 1, 100));
                }
                break;
            case OpKind::Join:
                if (m == 0 || up_[static_cast<size_t>(m)]) return;
                ++counters_["joins"];
                up_[static_cast<size_t>(m)] = true;
                net_.setConnected(m, true);
                cores_[static_cast<size_t>(m)]->setConnected(true);
                cores_[static_cast<size_t>(m)]->reassertPawn();
                cores_[0]->setHostPresence(m, true);
                break;
            case OpKind::Leave:
                if (m == 0 || !up_[static_cast<size_t>(m)]) return;
                ++counters_["leaves"];
                up_[static_cast<size_t>(m)] = false;
                net_.setConnected(m, false);
                cores_[static_cast<size_t>(m)]->setConnected(false);
                cores_[0]->setHostPresence(m, false);
                cores_[0]->hostReleaseHeld(m);
                break;
            case OpKind::Save:
                ++counters_["saves"];
                cores_[0]->hostSave(saved_);
                hasSave_ = true;
                break;
            case OpKind::Load:
                if (!hasSave_) return;
                ++counters_["loads"];
                cores_[0]->hostLoad(saved_);
                break;
            case OpKind::SetLink: {
                LinkParams lp;
                lp.minLatencyMs = static_cast<uint32_t>(std::max(0, op.a));
                lp.maxLatencyMs = static_cast<uint32_t>(std::max(std::max(0, op.a), op.b));
                lp.loss = static_cast<double>(std::max(0, op.c)) / 1000.0;
                lp.duplicate = static_cast<double>(std::max(0, op.d)) / 1000.0;
                if (!frozen_) net_.setLink(lp);
                ++counters_["op_setlink"];
                break;
            }
            case OpKind::Stale: {
                int from = modSlot(op.a, n_), to = modSlot(op.m, n_);
                if (from == to) return;
                cores_[static_cast<size_t>(from)]->replayStale(to, static_cast<uint32_t>(std::max(0, op.b)));
                ++counters_["stale_replayed"];
                break;
            }
            default: break;
        }
    }

    void advance(uint64_t toMs) override {
        while (now_ < toMs) {
            now_ = std::min<uint64_t>(now_ + 10, toMs);
            net_.advance(now_);
            for (int m = 0; m < n_; ++m) { int from = 0; Bytes d; while (net_.poll(m, from, d)) cores_[static_cast<size_t>(m)]->onPacket(from, d); }
            while (now_ >= next50_) { next50_ += 50; for (auto& c : cores_) c->tick(); }
            while (now_ >= next200_) { next200_ += 200; if (!frozen_) cores_[0]->hostWander(wander_); }
        }
    }

    void heal() override { frozen_ = true; net_.setLink(LinkParams()); }

    bool quiescent() const override {
        if (!frozen_ || !net_.idle()) return false;
        const Re2Core& host = *cores_[0];
        for (int mc = 1; mc < n_; ++mc) {
            if (!up_[static_cast<size_t>(mc)]) continue;
            const Re2Core& me = *cores_[static_cast<size_t>(mc)];
            if (me.myVersion() != host.hostVersion()) return false;
            if (host.snapAckedByHost(mc) != host.hostVersion()) return false;
            if (me.moveAcked() != me.localSeqTotal()) return false;
            if (me.reqAcked() != me.reqHigh()) return false;
            if (!me.helloSynced()) return false;
        }
        return true;
    }

    std::vector<Thing> view(int m) const override {
        std::vector<Thing> v;
        const Re2Core& c = *cores_[static_cast<size_t>(m)];
        for (int p = 0; p < n_; ++p) {
            if (!c.pawnPresent(p)) continue;
            Thing t; t.netId = kPawnBase + static_cast<uint32_t>(p); t.type = 0; t.owner = p;
            t.x = c.pawnPos(p).x; t.y = c.pawnPos(p).y; t.hp = 100;
            v.push_back(t);
        }
        for (int z = 0; z < kEnemies; ++z) {
            Thing t; t.netId = kEnemyBase + static_cast<uint32_t>(z); t.type = 1;
            t.x = c.zombiePos(z).x; t.y = c.zombiePos(z).y;
            t.hp = c.zombieAlive(z) ? c.zombieHp(z) : 0;
            t.dead = !c.zombieAlive(z);
            t.owner = c.zombieAlive(z) ? 0 : -1;
            t.epoch = c.zombieEpoch(z);
            v.push_back(t);
        }
        for (int d = 0; d < kDoors; ++d) {
            Thing t; t.netId = kDoorBase + static_cast<uint32_t>(d); t.type = 3; t.open = c.doorOpen(d); t.owner = 0;
            v.push_back(t);
        }
        for (int it = 0; it < kItems; ++it) {
            Thing t; t.netId = kItemBase + static_cast<uint32_t>(it); t.type = 2;
            t.holder = c.itemHolder(it); t.owner = c.itemHolder(it) < 0 ? 0 : c.itemHolder(it);
            t.epoch = c.itemEpoch(it);
            v.push_back(t);
        }
        std::sort(v.begin(), v.end(), [](const Thing& a, const Thing& b) { return a.netId < b.netId; });
        return v;
    }

    uint64_t digest(int m, int cat) const override {
        uint64_t h = 1469598103934665603ull + static_cast<uint64_t>(cat);
        if (cat == 0) { h ^= cores_[static_cast<size_t>(m)]->worldEpoch(); h *= 1099511628211ull; return h; }
        for (const Thing& t : view(m)) {
            h ^= static_cast<uint64_t>(t.netId) + static_cast<uint64_t>(t.x) * 2u + static_cast<uint64_t>(t.y) * 3u +
                 static_cast<uint64_t>(t.holder + 1) * 7919u + static_cast<uint64_t>(t.owner + 1) * 104729u +
                 static_cast<uint64_t>(t.epoch) * 31u + static_cast<uint64_t>(t.hp) * 17u + (t.open ? 13u : 0u) + (t.dead ? 101u : 0u);
            h *= 1099511628211ull;
        }
        return h;
    }

private:
    static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
    static int modSlot(int v, int mod) { return ((v % mod) + mod) % mod; }

    std::vector<int> itemsWhere(const Re2Core& c, int holder) const {
        std::vector<int> r;
        for (int it = 0; it < kItems; ++it) if (c.itemHolder(it) == holder) r.push_back(it);
        return r;
    }
    int pickSlot(const std::vector<int>& slots, int which) const {
        if (slots.empty()) return -1;
        int n = static_cast<int>(slots.size());
        return slots[static_cast<size_t>(modSlot(which, n))];
    }

    Bugs bugs_;
    LossyLink net_;
    int n_ = 0;
    bool frozen_ = false;
    uint64_t now_ = 0, next50_ = 50, next200_ = 200;
    Rng wander_;
    std::vector<bool> up_;
    std::vector<std::unique_ptr<Re2Core>> cores_;
    Re2Core::SavedWorld saved_;
    bool hasSave_ = false;
};

std::unique_ptr<ITarget> makeRe2(const Scenario& s, std::string* err) {
    Bugs b;
    for (const std::string& n : s.bugs) {
        if (n == "no_snapshot_dedupe") b.noSnapshotDedupe = true;
        else if (n == "no_request_dedupe") b.noRequestDedupe = true;
        else if (n == "no_world_epoch") b.noWorldEpoch = true;
        else { if (err) *err = "unknown bug class for target re2: " + n; return nullptr; }
    }
    return std::make_unique<Re2Target>(s, b);
}

const TargetRegistrar kRegisterRe2(TargetInfo{
    "re2",
    "RE2 remake online prototype: host-authoritative versioned snapshot + ordered request streams + world epoch (save/load, join/leave, ownership epochs), 3 bug classes",
    {"no_snapshot_dedupe", "no_request_dedupe", "no_world_epoch"},
    makeRe2});

#endif  // COOPNET_MOD_CORE_ONLY

}  // namespace
}  // namespace coopfuzz
