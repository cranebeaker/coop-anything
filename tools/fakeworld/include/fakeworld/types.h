// Fake game world: plain data for the parts of a typical game world you sync (actors, containers, items, doors,
// spawn sites, players, quests). Game free and deterministic. Rename and reshape these for your own game.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "coop/types.h"
#include "fakeworld/effect.h"
#include "fakeworld/wire.h"

namespace coop::fakeworld {

using NetId = uint32_t;     // one identity per thing; created refs use the 0xFF prefix (see isCreatedId)
using CellId = uint32_t;
constexpr uint32_t kCreatedIdBase = 0xFF000000u;
inline bool isCreatedId(NetId id) { return (id >> 24) == 0xFF; }

constexpr float kCellSize = 4096.0f;

struct Vec3 {
    float x = 0, y = 0, z = 0;
    friend bool operator==(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
};
float distance(const Vec3& a, const Vec3& b);
float distance2d(const Vec3& a, const Vec3& b);

enum class Limb : uint8_t { Torso = 0, Head, LeftArm, RightArm, LeftLeg, RightLeg, Count_ };

enum class Category : uint8_t { Story = 0, Actors = 1, Containers = 2, Objects = 3, Count_ };
const char* toString(Category c);

struct CellDef {
    CellId id = 0;
    bool interior = false;
    int32_t gx = 0, gy = 0;   // exterior grid position (interiors: unused)
};

struct ActorState {
    NetId id = 0;
    Ref ref, base;
    bool created = false;       // spawned at run time (host-allocated 0xFF id)
    CellId cell = 0;
    Vec3 pos;
    float yaw = 0;
    int32_t health = 100, maxHealth = 100;
    bool dead = false, enabled = true, aiOn = false;   // aiOn is per machine (owner true, others false): not digested
    bool hostile = false;
    bool named = false;         // story NPC: only a story KillActor effect may kill it
    PlayerId recruiter = kNoPlayer;
    NetId site = 0;             // spawn site that produced it (0 = placed)
    uint8_t limbMask = 0;       // crippled limbs
    uint32_t snapSeq = 0;       // owner's snapshot counter (newest wins)
    Ref pick;               // leveled pick, made once by the host (equals base for non-leveled)
};

struct ContainerState {
    NetId id = 0;
    CellId cell = 0;
    Vec3 pos;
    bool corpse = false;
    std::map<Ref, int32_t> items;
    PlayerId inUseBy = kNoPlayer;   // transient: not digested
    std::vector<uint64_t> appliedOps;   // idempotency ids of take/put ops already applied (host side, bounded)
};

struct ItemState {
    NetId id = 0;
    Ref base;
    CellId cell = 0;
    Vec3 pos;
    bool taken = false;
    PlayerId takenBy = kNoPlayer;
    uint64_t origin = 0;        // (dropper slot << 32 | request id): one drop = one item
};

enum class ObjKind : uint8_t { Door = 0, Terminal = 1, Switch = 2 };
struct WorldObject {
    NetId id = 0;
    ObjKind kind = ObjKind::Door;
    CellId cell = 0;
    Vec3 pos;
    bool open = false, locked = false;
    int32_t lockLevel = 0;
    bool destroyed = false, activatedOnce = false, enabled = true;
    CellId targetCell = 0;      // doors that teleport
    Vec3 targetPos;
};

struct SpawnSiteState {
    NetId id = 0;
    CellId cell = 0;
    Vec3 pos;
    Ref variantA, variantB;   // leveled choices
    uint32_t respawnMs = 30000;
    NetId spawned = 0;            // live actor from this site (0 = none)
    PlayerId rolledBy = kNoPlayer;   // arbitration winner (diagnostics)
    uint32_t spawnCount = 0;
    uint64_t clearedAtMs = 0;     // virtual time its actor died
    bool cleared = false;
    bool permanent = false;       // interior cleared stays cleared
};

struct PlayerState {
    PlayerId slot = kNoPlayer;
    bool present = false;
    CellId cell = 0;
    Vec3 pos;
    float yaw = 0;
    int32_t health = 100, maxHealth = 100;
    bool dead = false;
    uint32_t poseSeq = 0, vitalsSeq = 0;
    std::map<Ref, int32_t> inventory;   // authoritative only on the owning machine
};

// serialisation (symmetric, via Archive)
void io(Archive& a, Vec3& v);
void io(Archive& a, CellDef& v);
void io(Archive& a, ActorState& v);
void io(Archive& a, ContainerState& v);
void io(Archive& a, ItemState& v);
void io(Archive& a, WorldObject& v);
void io(Archive& a, SpawnSiteState& v);
void io(Archive& a, PlayerState& v);

}  // namespace coop::fakeworld
