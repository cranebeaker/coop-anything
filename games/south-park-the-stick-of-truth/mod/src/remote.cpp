#include "remote.h"

#include <cmath>

namespace coop {
namespace {

// Signed a - b in milliseconds, correct across the uint32 wrap (which is 49 days, so it is
// also correct for any session this game will ever have).
int32_t diff_ms(uint32_t a, uint32_t b)
{
    return static_cast<int32_t>(a - b);
}

// Degrees into [-180, 180). Anything outside is the same angle plus whole turns.
float wrap_degrees(float degrees)
{
    if (!std::isfinite(degrees)) {
        return 0.0f;   // a NaN yaw must not poison the pose
    }
    float wrapped = std::fmod(degrees + 180.0f, 360.0f);
    if (wrapped < 0.0f) {
        wrapped += 360.0f;
    }
    return wrapped - 180.0f;
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

void hold(RemoteBody::Pose& pose, const Snapshot& snapshot)
{
    pose.x = snapshot.x;
    pose.y = snapshot.y;
    pose.z = snapshot.z;
    pose.yaw = wrap_degrees(snapshot.yaw);
}

}  // namespace

void RemoteBody::Clear()
{
    history_[0] = Snapshot{};
    head_ = 0;
    count_ = 0;
}

const Snapshot& RemoteBody::at(size_t age) const
{
    const size_t index = (head_ + kHistory - (age % kHistory)) % kHistory;
    return history_[index];
}

bool RemoteBody::PushSnapshot(const Snapshot& snapshot)
{
    if (count_ > 0 && diff_ms(snapshot.time_ms, at(0).time_ms) <= 0) {
        return false;   // late or duplicate: it tells us nothing newer than what we have
    }
    head_ = (head_ + 1) % kHistory;
    history_[head_] = snapshot;
    if (count_ < kHistory) {
        ++count_;
    }
    return true;
}

bool RemoteBody::Push(const PlayerState& state, uint32_t now_ms)
{
    Snapshot snapshot;
    snapshot.time_ms = now_ms;
    snapshot.x = state.x;
    snapshot.y = state.y;
    snapshot.z = state.z;
    snapshot.yaw = state.yaw;
    return PushSnapshot(snapshot);
}

bool RemoteBody::Pull(Net& net, uint32_t now_ms)
{
    if (!net.Poll(now_ms)) {
        return false;
    }
    return Push(net.Peer(), now_ms);
}

Snapshot RemoteBody::Newest() const
{
    return count_ == 0 ? Snapshot{} : at(0);
}

RemoteBody::Pose RemoteBody::At(uint32_t now_ms) const
{
    Pose pose;   // zeroed and stale: the answer when the peer has never been heard
    if (count_ == 0) {
        return pose;
    }
    const Snapshot& newest = at(0);
    if (count_ < 2) {
        hold(pose, newest);
        pose.stale = true;
        return pose;
    }
    if (diff_ms(now_ms, newest.time_ms) > static_cast<int32_t>(kStaleGapMs)) {
        // The peer went quiet. Holding the last known spot beats a body sliding across the
        // level on a guess.
        hold(pose, newest);
        pose.stale = true;
        return pose;
    }

    const uint32_t target = now_ms - delay_ms_;
    // Walk the pairs from oldest to newest and take the one that brackets the target. Pairs
    // are disjoint, so there is at most one.
    for (size_t age = count_ - 1; age > 0; --age) {
        const Snapshot& older = at(age);
        const Snapshot& newer = at(age - 1);
        if (diff_ms(target, older.time_ms) < 0 || diff_ms(target, newer.time_ms) > 0) {
            continue;
        }
        const int32_t span = diff_ms(newer.time_ms, older.time_ms);
        if (span > static_cast<int32_t>(kStaleGapMs)) {
            // A hole in the stream. Interpolating across it would invent movement the peer
            // never made, so hold the newer end of the hole.
            hold(pose, newer);
            pose.stale = true;
            return pose;
        }
        const float t = span > 0 ? static_cast<float>(diff_ms(target, older.time_ms)) /
                                       static_cast<float>(span)
                                 : 1.0f;
        pose.x = lerp(older.x, newer.x, t);
        pose.y = lerp(older.y, newer.y, t);
        pose.z = lerp(older.z, newer.z, t);
        pose.yaw = wrap_degrees(older.yaw + wrap_degrees(newer.yaw - older.yaw) * t);
        pose.stale = false;
        return pose;
    }

    // The target is outside the history. Either the history has not caught up yet (startup
    // with a delay of a few send intervals) or it has run out (a delay longer than the
    // buffer). Clamp to the nearest end, and do not call it stale: everything we hold is
    // under kStaleGapMs old, which the check above already proved.
    hold(pose, diff_ms(target, newest.time_ms) > 0 ? newest : at(count_ - 1));
    pose.stale = false;
    return pose;
}

}  // namespace coop
