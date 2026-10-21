// Cluster: N machines on one InMemoryNetwork (lossy link simulator), a virtual clock, join / leave / host save-load,
// the invariant oracles, and a game-free replay of a recorded run. A fuzzer can use this directly or wire
// Machines itself.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "fakeworld/machine.h"
#include "fakeworld/net.h"

namespace coop::fakeworld {

// One input a machine received. A list of these (plus the config and seed) reproduces a run exactly.
struct TraceEvent {
    enum Kind : uint8_t { Tick = 0, Action = 1, Msg = 2, PeerUp = 3, PeerDown = 4, HostLoad = 5 };
    uint64_t tMs = 0;
    Kind kind = Tick;
    PlayerId machine = 0;     // the machine the event happens on
    PlayerId from = 0;        // Msg / PeerUp / PeerDown: the other end
    uint8_t channel = 0;      // Msg
    uint32_t connEpoch = 0;   // PeerUp
    Bytes data;               // Msg: encoded envelope; Action: encoded Action; HostLoad: save bytes
};

// Observer that captures the replayable input stream (application level, after the link layer).
class TraceCapture : public IEngineObserver {
public:
    std::vector<TraceEvent> events;
    void onRecv(PlayerId self, uint64_t now, PeerId from, uint8_t channel, const Bytes& env) override;
    void onAction(PlayerId self, uint64_t now, const Action& i) override;
    void note(const TraceEvent& e) { events.push_back(e); }
};

struct ClusterConfig {
    uint64_t seed = 1;
    int players = 2;                 // 1..4 (slot 0 is the host)
    uint32_t tickMs = 10;
    LinkParams link;                 // applies to every direction
    MachineConfig machine;           // template; self and seed are filled in per machine
};

struct Violation { uint64_t tMs = 0; std::string what; };

class Cluster {
public:
    explicit Cluster(const ClusterConfig& cfg, TraceCapture* capture = nullptr);
    ~Cluster();

    uint64_t now() const { return now_; }
    int players() const { return cfg_.players; }
    bool connected(PlayerId p) const { return p < machines_.size() && machines_[p] != nullptr; }
    Machine& machine(PlayerId p) { return *machines_.at(p); }
    const Machine& machine(PlayerId p) const { return *machines_.at(p); }
    Machine& host() { return *machines_.at(0); }
    InMemoryNetwork& net() { return net_; }
    const ClusterConfig& config() const { return cfg_; }

    void join(PlayerId p);       // fresh machine (a restart loses all client state), full state arrives over the link
    void leave(PlayerId p);
    size_t saveHost();           // returns a handle for loadHost
    bool loadHost(size_t handle);

    void step();                 // one tick for everything
    void run(uint32_t ms);
    // Runs until nothing is in flight and no actor moved for a while, or maxMs elapse. True when quiescent.
    bool quiesce(uint32_t maxMs = 20000);

    // Observer on all machines (also ones that join later). The cluster keeps its own oracle observer chained in front.
    void setObserver(IEngineObserver* o);
    void setCapture(TraceCapture* c);   // records the replayable input stream (ticks, joins, intents, messages)
    // Injects an application-level envelope as if `from` had sent it (stale-echo injection, replay of old messages).
    void inject(PlayerId to, PlayerId from, const Bytes& envelope, uint8_t channel = 0);

    // ---- oracles ----
    // Always-valid checks (safe mid-run): one owner per epoch, one AI driver per actor, nothing stale accepted,
    // nothing applied twice, no duplicate actors/items. New violations since the last call are returned.
    std::vector<Violation> checkAlways();
    // Valid after quiesce(): every machine equals the host (digests, exact actor state, ownership table), item
    // conservation, and no repairs happened since `repairBaseline`.
    std::vector<Violation> checkQuiescent();
    uint64_t totalRepairs() const;
    uint64_t totalDoubleApplied() const;
    const std::vector<Violation>& allViolations() const { return violations_; }
    std::map<Ref, int64_t> itemCensus() const;   // ground + containers (host) + every present player's inventory
    int64_t baselineItems(const Ref& base) const;
    uint64_t repairBaseline = 0;
    bool checkConservation = true;   // off after a host load (clients keep their own inventories, only the host's save is the world)
    void freezeAi(bool f);           // stop NPC decisions on every machine (quiesce() freezes and leaves it frozen)

private:
    class Oracle;
    void spawnMachine(PlayerId p);
    void pump();
    void deliverAll();

    ClusterConfig cfg_;
    InMemoryNetwork net_;
    std::vector<std::unique_ptr<Machine>> machines_;
    std::vector<Bytes> saves_;
    uint64_t now_ = 0;
    uint32_t connCounter_ = 1;
    IEngineObserver* user_ = nullptr;
    std::unique_ptr<Oracle> oracle_;
    std::vector<Violation> violations_;
    size_t reported_ = 0;
    std::map<Ref, int64_t> baseline_;
    uint64_t lastBusyMs_ = 0;
};

// Replays a captured event stream into fresh machines (no network). Used by shrinkers and by regression tests.
struct ReplayResult {
    std::vector<uint64_t> digests;   // per machine, per category (machine-major): 4 values each, 0 for absent machines
    std::vector<std::string> violations;
    uint64_t events = 0;
};
ReplayResult replay(const ClusterConfig& cfg, const std::vector<TraceEvent>& events);

}  // namespace coop::fakeworld
