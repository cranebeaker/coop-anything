// Mutation twins. Each twin switches on one known bug class (an OwnershipFaults flag) and runs the scenario that guards
// it. The scenario must pass without the fault and must report a violation with it. If a twin passes while the
// scenario also passes under the fault, the scenario would not have caught that bug, and the twin fails.
#include <string>
#include "minitest.h"
#include "scenarios.h"

using namespace coop;
using namespace coop::testing;

#define OWN_TWIN(name, flag, scenario)                                                                    \
    TEST(twin, name) {                                                                                    \
        CHECK_MSG((scenario), "the scenario must pass without the fault");                                \
        OwnershipFaults f; f.flag = true;                                                                 \
        { OwnershipFaultScope s(f); CHECK_MSG(!(scenario), "the scenario did not catch the fault " #flag); } \
    }

OWN_TWIN(own_no_epoch, noEpochBump, churnHolds(1, 400))
OWN_TWIN(own_stale_epoch, acceptStaleEpoch, staleEchoRejected())
OWN_TWIN(own_adopt_echo, adoptEcho, staleEchoRejected())
OWN_TWIN(own_transfer_no_ack, transferNoAck, handoffKeepsLastState())
OWN_TWIN(own_no_hysteresis, noHysteresis, noFlap())
OWN_TWIN(own_follower_ignored, followerIgnored, followerStaysWithRecruiter())
OWN_TWIN(own_accept_non_owner, acceptNonOwner, churnHolds(1, 400))
