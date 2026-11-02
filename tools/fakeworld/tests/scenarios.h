// Scenarios shared by the real tests and their mutation twins. Each returns true iff every invariant held; the
// twin turns one fault switch on and expects false. `why` (optional) receives the first few problems.
#pragma once
#include <string>
#include "fakeworld/cluster.h"
#include "coop/ownership.h"

namespace coop::fakeworld::testing {

// Debug aid: called with the cluster when a scenario ends with violations (set by a test to print diagnostics).
extern void (*dumpHook)(coop::fakeworld::Cluster&);
extern coop::fakeworld::IEngineObserver* debugObserver;   // optional: attached to the cluster of randomSession

// ---- ownership on a full cluster ----
bool noFlap(std::string* why = nullptr);                                 // two players equally near: ownership must not flap 
bool followerStaysWithRecruiter(std::string* why = nullptr);

// ---- engine scenarios (Cluster) ----
coop::fakeworld::ClusterConfig baseConfig(uint64_t seed, int players, const LinkParams& link = {});
LinkParams lossyLink();

bool joinAndSync(uint64_t seed, int players, const LinkParams& link, std::string* why = nullptr);
bool fightScenario(uint64_t seed, const LinkParams& link, std::string* why = nullptr);
bool handoffMidFight(uint64_t seed, std::string* why = nullptr);
bool handoffKeepsLastState(uint64_t seed, std::string* why = nullptr);   // guest to guest: the old owner's last state reaches the new owner before it takes over
bool lootRace(uint64_t seed, std::string* why = nullptr);
bool spawnOnce(uint64_t seed, std::string* why = nullptr);
bool containerMatrix(uint64_t seed, std::string* why = nullptr);
bool doorsAndLocks(uint64_t seed, std::string* why = nullptr);
bool namedProtected(uint64_t seed, std::string* why = nullptr);
bool questStageSync(uint64_t seed, const LinkParams& link, std::string* why = nullptr);
bool recruitCompanion(uint64_t seed, std::string* why = nullptr);
bool saveLoadRejoin(uint64_t seed, std::string* why = nullptr);
bool healsCorruption(uint64_t seed, std::string* why = nullptr);
bool dropIdempotent(uint64_t seed, std::string* why = nullptr);
bool forgedHitsRejected(uint64_t seed, std::string* why = nullptr);
bool hitToNonOwnerRejected(uint64_t seed, std::string* why = nullptr);
bool lateJoinerGetsOwnership(uint64_t seed, std::string* why = nullptr);
bool formerOwnerSilenced(uint64_t seed, std::string* why = nullptr);
bool randomSession(uint64_t seed, int players, uint32_t ms, const LinkParams& link, std::string* why = nullptr);

}  // namespace coop::fakeworld::testing
