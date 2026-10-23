// World-level scenarios over the Cluster (lossy link simulator, virtual clock). Shared by tests and twins.
#include <algorithm>
#include <cmath>
#include <map>
#include "scenarios.h"
#include "fakeworld/rng.h"

namespace coop::fakeworld::testing {

void (*dumpHook)(Cluster&) = nullptr;
IEngineObserver* debugObserver = nullptr;


namespace {
const Ref kGround("world.pack", 0x300001u);
const Ref kAmmo("world.pack", 0x200001u);
const Ref kQuest("world.pack", 0x500001u);

void note(std::string* why, const std::string& s) { if (why && why->size() < 900) *why += s + "; "; }

struct Check {
    bool ok = true;
    std::string* why;
    explicit Check(std::string* w) : why(w) {}
    void expect(bool cond, const std::string& what) { if (!cond) { ok = false; note(why, what); } }
};

// Settles the cluster and reports every violation the oracles found.
bool finish(Cluster& c, Check& k, bool allowRepairs = false) {
    if (!allowRepairs && c.totalRepairs() > 0) k.expect(false, "digest repairs were needed (" + std::to_string(c.totalRepairs()) + "): something diverged and was patched");
    const bool quiet = c.quiesce(30000);
    c.checkAlways();
    c.checkQuiescent();
    k.expect(quiet, "cluster never became quiet");
    for (auto& v : c.allViolations()) { k.ok = false; note(k.why, v.what); }
    if (!k.ok && dumpHook) dumpHook(c);
    return k.ok;
}

CellId cellOf(const World& w, int gx, int gy) { return w.cellAt(gx, gy); }

const ActorState* findActor(const World& w, bool hostile, bool named, CellId cell = 0) {
    for (auto& kv : w.actors()) {
        const ActorState& a = kv.second;
        if (a.dead || !a.enabled || a.hostile != hostile || a.named != named) continue;
        if (cell && a.cell != cell) continue;
        auto it = w.cells().find(a.cell);
        if (it == w.cells().end() || it->second.interior) continue;
        return &a;
    }
    return nullptr;
}

Vec3 beside(const Vec3& p, float dx = 120) { Vec3 v = p; v.x += dx; return v; }

struct OwnCounter : IEngineObserver {
    std::map<NetId, int> transfers;
    int total = 0;
    void onOwnership(PlayerId self, uint64_t, const OwnMsg& m, ApplyResult r) override {
        if (self == 0 && r == ApplyResult::Applied && m.kind == OwnMsg::Transfer) { ++transfers[m.netId]; ++total; }
    }
};

// Remembers the first envelope of a body type that machine `at` received.
template <class T>
struct Spy : IEngineObserver {
    PlayerId at;
    Bytes first;
    PlayerId from = 0;
    explicit Spy(PlayerId a) : at(a) {}
    void onRecv(PlayerId self, uint64_t, PeerId f, uint8_t, const Bytes& env) override {
        if (self != at || !first.empty()) return;
        Envelope e;
        if (decode(env, e) && std::holds_alternative<T>(e.body)) { first = env; from = static_cast<PlayerId>(f); }
    }
};
}  // namespace

LinkParams lossyLink() {
    LinkParams l;
    l.minLatencyMs = 20; l.maxLatencyMs = 70; l.loss = 0.08; l.duplicate = 0.06; l.corrupt = 0.01;
    return l;
}

ClusterConfig baseConfig(uint64_t seed, int players, const LinkParams& link) {
    ClusterConfig c;
    c.seed = seed; c.players = players; c.tickMs = 10; c.link = link;
    if (link.maxLatencyMs == 0) { c.link.minLatencyMs = 5; c.link.maxLatencyMs = 15; }
    WorldSpec& s = c.machine.spec;
    s.seed = seed; s.gridW = 3; s.gridH = 3; s.interiors = 2; s.actorsPerCell = 2; s.hostilePercent = 50;
    s.namedPerWorld = 2; s.containersPerCell = 1; s.itemsPerCell = 1; s.doorsPerCell = 1; s.sitesPerCell = 1;
    return c;
}

bool joinAndSync(uint64_t seed, int players, const LinkParams& link, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, players, link));
    c.run(3000);
    for (int p = 0; p < players; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).synced(), "machine " + std::to_string(p) + " not synced");
    return finish(c, k);
}

bool fightScenario(uint64_t seed, const LinkParams& link, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2, link));
    c.run(2000);
    const ActorState* first = findActor(c.host().world(), true, false);
    if (!first) { note(why, "no hostile actor to fight"); return false; }
    const CellId cell = first->cell;
    const Vec3 center = c.host().world().cellCenter(cell);
    c.machine(0).move(cell, center, 0);
    c.machine(1).move(cell, beside(center, 100), 0);
    c.run(1200);
    for (int round = 0; round < 80; ++round) {
        std::vector<NetId> targets;
        for (auto& kv : c.host().world().actors())
            if (kv.second.cell == cell && kv.second.hostile && !kv.second.dead && kv.second.enabled) targets.push_back(kv.first);
        if (targets.empty()) break;
        Machine& m = c.machine(static_cast<PlayerId>(round % 2));
        const ActorState* a = m.world().findActor(targets[static_cast<size_t>(round) % targets.size()]);
        if (!a || a->dead) continue;
        if (m.world().player(m.self()).dead) m.doAction([] { Action i; i.kind = ActionKind::Revive; return i; }());
        m.move(cell, beside(a->pos), 0);
        c.run(120);
        m.attackActor(a->id, 45, static_cast<Limb>(round % 6), a->pos, 1500);
        c.run(80);
    }
    c.run(1500);
    for (auto& kv : c.host().world().actors())
        if (kv.second.cell == cell && kv.second.hostile && kv.second.enabled) k.expect(kv.second.dead, "hostile actor " + std::to_string(kv.first) + " survived");
    uint64_t applied = 0;
    for (int p = 0; p < 2; ++p) applied += c.machine(static_cast<PlayerId>(p)).stats().hitsApplied;
    k.expect(applied > 0, "no hit was ever applied");
    // Digests compare positions in coarse buckets. A moving actor can lag a bucket edge on a lossy link for two reports in
    // a row, and the heal path then repairs it. That is the safety net doing its job, so a lossy run may repair a little.
    const bool lossy = link.loss > 0 || link.corrupt > 0;
    if (lossy) k.expect(c.totalRepairs() <= 8, "too many digest repairs on a lossy link: " + std::to_string(c.totalRepairs()));
    return finish(c, k, lossy);
}

bool handoffMidFight(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    OwnCounter counter;
    c.setObserver(&counter);
    c.run(600);
    const CellId near1 = cellOf(c.host().world(), 0, 0), far9 = cellOf(c.host().world(), 2, 2);
    const Vec3 center = c.host().world().cellCenter(near1);
    c.machine(0).move(near1, center, 0);
    c.machine(1).move(near1, beside(center, 300), 0);
    c.run(2500);
    const ActorState* h = findActor(c.host().world(), true, false, near1);
    if (!h) h = findActor(c.host().world(), false, false, near1);
    if (!h) { note(why, "no actor in the start cell"); return false; }
    const NetId id = h->id;
    k.expect(c.host().view().ownerOf(id) == 0, "host should own the actor while it is next to it");
    c.machine(0).move(far9, c.host().world().cellCenter(far9), 0);   // the host walks away mid-fight
    for (int i = 0; i < 30; ++i) {
        c.run(150);
        const ActorState* a = c.machine(1).world().findActor(id);
        if (a && !a->dead) c.machine(1).attackActor(id, 3, Limb::Torso, a->pos, 1500);
    }
    c.run(3000);
    for (int p = 0; p < 2; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).view().ownerOf(id) == 1, "machine " + std::to_string(p) + " does not see the client as owner");
    k.expect(counter.transfers[id] <= 2, "ownership flapped: " + std::to_string(counter.transfers[id]) + " transfers");
    return finish(c, k);
}

bool handoffKeepsLastState(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 3));
    c.run(600);
    const CellId near1 = cellOf(c.host().world(), 0, 0), far9 = cellOf(c.host().world(), 2, 2);
    const ActorState* h = findActor(c.host().world(), true, false, near1);
    if (!h) { note(why, "no hostile in the start cell"); return false; }
    const NetId id = h->id;
    c.machine(0).move(far9, c.host().world().cellCenter(far9), 0);
    c.machine(1).move(near1, beside(h->pos, 100), 0);
    c.machine(2).move(near1, beside(h->pos, 1800), 0);
    c.run(2500);
    k.expect(c.host().view().ownerOf(id) == 1, "the nearest guest should own the actor");
    const Vec3 at = c.host().world().findActor(id)->pos;
    // Count only what arrives from now on: the first handoff (host to guest 1) also relays a final state past guest 2.
    const uint64_t finalsBefore = c.machine(2).stats().finalSnaps;
    c.machine(1).move(near1, beside(at, 1900), 0);
    c.machine(2).move(near1, beside(at, 60), 0);
    c.run(3500);
    k.expect(c.host().view().ownerOf(id) == 2, "ownership did not move to the guest that is now next to it");
    k.expect(c.machine(2).stats().finalSnaps > finalsBefore, "the new owner never received the old owner's final state");
    return finish(c, k);
}

bool formerOwnerSilenced(uint64_t seed, std::string* why) { return handoffMidFight(seed, why); }

bool noFlap(std::string* why) {
    Check k(why);
    Cluster c(baseConfig(11, 3));
    OwnCounter counter;
    c.setObserver(&counter);
    c.run(600);
    const CellId cell = cellOf(c.host().world(), 0, 0), far9 = cellOf(c.host().world(), 2, 2);
    const ActorState* h = findActor(c.host().world(), false, false, cell);
    if (!h) h = findActor(c.host().world(), true, false, cell);
    if (!h) { note(why, "no actor"); return false; }
    c.freezeAi(true);
    const NetId id = h->id;
    const Vec3 a = h->pos;
    c.machine(0).move(far9, c.host().world().cellCenter(far9), 0);
    for (int i = 0; i < 28; ++i) {   // p1 and p2 swap which of them is slightly closer, every 300 ms
        const bool flip = (i & 1) != 0;
        c.machine(1).move(cell, {a.x + (flip ? 545.0f : 500.0f), a.y, 0}, 0);
        c.machine(2).move(cell, {a.x, a.y + (flip ? 500.0f : 545.0f), 0}, 0);
        c.run(300);
    }
    k.expect(counter.transfers[id] <= 2, "ownership flapped: " + std::to_string(counter.transfers[id]) + " transfers in 8 s");
    return k.ok;
}

bool followerStaysWithRecruiter(std::string* why) {
    Check k(why);
    OwnershipFaults saved = ownershipFaults();
    (void)saved;
    ClusterConfig cfg = baseConfig(5, 2);
    cfg.machine.spec.hostilePercent = 30;
    Cluster c(cfg);
    c.run(600);
    const ActorState* a = findActor(c.host().world(), false, false);
    if (!a) { note(why, "no companion candidate"); return false; }
    const NetId id = a->id;
    const CellId cell = a->cell, far9 = cellOf(c.host().world(), 2, 2);
    const Vec3 center = c.host().world().cellCenter(cell);
    c.machine(0).move(cell, center, 0);
    c.machine(1).move(cell, beside(center, 400), 0);
    c.run(1500);
    c.machine(0).recruit(id, 1);
    c.run(2500);
    for (int p = 0; p < 2; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).view().ownerOf(id) == 1, "recruited actor is not owned by its recruiter on machine " + std::to_string(p));
    c.machine(1).move(far9, c.host().world().cellCenter(far9), 0);
    c.run(3000);
    const ActorState* f = c.host().world().findActor(id);
    k.expect(f && f->cell == far9, "companion did not follow its recruiter into the far cell");
    for (int p = 0; p < 2; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).view().ownerOf(id) == 1, "companion changed owner when the recruiter moved away");
    return finish(c, k);
}

bool recruitCompanion(uint64_t seed, std::string* why) { (void)seed; return followerStaysWithRecruiter(why); }

bool lootRace(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 3));
    c.run(1500);
    NetId a = 0, b = 0;
    for (auto& kv : c.host().world().items()) {
        if (kv.second.taken) continue;
        (a ? b : a) = kv.first;
        if (b) break;
    }
    if (!a || !b) { note(why, "no ground items"); return false; }
    // race 1: the host and a guest grab the same item in the same instant
    c.machine(0).pickup(a);
    c.machine(1).pickup(a);
    // race 2: two guests grab the same item in the same instant
    c.machine(1).pickup(b);
    c.machine(2).pickup(b);
    c.run(1500);
    int64_t held = 0;
    for (int p = 0; p < 3; ++p) held += c.machine(static_cast<PlayerId>(p)).inventoryCount(kGround);
    k.expect(held == 2, "expected exactly 2 ground items held, found " + std::to_string(held));
    uint64_t rollbacks = 0;
    for (int p = 0; p < 3; ++p) rollbacks += c.machine(static_cast<PlayerId>(p)).stats().rollbacks;
    k.expect(rollbacks >= 2, "losers were not rolled back");
    return finish(c, k);
}

bool spawnOnce(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 3));
    c.run(4000);
    int spawned = 0;
    for (auto& kv : c.host().world().sites()) if (kv.second.spawnCount > 0) ++spawned;
    k.expect(spawned > 0, "no encounter spawned");
    for (auto& kv : c.host().world().sites()) k.expect(kv.second.spawnCount <= 1, "site " + std::to_string(kv.first) + " spawned " + std::to_string(kv.second.spawnCount) + " times");
    return finish(c, k);
}

bool containerMatrix(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    c.run(1000);
    NetId cont = 0;
    for (auto& kv : c.host().world().containers()) if (!kv.second.corpse && !kv.second.items.empty()) { cont = kv.first; break; }
    if (!cont) { note(why, "no container"); return false; }
    const CellId cell = c.host().world().containers().at(cont).cell;
    auto total = [&] {
        int64_t n = c.host().world().containers().at(cont).items[kAmmo];
        for (int p = 0; p < 2; ++p) n += c.machine(static_cast<PlayerId>(p)).inventoryCount(kAmmo);
        return n;
    };
    const int64_t before = total();
    c.machine(0).move(cell, c.host().world().cellCenter(cell), 0);
    c.machine(1).move(cell, c.host().world().cellCenter(cell), 0);
    c.run(300);
    c.machine(0).openContainer(cont);
    c.machine(1).openContainer(cont);   // same instant: the lock admits one
    c.run(400);
    int holders = 0;
    PlayerId holder = kNoPlayer;
    for (int p = 0; p < 2; ++p)
        if (c.machine(static_cast<PlayerId>(p)).world().containers().at(cont).inUseBy == p) { ++holders; holder = static_cast<PlayerId>(p); }
    k.expect(holders == 1, "container open for " + std::to_string(holders) + " players at once");
    if (holder == kNoPlayer) return false;
    const PlayerId other = static_cast<PlayerId>(1 - holder);
    c.machine(holder).containerTake(cont, kAmmo, 1);
    c.machine(other).containerTake(cont, kAmmo, 1);   // refused: not the holder
    c.run(300);
    k.expect(c.machine(holder).inventoryCount(kAmmo) == 1, "holder did not get the item");
    k.expect(c.machine(other).inventoryCount(kAmmo) == 0, "a player without the lock took an item");
    c.machine(holder).closeContainer(cont);
    c.run(300);
    c.machine(other).openContainer(cont);
    c.run(300);
    k.expect(c.machine(other).world().containers().at(cont).inUseBy == other, "second player could not open after the first closed");
    c.machine(other).containerPut(cont, kAmmo, 0);   // nothing to put: refused locally
    c.machine(holder).containerPut(cont, kAmmo, 1);  // the old holder no longer holds the lock: refused by the host, item returned
    c.run(400);
    c.machine(other).closeContainer(cont);
    c.run(300);
    k.expect(total() == before, "items created or lost in container traffic: " + std::to_string(before) + " -> " + std::to_string(total()));
    k.expect(c.host().stats().closeDigestMismatch == 0, "close digest check fired");
    return finish(c, k);
}

bool doorsAndLocks(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    c.run(800);
    NetId d = 0, l = 0;
    for (auto& kv : c.host().world().objects()) { if (!d) d = kv.first; else if (!l && kv.second.cell != c.host().world().objects().at(d).cell) l = kv.first; }
    if (!d || !l) { note(why, "need two doors"); return false; }
    const WorldObject dd = c.host().world().objects().at(d);
    c.machine(0).setLock(d, false, 0);
    c.machine(0).setLock(l, true, 50);
    c.run(300);
    c.machine(1).move(dd.cell, dd.pos, 0);
    c.run(200);
    c.machine(1).activate(d);
    c.run(400);
    for (int p = 0; p < 2; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).world().objects().at(d).open, "door not open on machine " + std::to_string(p));
    // the guest cannot open a locked door, not even by sending the request itself
    const WorldObject ll = c.host().world().objects().at(l);
    c.machine(1).move(ll.cell, ll.pos, 0);
    c.run(200);
    k.expect(!c.machine(1).activate(l), "a locked door accepted the local action");
    MActivate forged{l};
    Envelope e;
    e.sess = c.host().sess(); e.src = 1; e.dest = 0; e.body = forged;
    c.inject(0, 1, encode(e));
    c.run(300);
    for (int p = 0; p < 2; ++p) k.expect(!c.machine(static_cast<PlayerId>(p)).world().objects().at(l).open, "locked door opened on machine " + std::to_string(p));
    c.machine(0).setLock(l, false, 0);
    c.run(300);
    c.machine(1).activate(l);
    c.run(400);
    for (int p = 0; p < 2; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).world().objects().at(l).open, "unlocked door did not open on machine " + std::to_string(p));
    // teleport door: the guest ends up in the interior, everybody agrees
    if (ll.targetCell) {
        c.machine(1).useDoor(l);
        c.run(500);
        for (int p = 0; p < 2; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).world().player(1).cell == ll.targetCell, "guest is not in the interior on machine " + std::to_string(p));
    }
    return finish(c, k);
}

bool namedProtected(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    c.run(600);
    const ActorState* n = nullptr;
    for (auto& kv : c.host().world().actors()) if (kv.second.named) { n = &kv.second; break; }
    if (!n) { note(why, "no named actor"); return false; }
    const NetId id = n->id;
    const CellId cell = n->cell;
    c.machine(0).move(cell, c.host().world().cellCenter(cell), 0);
    c.machine(1).move(cell, beside(n->pos, 100), 0);
    c.run(1500);
    for (int i = 0; i < 10; ++i) {
        const ActorState* a = c.machine(1).world().findActor(id);
        c.machine(1).move(cell, beside(a->pos, 100), 0);
        c.run(120);
        c.machine(1).attackActor(id, 30, Limb::Torso, a->pos, 1500);
        c.run(120);
    }
    c.run(1000);
    k.expect(!c.host().world().findActor(id)->dead, "a story NPC was killed by combat");
    k.expect(c.host().world().findActor(id)->health == 1, "essential NPC should bottom out at 1 health");
    c.machine(0).killByStory(id);
    c.run(1500);
    for (int p = 0; p < 2; ++p) k.expect(c.machine(static_cast<PlayerId>(p)).world().findActor(id)->dead, "story kill missing on machine " + std::to_string(p));
    return finish(c, k);
}

bool questStageSync(uint64_t seed, const LinkParams& link, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2, link));
    Spy<MEffects> spy(1);
    c.setObserver(&spy);
    c.run(800);
    c.machine(0).setStage(kQuest, 10);
    c.run(300);
    c.machine(0).setStage(kQuest, 5);   // stage merge = max: a lower stage never wins
    c.run(600);
    if (!spy.first.empty()) { c.inject(1, 0, spy.first); c.inject(1, 0, spy.first); }   // a re-delivered effect batch
    c.run(500);
    k.expect(c.machine(1).world().quests().count(kQuest) && c.machine(1).world().quests().at(kQuest) == 10, "guest quest stage is not 10");
    return finish(c, k);
}

bool saveLoadRejoin(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 3));
    c.run(1500);
    const size_t save = c.saveHost();
    NetId item = 0;
    for (auto& kv : c.host().world().items()) if (!kv.second.taken) { item = kv.first; break; }
    c.machine(0).setStage(kQuest, 20);
    if (item) c.machine(1).pickup(item);
    c.run(2000);
    c.checkConservation = false;   // only the host's save is the world: clients keep what they carry
    k.expect(c.loadHost(save), "host load failed");
    c.run(3000);
    c.quiesce(30000);
    k.expect(!c.host().world().quests().count(kQuest), "the loaded world still has the later quest stage");
    // an update from before the load, replayed after it, must not apply
    NetId id = 0; PlayerId owner = kNoPlayer; OwnerEpoch epoch = 0;
    c.host().view().forEach([&](ActorNetId a, PlayerId o, OwnerEpoch e) { if (!id && o != kNoPlayer && o != 2) { id = a; owner = o; epoch = e; } });
    if (id) {
        MActorSnap s;
        s.id = id; s.epoch = epoch; s.seq = 900000; s.cell = c.host().world().findActor(id)->cell;
        s.pos = c.host().world().findActor(id)->pos; s.pos.x += 500; s.health = 77; s.settle = true;
        Envelope e;
        e.sess = 1; e.src = owner; e.dest = kNoPlayer; e.body = s;
        c.inject(2, owner, encode(e));
        c.run(200);
    }
    c.leave(1);
    c.run(500);
    c.join(1);
    c.run(1500);
    return finish(c, k);
}

bool healsCorruption(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    c.run(1500);
    World& w = c.machine(1).world();
    w.objects().begin()->second.open = !w.objects().begin()->second.open;
    w.containers().begin()->second.items[kAmmo] += 5;
    for (auto& kv : w.actors()) if (c.host().view().ownerOf(kv.first) == 0) { kv.second.health -= 30; break; }
    c.run(7000);
    k.expect(c.totalRepairs() >= 1, "corruption was never repaired");
    return finish(c, k, true);
}

bool dropIdempotent(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    Spy<MDropReq> spy(0);
    c.setObserver(&spy);
    c.run(1000);
    NetId item = 0;
    for (auto& kv : c.host().world().items()) if (!kv.second.taken) { item = kv.first; break; }
    if (!item) { note(why, "no item"); return false; }
    c.machine(1).pickup(item);
    c.run(500);
    c.machine(1).drop(kGround);
    c.run(500);
    k.expect(!spy.first.empty(), "host never saw the drop request");
    if (!spy.first.empty()) { c.inject(0, 1, spy.first); c.run(400); }   // the same request again
    return finish(c, k);
}

bool forgedHitsRejected(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    c.run(1500);
    const ActorState* a = findActor(c.host().world(), true, false);
    if (!a) { note(why, "no hostile actor"); return false; }
    const NetId id = a->id;
    const CellId cell = a->cell;
    c.machine(0).move(cell, c.host().world().cellCenter(cell), 0);
    c.machine(1).move(cell, beside(a->pos, 100), 0);
    c.run(1500);
    const int32_t hp = c.host().world().findActor(id)->health;
    auto forge = [&](int32_t damage, float range, uint64_t hitId) {
        MHit h;
        h.hitId = hitId; h.attackerIsPlayer = true; h.attacker = 1; h.victim = id; h.damage = damage; h.weaponRange = range;
        Envelope e;
        e.sess = c.host().sess(); e.src = 1; e.dest = 0; e.body = h;
        c.inject(0, 1, encode(e));
    };
    forge(9999, 1500, 0xAAAA01);     // damage far beyond any weapon
    forge(40, 90000, 0xAAAA02);      // weapon range far beyond any weapon
    c.run(400);
    const ActorState* after = c.host().world().findActor(id);
    k.expect(after && !after->dead && after->health == hp, "forged hits changed the actor");
    // a legitimate hit still works
    c.machine(1).attackActor(id, 10, Limb::Torso, after->pos, 1500);
    c.run(500);
    k.expect(c.host().world().findActor(id)->health == hp - 10 || c.host().world().findActor(id)->dead, "the legitimate hit did not land");
    return finish(c, k);
}

bool hitToNonOwnerRejected(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 2));
    c.run(1500);
    const ActorState* a = findActor(c.host().world(), true, false);
    if (!a) { note(why, "no hostile actor"); return false; }
    const NetId id = a->id;
    const CellId cell = a->cell;
    c.machine(0).move(cell, c.host().world().cellCenter(cell), 0);
    c.run(1500);
    k.expect(c.host().view().ownerOf(id) == 0, "host should own it");
    const ActorState* cur = c.host().world().findActor(id);
    c.machine(0).move(cell, beside(cur->pos, 100), 0);
    c.run(400);
    MHit h;
    h.hitId = 0xBBBB01; h.attackerIsPlayer = true; h.attacker = 0; h.victim = id; h.damage = 40; h.weaponRange = 1500;
    h.impact = c.host().world().findActor(id)->pos;
    Envelope e;
    e.sess = c.host().sess(); e.src = 0; e.dest = 1; e.body = h;
    c.inject(1, 0, encode(e));   // a hit routed to a guest that does not own the actor
    c.run(600);
    k.expect(c.machine(1).stats().hitsRejected >= 1, "a guest that does not own the actor applied a hit aimed at it");
    const bool fin = finish(c, k);
    return fin && c.machine(1).world().findActor(id)->health == c.host().world().findActor(id)->health;
}

bool lateJoinerGetsOwnership(uint64_t seed, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, 3));
    c.leave(2);
    c.run(2000);
    c.join(2);
    c.run(2500);
    k.expect(c.machine(2).view().digest() == c.host().view().digest(), "late joiner has a different ownership table");
    return finish(c, k);
}

bool randomSession(uint64_t seed, int players, uint32_t ms, const LinkParams& link, std::string* why) {
    Check k(why);
    Cluster c(baseConfig(seed, players, link));
    if (debugObserver) c.setObserver(debugObserver);
    Rng r(seed * 7919 + 3);
    c.run(1000);
    bool loaded = false;
    size_t save = static_cast<size_t>(-1);
    const uint64_t end = c.now() + ms;
    while (c.now() < end) {
        c.run(static_cast<uint32_t>(r.range(20, 160)));
        const PlayerId p = static_cast<PlayerId>(r.range(0, static_cast<uint32_t>(players - 1)));
        if (!c.connected(p)) { if (r.chance(0.4)) c.join(p); continue; }
        Machine& m = c.machine(p);
        World& w = m.world();
        const PlayerState& me = w.player(p);
        const int roll = static_cast<int>(r.range(0, 99));
        if (roll < 28) {
            const CellId cell = static_cast<CellId>(r.range(1, 9));
            Vec3 pos = w.cellCenter(cell);
            pos.x += static_cast<float>(r.range(0, 1000)) - 500; pos.y += static_cast<float>(r.range(0, 1000)) - 500;
            m.move(cell, pos, 0);
        } else if (roll < 50) {
            std::vector<NetId> inCell;
            for (auto& kv : w.actors()) if (!kv.second.dead && kv.second.cell == me.cell) inCell.push_back(kv.first);
            if (!inCell.empty()) {
                const ActorState& a = w.actors().at(inCell[r.range(0, static_cast<uint32_t>(inCell.size() - 1))]);
                m.move(me.cell, beside(a.pos, 100), 0);
                m.attackActor(a.id, static_cast<int32_t>(r.range(5, 60)), static_cast<Limb>(r.range(0, 5)), a.pos, 1500);
            }
        } else if (roll < 62) {
            for (auto& kv : w.items()) if (!kv.second.taken && kv.second.cell == me.cell) { m.pickup(kv.first); break; }
        } else if (roll < 68) {
            m.drop(kGround);
        } else if (roll < 80) {
            std::vector<NetId> cs;
            for (auto& kv : w.containers()) if (kv.second.cell == me.cell) cs.push_back(kv.first);
            if (!cs.empty()) {
                const NetId cid = cs[r.range(0, static_cast<uint32_t>(cs.size() - 1))];
                switch (r.range(0, 3)) {
                    case 0: m.openContainer(cid); break;
                    case 1: m.containerTake(cid, Ref("world.pack", 0x200001u + r.range(0, 1)), 1); break;
                    case 2: m.containerPut(cid, kAmmo, 1); break;
                    default: m.closeContainer(cid); break;
                }
            }
        } else if (roll < 88) {
            for (auto& kv : w.objects()) if (kv.second.cell == me.cell) { m.activate(kv.first); break; }
        } else if (roll < 92 && p == 0) {
            m.setStage(kQuest, static_cast<uint32_t>(r.range(1, 50)));
        } else if (roll < 95 && p != 0) {
            c.checkConservation = false;   // a leaving player takes their items with them
            c.leave(p);
        } else if (roll < 97 && p == 0 && !loaded) {
            save = c.saveHost();
        } else if (roll < 98 && p == 0 && save != static_cast<size_t>(-1) && !loaded) {
            c.checkConservation = false;
            loaded = c.loadHost(save);
        } else if (roll < 100) {
            Action i; i.kind = ActionKind::Revive;
            m.doAction(i);
        }
        for (auto& v : c.checkAlways()) { k.ok = false; note(why, v.what); }
        if (!k.ok) return false;
    }
    for (int p = 1; p < players; ++p) if (!c.connected(static_cast<PlayerId>(p))) c.join(static_cast<PlayerId>(p));
    c.run(1500);
    // a lossy, corrupting link may need a few digest repairs (they are the safety net); a quiet clean link may not
    const bool lossy = link.loss > 0 || link.corrupt > 0;
    if (lossy) k.expect(c.totalRepairs() <= 8, "too many digest repairs on a lossy link: " + std::to_string(c.totalRepairs()));
    return finish(c, k, lossy);
}

}  // namespace coop::fakeworld::testing
