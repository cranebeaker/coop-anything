// Engine scenarios on the lossy-link cluster. Each is shared with a mutation twin (test_twins.cpp).
#include "scenarios.h"
#include "fakeworld/wire.h"
#include "minitest.h"

using namespace coop;
using namespace coop::fakeworld;
using namespace coop::fakeworld::testing;

namespace {
#define SCENARIO(group, name, expr) \
    TEST(group, name) { std::string why; const bool ok = (expr); CHECK_MSG(ok, why); }
}  // namespace

SCENARIO(engine, join_and_sync_2_players_clean, joinAndSync(1, 2, {}, &why))
SCENARIO(engine, join_and_sync_4_players_clean, joinAndSync(2, 4, {}, &why))
SCENARIO(engine, join_and_sync_3_players_lossy_duplicating_corrupting_link, joinAndSync(3, 3, lossyLink(), &why))
SCENARIO(engine, fight_two_players_against_raiders, fightScenario(1, {}, &why))
SCENARIO(engine, fight_over_lossy_link, fightScenario(2, lossyLink(), &why))
SCENARIO(engine, handoff_when_host_walks_away_mid_fight, handoffMidFight(1, &why))
SCENARIO(engine, handoff_mid_fight_seed_2, handoffMidFight(2, &why))
SCENARIO(engine, guest_to_guest_handoff_delivers_the_last_state_first, handoffKeepsLastState(1, &why))
SCENARIO(engine, ownership_does_not_flap_between_equally_near_players, noFlap(&why))
SCENARIO(engine, companion_stays_with_recruiter_and_follows_across_cells, followerStaysWithRecruiter(&why))
SCENARIO(engine, loot_race_has_one_winner_and_losers_roll_back, lootRace(1, &why))
SCENARIO(engine, loot_race_seed_2, lootRace(2, &why))
SCENARIO(engine, encounter_spawns_once_for_three_players, spawnOnce(1, &why))
SCENARIO(engine, encounter_spawns_once_seed_2, spawnOnce(2, &why))
SCENARIO(engine, container_lock_take_put_close_conserves_items, containerMatrix(1, &why))
SCENARIO(engine, doors_locks_and_teleports_agree_everywhere, doorsAndLocks(1, &why))
SCENARIO(engine, named_npc_survives_combat_and_dies_to_story_effect, namedProtected(1, &why))
SCENARIO(engine, quest_stage_is_max_and_redelivery_is_ignored, questStageSync(1, {}, &why))
SCENARIO(engine, save_load_and_guest_rejoin, saveLoadRejoin(1, &why))
SCENARIO(engine, silent_corruption_is_detected_and_repaired, healsCorruption(1, &why))
SCENARIO(engine, repeated_drop_request_creates_one_item, dropIdempotent(1, &why))
SCENARIO(engine, forged_hits_are_rejected_real_hits_land, forgedHitsRejected(1, &why))
SCENARIO(engine, hit_sent_to_a_non_owner_is_not_applied, hitToNonOwnerRejected(1, &why))
SCENARIO(engine, late_joiner_receives_the_ownership_table, lateJoinerGetsOwnership(1, &why))
SCENARIO(engine, random_session_3_players_clean_seed_1, randomSession(1, 3, 12000, {}, &why))
SCENARIO(engine, random_session_3_players_clean_seed_2, randomSession(2, 3, 12000, {}, &why))
SCENARIO(engine, random_session_4_players_lossy_seed_3, randomSession(3, 4, 12000, lossyLink(), &why))
SCENARIO(engine, random_session_2_players_lossy_seed_4, randomSession(4, 2, 12000, lossyLink(), &why))

TEST(engine, same_seed_same_world_different_seed_different_world) {
    auto run = [](uint64_t seed) {
        Cluster c(baseConfig(seed, 3));
        c.run(3000);
        std::vector<uint64_t> d;
        for (int p = 0; p < 3; ++p) for (int k = 0; k < 4; ++k) d.push_back(c.machine(static_cast<PlayerId>(p)).digest(static_cast<Category>(k)));
        d.push_back(c.host().view().digest());
        return d;
    };
    CHECK(run(7) == run(7));
    CHECK(run(7) != run(8));
}

TEST(engine, recorded_run_replays_to_identical_digests) {
    TraceCapture cap;
    Cluster c(baseConfig(5, 3), &cap);
    c.run(1500);
    const ActorState* a = nullptr;
    for (auto& kv : c.host().world().actors()) if (kv.second.hostile && !kv.second.dead) { a = &kv.second; break; }
    REQUIRE(a != nullptr);
    const NetId id = a->id;
    const CellId cell = a->cell;
    c.machine(1).move(cell, c.host().world().cellCenter(cell), 0);
    c.run(800);
    c.machine(1).attackActor(id, 30, Limb::Torso, c.machine(1).world().findActor(id)->pos, 1500);
    c.machine(2).pickup(c.host().world().items().begin()->first);
    c.machine(0).setStage(Ref("world.pack", 0x500001u), 3);
    c.run(2500);
    REQUIRE(!cap.events.empty());
    ReplayResult rr = replay(c.config(), cap.events);
    for (int p = 0; p < 3; ++p)
        for (int k = 0; k < 4; ++k)
            CHECK_MSG(rr.digests[static_cast<size_t>(p * 4 + k)] == c.machine(static_cast<PlayerId>(p)).digest(static_cast<Category>(k)),
                      "machine " + std::to_string(p) + " category " + std::to_string(k) + " differs after replay");
}

TEST(engine, recorded_run_with_a_message_removed_replays_without_crashing) {
    TraceCapture cap;
    Cluster c(baseConfig(6, 2), &cap);
    c.run(2000);
    std::vector<TraceEvent> cut;
    bool dropped = false;
    for (auto& e : cap.events) { if (!dropped && e.kind == TraceEvent::Msg && e.machine == 1) { dropped = true; continue; } cut.push_back(e); }
    ReplayResult rr = replay(c.config(), cut);
    CHECK(rr.events == cut.size());
}

TEST(engine, machine_survives_garbage_frames_and_envelopes) {
    Cluster c(baseConfig(9, 2));
    c.run(500);
    Rng r(3);
    for (int i = 0; i < 3000; ++i) {
        Bytes junk(r.range(0, 120));
        for (auto& v : junk) v = static_cast<uint8_t>(r.u32());
        c.machine(1).receive(0, junk);
        c.machine(0).receiveEnvelope(1, junk, 0);
    }
    c.run(1000);
    CHECK(c.machine(1).stats().framesBadCrc + c.machine(1).stats().undecodable > 0);
    CHECK(c.checkAlways().empty());
}

TEST(engine, link_dedupes_retransmits_and_orders_reliable_messages) {
    LinkParams l = lossyLink();
    l.loss = 0.3; l.duplicate = 0.3;
    Cluster c(baseConfig(4, 2, l));
    c.run(6000);
    CHECK(c.machine(1).stats().linkDuplicates + c.machine(0).stats().linkDuplicates > 0);
    CHECK(c.machine(0).stats().linkRetransmits + c.machine(1).stats().linkRetransmits > 0);
    CHECK(c.totalDoubleApplied() == 0);
    CHECK(c.machine(1).synced());
}

TEST(engine, guest_rejoin_gets_fresh_state_and_old_connection_traffic_is_dropped) {
    Cluster c(baseConfig(8, 2));
    c.run(1500);
    c.leave(1);
    c.run(300);
    c.join(1);
    c.run(2000);
    CHECK(c.machine(1).synced());
    CHECK(c.quiesce(20000));
    CHECK(c.checkQuiescent().empty());
}
