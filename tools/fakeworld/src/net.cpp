#include "fakeworld/net.h"
#include <algorithm>

namespace coop::fakeworld {

bool InMemoryNetwork::connected(PeerId id) const {
    auto it = up_.find(id);
    return it != up_.end() && it->second;
}

void InMemoryNetwork::setConnected(PeerId id, bool up) {
    up_[id] = up;
    if (up) return;
    // Drop everything queued to or from the peer, including its unread inbox.
    std::vector<Msg> keep;
    while (!pending_.empty()) {
        Msg m = pending_.top();
        pending_.pop();
        if (m.from == id || m.to == id) ++stats_.droppedDown;
        else keep.push_back(std::move(m));
    }
    for (auto& m : keep) pending_.push(std::move(m));
    inbox_[id].clear();
}

void InMemoryNetwork::enqueue(PeerId from, PeerId to, Bytes data) {
    uint64_t lat = link_.minLatencyMs;
    if (link_.maxLatencyMs > link_.minLatencyMs) lat = rng_.range(link_.minLatencyMs, link_.maxLatencyMs);
    pending_.push(Msg{now_ + lat, order_++, from, to, std::move(data)});
}

void InMemoryNetwork::send(PeerId from, PeerId to, Bytes data) {
    ++stats_.sent;
    if (!connected(from) || !connected(to)) { ++stats_.droppedDown; return; }
    if (link_.loss > 0 && rng_.chance(link_.loss)) { ++stats_.lost; return; }
    if (link_.corrupt > 0 && !data.empty() && rng_.chance(link_.corrupt)) {
        data[rng_.u32() % data.size()] ^= static_cast<uint8_t>(1u << (rng_.u32() % 8));
        ++stats_.corrupted;
    }
    if (link_.duplicate > 0 && rng_.chance(link_.duplicate)) { enqueue(from, to, data); ++stats_.duplicated; }
    enqueue(from, to, std::move(data));
}

void InMemoryNetwork::advance(uint64_t nowMs) {
    now_ = nowMs;
    while (!pending_.empty() && pending_.top().at <= now_) {
        Msg m = pending_.top();
        pending_.pop();
        if (!connected(m.to) || !connected(m.from)) { ++stats_.droppedDown; continue; }
        inbox_[m.to].emplace_back(m.from, std::move(m.data));
        ++stats_.delivered;
    }
}

bool InMemoryNetwork::poll(PeerId self, PeerId& from, Bytes& data) {
    auto it = inbox_.find(self);
    if (it == inbox_.end() || it->second.empty()) return false;
    from = it->second.front().first;
    data = std::move(it->second.front().second);
    it->second.pop_front();
    return true;
}

bool InMemoryNetwork::idle() const {
    if (!pending_.empty()) return false;
    for (auto& kv : inbox_) if (!kv.second.empty()) return false;
    return true;
}

}  // namespace coop::fakeworld
