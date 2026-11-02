// Mutation twins. Each twin switches on one known bug class and runs the scenario that guards it. The scenario must
// pass without the fault and must report a violation with it. If it still passes under the fault, the scenario would
// not have caught that bug, and the twin fails.
#include <string>
#include "minitest.h"
#include "scenarios.h"

using namespace coop;
using namespace coop::fakeworld;
using namespace coop::fakeworld::testing;

namespace {
LinkParams dupLink() {
    LinkParams l;
    l.minLatencyMs = 5; l.maxLatencyMs = 25; l.duplicate = 0.3; l.loss = 0.05;
    return l;
}
}  // namespace

#define OWN_TWIN(name, flag, scenario)                                                                    \
    TEST(twin, name) {                                                                                    \
        CHECK_MSG((scenario), "the scenario must pass without the fault");                                \
        OwnershipFaults f; f.flag = true;                                                                 \
        { OwnershipFaultScope s(f); CHECK_MSG(!(scenario), "the scenario did not catch the fault " #flag); } \
    }
#define ENGINE_TWIN(name, flag, scenario)                                                                 \
    TEST(twin, name) {                                                                                    \
        CHECK_MSG((scenario), "the scenario must pass without the fault");                                \
        EngineFaults f; f.flag = true;                                                                    \
        { EngineFaultScope s(f); CHECK_MSG(!(scenario), "the scenario did not catch the fault " #flag); } \
    }

// The ownership library's own faults, seen through the whole cluster.
OWN_TWIN(own_transfer_no_ack, transferNoAck, handoffKeepsLastState(1))
OWN_TWIN(own_no_hysteresis, noHysteresis, noFlap())
OWN_TWIN(own_follower_ignored, followerIgnored, followerStaysWithRecruiter())

// The fake world's own faults.
ENGINE_TWIN(no_dedupe, noDedupe, questStageSync(1, dupLink()))
ENGINE_TWIN(hit_no_validation, hitNoValidation, forgedHitsRejected(1))
ENGINE_TWIN(hit_by_non_owner, hitAppliedByNonOwner, hitToNonOwnerRejected(1))
ENGINE_TWIN(pickup_no_arbitration, pickupNoArbitration, lootRace(1))
ENGINE_TWIN(pickup_no_rollback, noPickupRollback, lootRace(1))
ENGINE_TWIN(spawn_no_arbitration, spawnNoArbitration, spawnOnce(1))
ENGINE_TWIN(loot_per_machine, lootPerMachine, fightScenario(1, {}))
ENGINE_TWIN(pick_per_machine, pickPerMachine, spawnOnce(1))
ENGINE_TWIN(container_no_lock, containerNoLock, containerMatrix(1))
ENGINE_TWIN(named_killable, namedActorKillable, namedProtected(1))
ENGINE_TWIN(no_session_epoch, noSessionEpoch, saveLoadRejoin(1))
ENGINE_TWIN(heal_disabled, healDisabled, healsCorruption(1))
ENGINE_TWIN(drop_no_idempotency, dropNoIdempotency, dropIdempotent(1))
ENGINE_TWIN(join_no_ownership, joinNoOwnership, lateJoinerGetsOwnership(1))

// A former owner that never noticed it lost the actor keeps publishing; the receivers must reject it. The bug only
// bites when a receiver also applies old-epoch updates, so the twin combines the two switches.
TEST(twin, owner_keeps_publishing) {
    CHECK_MSG(formerOwnerSilenced(1), "the scenario must pass without the fault");
    EngineFaults e; e.ownerKeepsPublishing = true;
    OwnershipFaults o; o.acceptStaleEpoch = true;
    EngineFaultScope es(e);
    OwnershipFaultScope os(o);
    CHECK_MSG(!formerOwnerSilenced(1), "the scenario did not catch a former owner that keeps publishing");
}
