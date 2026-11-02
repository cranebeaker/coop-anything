#include "fakeworld/world.h"
#include <algorithm>
#include <cmath>
#include "fakeworld/wire.h"

namespace coop::fakeworld {

const char* toString(Category c) {
    switch (c) {
        case Category::Story: return "story";
        case Category::Actors: return "actors";
        case Category::Containers: return "containers";
        case Category::Objects: return "objects";
        default: return "?";
    }
}

float distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
float distance2d(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ---- serialisation ---------------------------------------------------------------------------

namespace {
void ioItems(Archive& a, std::map<Ref, int32_t>& m) {
    size_t n = m.size();
    if (!a.count(n, 4096)) return;
    if (a.reading()) {
        m.clear();
        for (size_t i = 0; i < n && a.ok(); ++i) {
            Ref f; int32_t c = 0;
            io(a, f); a.vari32(c);
            m[f] = c;
        }
    } else {
        for (auto& kv : m) { Ref f = kv.first; int32_t c = kv.second; io(a, f); a.vari32(c); }
    }
}
}  // namespace

void io(Archive& a, Vec3& v) { a.f32(v.x); a.f32(v.y); a.f32(v.z); }
void io(Archive& a, CellDef& v) { a.varu32(v.id); a.boolean(v.interior); a.vari32(v.gx); a.vari32(v.gy); }
void io(Archive& a, ActorState& v) {
    a.varu32(v.id); io(a, v.ref); io(a, v.base); a.boolean(v.created); a.varu32(v.cell); io(a, v.pos); a.f32(v.yaw);
    a.vari32(v.health); a.vari32(v.maxHealth);
    a.boolean(v.dead); a.boolean(v.enabled); a.boolean(v.aiOn); a.boolean(v.hostile); a.boolean(v.named);
    a.u8(v.recruiter); a.varu32(v.site); a.u8(v.limbMask); a.varu32(v.snapSeq); io(a, v.pick);
}
void io(Archive& a, ContainerState& v) {
    a.varu32(v.id); a.varu32(v.cell); io(a, v.pos); a.boolean(v.corpse); ioItems(a, v.items); a.u8(v.inUseBy);
    size_t n = v.appliedOps.size();
    if (!a.count(n, 256)) return;
    if (a.reading()) v.appliedOps.assign(n, 0);
    for (auto& op : v.appliedOps) a.u64(op);
}
void io(Archive& a, ItemState& v) {
    a.varu32(v.id); io(a, v.base); a.varu32(v.cell); io(a, v.pos); a.boolean(v.taken); a.u8(v.takenBy); a.u64(v.origin);
}
void io(Archive& a, WorldObject& v) {
    uint8_t k = static_cast<uint8_t>(v.kind);
    a.varu32(v.id); a.u8(k); v.kind = static_cast<ObjKind>(k % 3); a.varu32(v.cell); io(a, v.pos);
    a.boolean(v.open); a.boolean(v.locked); a.vari32(v.lockLevel); a.boolean(v.destroyed); a.boolean(v.activatedOnce);
    a.boolean(v.enabled); a.varu32(v.targetCell); io(a, v.targetPos);
}
void io(Archive& a, SpawnSiteState& v) {
    a.varu32(v.id); a.varu32(v.cell); io(a, v.pos); io(a, v.variantA); io(a, v.variantB); a.varu32(v.respawnMs);
    a.varu32(v.spawned); a.u8(v.rolledBy); a.varu32(v.spawnCount); a.varu(v.clearedAtMs); a.boolean(v.cleared); a.boolean(v.permanent);
}
void io(Archive& a, PlayerState& v) {
    a.u8(v.slot); a.boolean(v.present); a.varu32(v.cell); io(a, v.pos); a.f32(v.yaw); a.vari32(v.health); a.vari32(v.maxHealth);
    a.boolean(v.dead); a.varu32(v.poseSeq); a.varu32(v.vitalsSeq); ioItems(a, v.inventory);
}

uint32_t keyOfQuest(const Ref& r) {
    uint64_t h = hash64(r.pack.data(), r.pack.size());
    h = hash64(&r.id, sizeof r.id, h);
    return static_cast<uint32_t>(h ^ (h >> 32)) & 0x7FFFFFFFu;
}

// ---- build -----------------------------------------------------------------------------------

World World::build(const WorldSpec& s) {
    World w;
    Rng r(s.seed * 0x9E3779B97F4A7C15ull + 77);
    w.gridW_ = s.gridW; w.gridH_ = s.gridH;
    for (int gy = 0; gy < s.gridH; ++gy)
        for (int gx = 0; gx < s.gridW; ++gx) {
            CellDef c; c.id = static_cast<CellId>(1 + gy * s.gridW + gx); c.gx = gx; c.gy = gy;
            w.cells_[c.id] = c;
        }
    for (int i = 0; i < s.interiors; ++i) {
        CellDef c; c.id = static_cast<CellId>(1000 + i); c.interior = true;
        w.cells_[c.id] = c;
    }
    auto place = [&](CellId cell) {
        Vec3 c = w.cellCenter(cell);
        c.x += (static_cast<float>(r.uni()) - 0.5f) * (kCellSize - 400.0f);
        c.y += (static_cast<float>(r.uni()) - 0.5f) * (kCellSize - 400.0f);
        return c;
    };
    NetId nActor = 0x1000, nCont = 0x2000, nItem = 0x3000, nObj = 0x4000, nSite = 0x5000;
    int named = 0;
    for (auto& kv : w.cells_) {
        const CellDef& cd = kv.second;
        const int actors = cd.interior ? 1 : s.actorsPerCell;
        for (int i = 0; i < actors; ++i) {
            ActorState a;
            a.id = nActor++; a.ref = placedRef(a.id);
            a.hostile = static_cast<int>(r.range(0, 99)) < s.hostilePercent;
            if (named < s.namedPerWorld && !cd.interior) { a.named = true; a.hostile = false; ++named; }
            a.base = Ref("world.pack", a.hostile ? 0x100001u : 0x100002u);
            a.pick = a.base;
            a.cell = cd.id; a.pos = place(cd.id); a.yaw = static_cast<float>(r.uni()) * 6.28f;
            w.putActor(a);
        }
        const int conts = cd.interior ? 1 : s.containersPerCell;
        for (int i = 0; i < conts; ++i) {
            ContainerState c; c.id = nCont++; c.cell = cd.id; c.pos = place(cd.id);
            c.items[Ref("world.pack", 0x200001u)] = static_cast<int32_t>(r.range(1, 5));
            c.items[Ref("world.pack", 0x200002u)] = static_cast<int32_t>(r.range(1, 3));
            w.containers_[c.id] = c;
        }
        if (!cd.interior) {
            for (int i = 0; i < s.itemsPerCell; ++i) {
                ItemState it; it.id = nItem++; it.base = Ref("world.pack", 0x300001u); it.cell = cd.id; it.pos = place(cd.id);
                w.items_[it.id] = it;
            }
            for (int i = 0; i < s.doorsPerCell; ++i) {
                WorldObject o; o.id = nObj++; o.kind = ObjKind::Door; o.cell = cd.id; o.pos = place(cd.id);
                o.locked = r.chance(0.3); o.lockLevel = o.locked ? static_cast<int32_t>(r.range(1, 100)) : 0;
                if (s.interiors > 0) { o.targetCell = static_cast<CellId>(1000 + r.range(0, s.interiors - 1)); o.targetPos = w.cellCenter(o.targetCell); }
                w.refToNet_[placedRef(o.id)] = o.id;
                w.objects_[o.id] = o;
            }
        }
        const int sites = cd.interior ? 1 : s.sitesPerCell;
        for (int i = 0; i < sites; ++i) {
            SpawnSiteState st; st.id = nSite++; st.cell = cd.id; st.pos = place(cd.id);
            st.variantA = Ref("world.pack", 0x100011u); st.variantB = Ref("world.pack", 0x100012u);
            st.respawnMs = s.respawnMs; st.permanent = cd.interior;
            w.sites_[st.id] = st;
        }
    }
    return w;
}

// ---- cells -----------------------------------------------------------------------------------

CellId World::cellAt(int gx, int gy) const {
    if (gx < 0 || gy < 0 || gx >= gridW_ || gy >= gridH_) return 0;
    return static_cast<CellId>(1 + gy * gridW_ + gx);
}

bool World::adjacent(CellId a, CellId b) const {
    if (a == b) return cells_.count(a) != 0;
    auto ia = cells_.find(a), ib = cells_.find(b);
    if (ia == cells_.end() || ib == cells_.end() || ia->second.interior || ib->second.interior) return false;
    return std::abs(ia->second.gx - ib->second.gx) <= 1 && std::abs(ia->second.gy - ib->second.gy) <= 1;
}

std::vector<CellId> World::loadedAround(CellId c) const {
    std::vector<CellId> out;
    auto it = cells_.find(c);
    if (it == cells_.end()) return out;
    if (it->second.interior) { out.push_back(c); return out; }
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            CellId n = cellAt(it->second.gx + dx, it->second.gy + dy);
            if (n) out.push_back(n);
        }
    return out;
}

Vec3 World::cellCenter(CellId c) const {
    auto it = cells_.find(c);
    Vec3 v;
    if (it == cells_.end()) return v;
    if (it->second.interior) { v.x = -100000.0f - 5000.0f * static_cast<float>(c - 1000); return v; }
    v.x = (static_cast<float>(it->second.gx) + 0.5f) * kCellSize;
    v.y = (static_cast<float>(it->second.gy) + 0.5f) * kCellSize;
    return v;
}

Ref World::cellRef(CellId c) const { return Ref("world.pack", 0x900000u + c); }
CellId World::cellFromRef(const Ref& r) const {
    if (r.pack != "world.pack" || r.id < 0x900000u) return 0;
    CellId c = r.id - 0x900000u;
    return cells_.count(c) ? c : 0;
}

ActorState& World::putActor(const ActorState& a) {
    ActorState& slot = actors_[a.id];
    slot = a;
    refToNet_[a.ref] = a.id;
    noteCreatedId(a.id);
    return slot;
}

void World::eraseActor(NetId id) {
    auto it = actors_.find(id);
    if (it == actors_.end()) return;
    refToNet_.erase(it->second.ref);
    actors_.erase(it);
}

// ---- effects ---------------------------------------------------------------------------------

bool World::applyEffect(const Effect& e) {
    auto ok = [&] { ++fx_.applied; return true; };
    auto missing = [&] { ++fx_.unknownTarget; return false; };
    if (auto* x = std::get_if<SetStage>(&e)) {
        uint32_t& s = quests_[x->quest];
        s = std::max(s, x->stage);
        return ok();
    }
    if (auto* x = std::get_if<StartQuest>(&e)) { quests_.emplace(x->quest, 0u); return ok(); }
    if (auto* x = std::get_if<StopQuest>(&e)) { quests_.erase(x->quest); return ok(); }
    if (auto* x = std::get_if<KillActor>(&e)) {
        NetId id = actorByRef(x->actor);
        ActorState* a = findActor(id);
        if (!a) return missing();
        a->dead = true; a->health = 0;
        auto& c = containers_[id];
        if (c.id == 0) { c.id = id; c.cell = a->cell; c.pos = a->pos; c.corpse = true; }
        return ok();
    }
    if (auto* x = std::get_if<ResurrectActor>(&e)) {
        ActorState* a = findActor(actorByRef(x->actor));
        if (!a) return missing();
        a->dead = false; a->health = a->maxHealth;
        return ok();
    }
    if (auto* x = std::get_if<MoveRef>(&e)) {
        ActorState* a = findActor(actorByRef(x->ref));
        if (!a) return missing();
        CellId c = cellFromRef(x->cell);
        if (c) a->cell = c;
        a->pos = {x->x, x->y, x->z};
        return ok();
    }
    if (auto* x = std::get_if<EnableRef>(&e)) {
        NetId id = actorByRef(x->ref);
        if (ActorState* a = findActor(id)) { a->enabled = true; return ok(); }
        auto o = objects_.find(id);
        if (o != objects_.end()) { o->second.enabled = true; return ok(); }
        return missing();
    }
    if (auto* x = std::get_if<DisableRef>(&e)) {
        NetId id = actorByRef(x->ref);
        if (ActorState* a = findActor(id)) { a->enabled = false; return ok(); }
        auto o = objects_.find(id);
        if (o != objects_.end()) { o->second.enabled = false; return ok(); }
        return missing();
    }
    if (auto* x = std::get_if<ObjectState>(&e)) {
        auto it = objects_.find(actorByRef(x->ref));
        if (it == objects_.end()) return missing();
        WorldObject& o = it->second;
        if (x->mask & kObjOpen) o.open = x->open;
        if (x->mask & kObjLock) { o.locked = x->locked; o.lockLevel = x->level; }
        if (x->mask & kObjDestroyed) o.destroyed = o.destroyed || x->destroyed;   // sticky
        if (x->mask & kObjActivated) o.activatedOnce = o.activatedOnce || x->activatedOnce;
        return ok();
    }
    if (auto* x = std::get_if<SetLock>(&e)) {
        auto it = objects_.find(actorByRef(x->ref));
        if (it == objects_.end()) return missing();
        it->second.locked = x->locked; it->second.lockLevel = x->level;
        return ok();
    }
    if (auto* x = std::get_if<SpawnRef>(&e)) {
        if (actors_.count(x->id)) return ok();   // replay: idempotent
        ActorState a;
        a.id = x->id; a.ref = x->ref(); a.base = x->base; a.pick = x->base; a.created = true;
        a.cell = cellFromRef(x->cell); a.pos = {x->x, x->y, x->z};
        a.hostile = true;
        putActor(a);
        return ok();
    }
    if (auto* x = std::get_if<DespawnRef>(&e)) {
        if (!actors_.count(x->id)) return ok();
        eraseActor(x->id);
        containers_.erase(x->id);
        return ok();
    }
    if (auto* x = std::get_if<CompanionOwner>(&e)) {
        ActorState* a = findActor(actorByRef(x->npc));
        if (!a) return missing();
        a->recruiter = x->active ? x->owner : kNoPlayer;
        return ok();
    }
    ++fx_.ignored;
    return false;
}

// ---- digests ---------------------------------------------------------------------------------

namespace {
int32_t bucket(float v) { return static_cast<int32_t>(std::floor(v / 256.0f)); }

void canonActor(Archive& a, const ActorState& s) {
    // a corpse stops where it fell and ragdolls are visual only: the final resting position is not part of the digest
    int32_t b[3] = {s.dead ? 0 : bucket(s.pos.x), s.dead ? 0 : bucket(s.pos.y), s.dead ? 0 : bucket(s.pos.z)};
    a.varu32(const_cast<uint32_t&>(s.id)); a.boolean(const_cast<bool&>(s.dead)); a.boolean(const_cast<bool&>(s.enabled));
    uint32_t cell = s.cell; a.varu32(cell);
    int32_t hb = s.health <= 0 ? 0 : (s.health + 9) / 10; a.vari32(hb);
    for (int i = 0; i < 3; ++i) a.vari32(b[i]);
    uint8_t rec = s.recruiter, limbs = s.limbMask; a.u8(rec); a.u8(limbs);
    Ref pick = s.pick; io(a, pick);
}
void canonPlayer(Archive& a, const PlayerState& s) {
    int32_t b[3] = {bucket(s.pos.x), bucket(s.pos.y), bucket(s.pos.z)};
    uint8_t slot = s.slot; a.u8(slot);
    uint32_t cell = s.cell; a.varu32(cell);
    int32_t hb = s.health <= 0 ? 0 : (s.health + 9) / 10; a.vari32(hb);
    bool dead = s.dead; a.boolean(dead);
    for (int i = 0; i < 3; ++i) a.vari32(b[i]);
}
void canonContainer(Archive& a, const ContainerState& s) {
    uint32_t id = s.id, cell = s.cell; a.varu32(id); a.varu32(cell);
    bool corpse = s.corpse; a.boolean(corpse);
    auto items = s.items; ioItems(a, items);
}
void canonItem(Archive& a, const ItemState& s) {
    uint32_t id = s.id, cell = s.cell; a.varu32(id); a.varu32(cell);
    Ref b = s.base; io(a, b);
    bool taken = s.taken; a.boolean(taken);
    uint8_t by = s.takenBy; a.u8(by);
}
void canonObject(Archive& a, const WorldObject& s) {
    WorldObject c = s;
    c.pos = {}; c.targetPos = {};   // static
    io(a, c);
}
void canonSite(Archive& a, const SpawnSiteState& s) {
    uint32_t id = s.id, sp = s.spawned, cnt = s.spawnCount; a.varu32(id); a.varu32(sp); a.varu32(cnt);
    bool cl = s.cleared; a.boolean(cl);
}
constexpr uint32_t kPlayerKeyBase = 0xF0000000u;
uint64_t hashOf(Archive& a) { auto b = a.finish(); return hash64(b.data(), b.size()); }
}  // namespace

std::map<uint32_t, uint64_t> World::keyHashes(Category c) const {
    std::map<uint32_t, uint64_t> out;
    auto put = [&](uint32_t key, Archive& a) { out[key] = hashOf(a); };
    switch (c) {
        case Category::Story:
            for (auto& kv : quests_) { Archive a = Archive::writer(); Ref f = kv.first; uint32_t s = kv.second; io(a, f); a.varu32(s); put(keyOfQuest(kv.first), a); }
            break;
        case Category::Actors:
            for (auto& kv : actors_) { Archive a = Archive::writer(); canonActor(a, kv.second); put(kv.first, a); }
            for (uint8_t p = 0; p < kMaxSlots; ++p)
                if (players_[p].present) { Archive a = Archive::writer(); canonPlayer(a, players_[p]); put(kPlayerKeyBase | p, a); }
            break;
        case Category::Containers:
            for (auto& kv : containers_) { Archive a = Archive::writer(); canonContainer(a, kv.second); put(kv.first, a); }
            for (auto& kv : items_) { Archive a = Archive::writer(); canonItem(a, kv.second); put(kv.first, a); }
            break;
        case Category::Objects:
            for (auto& kv : objects_) { Archive a = Archive::writer(); canonObject(a, kv.second); put(kv.first, a); }
            for (auto& kv : sites_) { Archive a = Archive::writer(); canonSite(a, kv.second); put(kv.first, a); }
            break;
        default: break;
    }
    return out;
}

uint64_t World::digest(Category c) const {
    uint64_t h = 1469598103934665603ull;
    for (auto& kv : keyHashes(c)) {
        h = hash64(&kv.first, sizeof kv.first, h);
        h = hash64(&kv.second, sizeof kv.second, h);
    }
    return h;
}

uint64_t World::keyHash(Category c, uint32_t key) const {
    auto m = keyHashes(c);
    auto it = m.find(key);
    return it == m.end() ? 0 : it->second;
}

bool World::exportKey(Category c, uint32_t key, Archive& a) const {
    switch (c) {
        case Category::Story:
            for (auto& kv : quests_)
                if (keyOfQuest(kv.first) == key) { Ref f = kv.first; uint32_t s = kv.second; io(a, f); a.varu32(s); return true; }
            return false;
        case Category::Actors: {
            if (key >= kPlayerKeyBase) {
                const PlayerState& p = players_[(key & 0xFF) % kMaxSlots];
                if (!p.present) return false;
                PlayerState copy = p; copy.inventory.clear();   // inventories never travel
                io(a, copy); return true;
            }
            auto it = actors_.find(key);
            if (it == actors_.end()) return false;
            ActorState copy = it->second; io(a, copy); return true;
        }
        case Category::Containers: {
            auto it = containers_.find(key);
            if (it != containers_.end()) { uint8_t kind = 0; a.u8(kind); ContainerState copy = it->second; io(a, copy); return true; }
            auto jt = items_.find(key);
            if (jt != items_.end()) { uint8_t kind = 1; a.u8(kind); ItemState copy = jt->second; io(a, copy); return true; }
            return false;
        }
        case Category::Objects: {
            auto it = objects_.find(key);
            if (it != objects_.end()) { uint8_t kind = 0; a.u8(kind); WorldObject copy = it->second; io(a, copy); return true; }
            auto jt = sites_.find(key);
            if (jt != sites_.end()) { uint8_t kind = 1; a.u8(kind); SpawnSiteState copy = jt->second; io(a, copy); return true; }
            return false;
        }
        default: return false;
    }
}

bool World::importKey(Category c, uint32_t key, Archive& a) {
    switch (c) {
        case Category::Story: {
            Ref f; uint32_t s = 0; io(a, f); a.varu32(s);
            if (!a.ok() || keyOfQuest(f) != key) return false;
            quests_[f] = s; return true;
        }
        case Category::Actors: {
            if (key >= kPlayerKeyBase) {
                PlayerState p; io(a, p);
                if (!a.ok() || p.slot >= kMaxSlots) return false;
                p.inventory = players_[p.slot].inventory;   // never overwritten by a repair
                players_[p.slot] = p; return true;
            }
            ActorState s; io(a, s);
            if (!a.ok() || s.id != key) return false;
            auto it = actors_.find(key);
            if (it != actors_.end()) s.aiOn = it->second.aiOn;
            putActor(s); return true;
        }
        case Category::Containers: {
            uint8_t kind = 0; a.u8(kind);
            if (kind == 0) {
                ContainerState s; io(a, s);
                if (!a.ok() || s.id != key) return false;
                auto it = containers_.find(key);
                if (it != containers_.end()) { s.inUseBy = it->second.inUseBy; s.appliedOps = it->second.appliedOps; }
                containers_[key] = s; return true;
            }
            ItemState s; io(a, s);
            if (!a.ok() || s.id != key) return false;
            items_[key] = s; return true;
        }
        case Category::Objects: {
            uint8_t kind = 0; a.u8(kind);
            if (kind == 0) {
                WorldObject s; io(a, s);
                if (!a.ok() || s.id != key) return false;
                objects_[key] = s; refToNet_[placedRef(key)] = key; return true;
            }
            SpawnSiteState s; io(a, s);
            if (!a.ok() || s.id != key) return false;
            sites_[key] = s; return true;
        }
        default: return false;
    }
}

void World::removeKey(Category c, uint32_t key) {
    switch (c) {
        case Category::Story:
            for (auto it = quests_.begin(); it != quests_.end(); ++it) if (keyOfQuest(it->first) == key) { quests_.erase(it); return; }
            return;
        case Category::Actors:
            if (key >= kPlayerKeyBase) { players_[(key & 0xFF) % kMaxSlots].present = false; return; }
            eraseActor(key); return;
        case Category::Containers: containers_.erase(key); items_.erase(key); return;
        case Category::Objects: objects_.erase(key); sites_.erase(key); return;
        default: return;
    }
}

// ---- save / load -----------------------------------------------------------------------------

namespace {
template <class T>
void ioEntities(Archive& a, std::map<NetId, T>& m, size_t maxN) {
    size_t n = m.size();
    if (!a.count(n, maxN)) return;
    if (a.reading()) {
        m.clear();
        for (size_t i = 0; i < n && a.ok(); ++i) { T v; io(a, v); if (a.ok()) m[v.id] = v; }
    } else {
        for (auto& kv : m) { T v = kv.second; io(a, v); }
    }
}
}  // namespace

Bytes World::save() const {
    Archive a = Archive::writer();
    World& self = const_cast<World&>(*this);   // io() is symmetric and takes non-const refs; writing never mutates
    uint32_t cc = createdCounter_; int32_t gw = gridW_, gh = gridH_;
    a.varu32(cc); a.vari32(gw); a.vari32(gh);
    { size_t n = cells_.size(); a.count(n, 100000); for (auto& kv : cells_) { CellDef c = kv.second; io(a, c); } }
    ioEntities(a, self.actors_, 1u << 20);
    ioEntities(a, self.containers_, 1u << 20);
    ioEntities(a, self.items_, 1u << 20);
    ioEntities(a, self.objects_, 1u << 20);
    ioEntities(a, self.sites_, 1u << 20);
    for (auto& p : players_) { PlayerState c = p; io(a, c); }
    { size_t n = quests_.size(); a.count(n, 100000); for (auto& kv : quests_) { Ref f = kv.first; uint32_t s = kv.second; io(a, f); a.varu32(s); } }
    return a.finish();
}

bool World::load(const Bytes& b) {
    Archive a = Archive::reader(b.data(), b.size());
    World w;
    int32_t gw = 0, gh = 0;
    a.varu32(w.createdCounter_); a.vari32(gw); a.vari32(gh);
    w.gridW_ = gw; w.gridH_ = gh;
    { size_t n = 0; if (a.count(n, 100000)) for (size_t i = 0; i < n && a.ok(); ++i) { CellDef c; io(a, c); w.cells_[c.id] = c; } }
    ioEntities(a, w.actors_, 1u << 20);
    ioEntities(a, w.containers_, 1u << 20);
    ioEntities(a, w.items_, 1u << 20);
    ioEntities(a, w.objects_, 1u << 20);
    ioEntities(a, w.sites_, 1u << 20);
    for (auto& p : w.players_) io(a, p);
    { size_t n = 0; if (a.count(n, 100000)) for (size_t i = 0; i < n && a.ok(); ++i) { Ref f; uint32_t s = 0; io(a, f); a.varu32(s); w.quests_[f] = s; } }
    if (!a.ok() || !a.atEnd()) return false;
    for (auto& kv : w.actors_) w.refToNet_[kv.second.ref] = kv.first;
    for (auto& kv : w.objects_) w.refToNet_[placedRef(kv.first)] = kv.first;
    w.fx_ = fx_;
    w.nowMs = nowMs;
    *this = std::move(w);
    return true;
}

// ---- oracle helpers --------------------------------------------------------------------------

std::vector<std::string> World::duplicates() const {
    std::vector<std::string> out;
    std::map<NetId, int> perSite;
    for (auto& kv : actors_)
        if (kv.second.site && !kv.second.dead && kv.second.enabled) ++perSite[kv.second.site];
    for (auto& kv : perSite)
        if (kv.second > 1) out.push_back("spawn site " + std::to_string(kv.first) + " has " + std::to_string(kv.second) + " live actors");
    std::map<uint64_t, int> perOrigin;
    for (auto& kv : items_) if (kv.second.origin && !kv.second.taken) ++perOrigin[kv.second.origin];
    for (auto& kv : perOrigin)
        if (kv.second > 1) out.push_back("drop request " + std::to_string(kv.first) + " produced " + std::to_string(kv.second) + " items");
    return out;
}

std::map<Ref, int64_t> World::worldItemCensus() const {
    std::map<Ref, int64_t> out;
    for (auto& kv : items_) if (!kv.second.taken) out[kv.second.base] += 1;
    for (auto& kv : containers_) for (auto& it : kv.second.items) out[it.first] += it.second;
    return out;
}

uint32_t World::standIns(const std::vector<CellId>& loaded) const {
    uint32_t n = 0;
    for (auto& kv : actors_)
        if (!kv.second.dead && kv.second.enabled && std::find(loaded.begin(), loaded.end(), kv.second.cell) == loaded.end()) ++n;
    return n;
}

}  // namespace coop::fakeworld
