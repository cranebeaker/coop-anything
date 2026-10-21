// TEST-ONLY fault switches for the fake world (same idea as coop::OwnershipFaults in the ownership library).
// Each flag re-introduces one classic bug; the invariant oracle that guards it must then fire. All false in production.
#pragma once

namespace coop::fakeworld {

struct EngineFaults {
    bool noDedupe = false;              // links and txn ids stop dropping duplicates: re-delivered messages apply twice
    bool hitNoValidation = false;       // the victim's owner applies any hit (range, damage, rate, impact point unchecked)
    bool hitAppliedByNonOwner = false;  // a machine that is not the victim's owner applies a hit it receives (and forwards it)
    bool pickupNoArbitration = false;   // the host grants every pickup claim: both racers keep the item
    bool noPickupRollback = false;      // a pickup loser keeps the optimistically added item
    bool spawnNoArbitration = false;    // the host spawns for every roll: an encounter appears once per roller
    bool lootPerMachine = false;        // every machine rolls corpse loot itself instead of using the owner's roll
    bool pickPerMachine = false;        // every machine makes its own leveled pick
    bool containerNoLock = false;       // a container can be open for two players at once
    bool namedActorKillable = false;    // the owner can kill story NPCs through combat
    bool noSessionEpoch = false;        // messages from before a host load are applied after it
    bool healDisabled = false;          // digest mismatches are detected but never repaired
    bool dropNoIdempotency = false;     // a re-sent drop request creates a second world item
    bool ownerKeepsPublishing = false;  // a former owner keeps sending snapshots after losing the actor
    bool joinNoOwnership = false;       // the full-state sent to a joiner omits the ownership table
};
inline EngineFaults& engineFaults() { static EngineFaults f; return f; }
struct EngineFaultScope {
    EngineFaults saved;
    explicit EngineFaultScope(const EngineFaults& f) : saved(engineFaults()) { engineFaults() = f; }
    ~EngineFaultScope() { engineFaults() = saved; }
};

}  // namespace coop::fakeworld
