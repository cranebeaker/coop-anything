// A tiny lossy link simulator on a virtual clock. Messages can be delayed, reordered (latency jitter), lost,
// duplicated and corrupted (one flipped bit). Everything is driven by a seeded RNG, so a run is repeatable.
// Replace it with your own transport model if you have one: the Machine only needs "send bytes to a peer" and
// "poll bytes from a peer".
#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <vector>
#include "fakeworld/rng.h"
#include "fakeworld/wire.h"

namespace coop::fakeworld {

using PeerId = uint32_t;

// Which channel a message used. The simulator treats both the same (it is lossy either way); the link layer inside
// Machine adds its own retransmission for Reliable and the recorder hook reports the channel.
enum class Channel : uint8_t { Reliable = 0, Unreliable = 1 };

struct LinkParams {
    uint32_t minLatencyMs = 0, maxLatencyMs = 0;  // uniform; jitter above min causes reordering
    double loss = 0;                              // 0..1 per message
    double duplicate = 0;                         // 0..1 chance of an extra copy
    double corrupt = 0;                           // 0..1 chance of a single flipped bit
};

class InMemoryNetwork {
public:
    explicit InMemoryNetwork(uint64_t seed = 1) : rng_(seed) {}

    void setLink(const LinkParams& p) { link_ = p; }   // applies to every direction
    const LinkParams& link() const { return link_; }
    void send(PeerId from, PeerId to, Bytes data);
    // Pops one delivered message for `self`; false when none are waiting.
    bool poll(PeerId self, PeerId& from, Bytes& data);
    void setConnected(PeerId id, bool up);             // down: its queued traffic is dropped, sends vanish
    bool connected(PeerId id) const;
    void advance(uint64_t nowMs);                      // delivers everything whose time has come
    size_t inFlight() const { return pending_.size(); }
    bool idle() const;                                 // nothing in flight and no unread inbox

    struct Stats { uint64_t sent = 0, delivered = 0, lost = 0, duplicated = 0, corrupted = 0, droppedDown = 0; };
    const Stats& stats() const { return stats_; }

private:
    struct Msg {
        uint64_t at; uint64_t order; PeerId from, to; Bytes data;
        bool operator>(const Msg& o) const { return at != o.at ? at > o.at : order > o.order; }
    };
    void enqueue(PeerId from, PeerId to, Bytes data);

    Rng rng_;
    LinkParams link_;
    uint64_t now_ = 0, order_ = 0;
    std::map<PeerId, std::deque<std::pair<PeerId, Bytes>>> inbox_;   // per receiver, in arrival order
    std::map<PeerId, bool> up_;
    std::priority_queue<Msg, std::vector<Msg>, std::greater<Msg>> pending_;
    Stats stats_;
};

}  // namespace coop::fakeworld
