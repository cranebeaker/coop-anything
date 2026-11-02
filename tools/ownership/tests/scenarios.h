// Scenarios shared by the real tests and their mutation twins. Each returns true iff every invariant held. A twin
// turns one fault switch on and expects the same scenario to return false. `why` (optional) receives the first
// few problems found.
#pragma once
#include <string>
#include "coop/ownership.h"

namespace coop::testing {

bool churnHolds(uint64_t seed, int steps, std::string* why = nullptr);   // random presence churn, lost acks, reordered and duplicated messages
bool staleEchoRejected(std::string* why = nullptr);                      // an old assignment replayed later must not win
bool rejoinEpochsMonotonic(std::string* why = nullptr);                  // epochs keep rising across leave and rejoin
bool noFlap(std::string* why = nullptr);                                 // two players about equally near: ownership must not flap
bool followerStaysWithRecruiter(std::string* why = nullptr);             // a companion follows its recruiter everywhere
bool handoffKeepsLastState(std::string* why = nullptr);                  // the old owner stays authoritative until the Ack and Commit

}  // namespace coop::testing
