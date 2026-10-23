// Actor ownership with epochs. Plain C++17, no dependencies, no network, no clock.
//
// Rule: every live actor is simulated by exactly one machine, its OWNER. Ownership carries an EPOCH that rises on
// every change. Only the owner at the current epoch may publish the actor's movement, stats, death or inventory.
//
//   OwnershipService  host side. Decides who owns what (nearest player, host preferred, hysteresis, follower
//                     exception) and emits OwnMsg for you to broadcast. Time is passed in, never read.
//   OwnershipView     every machine (the host too). Applies OwnMsg and judges incoming actor updates (accept or
//                     reject with a reason). Epoch ordering makes every message idempotent and order independent.
//
// A handoff is a handshake, so no state is lost and the AI never runs twice:
//   Transfer(e+1, to) -> old owner keeps publishing at epoch e -> new owner applies the last state and sends Ack
//   -> host sends Commit(e+1) -> new owner starts its AI and publishes at e+1; the old owner stops.
// If the Ack does not arrive in time, the host commits anyway (the new owner is still connected) or grants the
// actor back to the old owner.
//
// Limits: at most 8 player slots (kMaxSlots), slot 0 is the host. Not thread safe: use one instance per thread.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include "coop/types.h"

namespace coop {

using ActorNetId = uint32_t;
using OwnerEpoch = uint32_t;

struct OwnMsg {
    enum Kind : uint8_t {
        Grant = 1,     // actor is owned by `owner` at `epoch` (unowned -> owned, or abort of a handoff)
        Transfer = 2,  // begin handoff prevOwner -> owner; takes effect (epoch) only at Commit
        Ack = 3,       // new owner -> host: last state applied, ready to take over (carries the Transfer's epoch)
        Commit = 4,    // host -> all: handoff finished, `owner` is authoritative at `epoch`
        Release = 5,   // actor has no owner at `epoch` (cell unloaded for everyone, owner left)
    };
    Kind kind = Grant;
    ActorNetId netId = 0;
    OwnerEpoch epoch = 0;
    PlayerId owner = kNoPlayer;
    PlayerId prevOwner = kNoPlayer;
    friend bool operator==(const OwnMsg& a, const OwnMsg& b) {
        return a.kind == b.kind && a.netId == b.netId && a.epoch == b.epoch && a.owner == b.owner && a.prevOwner == b.prevOwner;
    }
};
const char* toString(OwnMsg::Kind k);

// Optional compact wire form (LEB128 varints). Use it or write your own: the library never touches a network.
void encode(const OwnMsg& m, std::vector<uint8_t>& out);              // appends to `out`
bool decode(const uint8_t* data, size_t size, OwnMsg& out);           // false on any malformed or truncated input

// What the host knows about one player relative to one actor, at evaluation time.
struct Presence {
    PlayerId player = kNoPlayer;
    bool connected = true;
    bool inCell = false;     // player is loaded in the actor's cell
    bool adjacent = false;   // player is in a neighbouring loaded cell
    float distance = 0;      // to the actor (any unit, only compared)
    bool eligible() const { return connected && (inCell || adjacent); }
};

struct OwnershipConfig {
    uint32_t minHoldMs = 1500;         // after a change the owner is kept this long unless it became ineligible
    double switchMargin = 0.25;        // a challenger must be this fraction closer than the current owner
    uint32_t handoffTimeoutMs = 1000;  // Ack wait before the host forces the outcome
};

// TEST-ONLY fault switches (the mutation twins). Each one re-creates a known bug so a test can prove it would have
// caught it. All false in production. They are process-global: set them only inside OwnershipFaultScope.
struct OwnershipFaults {
    bool noEpochBump = false;       // changes keep the old epoch: stale and fresh owners are indistinguishable
    bool acceptStaleEpoch = false;  // views apply older messages and accept older-epoch updates (the stale-echo bug)
    bool adoptEcho = false;         // a view lets a replayed older assignment overwrite a newer one
    bool transferNoAck = false;     // handoff commits at once: the old owner's last state is lost
    bool noHysteresis = false;      // ownership flaps with distance
    bool followerIgnored = false;   // companions are owned by whoever is nearest
    bool acceptNonOwner = false;    // views accept updates from any player
};
inline OwnershipFaults& ownershipFaults() { static OwnershipFaults f; return f; }
struct OwnershipFaultScope {
    OwnershipFaults saved;
    explicit OwnershipFaultScope(const OwnershipFaults& f) : saved(ownershipFaults()) { ownershipFaults() = f; }
    ~OwnershipFaultScope() { ownershipFaults() = saved; }
};

struct OwnershipStats {
    uint64_t grants = 0, transfers = 0, commits = 0, releases = 0, forcedCommits = 0, abortedHandoffs = 0;
};

// The pure owner-choice rule (exposed for tests). `current` is kNoPlayer if unowned; `held` is true while
// minHoldMs since the last change has NOT yet elapsed. Returns the owner to use (kNoPlayer = nobody).
PlayerId pickOwner(const std::vector<Presence>& who, PlayerId current, bool held, const OwnershipConfig& cfg);

class OwnershipService {
public:
    using Out = std::vector<OwnMsg>;
    explicit OwnershipService(OwnershipConfig cfg = {}) : cfg_(cfg) {}

    void playerJoined(PlayerId p);
    // Marks the player gone. Actors they owned are released (Release msgs appended) and returned so the caller
    // can evaluate() them again; a handoff pointing at them is aborted; recruiter links to them are cleared.
    std::vector<ActorNetId> playerLeft(PlayerId p, uint32_t nowMs, Out& out);

    // Starts tracking an actor and picks its first owner (Grant appended if anyone is eligible).
    void track(ActorNetId id, const std::vector<Presence>& who, uint32_t nowMs, Out& out);
    void untrack(ActorNetId id, Out& out);   // Release appended if it was owned

    // Re-decides one actor from fresh presence data. Idempotent: no change -> nothing appended.
    void evaluate(ActorNetId id, const std::vector<Presence>& who, uint32_t nowMs, Out& out);

    // Follower exception: a companion is owned by its recruiter wherever it goes (kNoPlayer clears it).
    void setRecruiter(ActorNetId id, PlayerId recruiter, const std::vector<Presence>& who, uint32_t nowMs, Out& out);

    // A client's Ack for a pending Transfer. Stale or unknown acks are ignored (returns false).
    bool onAck(const OwnMsg& ack, PlayerId from, uint32_t nowMs, Out& out);
    // Drives handoff timeouts. Call once per host tick.
    void tick(uint32_t nowMs, Out& out);

    // Current assignment as Grant messages (join / rejoin / resync). Pending handoffs report their old owner.
    void snapshot(Out& out) const;
    // The same, for exactly one actor (used to answer a client that complains about a stale epoch).
    bool current(ActorNetId id, OwnMsg& out) const;

    PlayerId ownerOf(ActorNetId id) const;
    OwnerEpoch epochOf(ActorNetId id) const;
    bool handoffPending(ActorNetId id) const;
    size_t actorCount() const { return actors_.size(); }
    template <class F> void forEach(F&& f) const { for (auto& kv : actors_) f(kv.first, kv.second.owner, kv.second.epoch); }
    const OwnershipStats& stats() const { return stats_; }
    const OwnershipConfig& config() const { return cfg_; }

private:
    struct Rec {
        PlayerId owner = kNoPlayer;
        OwnerEpoch epoch = 0;
        uint32_t changedAtMs = 0;
        PlayerId recruiter = kNoPlayer;
        // pending handoff
        bool pending = false;
        PlayerId to = kNoPlayer;
        OwnerEpoch pendingEpoch = 0;
        uint32_t pendingSinceMs = 0;
    };
    OwnerEpoch bump(const Rec& r) const;
    void grant(ActorNetId id, Rec& r, PlayerId owner, uint32_t nowMs, Out& out);
    void beginTransfer(ActorNetId id, Rec& r, PlayerId to, uint32_t nowMs, Out& out);
    void commit(ActorNetId id, Rec& r, uint32_t nowMs, bool forced, Out& out);
    void release(ActorNetId id, Rec& r, uint32_t nowMs, Out& out);
    bool isConnected(PlayerId p) const { return p < kMaxSlots && ((connected_ >> p) & 1u); }

    OwnershipConfig cfg_;
    OwnershipStats stats_;
    uint8_t connected_ = 0;
    std::map<ActorNetId, Rec> actors_;
};

enum class ApplyResult : uint8_t { Applied, Duplicate, Stale };
enum class UpdateVerdict : uint8_t {
    Accept,
    NotOwner,       // sender is not the owner we know
    StaleEpoch,     // sender used an older epoch than ours (a past owner still talking)
    FutureEpoch,    // sender claims an epoch we have not seen yet (our view is behind: ask the host)
    NotYetActive,   // sender is the pending new owner, handoff not committed
    EchoOfSelf,     // the update came from ourselves (our own publish reflected back)
    UnknownActor,
};
const char* toString(UpdateVerdict v);
const char* toString(ApplyResult v);

struct ViewStats {
    uint64_t applied = 0, duplicates = 0, staleMsgs = 0;
    uint64_t accepted = 0, notOwner = 0, staleEpoch = 0, futureEpoch = 0, notYetActive = 0, echoes = 0, unknown = 0;
};

class OwnershipView {
public:
    explicit OwnershipView(PlayerId self = kNoPlayer) : self_(self) {}
    void setSelf(PlayerId p) { self_ = p; }
    PlayerId self() const { return self_; }

    ApplyResult apply(const OwnMsg& m);
    UpdateVerdict check(ActorNetId id, PlayerId from, OwnerEpoch epoch);   // counts into stats
    // The epoch to stamp on an update we publish for `id`, or false if we are not allowed to publish it.
    bool mayPublish(ActorNetId id, OwnerEpoch& epochOut) const;
    // True while we are the NEW owner of a handoff that is not committed yet (apply the state, then Ack).
    bool handoffIncoming(ActorNetId id, OwnerEpoch& epochOut) const;

    PlayerId ownerOf(ActorNetId id) const;
    OwnerEpoch epochOf(ActorNetId id) const;
    bool known(ActorNetId id) const { return actors_.count(id) != 0; }
    void forget(ActorNetId id) { actors_.erase(id); }
    void clear() { actors_.clear(); }
    template <class F> void forEach(F&& f) const { for (auto& kv : actors_) f(kv.first, kv.second.owner, kv.second.epoch); }
    const ViewStats& stats() const { return stats_; }
    uint64_t digest() const;   // hash of (id, owner, epoch) over committed assignments; equal views give equal digests

private:
    struct Rec {
        PlayerId owner = kNoPlayer;
        OwnerEpoch epoch = 0;
        bool pending = false;
        PlayerId to = kNoPlayer;
        OwnerEpoch pendingEpoch = 0;
    };
    PlayerId self_;
    ViewStats stats_;
    std::map<ActorNetId, Rec> actors_;
};

}  // namespace coop
