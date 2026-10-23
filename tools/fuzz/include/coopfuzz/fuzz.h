// coopfuzz: a seeded co-op session fuzzer with invariant oracles.
//
// The fuzzer builds a random N-player session (a list of timed ops), drives your sync core through the ITarget
// interface, heals the network, waits for quiet, and then asks the oracles whether every machine ended up in the same
// world and whether the apply log shows any forbidden behaviour. When something breaks it shrinks the session to a
// small replayable scenario and writes a JSON report.
//
// You implement ITarget (see GUIDE.md). example/example_target.cpp is a complete, game-free example.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "coopfuzz/net.h"

namespace coopfuzz {

// ------------------------------------------------------------------ scenario
// The generic vocabulary of things a player can do. A target maps each op onto what its game has and may ignore ops it
// cannot express (for example a game with no doors ignores Activate).
enum class OpKind : uint8_t { Move, Hit, Pickup, Drop, Spawn, Activate, Join, Leave, Save, Load, SetLink, Stale, Count };
const char* opName(OpKind k);
bool opFromName(const std::string& s, OpKind& out);

// Every op carries its own absolute time and explicit small ints. References are slot numbers that the target maps onto
// whatever exists at that moment (modulo), so removing other ops during shrinking never makes an op invalid.
//   Move      m, a=x(0..1000) b=y(0..1000) c=cell(0..2)
//   Hit       m, a=victim slot (modulo live actors), b=damage
//   Pickup    m, a=item slot (modulo ground items)
//   Drop      m, a=held item slot, b=x c=y d=cell
//   Spawn     m, a=site (0..4): an encounter that must spawn once for the whole session
//   Activate  m, a=door slot
//   Join/Leave  m (machine 0, the host, never leaves)
//   Save/Load   host save point / load the latest save
//   SetLink   a=minLatencyMs b=maxLatencyMs c=loss permille d=duplicate permille
//   Stale     m=receiver, a=sender, b=which recorded message, c=0 old-content replay (fresh packet) / 1 raw duplicate
struct Op {
    uint32_t t = 0;
    OpKind kind = OpKind::Move;
    int m = 0, a = 0, b = 0, c = 0, d = 0;
};

struct Scenario {
    uint64_t seed = 1;
    int machines = 3;                 // 2..4, machine 0 is the host
    uint32_t durationMs = 20000;      // chaos phase length (virtual time)
    LinkParams link;                  // initial chaos link
    std::vector<std::string> bugs;    // injected bug classes (mutation twins); empty = clean
    std::string profile;              // link preset name (see linkProfileNames); empty = use `link` and SetLink ops
    std::string target;               // which target the scenario was made for; empty = the default target
    std::string expect;               // regression fixtures: the invariant that must break while the bug is injected
    std::vector<Op> ops;              // sorted by t
};

Scenario generate(uint64_t seed, int machines, uint32_t durationMs);
std::string toJson(const Scenario& s);
bool fromJson(const std::string& text, Scenario& out, std::string* err = nullptr);

// ------------------------------------------------------------------ observations (what the oracles read)
enum class ApplyKind : uint8_t { Pos, PlayerPos, Hp, Owner, Holder, Door, Spawn, Remove, Reset, Intent };
const char* applyKindName(ApplyKind k);

// One call per message a machine applies or rejects.
struct ApplyEvent {
    int machine = 0;
    uint64_t tMs = 0;
    uint64_t ord = 0;         // global order of occurrence, shared with OwnEvent
    uint64_t idem = 0;        // identity of the message being applied; the same id accepted twice on one machine = applied twice
    int ref = -1;             // net id of the thing, or -1
    uint32_t epoch = 0;       // ownership epoch the message carried (0 = n/a)
    uint32_t seq = 0;         // sequence inside (ref, epoch) for ordered streams (0 = n/a)
    int origin = -1;          // machine that produced the content
    ApplyKind kind = ApplyKind::Pos;
    bool accepted = true;
    uint32_t worldEpoch = 0;  // world (save/load) epoch the message was made under
    std::string why;          // reject reason or note
};

// One call per ownership decision made by whoever hands out ownership (usually the host).
struct OwnEvent {
    uint64_t tMs = 0;
    uint64_t ord = 0;
    enum Kind : uint8_t { Grant, Transfer, Release, WorldEpoch } kind = Grant;
    int ref = -1;
    int owner = -1;           // new owner (-1 on release)
    int prevOwner = -1;
    uint32_t epoch = 0;       // ownership epoch (WorldEpoch: the new world epoch)
};

// One actor, item, player or door as a machine sees it.
struct Thing {
    uint32_t netId = 0;
    uint8_t type = 0;         // 0 player 1 actor 2 item 3 door
    uint32_t ident = 0;       // identity independent of netId (spawn site slot / original item); 0 = none
    int x = 0, y = 0, cell = 0, hp = 0;
    bool dead = false;
    int owner = -1;
    uint32_t epoch = 0;
    int holder = -1;          // items: machine holding it, -1 on the ground
    bool open = false;
    bool operator==(const Thing& o) const;
    std::string str() const;
};

struct Violation {
    std::string invariant;   // converge, double_apply, multi_owner, dup_thing, stale_applied, echo_adopted, repairs_after_quiescence, not_quiescent
    std::string detail;
    uint64_t tMs = 0;
};

constexpr int kDigestCats = 4;   // digest categories compared between machines (story, actors, containers, objects in the example)

// ------------------------------------------------------------------ the target interface: wrap YOUR sync core in this
class ITarget {
public:
    virtual ~ITarget() = default;
    virtual void apply(const Op& op) = 0;          // run one op "now" on machine op.m
    virtual void advance(uint64_t toMs) = 0;       // run the virtual clock up to toMs (the target steps its machines and network)
    virtual uint64_t now() const = 0;
    virtual void heal() = 0;                       // perfect link, everyone reconnected, new activity frozen
    virtual int machines() const = 0;
    virtual bool connected(int m) const = 0;
    virtual bool quiescent() const = 0;            // nothing in flight, everyone caught up, activity frozen
    virtual std::vector<Thing> view(int m) const = 0;   // everything machine m currently has, sorted by netId
    virtual uint64_t digest(int m, int cat) const = 0;  // hash of machine m's state for one category; equal on all machines at rest
    virtual uint64_t repairs(int m) const = 0;     // heal or resync actions machine m performed so far (counter, never decreases)
    virtual const std::vector<ApplyEvent>& applyLog() const = 0;
    virtual const std::vector<OwnEvent>& ownLog() const = 0;
    virtual std::map<std::string, uint64_t> stats() const = 0;   // free-form counters, printed as coverage
    // Optional hooks.
    virtual void markQuiescent() {}                            // called once when quiescence was reached
    virtual std::vector<Violation> nativeViolations() { return {}; }   // your own checks, mapped to the invariant names above
};

// Convenience base: stores the two logs and counters. Call logApply for every message a machine applies or rejects and
// logOwn for every ownership decision. A rejected apply is counted as "reject_<why>" in stats().
class TargetBase : public ITarget {
public:
    const std::vector<ApplyEvent>& applyLog() const override { return apply_; }
    const std::vector<OwnEvent>& ownLog() const override { return own_; }
    std::map<std::string, uint64_t> stats() const override { return counters_; }

protected:
    void logApply(int machine, uint64_t idem, int ref, uint32_t epoch, uint32_t seq, int origin, ApplyKind k, bool accepted, uint32_t worldEpoch,
                  const std::string& why);
    void logOwn(OwnEvent::Kind k, int ref, int owner, int prev, uint32_t epoch);
    std::vector<ApplyEvent> apply_;
    std::vector<OwnEvent> own_;
    std::map<std::string, uint64_t> counters_;
    uint64_t ord_ = 0;
};

// ------------------------------------------------------------------ target registry
// A target registers itself from its own .cpp file with a static TargetRegistrar (see example/example_target.cpp).
struct TargetInfo {
    std::string name;
    std::string description;
    std::vector<std::string> bugs;   // names of the injectable bug classes (mutation twins)
    std::function<std::unique_ptr<ITarget>(const Scenario&, std::string*)> make;   // sets *err and returns null for unknown bugs
};
void registerTarget(TargetInfo info);
struct TargetRegistrar { explicit TargetRegistrar(TargetInfo info) { registerTarget(std::move(info)); } };
const TargetInfo* findTarget(const std::string& name);   // empty name = the first registered target
std::vector<const TargetInfo*> allTargets();
std::unique_ptr<ITarget> makeTarget(const std::string& name, const Scenario& s, std::string* err = nullptr);
std::vector<std::string> knownBugs(const std::string& target);

// ------------------------------------------------------------------ oracles
// Log-based checks (valid at any time): double_apply, multi_owner, stale_applied, echo_adopted.
void checkLogs(const ITarget& t, std::vector<Violation>& out);
// State checks (only meaningful when the target is quiescent): converge, dup_thing.
void checkConverged(const ITarget& t, std::vector<Violation>& out);

// ------------------------------------------------------------------ run / shrink / report
struct RunResult {
    bool ok = true;
    std::vector<Violation> violations;
    std::map<std::string, uint64_t> stats;
    uint64_t virtualMs = 0;
    uint64_t opsRun = 0;
};
struct RunOptions {
    std::string target;                // empty = the scenario's target, else the default target
    uint32_t quiesceCapMs = 60000;     // longest wait for quiescence after healing
    uint32_t afterQuiesceMs = 10000;   // repairs must stay flat this long
    std::string reportDir;             // non-empty: on failure write <name>.scenario.json and <name>.report.json here
    std::string reportName = "fail";
    std::string reportExtra;           // raw JSON members (without braces) appended to the report, e.g. "\"shrunk_from_ops\": 500"
};
RunResult run(const Scenario& s, const RunOptions& opt);
// Delta-debug the ops (then the link, the machine count and the duration) down to a smaller scenario that still violates `invariant`.
Scenario shrink(const Scenario& s, const RunOptions& opt, const std::string& invariant, int* runs = nullptr);
bool writeReport(const ITarget& t, const Scenario& s, const RunResult& r, const std::string& dir, const std::string& name, const std::string& extra);

std::string fnvHex(const std::string& s);
std::string jsonEscape(const std::string& s);

}  // namespace coopfuzz
