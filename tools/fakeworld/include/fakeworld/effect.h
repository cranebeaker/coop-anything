// Refs and story effects of the fake world.
//
// A Ref names a thing the way a plugin-based game does: (content pack name, id inside that pack). It never uses a
// raw load-order index, because load orders differ between machines. The ids used by the fake world are made up.
//
// An Effect is one primitive world change captured on the host and replayed on every machine ("effect capture"):
// quest stage, kill, move, enable, door state, lock, spawn, companion owner. Clients apply effects directly and
// never re-run the script that caused them. This is a small subset of what a real game needs; add yours here.
#pragma once
#include <cstdint>
#include <string>
#include <tuple>
#include <variant>
#include "coop/types.h"
#include "fakeworld/wire.h"

namespace coop::fakeworld {

struct Ref {
    std::string pack;
    uint32_t id = 0;

    Ref() = default;
    Ref(std::string p, uint32_t i) : pack(std::move(p)), id(i) {}
    friend bool operator==(const Ref& a, const Ref& b) { return a.id == b.id && a.pack == b.pack; }
    friend bool operator!=(const Ref& a, const Ref& b) { return !(a == b); }
    friend bool operator<(const Ref& a, const Ref& b) { return std::tie(a.pack, a.id) < std::tie(b.pack, b.id); }
};

// Refs made at run time (spawned encounters, dropped items) get host-allocated ids with this prefix.
constexpr uint32_t kCreatedRefPrefix = 0xFF000000u;
inline Ref createdRef(uint32_t id) { return Ref("created", id); }

struct SetStage { Ref quest; uint32_t stage = 0; };            // merge rule: the highest stage wins
struct StartQuest { Ref quest; };
struct StopQuest { Ref quest; };
struct KillActor { Ref actor; };
struct ResurrectActor { Ref actor; };
struct MoveRef { Ref ref; Ref cell; float x = 0, y = 0, z = 0; };
struct EnableRef { Ref ref; };
struct DisableRef { Ref ref; };
// Object state. mask bit0: set open/closed, bit1: set lock (locked + level), bit2: destroyed (sticky), bit3: activated once (sticky).
constexpr uint8_t kObjOpen = 1, kObjLock = 2, kObjDestroyed = 4, kObjActivated = 8;
struct ObjectState {
    Ref ref;
    uint8_t mask = 0;
    bool open = false, locked = false;
    int32_t level = 0;
    bool destroyed = false, activatedOnce = false;
};
struct SetLock { Ref ref; bool locked = false; int32_t level = 0; };
struct SpawnRef {
    uint32_t id = kCreatedRefPrefix;   // host-allocated, 0xFF prefixed
    Ref base, cell;
    float x = 0, y = 0, z = 0;
    Ref ref() const { return createdRef(id); }
};
struct DespawnRef { uint32_t id = kCreatedRefPrefix; };
struct CompanionOwner { Ref npc; uint8_t owner = 0; bool active = true; };   // recruit (active) or dismiss a follower

using Effect = std::variant<SetStage, StartQuest, StopQuest, KillActor, ResurrectActor, MoveRef, EnableRef, DisableRef,
                            ObjectState, SetLock, SpawnRef, DespawnRef, CompanionOwner>;

void io(Archive& a, Ref& v);
void io(Archive& a, Effect& v);

}  // namespace coop::fakeworld
