// Machine: reliable-ordered link over a lossy datagram transport, envelope dispatch, peer lifecycle.
#include <algorithm>
#include <cstring>
#include "fakeworld/machine.h"
#include "fakeworld/wire.h"

namespace coop::fakeworld {

namespace {
constexpr uint8_t kMagic = 0xE7;
constexpr uint8_t kData = 0, kAck = 1, kRaw = 2;
constexpr size_t kHeader = 1 + 1 + 4 + 8;
constexpr size_t kAheadCap = 8192;

void put32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
void put64(Bytes& b, uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
uint32_t get32(const uint8_t* p) { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i); return v; }
uint64_t get64(const uint8_t* p) { uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(p[i]) << (8 * i); return v; }
}  // namespace

Machine::Machine(const MachineConfig& cfg)
    : cfg_(cfg), rng_(cfg.seed * 0x2545F4914F6CDD1Dull + cfg.self * 1315423911ull + 5),
      world_(World::build(cfg.spec)), view_(cfg.self) {
    world_.player(cfg_.self).slot = cfg_.self;
    if (!isHost()) for (auto& kv : world_.actors()) kv.second.aiOn = false;
    if (isHost()) {
        svc_ = std::make_unique<OwnershipService>(cfg_.own);
        svc_->playerJoined(kHostPlayer);
        PlayerState& p = world_.player(kHostPlayer);
        p.present = true; p.cell = 1; p.pos = world_.cellCenter(1);
        loaded_ = world_.loadedAround(p.cell);
        synced_ = true;
        std::vector<OwnMsg> out;
        for (auto& kv : world_.actors()) {
            if (kv.second.dead || !kv.second.enabled) continue;
            svc_->track(kv.first, presenceFor(kv.second), 0, out);
        }
        emitOwn(out);
    }
}

// ---- link ------------------------------------------------------------------------------------

void Machine::emitFrame(PeerId to, uint8_t type, uint64_t seq, const Bytes& payload, Channel ch) {
    auto it = links_.find(to);
    Bytes f;
    f.reserve(kHeader + payload.size() + 4);
    f.push_back(kMagic); f.push_back(type);
    put32(f, it == links_.end() ? 0 : it->second.connEpoch);
    put64(f, seq);
    f.insert(f.end(), payload.begin(), payload.end());
    put32(f, crc32(f.data(), f.size()));
    outbox_.push_back({to, ch, std::move(f)});
}

void Machine::sendPayload(PeerId to, const Bytes& payload, bool reliable) {
    auto it = links_.find(to);
    if (it == links_.end()) return;
    Link& l = it->second;
    if (!reliable) { emitFrame(to, kRaw, 0, payload, Channel::Unreliable); return; }
    const uint64_t seq = l.nextSend++;
    emitFrame(to, kData, seq, payload, Channel::Reliable);
    l.unacked.push_back({seq, outbox_.back().data, now_});
}

void Machine::sendBody(Body b, PlayerId dest) {
    Envelope e;
    e.sess = sess_; e.src = cfg_.self; e.dest = dest; e.body = std::move(b);
    const Bytes payload = encode(e);
    const bool rel = reliableByDefault(e.body);
    auto go = [&](PeerId to) {
        if (!links_.count(to)) return;
        if (obs_) obs_->onSend(cfg_.self, now_, to, rel ? 0 : 1, payload);
        sendPayload(to, payload, rel);
    };
    if (!isHost()) { go(kHostPlayer); return; }
    if (dest == kNoPlayer) { for (auto& kv : links_) go(kv.first); }
    else go(dest);
}

void Machine::relay(const Envelope& e) {
    const Bytes payload = encode(e);
    const bool rel = reliableByDefault(e.body);
    for (auto& kv : links_) {
        if (kv.first == e.src) continue;
        if (e.dest != kNoPlayer && kv.first != e.dest) continue;
        if (obs_) obs_->onSend(cfg_.self, now_, kv.first, rel ? 0 : 1, payload);
        sendPayload(kv.first, payload, rel);
    }
}

void Machine::receive(PeerId from, const Bytes& data) {
    onFrame(from, data);
    flushAcks();
}

void Machine::onFrame(PeerId from, const Bytes& data) {
    if (data.size() < kHeader + 4 || data[0] != kMagic) { ++stats_.undecodable; if (obs_) obs_->onDrop(cfg_.self, now_, from, "short or bad magic"); return; }
    const size_t body = data.size() - 4;
    if (crc32(data.data(), body) != get32(data.data() + body)) {
        ++stats_.framesBadCrc;
        if (obs_) obs_->onDrop(cfg_.self, now_, from, "bad crc");
        return;
    }
    auto lit = links_.find(from);
    if (lit == links_.end()) { ++stats_.framesStaleConn; if (obs_) obs_->onDrop(cfg_.self, now_, from, "no link"); return; }
    Link& l = lit->second;
    const uint8_t type = data[1];
    const uint32_t ce = get32(data.data() + 2);
    const uint64_t seq = get64(data.data() + 6);
    if (ce != l.connEpoch) { ++stats_.framesStaleConn; if (obs_) obs_->onDrop(cfg_.self, now_, from, "stale connection"); return; }
    Bytes payload(data.begin() + kHeader, data.begin() + static_cast<std::ptrdiff_t>(body));

    auto deliver = [&](const Bytes& p, uint8_t ch) { receiveEnvelope(from, p, ch); };

    if (type == kAck) {
        while (!l.unacked.empty() && l.unacked.front().seq <= seq) l.unacked.pop_front();
        return;
    }
    if (type == kRaw) { deliver(payload, 1); return; }
    if (type != kData) { ++stats_.undecodable; return; }
    l.ackDirty = true;
    if (seq < l.expect) {
        ++stats_.linkDuplicates;
        if (engineFaults().noDedupe) deliver(payload, 0);
        return;
    }
    if (seq > l.expect) {
        if (l.ahead.size() < kAheadCap) l.ahead.emplace(seq, std::move(payload));
        return;
    }
    deliver(payload, 0);
    ++l.expect;
    // delivering can synchronously reconnect or reset the link; re-find it every step
    for (;;) {
        auto again = links_.find(from);
        if (again == links_.end() || again->second.connEpoch != ce) return;
        Link& ll = again->second;
        auto nx = ll.ahead.find(ll.expect);
        if (nx == ll.ahead.end()) break;
        Bytes p = std::move(nx->second);
        ll.ahead.erase(nx);
        ++ll.expect;
        deliver(p, 0);
    }
}

void Machine::receiveEnvelope(PeerId from, const Bytes& envelope, uint8_t channel) {
    Envelope e;
    if (!decode(envelope, e)) { ++stats_.undecodable; if (obs_) obs_->onDrop(cfg_.self, now_, from, "undecodable"); return; }
    if (obs_) obs_->onRecv(cfg_.self, now_, from, channel, envelope);
    dispatch(from, e);
}

void Machine::flushAcks() {
    for (auto& kv : links_) {
        Link& l = kv.second;
        if (!l.ackDirty) continue;
        l.ackDirty = false;
        emitFrame(kv.first, kAck, l.expect - 1, {}, Channel::Unreliable);
    }
}

std::vector<Outgoing> Machine::takeOutbox() {
    std::vector<Outgoing> out;
    out.swap(outbox_);
    return out;
}

// ---- lifecycle -------------------------------------------------------------------------------

void Machine::peerConnected(PeerId peer, uint32_t connEpoch) {
    Link l;
    l.connEpoch = connEpoch;
    links_[peer] = std::move(l);
    const PlayerId p = static_cast<PlayerId>(peer);
    if (!isHost()) {
        connEpoch_ = connEpoch;
        synced_ = false;
        handingOff_.clear(); acked_.clear(); pendingClaims_.clear(); pendingPuts_.clear(); rolledSites_.clear();
        return;
    }
    svc_->playerJoined(p);
    PlayerState& ps = world_.player(p);
    ps = PlayerState{};
    ps.slot = p; ps.present = true; ps.cell = 1; ps.pos = world_.cellCenter(1);
    ps.poseSeq = connEpoch * 100000u;
    ps.vitalsSeq = connEpoch * 100000u;
    mismatch_.clear();
    sendBody(buildFullState(), p);
    MPose pose;
    pose.slot = p; pose.present = true; pose.cell = ps.cell; pose.pos = ps.pos; pose.seq = ps.poseSeq;
    MVitals vit;   // a rejoining slot starts fresh: other guests must drop the vitals they remember from its last life
    vit.slot = p; vit.health = ps.health; vit.dead = ps.dead; vit.seq = ps.vitalsSeq;
    for (auto& kv : links_) if (kv.first != peer) { sendBody(pose, static_cast<PlayerId>(kv.first)); sendBody(vit, static_cast<PlayerId>(kv.first)); }
}

void Machine::peerDisconnected(PeerId peer) {
    links_.erase(peer);
    const PlayerId p = static_cast<PlayerId>(peer);
    if (!isHost()) { synced_ = false; return; }
    PlayerState& ps = world_.player(p);
    ps.present = false;
    MPose pose;
    pose.slot = p; pose.present = false; pose.cell = ps.cell; pose.pos = ps.pos; pose.seq = ps.poseSeq + 1;
    ps.poseSeq = pose.seq;
    sendBody(pose, kNoPlayer);
    for (auto& kv : world_.containers()) if (kv.second.inUseBy == p) kv.second.inUseBy = kNoPlayer;
    for (auto it = mismatch_.begin(); it != mismatch_.end();) it = it->first.first == p ? mismatch_.erase(it) : std::next(it);
    std::vector<OwnMsg> out;
    const std::vector<ActorNetId> orphans = svc_->playerLeft(p, static_cast<uint32_t>(now_), out);
    for (ActorNetId id : orphans) {
        ActorState* a = world_.findActor(id);
        if (a && !a->dead && a->enabled) svc_->evaluate(id, presenceFor(*a), static_cast<uint32_t>(now_), out);
    }
    emitOwn(out);
}

// ---- dispatch --------------------------------------------------------------------------------

void Machine::dispatch(PeerId from, const Envelope& e) {
    const bool isFull = std::holds_alternative<MFullState>(e.body);
    if (e.sess < sess_ && !engineFaults().noSessionEpoch) {
        ++stats_.droppedStaleSession;
        if (obs_) obs_->onDrop(cfg_.self, now_, from, "stale session");
        return;
    }
    if (isHost() && e.src != from) { ++stats_.undecodable; return; }   // a client may only speak as itself
    if (!isHost() && !isFull && !synced_) return;                       // nothing applies before the full state
    // The host forwards anything addressed to another client; broadcasts are relayed by the handler that approves them.
    if (isHost() && e.dest != kHostPlayer && e.dest != kNoPlayer && !std::holds_alternative<MHit>(e.body)) { relay(e); return; }

    bool doRelay = false;
    const Body& b = e.body;
    if (auto* m = std::get_if<MFullState>(&b)) { if (!isHost()) onFullState(*m); }
    else if (auto* m = std::get_if<MPose>(&b)) doRelay = onPose(e, *m);
    else if (auto* m = std::get_if<MVitals>(&b)) doRelay = onVitals(e, *m);
    else if (auto* m = std::get_if<MActorSnap>(&b)) doRelay = onActorSnap(e, *m);
    else if (auto* m = std::get_if<MActorEvent>(&b)) doRelay = onActorEvent(e, *m);
    else if (auto* m = std::get_if<MHit>(&b)) onHit(e, *m);
    else if (auto* m = std::get_if<MOwn>(&b)) onOwn(e, *m);
    else if (auto* m = std::get_if<MEffects>(&b)) { if (!isHost()) onEffects(*m); }
    else if (auto* m = std::get_if<MDropReq>(&b)) onDropReq(e, *m);
    else if (auto* m = std::get_if<MItemCreated>(&b)) { if (!isHost()) onItemCreated(*m); }
    else if (auto* m = std::get_if<MPickup>(&b)) onPickup(e, *m);
    else if (auto* m = std::get_if<MItemTaken>(&b)) { if (!isHost()) onItemTaken(*m); }
    else if (auto* m = std::get_if<MContainerReq>(&b)) onContainerReq(e, *m);
    else if (auto* m = std::get_if<MContainerRes>(&b)) { if (!isHost()) onContainerRes(*m); }
    else if (auto* m = std::get_if<MContainerSync>(&b)) { if (!isHost()) onContainerSync(*m); }
    else if (auto* m = std::get_if<MSpawnRoll>(&b)) onSpawnRoll(e, *m);
    else if (auto* m = std::get_if<MSpawnActor>(&b)) { if (!isHost()) onSpawnActor(*m); }
    else if (auto* m = std::get_if<MSiteState>(&b)) { if (!isHost()) onSiteState(*m); }
    else if (auto* m = std::get_if<MActivate>(&b)) onActivate(e, *m);
    else if (auto* m = std::get_if<MDigest>(&b)) onDigest(e, *m);
    else if (auto* m = std::get_if<MRepair>(&b)) { if (!isHost()) onRepair(*m); }
    if (doRelay && isHost() && e.src != cfg_.self) relay(e);
    if (!ackAfterRelay_.empty()) { auto ids = std::move(ackAfterRelay_); ackAfterRelay_.clear(); for (NetId id : ids) maybeAck(id); }
    if (!diedAfterRelay_.empty()) { auto dead = std::move(diedAfterRelay_); diedAfterRelay_.clear(); for (NetId id : dead) onActorDiedHost(id); }
}

bool Machine::applyOnce(uint64_t key) {
    if (applied_.insert(key).second) return true;
    if (engineFaults().noDedupe) { ++stats_.doubleApplied; return true; }
    return false;
}

// ---- tick ------------------------------------------------------------------------------------

void Machine::tick(uint64_t nowMs) {
    const uint32_t dt = lastTick_ == ~0ull || nowMs < lastTick_ ? 0 : static_cast<uint32_t>(nowMs - lastTick_);
    now_ = nowMs;
    lastTick_ = nowMs;
    world_.nowMs = nowMs;

    for (auto& kv : links_) {
        int budget = 64;
        for (auto& u : kv.second.unacked) {
            if (budget == 0) break;
            if (now_ >= u.sentAt + cfg_.rtoMs) {
                outbox_.push_back({kv.first, Channel::Reliable, u.frame});
                u.sentAt = now_;
                ++stats_.linkRetransmits;
                --budget;
            }
        }
    }
    if (!synced_) return;

    if (isHost()) {
        std::vector<OwnMsg> out;
        svc_->tick(static_cast<uint32_t>(now_), out);
        emitOwn(out);
        if (now_ >= nextOwnEvalMs_) {
            nextOwnEvalMs_ = now_ + cfg_.ownEvalMs;
            evalOwnership();
            evalSites();
        }
    }

    // ownership drives AI: owner runs it, everybody else shows a remote-driven copy
    for (auto& kv : world_.actors()) {
        ActorState& a = kv.second;
        const bool mine = view_.ownerOf(a.id) == cfg_.self && !handingOff_.count(a.id) && !a.dead && a.enabled;
        a.aiOn = mine;
        if (mine) aiStep(a, dt);
        else if (engineFaults().ownerKeepsPublishing && lastOwnedEpoch_.count(a.id) && !a.dead && view_.ownerOf(a.id) != cfg_.self &&
                 now_ >= lastPublishMs_[a.id] + cfg_.snapshotMs) {
            lastPublishMs_[a.id] = now_;   // a former owner that never noticed it lost the actor keeps publishing at its old epoch
            MActorSnap s;
            s.id = a.id; s.epoch = lastOwnedEpoch_[a.id]; s.seq = a.snapSeq + 1000; s.cell = a.cell; s.pos = a.pos; s.yaw = a.yaw;
            s.pos.x += 40; s.health = a.health;
            sendBody(s, kNoPlayer);
        }
    }

    publishPose(false);

    if (now_ >= nextDigestMs_) {
        nextDigestMs_ = now_ + cfg_.digestMs;
        for (int c = 0; c < static_cast<int>(Category::Count_); ++c) {
            const Category cat = static_cast<Category>(c);
            if (!isHost() || obs_) {
                MDigest d;
                d.cat = static_cast<uint8_t>(c);
                auto keys = world_.keyHashes(cat);
                d.keys.assign(keys.begin(), keys.end());
                uint64_t h = 1469598103934665603ull;
                for (auto& kv : keys) { h = hash64(&kv.first, sizeof kv.first, h); h = hash64(&kv.second, sizeof kv.second, h); }
                d.total = h;
                if (obs_) obs_->onDigest(cfg_.self, now_, cat, d.total);
                if (!isHost()) sendBody(std::move(d), kHostPlayer);
            }
        }
    }
    flushAcks();
}

}  // namespace coop::fakeworld
