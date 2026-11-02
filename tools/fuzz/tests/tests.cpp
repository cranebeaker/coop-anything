// Self tests of coopfuzz: oracle unit tests on hand-built logs, clean property runs on the example target, one mutation twin per
// bug class (the oracles must catch each), shrinking, replay and the failure report.
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "coopfuzz/fuzz.h"
#include "minitest.h"

using namespace coopfuzz;

namespace {
uint64_t g_sessions = 0;   // every fuzz session this program ran, printed at the end

int seedCount(int dflt) {
    const char* e = std::getenv("COOPFUZZ_SEEDS");
    return e ? std::atoi(e) : dflt;
}
std::string outDir() {
    const char* e = std::getenv("COOPFUZZ_TEST_OUT");
    return e ? e : "out/tests";
}

RunOptions opts() { RunOptions o; o.target = "example"; return o; }
RunResult runCounted(const Scenario& s, const RunOptions& o) { ++g_sessions; return coopfuzz::run(s, o); }

Scenario withBug(uint64_t seed, int machines, uint32_t duration, const std::string& bug) {
    Scenario s = generate(seed, machines, duration);
    if (!bug.empty()) s.bugs.push_back(bug);
    return s;
}

struct Caught { int failed = 0, ran = 0; std::map<std::string, int> by; std::string first; uint64_t firstSeed = 0; };
Caught sweep(const std::string& bug, int first, int count, uint32_t duration = 8000) {
    Caught c;
    for (int i = 0; i < count; ++i) {
        uint64_t sd = static_cast<uint64_t>(first + i);
        int machines = 2 + static_cast<int>(sd % 3);
        RunResult r = runCounted(withBug(sd, machines, duration, bug), opts());
        ++c.ran;
        if (!r.ok) {
            ++c.failed;
            for (auto& v : r.violations) ++c.by[v.invariant];
            if (c.first.empty()) { c.first = r.violations[0].invariant + ": " + r.violations[0].detail; c.firstSeed = sd; }
        }
    }
    return c;
}

// ---- a hand-built target so the oracles can be tested on their own
class FakeTarget : public ITarget {
public:
    int n = 3;
    std::vector<ApplyEvent> ap;
    std::vector<OwnEvent> ow;
    std::map<int, std::vector<Thing>> views;
    uint64_t ord = 0;
    void apply(const Op&) override {}
    void advance(uint64_t) override {}
    uint64_t now() const override { return 0; }
    void heal() override {}
    int machines() const override { return n; }
    bool connected(int) const override { return true; }
    bool quiescent() const override { return true; }
    std::vector<Thing> view(int m) const override { auto it = views.find(m); return it == views.end() ? std::vector<Thing>() : it->second; }
    uint64_t digest(int m, int cat) const override { uint64_t h = 7 + static_cast<uint64_t>(cat); for (auto& t : view(m)) h = h * 31 + static_cast<uint64_t>(t.x) + t.netId; return h; }
    uint64_t repairs(int) const override { return 0; }
    const std::vector<ApplyEvent>& applyLog() const override { return ap; }
    const std::vector<OwnEvent>& ownLog() const override { return ow; }
    std::map<std::string, uint64_t> stats() const override { return {}; }
    void addApply(int m, uint64_t idem, int ref, uint32_t epoch, uint32_t seq, int origin, ApplyKind k, bool acc = true, uint32_t wep = 0) {
        ApplyEvent e; e.machine = m; e.ord = ++ord; e.idem = idem; e.ref = ref; e.epoch = epoch; e.seq = seq; e.origin = origin; e.kind = k; e.accepted = acc; e.worldEpoch = wep;
        ap.push_back(e);
    }
    void addOwn(OwnEvent::Kind k, int ref, int owner, uint32_t epoch) {
        OwnEvent o; o.ord = ++ord; o.kind = k; o.ref = ref; o.owner = owner; o.epoch = epoch; ow.push_back(o);
    }
};
bool has(const std::vector<Violation>& v, const char* inv) { for (auto& x : v) if (x.invariant == inv) return true; return false; }
Thing mk(uint32_t id, int x, uint32_t ident = 0, uint8_t type = 1) { Thing t; t.netId = id; t.x = x; t.ident = ident; t.type = type; return t; }

// A target that never quiesces and a target that keeps repairing: the runner must report them.
class StuckTarget : public FakeTarget {
public:
    bool quiet = false, repairForever = false;
    uint64_t clock = 0, rep = 0;
    uint64_t now() const override { return clock; }
    void advance(uint64_t to) override { if (repairForever && to > clock) ++rep; clock = to; }
    bool quiescent() const override { return quiet; }
    uint64_t repairs(int) const override { return rep; }
};
}  // namespace

TEST(coopfuzz, oracle_double_apply) {
    FakeTarget f;
    f.addApply(1, 5, 10, 0, 0, 0, ApplyKind::Hp);
    f.addApply(2, 5, 10, 0, 0, 0, ApplyKind::Hp);           // other machine, same id: fine
    f.addApply(1, 5, 10, 0, 0, 0, ApplyKind::Hp, false);    // rejected duplicate: fine
    std::vector<Violation> v;
    checkLogs(f, v);
    CHECK(v.empty());
    f.addApply(1, 5, 10, 0, 0, 0, ApplyKind::Hp);           // accepted twice on one machine
    checkLogs(f, v);
    CHECK(has(v, "double_apply"));
}

TEST(coopfuzz, oracle_one_owner_per_epoch) {
    FakeTarget f;
    f.addOwn(OwnEvent::Grant, 10, 1, 1);
    f.addOwn(OwnEvent::Transfer, 10, 2, 2);
    f.addOwn(OwnEvent::Release, 11, -1, 3);
    std::vector<Violation> v;
    checkLogs(f, v);
    CHECK(v.empty());
    f.addOwn(OwnEvent::Transfer, 10, 0, 2);                  // second owner in epoch 2
    checkLogs(f, v);
    CHECK(has(v, "multi_owner"));
    FakeTarget g;
    g.addOwn(OwnEvent::Grant, 10, 1, 5);
    g.addOwn(OwnEvent::Transfer, 10, 2, 4);                  // epoch backwards
    v.clear();
    checkLogs(g, v);
    CHECK(has(v, "multi_owner"));
}

TEST(coopfuzz, oracle_stale_and_echo) {
    FakeTarget f;
    f.addOwn(OwnEvent::Grant, 10, 1, 1);
    f.addApply(0, 100, 10, 1, 1, 1, ApplyKind::Pos);
    f.addApply(0, 101, 10, 1, 2, 1, ApplyKind::Pos);
    f.addApply(2, 1, 10, 1, 1, 1, ApplyKind::Pos);
    f.addOwn(OwnEvent::Transfer, 10, 2, 2);
    f.addApply(0, 102, 10, 2, 1, 2, ApplyKind::Pos);
    f.addApply(0, 103, 10, 1, 3, 1, ApplyKind::Pos, false);  // correctly rejected stale update
    std::vector<Violation> v;
    checkLogs(f, v);
    CHECK(v.empty());
    FakeTarget a = f; a.addApply(0, 104, 10, 1, 4, 1, ApplyKind::Pos);   // old owner accepted after the transfer
    v.clear(); checkLogs(a, v); CHECK(has(v, "stale_applied"));
    FakeTarget b = f;                                                    // an older epoch applied on a machine that already saw a newer one
    b.addApply(2, 10, 10, 2, 1, 2, ApplyKind::Pos); b.addApply(2, 11, 10, 1, 8, 1, ApplyKind::Pos);
    v.clear(); checkLogs(b, v); CHECK(has(v, "stale_applied"));
    FakeTarget c = f; c.addApply(1, 12, 10, 1, 2, 1, ApplyKind::Pos);    // first update seen on m1: fine
    c.addApply(1, 13, 10, 1, 1, 1, ApplyKind::Pos);                      // sequence went backwards on machine 1
    v.clear(); checkLogs(c, v); CHECK(has(v, "stale_applied"));
    FakeTarget d = f; d.addApply(1, 14, 77, 0, 0, 1, ApplyKind::PlayerPos);   // m1 applied the echo of its own move
    v.clear(); checkLogs(d, v); CHECK(has(v, "echo_adopted"));
    FakeTarget e; e.addOwn(OwnEvent::WorldEpoch, -1, -1, 2); e.addApply(0, 1, 5, 0, 0, 1, ApplyKind::Intent, true, 1);   // request from before the load
    v.clear(); checkLogs(e, v); CHECK(has(v, "stale_applied"));
}

TEST(coopfuzz, oracle_converge_and_duplicates) {
    FakeTarget f;
    f.views[0] = {mk(1, 5, 100), mk(2, 6, 101)};
    f.views[1] = {mk(1, 5, 100), mk(2, 6, 101)};
    f.views[2] = {mk(1, 5, 100), mk(2, 6, 101)};
    std::vector<Violation> v;
    checkConverged(f, v);
    CHECK(v.empty());
    f.views[2][1].x = 99;
    checkConverged(f, v);
    CHECK(has(v, "converge"));
    FakeTarget g;
    g.views[0] = g.views[1] = g.views[2] = {mk(1, 5, 100), mk(2, 6, 100)};      // two things with one identity
    v.clear(); checkConverged(g, v); CHECK(has(v, "dup_thing"));
    FakeTarget h;
    h.views[0] = {mk(1, 5), mk(2, 6)}; h.views[1] = {mk(1, 5)}; h.views[2] = {mk(1, 5), mk(2, 6)};     // a missing actor
    v.clear(); checkConverged(h, v); CHECK(has(v, "converge"));
}

TEST(coopfuzz, registry_rejects_a_scenario_for_a_missing_target) {
    TargetInfo info;
    info.name = "stuck";
    info.make = [](const Scenario& sc, std::string*) {
        auto* t = new StuckTarget();
        t->quiet = sc.seed == 2;            // seed 2: quiet, but still repairing; any other seed never goes quiet
        t->repairForever = sc.seed == 2;
        return std::unique_ptr<ITarget>(t);
    };
    registerTarget(info);
    Scenario s = generate(1, 2, 1000);
    RunOptions o; o.target = "no_such_target";
    RunResult r = runCounted(s, o);
    CHECK(!r.ok && r.violations[0].invariant == "setup");
}

TEST(coopfuzz, runner_reports_no_quiescence_and_endless_repairs) {
    Scenario s = generate(1, 2, 500);
    RunOptions o; o.target = "stuck"; o.quiesceCapMs = 3000;
    RunResult r = runCounted(s, o);                 // quiescent() never turns true
    CHECK(!r.ok);
    CHECK(has(r.violations, "not_quiescent"));
    CHECK(!has(r.violations, "repairs_after_quiescence"));
    // A target that is quiet but keeps repairing after quiescence must trip repairs_after_quiescence.
    // (the stuck target decides which of the two it is from the scenario seed)
    Scenario s2 = generate(2, 2, 500);
    RunResult r2 = runCounted(s2, o);
    CHECK(has(r2.violations, "repairs_after_quiescence"));
    CHECK(!has(r2.violations, "not_quiescent"));
}

TEST(coopfuzz, link_drops_duplicates_delays_and_reconnects) {
    LossyLink net(2, 5);
    LinkParams p; p.minLatencyMs = 10; p.maxLatencyMs = 10;
    net.setLink(p);
    net.send(0, 1, Bytes{1});
    net.advance(5);
    int from = -1; Bytes d;
    CHECK(!net.poll(1, from, d));                  // not due yet
    net.advance(10);
    CHECK(net.poll(1, from, d) && from == 0 && d[0] == 1);
    p.loss = 1.0; net.setLink(p);
    net.send(0, 1, Bytes{2});
    net.advance(100);
    CHECK(!net.poll(1, from, d) && net.stats().lost == 1);
    p.loss = 0; p.duplicate = 1.0; net.setLink(p);
    net.send(0, 1, Bytes{3});
    net.advance(200);
    CHECK(net.poll(1, from, d) && net.poll(1, from, d) && !net.poll(1, from, d));
    net.send(0, 1, Bytes{4});
    net.setConnected(1, false);                    // queued traffic to a machine that goes down vanishes
    net.advance(400);
    CHECK(net.idle() && !net.poll(1, from, d));
    net.setConnected(1, true);
    net.send(0, 1, Bytes{5});
    net.advance(600);
    CHECK(net.poll(1, from, d) && d[0] == 5);
    LinkParams lp;
    CHECK(linkProfile("hell", lp) && lp.loss > 0.3);
    CHECK(!linkProfile("nope", lp));
}

TEST(coopfuzz, generator_is_deterministic_and_json_round_trips) {
    Scenario a = generate(42, 3, 10000), b = generate(42, 3, 10000);
    CHECK(toJson(a) == toJson(b));
    CHECK(toJson(generate(43, 3, 10000)) != toJson(a));
    CHECK(generate(1, 9, 1000).machines == 4 && generate(1, 1, 1000).machines == 2);   // 2 to 4 players
    a.bugs.push_back("two_owners");
    Scenario c;
    std::string err;
    CHECK_MSG(fromJson(toJson(a), c, &err), err);
    CHECK(toJson(c) == toJson(a));
    CHECK(c.ops.size() == a.ops.size() && c.bugs.size() == 1);
    Scenario bad;
    CHECK(!fromJson("{\"format\": \"nope\"}", bad, &err));
    CHECK(!fromJson("not json", bad, &err));
}

TEST(coopfuzz, run_is_deterministic) {
    Scenario s = generate(7, 4, 8000);
    RunResult a = runCounted(s, opts()), b = runCounted(s, opts());
    CHECK(a.ok && b.ok);
    CHECK(a.virtualMs == b.virtualMs);
    CHECK(a.stats == b.stats);
}

TEST(coopfuzz, clean_sessions_hold_every_invariant) {
    int n = seedCount(300);
    Caught c = sweep("", 1, n, 10000);
    std::printf("    %d clean sessions (2 to 4 players), %d failed\n", c.ran, c.failed);
    CHECK_MSG(c.failed == 0, "seed " + std::to_string(c.firstSeed) + " " + c.first);
}

TEST(coopfuzz, clean_sessions_exercise_the_hard_paths) {
    // Guard against a fuzzer that passes because it does nothing: the sweep must really cause these events.
    std::map<std::string, uint64_t> total;
    for (int sd = 1; sd <= 40; ++sd) {
        RunResult r = runCounted(generate(static_cast<uint64_t>(sd), 2 + sd % 3, 10000), opts());
        for (auto& kv : r.stats) total[kv.first] += kv.second;
    }
    for (const char* k : {"grants", "transfers", "releases", "deaths", "loot_races_lost", "spawn_races_lost", "loads", "joins", "leaves", "stale_old_content_injected",
                          "stale_raw_injected", "dup_packets_dropped", "reject_stale epoch", "reject_stale update sequence", "net_lost", "net_dup"}) {
        CHECK_MSG(total[k] > 0, std::string("never exercised: ") + k);
    }
}

// ---- mutation twins: every injected bug class must be caught, with the right kind of violation
namespace {
int g_twinsCaught = 0;
void twin(const char* bug, std::initializer_list<const char*> expect, int count = 80) {
    Caught c = sweep(bug, 1, count);
    std::printf("    twin %-24s caught on %d/%d sessions  e.g. %s\n", bug, c.failed, c.ran, c.first.substr(0, 110).c_str());
    CHECK_MSG(c.failed > 0, std::string("bug not caught: ") + bug);
    bool matched = false;
    for (const char* e : expect) if (c.by.count(e)) matched = true;
    CHECK_MSG(matched, std::string("caught, but not as ") + *expect.begin() + ": " + c.first);
    if (c.failed > 0 && matched) ++g_twinsCaught;
}
}  // namespace
TEST(coopfuzz, twin_stale_epoch_accepted) { twin("stale_epoch_accepted", {"stale_applied"}); }
TEST(coopfuzz, twin_stale_seq_accepted) { twin("stale_seq_accepted", {"stale_applied"}); }
TEST(coopfuzz, twin_echo_adopted) { twin("echo_adopted", {"echo_adopted"}); }
TEST(coopfuzz, twin_no_idempotency) { twin("no_idempotency", {"double_apply"}); }
TEST(coopfuzz, twin_two_owners) { twin("two_owners", {"multi_owner"}); }
TEST(coopfuzz, twin_spawn_dup) { twin("spawn_dup", {"dup_thing"}); }
TEST(coopfuzz, twin_pickup_dup) { twin("pickup_dup", {"dup_thing"}); }
TEST(coopfuzz, twin_handoff_respawn) { twin("handoff_respawn", {"dup_thing"}); }
TEST(coopfuzz, twin_digest_unstable) { twin("digest_unstable", {"repairs_after_quiescence"}); }
TEST(coopfuzz, twin_no_retransmit) { twin("no_retransmit", {"converge", "not_quiescent"}); }
TEST(coopfuzz, twin_no_world_epoch) { twin("no_world_epoch", {"stale_applied"}, 150); }
TEST(coopfuzz, twin_stale_assignment_echo) { twin("stale_assignment_echo", {"dup_thing", "converge"}, 300); }

TEST(coopfuzz, every_known_bug_has_a_twin) {
    // Keep this list in step with the twins above: adding a bug class without a twin fails here.
    std::vector<std::string> covered = {"stale_epoch_accepted", "stale_seq_accepted", "echo_adopted", "no_idempotency", "two_owners", "spawn_dup",
                                        "pickup_dup", "handoff_respawn", "digest_unstable", "no_retransmit", "no_world_epoch", "stale_assignment_echo"};
    auto known = knownBugs("example");
    CHECK(known.size() == covered.size());
    for (auto& k : known) CHECK_MSG(std::find(covered.begin(), covered.end(), k) != covered.end(), "no twin for " + k);
    Scenario s = generate(1, 3, 1000);
    s.bugs.push_back("not_a_bug");
    RunResult r = runCounted(s, opts());
    CHECK(!r.ok && r.violations[0].invariant == "setup");
}

TEST(coopfuzz, skeleton_target_is_clean_and_its_twin_is_caught) {
    // The skeleton is the template a user copies: it must hold every oracle when clean and its one bug must be caught.
    RunOptions o; o.target = "skeleton";
    int failed = 0, caught = 0;
    uint64_t moves = 0;
    for (int sd = 1; sd <= 100; ++sd) {
        Scenario s = generate(static_cast<uint64_t>(sd), 2 + sd % 3, 8000);
        RunResult r = runCounted(s, o);
        if (!r.ok) { ++failed; if (failed == 1) std::printf("    skeleton seed %d: %s: %s\n", sd, r.violations[0].invariant.c_str(), r.violations[0].detail.c_str()); }
        moves += r.stats["op_move"];
        s.bugs = {"no_dedupe"};
        caught += !runCounted(s, o).ok;
    }
    std::printf("    skeleton: 100 clean sessions, %d failed; twin no_dedupe caught on %d/100\n", failed, caught);
    CHECK(failed == 0);
    CHECK(moves > 1000);
    CHECK(caught > 0);
    CHECK(knownBugs("skeleton").size() == 1);
}

TEST(coopfuzz, shrink_reduces_and_replay_reproduces) {
    // find a failing session with a known bug, shrink it, round-trip the file, replay it
    Caught c = sweep("two_owners", 1, 40);
    REQUIRE(c.failed > 0);
    Scenario s = withBug(c.firstSeed, 2 + static_cast<int>(c.firstSeed % 3), 8000, "two_owners");
    int runs = 0;
    Scenario small = shrink(s, opts(), "multi_owner", &runs);
    g_sessions += static_cast<uint64_t>(runs);
    std::printf("    shrunk %zu ops -> %zu ops in %d runs\n", s.ops.size(), small.ops.size(), runs);
    CHECK(small.ops.size() < s.ops.size() / 4);
    CHECK(small.ops.size() <= 40);
    Scenario back;
    std::string err;
    REQUIRE(fromJson(toJson(small), back, &err));
    RunResult r = runCounted(back, opts());
    CHECK(!r.ok);
    bool found = false;
    for (auto& v : r.violations) if (v.invariant == "multi_owner") found = true;
    CHECK(found);
    // dropping the injected bug makes the very same scenario pass: the shrunk file is the bug, not the environment
    back.bugs.clear();
    CHECK(runCounted(back, opts()).ok);
}

TEST(coopfuzz, failure_writes_scenario_and_json_report) {
    Caught c = sweep("pickup_dup", 1, 40);
    REQUIRE(c.failed > 0);
    Scenario s = withBug(c.firstSeed, 2 + static_cast<int>(c.firstSeed % 3), 8000, "pickup_dup");
    RunOptions o = opts();
    o.reportDir = outDir() + "/report_test";
    o.reportName = "t";
    RunResult r = runCounted(s, o);
    CHECK(!r.ok);
    std::ifstream sc(o.reportDir + "/t.scenario.json"), tr(o.reportDir + "/t.report.json");
    CHECK(static_cast<bool>(sc));
    CHECK(static_cast<bool>(tr));
    std::stringstream ss; ss << tr.rdbuf();
    CHECK(ss.str().find("coopfuzz-report-1") != std::string::npos);
    CHECK(ss.str().find("dup_thing") != std::string::npos);
    CHECK(ss.str().find("\"seed\": " + std::to_string(c.firstSeed)) != std::string::npos);
}

TEST(coopfuzz, link_profiles_clean_and_twins) {
    // Named link presets: clean sessions hold, injected bugs are still caught.
    for (const std::string& p : linkProfileNames()) {
        int failed = 0, lost = 0;
        for (int sd = 1; sd <= 40; ++sd) {
            Scenario s = generate(static_cast<uint64_t>(sd), 2 + sd % 3, 8000);
            s.profile = p;
            RunResult r = runCounted(s, opts());
            if (!r.ok) ++failed;
            lost += r.stats["net_lost"] > 0;
        }
        std::printf("    profile %-10s %d/40 failed, loss seen in %d\n", p.c_str(), failed, lost);
        CHECK_MSG(failed == 0, "clean run failed under profile " + p);
        if (p != "lan") CHECK_MSG(lost > 0, "profile never dropped anything: " + p);
    }
    for (const char* bug : {"two_owners", "no_idempotency", "no_retransmit", "stale_epoch_accepted"}) {
        int caught = 0;
        for (int sd = 1; sd <= 30; ++sd) {
            Scenario s = generate(static_cast<uint64_t>(sd), 2 + sd % 3, 8000);
            s.profile = "hell"; s.bugs = {bug};
            caught += !runCounted(s, opts()).ok;
        }
        CHECK_MSG(caught > 0, std::string("not caught under the hell profile: ") + bug);
    }
}

int main(int argc, char** argv) {
    const char* group = nullptr; const char* name = nullptr; bool list = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--group") && i + 1 < argc) group = argv[++i];
        else if (!std::strcmp(argv[i], "--name") && i + 1 < argc) name = argv[++i];
        else if (!std::strcmp(argv[i], "--list")) list = true;
    }
    int ran = 0, failed = 0;
    for (auto& c : minitest::registry()) {
        if (group && std::strcmp(group, c.group)) continue;
        if (name && std::strcmp(name, c.name)) continue;
        if (list) { std::printf("%s.%s\n", c.group, c.name); continue; }
        int before = minitest::state().failures, checksBefore = minitest::state().checks;
        try { c.fn(); } catch (const minitest::Abort&) {}
        catch (const std::exception& e) { ++minitest::state().failures; std::printf("    EXCEPTION %s\n", e.what()); }
        bool ok = minitest::state().failures == before;
        std::printf("[%s] %s.%s (%d checks)\n", ok ? "PASS" : "FAIL", c.group, c.name, minitest::state().checks - checksBefore);
        ++ran; if (!ok) ++failed;
    }
    if (list) return 0;
    std::printf("== %d tests, %d passed, %d failed, %d checks, %llu sessions, %d twins caught ==\n", ran, ran - failed, failed, minitest::state().checks,
                static_cast<unsigned long long>(g_sessions), g_twinsCaught);
    if (ran == 0) { std::printf("no tests matched\n"); return 1; }
    return failed ? 1 : 0;
}
