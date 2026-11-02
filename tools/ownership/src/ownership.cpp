#include "coop/ownership.h"
#include <algorithm>

namespace coop {

const char* toString(OwnMsg::Kind k) {
    switch (k) {
        case OwnMsg::Grant: return "Grant";
        case OwnMsg::Transfer: return "Transfer";
        case OwnMsg::Ack: return "Ack";
        case OwnMsg::Commit: return "Commit";
        case OwnMsg::Release: return "Release";
    }
    return "?";
}
const char* toString(UpdateVerdict v) {
    switch (v) {
        case UpdateVerdict::Accept: return "Accept";
        case UpdateVerdict::NotOwner: return "NotOwner";
        case UpdateVerdict::StaleEpoch: return "StaleEpoch";
        case UpdateVerdict::FutureEpoch: return "FutureEpoch";
        case UpdateVerdict::NotYetActive: return "NotYetActive";
        case UpdateVerdict::EchoOfSelf: return "EchoOfSelf";
        case UpdateVerdict::UnknownActor: return "UnknownActor";
    }
    return "?";
}
const char* toString(ApplyResult v) {
    switch (v) {
        case ApplyResult::Applied: return "Applied";
        case ApplyResult::Duplicate: return "Duplicate";
        case ApplyResult::Stale: return "Stale";
    }
    return "?";
}

// ---- wire form -------------------------------------------------------------------------------

namespace {
void putVar(std::vector<uint8_t>& out, uint32_t v) {
    while (v >= 0x80) { out.push_back(static_cast<uint8_t>(v | 0x80)); v >>= 7; }
    out.push_back(static_cast<uint8_t>(v));
}
// Canonical LEB128 only: an overlong encoding or a value above 32 bits is rejected.
bool getVar(const uint8_t* d, size_t size, size_t& pos, uint32_t& v) {
    uint64_t r = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos >= size) return false;
        const uint8_t b = d[pos++];
        r |= static_cast<uint64_t>(b & 0x7F) << (7 * i);
        if (!(b & 0x80)) {
            if (b == 0 && i > 0) return false;
            if (r > 0xFFFFFFFFull) return false;
            v = static_cast<uint32_t>(r);
            return true;
        }
    }
    return false;
}
}  // namespace

void encode(const OwnMsg& m, std::vector<uint8_t>& out) {
    out.push_back(static_cast<uint8_t>(m.kind));
    putVar(out, m.netId);
    putVar(out, m.epoch);
    out.push_back(m.owner);
    out.push_back(m.prevOwner);
}

bool decode(const uint8_t* d, size_t size, OwnMsg& out) {
    size_t pos = 0;
    if (size < 1) return false;
    const uint8_t k = d[pos++];
    if (k < OwnMsg::Grant || k > OwnMsg::Release) return false;
    OwnMsg m;
    m.kind = static_cast<OwnMsg::Kind>(k);
    if (!getVar(d, size, pos, m.netId)) return false;
    if (!getVar(d, size, pos, m.epoch)) return false;
    if (size - pos != 2) return false;
    m.owner = d[pos];
    m.prevOwner = d[pos + 1];
    out = m;
    return true;
}

// ---- owner choice ----------------------------------------------------------------------------

PlayerId pickOwner(const std::vector<Presence>& who, PlayerId current, bool held, const OwnershipConfig& cfg) {
    const Presence* best = nullptr;
    const Presence* cur = nullptr;
    for (auto& p : who) {
        if (p.player == current) cur = &p;
        if (!p.eligible()) continue;
        if (!best) { best = &p; continue; }
        const bool pHost = p.player == kHostPlayer, bHost = best->player == kHostPlayer;
        if (pHost != bHost) { if (pHost) best = &p; continue; }
        if (bHost) continue;   // only one host
        if (p.distance < best->distance || (p.distance == best->distance && p.player < best->player)) best = &p;
    }
    const bool curAlive = cur && cur->connected;
    if (current == kNoPlayer || !curAlive) return best ? best->player : kNoPlayer;
    if (!best || best->player == current) return current;       // nobody better, or nobody eligible: keep (dormant)
    if (!cur->eligible()) return best->player;                  // current walked away from the cell
    if (ownershipFaults().noHysteresis) return best->player;
    if (held) return current;
    if (best->player == kHostPlayer) return best->player;       // host preferred once the hold is over
    return best->distance < cur->distance * (1.0 - cfg.switchMargin) ? best->player : current;
}

// ---- service ---------------------------------------------------------------------------------

OwnerEpoch OwnershipService::bump(const Rec& r) const {
    OwnerEpoch base = r.pending ? std::max(r.epoch, r.pendingEpoch) : r.epoch;
    return ownershipFaults().noEpochBump ? base : base + 1;
}

void OwnershipService::playerJoined(PlayerId p) { if (p < kMaxSlots) connected_ = static_cast<uint8_t>(connected_ | (1u << p)); }

void OwnershipService::grant(ActorNetId id, Rec& r, PlayerId owner, uint32_t nowMs, Out& out) {
    OwnMsg m;
    m.kind = OwnMsg::Grant; m.netId = id; m.owner = owner; m.prevOwner = r.owner;
    m.epoch = bump(r);
    r.epoch = m.epoch; r.owner = owner; r.pending = false; r.to = kNoPlayer; r.changedAtMs = nowMs;
    ++stats_.grants;
    out.push_back(m);
}

void OwnershipService::commit(ActorNetId id, Rec& r, uint32_t nowMs, bool forced, Out& out) {
    OwnMsg m;
    m.kind = OwnMsg::Commit; m.netId = id; m.owner = r.to; m.prevOwner = r.owner; m.epoch = r.pendingEpoch;
    r.owner = r.to; r.epoch = r.pendingEpoch; r.pending = false; r.to = kNoPlayer; r.changedAtMs = nowMs;
    ++stats_.commits;
    if (forced) ++stats_.forcedCommits;
    out.push_back(m);
}

void OwnershipService::beginTransfer(ActorNetId id, Rec& r, PlayerId to, uint32_t nowMs, Out& out) {
    OwnMsg m;
    m.kind = OwnMsg::Transfer; m.netId = id; m.owner = to; m.prevOwner = r.owner;
    m.epoch = bump(r);
    r.pending = true; r.to = to; r.pendingEpoch = m.epoch; r.pendingSinceMs = nowMs;
    ++stats_.transfers;
    out.push_back(m);
    if (ownershipFaults().transferNoAck) commit(id, r, nowMs, false, out);
}

void OwnershipService::release(ActorNetId id, Rec& r, uint32_t nowMs, Out& out) {
    OwnMsg m;
    m.kind = OwnMsg::Release; m.netId = id; m.owner = kNoPlayer; m.prevOwner = r.owner;
    m.epoch = bump(r);
    r.epoch = m.epoch; r.owner = kNoPlayer; r.pending = false; r.to = kNoPlayer; r.changedAtMs = nowMs;
    ++stats_.releases;
    out.push_back(m);
}

void OwnershipService::track(ActorNetId id, const std::vector<Presence>& who, uint32_t nowMs, Out& out) {
    actors_.emplace(id, Rec{});
    evaluate(id, who, nowMs, out);
}

void OwnershipService::untrack(ActorNetId id, Out& out) {
    auto it = actors_.find(id);
    if (it == actors_.end()) return;
    if (it->second.owner != kNoPlayer || it->second.pending) release(id, it->second, 0, out);
    actors_.erase(it);
}

void OwnershipService::evaluate(ActorNetId id, const std::vector<Presence>& who, uint32_t nowMs, Out& out) {
    auto it = actors_.find(id);
    if (it == actors_.end()) return;
    Rec& r = it->second;
    if (r.pending) return;   // one handoff at a time
    PlayerId target;
    const bool follower = r.recruiter != kNoPlayer && isConnected(r.recruiter) && !ownershipFaults().followerIgnored;
    if (follower) {
        target = r.recruiter;
    } else {
        const bool held = r.owner != kNoPlayer && nowMs - r.changedAtMs < cfg_.minHoldMs;
        target = pickOwner(who, r.owner, held, cfg_);
    }
    if (target == r.owner) return;
    if (target == kNoPlayer) { release(id, r, nowMs, out); return; }
    if (r.owner == kNoPlayer || !isConnected(r.owner)) { grant(id, r, target, nowMs, out); return; }
    beginTransfer(id, r, target, nowMs, out);
}

void OwnershipService::setRecruiter(ActorNetId id, PlayerId recruiter, const std::vector<Presence>& who, uint32_t nowMs, Out& out) {
    auto it = actors_.find(id);
    if (it == actors_.end()) return;
    it->second.recruiter = recruiter;
    evaluate(id, who, nowMs, out);
}

bool OwnershipService::onAck(const OwnMsg& ack, PlayerId from, uint32_t nowMs, Out& out) {
    if (ack.kind != OwnMsg::Ack) return false;
    auto it = actors_.find(ack.netId);
    if (it == actors_.end()) return false;
    Rec& r = it->second;
    if (!r.pending || ack.epoch != r.pendingEpoch || from != r.to) return false;
    commit(ack.netId, r, nowMs, false, out);
    return true;
}

void OwnershipService::tick(uint32_t nowMs, Out& out) {
    for (auto& kv : actors_) {
        Rec& r = kv.second;
        if (!r.pending || nowMs - r.pendingSinceMs < cfg_.handoffTimeoutMs) continue;
        if (isConnected(r.to)) {
            commit(kv.first, r, nowMs, true, out);
        } else {   // new owner vanished without playerLeft reaching us: cancel the handoff
            ++stats_.abortedHandoffs;
            grant(kv.first, r, r.owner, nowMs, out);
        }
    }
}

std::vector<ActorNetId> OwnershipService::playerLeft(PlayerId p, uint32_t nowMs, Out& out) {
    if (p < kMaxSlots) connected_ = static_cast<uint8_t>(connected_ & ~(1u << p));
    std::vector<ActorNetId> orphans;
    for (auto& kv : actors_) {
        Rec& r = kv.second;
        if (r.recruiter == p) r.recruiter = kNoPlayer;
        if (r.pending) {
            if (r.to == p) {                       // the new owner left: cancel, the old owner keeps the actor
                ++stats_.abortedHandoffs;
                if (r.owner != p && isConnected(r.owner)) { grant(kv.first, r, r.owner, nowMs, out); continue; }
                release(kv.first, r, nowMs, out); orphans.push_back(kv.first); continue;
            }
            if (r.owner == p) { commit(kv.first, r, nowMs, true, out); }   // the old owner left: finish the handoff now
            continue;
        }
        if (r.owner == p) { release(kv.first, r, nowMs, out); orphans.push_back(kv.first); }
    }
    return orphans;
}

void OwnershipService::snapshot(Out& out) const {
    for (auto& kv : actors_) {
        const Rec& r = kv.second;
        if (r.epoch == 0 && r.owner == kNoPlayer) continue;   // never owned
        OwnMsg m;
        m.netId = kv.first; m.epoch = r.epoch; m.owner = r.owner;
        m.kind = r.owner == kNoPlayer ? OwnMsg::Release : OwnMsg::Grant;
        out.push_back(m);
    }
}

bool OwnershipService::current(ActorNetId id, OwnMsg& out) const {
    auto it = actors_.find(id);
    if (it == actors_.end()) return false;
    out = OwnMsg{};
    out.netId = id; out.epoch = it->second.epoch; out.owner = it->second.owner;
    out.kind = out.owner == kNoPlayer ? OwnMsg::Release : OwnMsg::Grant;
    return true;
}

PlayerId OwnershipService::ownerOf(ActorNetId id) const { auto it = actors_.find(id); return it == actors_.end() ? kNoPlayer : it->second.owner; }
OwnerEpoch OwnershipService::epochOf(ActorNetId id) const { auto it = actors_.find(id); return it == actors_.end() ? 0 : it->second.epoch; }
bool OwnershipService::handoffPending(ActorNetId id) const { auto it = actors_.find(id); return it != actors_.end() && it->second.pending; }

// ---- view ------------------------------------------------------------------------------------

ApplyResult OwnershipView::apply(const OwnMsg& m) {
    auto it = actors_.find(m.netId);
    const bool known = it != actors_.end();
    Rec fresh;
    Rec& r = known ? it->second : fresh;
    const bool noOrder = ownershipFaults().acceptStaleEpoch || ownershipFaults().adoptEcho;

    auto finish = [&](ApplyResult res) {
        if (res == ApplyResult::Applied) { ++stats_.applied; if (!known) actors_[m.netId] = r; }
        else if (res == ApplyResult::Duplicate) ++stats_.duplicates;
        else ++stats_.staleMsgs;
        return res;
    };

    switch (m.kind) {
        case OwnMsg::Ack:
            return finish(ApplyResult::Duplicate);   // host-only message
        case OwnMsg::Transfer: {
            if (!noOrder) {
                if (known && r.pending && m.epoch == r.pendingEpoch && m.owner == r.to) return finish(ApplyResult::Duplicate);
                if (known && m.epoch <= r.epoch) return finish(m.epoch == r.epoch && m.owner == r.owner ? ApplyResult::Duplicate : ApplyResult::Stale);
                if (known && r.pending && m.epoch < r.pendingEpoch) return finish(ApplyResult::Stale);
            }
            if (!known) { r.owner = m.prevOwner; r.epoch = m.epoch ? m.epoch - 1 : 0; }
            r.pending = true; r.to = m.owner; r.pendingEpoch = m.epoch;
            return finish(ApplyResult::Applied);
        }
        case OwnMsg::Grant:
        case OwnMsg::Commit:
        case OwnMsg::Release: {
            const PlayerId newOwner = m.kind == OwnMsg::Release ? kNoPlayer : m.owner;
            if (known && !noOrder) {
                if (m.epoch < r.epoch) return finish(ApplyResult::Stale);
                if (m.epoch == r.epoch) return finish(newOwner == r.owner ? ApplyResult::Duplicate : ApplyResult::Stale);
            }
            if (known && ownershipFaults().adoptEcho == false && r.pending && m.epoch < r.pendingEpoch && m.kind == OwnMsg::Commit)
                return finish(ApplyResult::Stale);
            r.owner = newOwner; r.epoch = m.epoch; r.pending = false; r.to = kNoPlayer;
            return finish(ApplyResult::Applied);
        }
    }
    return finish(ApplyResult::Stale);
}

UpdateVerdict OwnershipView::check(ActorNetId id, PlayerId from, OwnerEpoch epoch) {
    auto it = actors_.find(id);
    if (it == actors_.end()) { ++stats_.unknown; return UpdateVerdict::UnknownActor; }
    const Rec& r = it->second;
    if (from == self_) { ++stats_.echoes; return UpdateVerdict::EchoOfSelf; }
    if (ownershipFaults().acceptNonOwner) { ++stats_.accepted; return UpdateVerdict::Accept; }
    if (ownershipFaults().acceptStaleEpoch && epoch <= r.epoch) { ++stats_.accepted; return UpdateVerdict::Accept; }
    if (epoch < r.epoch) { ++stats_.staleEpoch; return UpdateVerdict::StaleEpoch; }
    if (epoch > r.epoch) {
        if (r.pending && epoch == r.pendingEpoch && from == r.to) { ++stats_.notYetActive; return UpdateVerdict::NotYetActive; }
        ++stats_.futureEpoch; return UpdateVerdict::FutureEpoch;
    }
    if (r.owner == kNoPlayer || from != r.owner) { ++stats_.notOwner; return UpdateVerdict::NotOwner; }
    ++stats_.accepted;
    return UpdateVerdict::Accept;
}

bool OwnershipView::mayPublish(ActorNetId id, OwnerEpoch& epochOut) const {
    auto it = actors_.find(id);
    if (it == actors_.end() || it->second.owner != self_ || self_ == kNoPlayer) return false;
    epochOut = it->second.epoch;
    return true;
}

bool OwnershipView::handoffIncoming(ActorNetId id, OwnerEpoch& epochOut) const {
    auto it = actors_.find(id);
    if (it == actors_.end() || !it->second.pending || it->second.to != self_) return false;
    epochOut = it->second.pendingEpoch;
    return true;
}

PlayerId OwnershipView::ownerOf(ActorNetId id) const { auto it = actors_.find(id); return it == actors_.end() ? kNoPlayer : it->second.owner; }
OwnerEpoch OwnershipView::epochOf(ActorNetId id) const { auto it = actors_.find(id); return it == actors_.end() ? 0 : it->second.epoch; }

uint64_t OwnershipView::digest() const {
    uint64_t h = 1469598103934665603ull;
    for (auto& kv : actors_) {
        if (kv.second.owner == kNoPlayer) continue;   // unowned entries only remember an epoch (a joiner may not have them)
        uint32_t rec[3] = {kv.first, kv.second.owner, kv.second.epoch};
        h = hash64(rec, sizeof rec, h);
    }
    return h;
}

}  // namespace coop
