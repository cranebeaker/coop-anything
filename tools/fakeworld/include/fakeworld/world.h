// The in-memory world one machine holds. Pure data plus deterministic mutations, queries and digests.
// Every machine has its own World; the sync protocol (machine.h) is what keeps them equal.
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "fakeworld/effect.h"
#include "fakeworld/types.h"
#include "fakeworld/rng.h"
#include "fakeworld/net.h"

namespace coop::fakeworld {

// Describes the static content every machine loads from the content packs (same on all machines).
struct WorldSpec {
    uint64_t seed = 1;
    int gridW = 4, gridH = 4;       // exterior cells
    int interiors = 2;
    int actorsPerCell = 3;
    int hostilePercent = 50;
    int namedPerWorld = 2;
    int containersPerCell = 1;
    int itemsPerCell = 1;
    int doorsPerCell = 1;
    int sitesPerCell = 1;
    uint32_t respawnMs = 30000;
};

struct EffectStats {
    uint64_t applied = 0, ignored = 0, unknownTarget = 0;
};

class World {
public:
    static World build(const WorldSpec& spec);

    // ---- cells ----
    CellId cellAt(int gx, int gy) const;                 // 0 if outside the grid
    bool cellKnown(CellId c) const { return cells_.count(c) != 0; }
    bool adjacent(CellId a, CellId b) const;             // same or neighbouring exterior cell
    std::vector<CellId> loadedAround(CellId c) const;    // cells loaded for a player standing in c (proximity)
    const std::map<CellId, CellDef>& cells() const { return cells_; }
    Vec3 cellCenter(CellId c) const;

    // ---- entities ----
    std::map<NetId, ActorState>& actors() { return actors_; }
    const std::map<NetId, ActorState>& actors() const { return actors_; }
    std::map<NetId, ContainerState>& containers() { return containers_; }
    const std::map<NetId, ContainerState>& containers() const { return containers_; }
    std::map<NetId, ItemState>& items() { return items_; }
    const std::map<NetId, ItemState>& items() const { return items_; }
    std::map<NetId, WorldObject>& objects() { return objects_; }
    const std::map<NetId, WorldObject>& objects() const { return objects_; }
    std::map<NetId, SpawnSiteState>& sites() { return sites_; }
    const std::map<NetId, SpawnSiteState>& sites() const { return sites_; }
    PlayerState& player(PlayerId p) { return players_[p % kMaxSlots]; }
    const PlayerState& player(PlayerId p) const { return players_[p % kMaxSlots]; }
    std::map<Ref, uint32_t>& quests() { return quests_; }
    const std::map<Ref, uint32_t>& quests() const { return quests_; }

    ActorState* findActor(NetId id) { auto it = actors_.find(id); return it == actors_.end() ? nullptr : &it->second; }
    NetId actorByRef(const Ref& r) const { auto it = refToNet_.find(r); return it == refToNet_.end() ? 0 : it->second; }
    NetId allocCreatedId() { return kCreatedIdBase | ++createdCounter_; }
    void noteCreatedId(NetId id) { if (isCreatedId(id) && (id & 0xFFFFFFu) > createdCounter_) createdCounter_ = id & 0xFFFFFFu; }
    // Inserts or replaces an actor and keeps the ref index right.
    ActorState& putActor(const ActorState& a);
    void eraseActor(NetId id);

    // ---- story / effects (the effect vocabulary the model can show) ----
    bool applyEffect(const Effect& e);
    const EffectStats& effectStats() const { return fx_; }
    Ref cellRef(CellId c) const;                     // Ref a MoveRef uses for a cell
    CellId cellFromRef(const Ref& r) const;

    // ---- digests : per category, per key ----
    uint64_t digest(Category c) const;
    uint64_t keyHash(Category c, uint32_t key) const;   // 0 if the key does not exist
    std::map<uint32_t, uint64_t> keyHashes(Category c) const;
    bool exportKey(Category c, uint32_t key, Archive& a) const;      // false if the key does not exist
    bool importKey(Category c, uint32_t key, Archive& a);            // replaces/creates the entity
    void removeKey(Category c, uint32_t key);
    // Whole-world serialisation (host save/load, full state for a joiner).
    Bytes save() const;
    bool load(const Bytes& b);

    // ---- oracle helpers ----
    // Duplicate identities: more than one live actor from one site, items sharing an origin, etc. One line each.
    std::vector<std::string> duplicates() const;
    // Items on the ground (not taken) plus corpse/container contents, summed per base. Players' inventories not included.
    std::map<Ref, int64_t> worldItemCensus() const;
    uint32_t standIns(const std::vector<CellId>& loaded) const;   // live actors whose cell is not loaded here

    uint64_t nowMs = 0;   // virtual time the world was last touched at (used for respawn bookkeeping)

private:
    std::map<CellId, CellDef> cells_;
    std::map<NetId, ActorState> actors_;
    std::map<NetId, ContainerState> containers_;
    std::map<NetId, ItemState> items_;
    std::map<NetId, WorldObject> objects_;
    std::map<NetId, SpawnSiteState> sites_;
    PlayerState players_[kMaxSlots];
    std::map<Ref, uint32_t> quests_;
    std::map<Ref, NetId> refToNet_;   // placed and created actors, placed objects
    uint32_t createdCounter_ = 0;
    EffectStats fx_;
    int gridW_ = 0, gridH_ = 0;

};

// Ref helpers: placed refs live in the base content pack.
inline Ref placedRef(NetId id) { return Ref("world.pack", id); }
uint32_t keyOfQuest(const Ref& r);

}  // namespace coop::fakeworld
