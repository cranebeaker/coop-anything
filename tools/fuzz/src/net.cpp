#include "coopfuzz/net.h"
#include <algorithm>

namespace coopfuzz {

bool linkProfile(const std::string& name, LinkParams& out) {
    LinkParams p;
    if (name == "lan") { p.minLatencyMs = 0; p.maxLatencyMs = 2; }
    else if (name == "good_wifi") { p.minLatencyMs = 5; p.maxLatencyMs = 25; p.loss = 0.005; p.duplicate = 0.002; }
    else if (name == "bad_wifi") { p.minLatencyMs = 20; p.maxLatencyMs = 150; p.loss = 0.05; p.duplicate = 0.02; p.corrupt = 0.002; }
    else if (name == "mobile") { p.minLatencyMs = 40; p.maxLatencyMs = 300; p.loss = 0.08; p.duplicate = 0.03; p.corrupt = 0.003; }
    else if (name == "hell") { p.minLatencyMs = 0; p.maxLatencyMs = 400; p.loss = 0.35; p.duplicate = 0.2; p.corrupt = 0.02; }
    else return false;
    out = p;
    return true;
}

std::vector<std::string> linkProfileNames() { return {"lan", "good_wifi", "bad_wifi", "mobile", "hell"}; }

LossyLink::LossyLink(int endpoints, uint64_t seed)
    : rng_(seed), inbox_(static_cast<size_t>(endpoints)), up_(static_cast<size_t>(endpoints), true) {}

bool LossyLink::connected(int id) const { return id >= 0 && static_cast<size_t>(id) < up_.size() && up_[static_cast<size_t>(id)]; }

void LossyLink::setConnected(int id, bool up) {
    if (id < 0 || static_cast<size_t>(id) >= up_.size()) return;
    up_[static_cast<size_t>(id)] = up;
    if (up) return;
    // Drop everything queued to or from the machine, including its unread inbox.
    std::vector<Msg> keep;
    while (!pending_.empty()) {
        Msg m = pending_.top();
        pending_.pop();
        if (m.from == id || m.to == id) ++stats_.droppedDown;
        else keep.push_back(std::move(m));
    }
    for (auto& m : keep) pending_.push(std::move(m));
    inbox_[static_cast<size_t>(id)].clear();
}

void LossyLink::enqueue(int from, int to, Bytes data) {
    uint64_t lat = link_.minLatencyMs;
    if (link_.maxLatencyMs > link_.minLatencyMs) lat = rng_.range(link_.minLatencyMs, link_.maxLatencyMs);
    pending_.push(Msg{now_ + lat, order_++, from, to, std::move(data)});
}

void LossyLink::send(int from, int to, Bytes data) {
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

void LossyLink::advance(uint64_t nowMs) {
    now_ = nowMs;
    while (!pending_.empty() && pending_.top().at <= now_) {
        Msg m = pending_.top();
        pending_.pop();
        if (!connected(m.to) || !connected(m.from)) { ++stats_.droppedDown; continue; }
        inbox_[static_cast<size_t>(m.to)].emplace_back(m.from, std::move(m.data));
        ++stats_.delivered;
    }
}

bool LossyLink::poll(int at, int& from, Bytes& data) {
    if (at < 0 || static_cast<size_t>(at) >= inbox_.size()) return false;
    auto& q = inbox_[static_cast<size_t>(at)];
    if (q.empty()) return false;
    from = q.front().first;
    data = std::move(q.front().second);
    q.pop_front();
    return true;
}

bool LossyLink::idle() const {
    if (!pending_.empty()) return false;
    for (auto& q : inbox_) if (!q.empty()) return false;
    return true;
}

}  // namespace coopfuzz
