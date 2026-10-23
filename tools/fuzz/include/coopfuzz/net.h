// Built-in lossy link: a virtual-time datagram network between N machines in one process.
// Messages can be delayed (uniform latency, so jitter reorders them), lost, duplicated and corrupted (one flipped bit).
// Your target may use it for its packets, or bring its own transport and ignore this file.
#pragma once
#include <cstdint>
#include <queue>
#include <deque>
#include <string>
#include <vector>
#include "coopfuzz/rng.h"

namespace coopfuzz {

using Bytes = std::vector<uint8_t>;

struct LinkParams {
    uint32_t minLatencyMs = 0, maxLatencyMs = 0;  // uniform; jitter above min causes reordering
    double loss = 0;                              // 0..1 per message
    double duplicate = 0;                         // 0..1 chance of an extra copy
    double corrupt = 0;                           // 0..1 chance of a single flipped bit
};

// Named presets: lan, good_wifi, bad_wifi, mobile, hell. Returns false for an unknown name.
bool linkProfile(const std::string& name, LinkParams& out);
std::vector<std::string> linkProfileNames();

class LossyLink {
public:
    LossyLink(int endpoints, uint64_t seed);

    void setLink(const LinkParams& p) { link_ = p; }   // applies to every direction
    const LinkParams& link() const { return link_; }

    void send(int from, int to, Bytes data);
    bool poll(int at, int& from, Bytes& data);          // pops one received message; false when none is pending
    void advance(uint64_t nowMs);                       // delivers everything whose time has come

    void setConnected(int id, bool up);                 // down: its queued traffic is dropped, sends vanish
    bool connected(int id) const;
    bool idle() const;                                  // nothing in flight and no unread inbox

    struct Stats { uint64_t sent = 0, delivered = 0, lost = 0, duplicated = 0, corrupted = 0, droppedDown = 0; };
    const Stats& stats() const { return stats_; }

private:
    struct Msg {
        uint64_t at, order;
        int from, to;
        Bytes data;
        bool operator>(const Msg& o) const { return at != o.at ? at > o.at : order > o.order; }
    };
    void enqueue(int from, int to, Bytes data);

    Rng rng_;
    LinkParams link_;
    uint64_t now_ = 0, order_ = 0;
    std::vector<std::deque<std::pair<int, Bytes>>> inbox_;
    std::vector<bool> up_;
    std::priority_queue<Msg, std::vector<Msg>, std::greater<Msg>> pending_;
    Stats stats_;
};

}  // namespace coopfuzz
