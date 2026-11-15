// The remote body: turns the snapshots arriving from coop::Net into a pose to draw.
//
// A snapshot says where the peer was when it sent it. Drawing that directly gives a body that
// teleports twenty times a second, and that freezes the moment one packet is lost. So the body
// keeps a short history and renders the peer where it was `delay_ms` ago (100 ms by default,
// five send intervals), interpolating between the two snapshots that bracket that moment. The
// delay is the whole smoothing: a late packet that still lands inside the window is used, and
// the pose moves at the peer's real speed instead of in steps.
//
// Timestamps are the receiver's own clock, taken when the snapshot was read - `Net::Poll`
// already stamps peer state that way - so the two machines do not need synchronised clocks.
//
// The rules are deliberately small, and every one of them holds rather than guesses:
//
// - no snapshot yet: origin, `stale`;
// - fewer than two snapshots: hold the newest, `stale` (there is no pair to interpolate);
// - nothing heard for more than 500 ms: hold the newest, `stale` - a lost peer is not a
//   reason to extrapolate a body off into the level;
// - a hole between two buffered snapshots of more than 500 ms: hold the newer one, `stale`;
// - the render time is past the newest snapshot but under that 500 ms: hold the newest, not
//   stale - that is one late packet, the normal case;
// - the render time is before the oldest snapshot (startup, while the history fills, or a
//   delay longer than the history): clamp to the oldest, not stale - the data is real, the
//   buffer is just young.
//
// Yaw is in degrees, like `PlayerState::yaw`, and interpolates the short way round the wrap:
// 170 to -170 goes through 180, not through 0. A pose's `yaw` is always in [-180, 180).
#pragma once

#include <cstddef>
#include <cstdint>

#include "net.h"

namespace coop {

struct Snapshot {
    uint32_t time_ms = 0;   // receiver's clock, when this state was read
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;       // degrees
};

class RemoteBody
{
public:
    struct Pose {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yaw = 0.0f;   // degrees, [-180, 180)
        bool stale = true;  // the pose is a hold, not an interpolation
    };

    static constexpr size_t kHistory = 16;          // snapshots kept; 800 ms at 20 Hz
    static constexpr uint32_t kDefaultDelayMs = 100;
    static constexpr uint32_t kStaleGapMs = 500;

    RemoteBody() = default;
    explicit RemoteBody(uint32_t delay_ms) : delay_ms_(delay_ms) {}

    void SetDelayMs(uint32_t delay_ms) { delay_ms_ = delay_ms; }
    uint32_t DelayMs() const { return delay_ms_; }

    // Forgets the peer, e.g. on leave or a level change: the next pose is stale until two
    // snapshots have arrived again.
    void Clear();

    // Records the peer's state as it was received at `now_ms`. A snapshot whose time is not
    // newer than the newest one is dropped - the history is a timeline, not a log - and the
    // return value says whether it was kept.
    bool Push(const PlayerState& state, uint32_t now_ms);
    bool PushSnapshot(const Snapshot& snapshot);

    // Polls `net` and records the peer state when a fresh one arrived. True when the history
    // grew; false means nothing new this frame, which is not an error.
    bool Pull(Net& net, uint32_t now_ms);

    // Where to draw the peer at `now_ms`: the interpolation target is `now_ms - delay_ms`.
    Pose At(uint32_t now_ms) const;

    size_t Count() const { return count_; }
    Snapshot Newest() const;   // zeroed when the history is empty

private:
    const Snapshot& at(size_t age) const;   // 0 = newest, Count()-1 = oldest

    Snapshot history_[kHistory] = {};
    size_t head_ = 0;                       // index of the newest snapshot
    size_t count_ = 0;
    uint32_t delay_ms_ = kDefaultDelayMs;
};

}  // namespace coop
