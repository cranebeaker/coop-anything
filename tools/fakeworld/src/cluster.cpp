#include "fakeworld/cluster.h"
#include <algorithm>
#include <set>
#include "fakeworld/wire.h"

namespace coop::fakeworld {

namespace {
uint64_t machineSeed(uint64_t seed, PlayerId p, uint32_t connEpoch) { return seed * 1000003ull + p * 7919ull + connEpoch * 104729ull; }
const Ref kGroundItem("world.pack", 0x300001u);
}  // namespace

// ---- trace capture ---------------------------------------------------------------------------

void TraceCapture::onRecv(PlayerId self, uint64_t now, PeerId from, uint8_t channel, const Bytes& env) {
    TraceEvent e;
    e.tMs = now; e.kind = TraceEvent::Msg; e.machine = self; e.from = static_cast<PlayerId>(from); e.channel = channel; e.data = env;
    events.push_back(std::move(e));
}
void TraceCapture::onAction(PlayerId self, uint64_t now, const Action& i) {
    TraceEvent e;
    e.tMs = now; e.kind = TraceEvent::Action; e.machine = self;
    Archive a = Archive::writer();
    Action copy = i;
    io(a, copy);
    e.data = a.finish();
    events.push_back(std::move(e));
}

// ---- oracle observer -------------------------------------------------------------------------

class Cluster::Oracle : public IEngineObserver {
public:
    explicit Oracle(Cluster& c) : c_(c) {}
    IEngineObserver* next = nullptr;
    TraceCapture* capture = nullptr;

    void onSend(PlayerId s, uint64_t t, PeerId to, uint8_t ch, const Bytes& b) override { if (next) next->onSend(s, t, to, ch, b); }
    void onRecv(PlayerId s, uint64_t t, PeerId from, uint8_t ch, const Bytes& b) override {
        if (capture) capture->onRecv(s, t, from, ch, b);
        if (next) next->onRecv(s, t, from, ch, b);
    }
    void onDrop(PlayerId s, uint64_t t, PeerId from, const char* why) override { if (next) next->onDrop(s, t, from, why); }
    void onOwnership(PlayerId s, uint64_t t, const OwnMsg& m, ApplyResult r) override { if (next) next->onOwnership(s, t, m, r); }
    void onActorUpdate(PlayerId self, uint64_t t, NetId id, PlayerId from, OwnerEpoch epoch, UpdateVerdict v) override {
        if (v == UpdateVerdict::Accept && c_.connected(self)) {
            const OwnershipView& view = c_.machine(self).view();
            if (view.epochOf(id) != epoch || view.ownerOf(id) != from)
                c_.violations_.push_back({t, "stale update applied: machine " + std::to_string(self) + " accepted actor " + std::to_string(id) +
                                                " from p" + std::to_string(from) + " epoch " + std::to_string(epoch) + " (view: owner p" +
                                                std::to_string(view.ownerOf(id)) + " epoch " + std::to_string(view.epochOf(id)) + ")"});
        }
        if (next) next->onActorUpdate(self, t, id, from, epoch, v);
    }
    void onEffectApplied(PlayerId s, uint64_t t, uint64_t q, const Effect& e) override { if (next) next->onEffectApplied(s, t, q, e); }
    void onDigest(PlayerId s, uint64_t t, Category cat, uint64_t h) override { if (next) next->onDigest(s, t, cat, h); }
    void onAction(PlayerId s, uint64_t t, const Action& i) override {
        if (capture) capture->onAction(s, t, i);
        if (next) next->onAction(s, t, i);
    }
    void onRepair(PlayerId s, uint64_t t, PlayerId target, Category cat, size_t keys) override { if (next) next->onRepair(s, t, target, cat, keys); }

private:
    Cluster& c_;
};

// ---- cluster ---------------------------------------------------------------------------------

Cluster::Cluster(const ClusterConfig& cfg, TraceCapture* capture) : cfg_(cfg), net_(cfg.seed * 31 + 7), oracle_(std::make_unique<Oracle>(*this)) {
    oracle_->capture = capture;
    net_.setLink(cfg_.link);
    machines_.resize(static_cast<size_t>(std::max(1, std::min(cfg_.players, static_cast<int>(kMaxSlots)))));
    spawnMachine(0);
    for (size_t p = 1; p < machines_.size(); ++p) join(static_cast<PlayerId>(p));
    baseline_ = itemCensus();
}

Cluster::~Cluster() = default;

void Cluster::spawnMachine(PlayerId p) {
    MachineConfig mc = cfg_.machine;
    mc.self = p;
    mc.seed = machineSeed(cfg_.seed, p, connCounter_);
    mc.spec.seed = cfg_.machine.spec.seed;
    machines_[p] = std::make_unique<Machine>(mc);
    machines_[p]->setObserver(oracle_.get());
    net_.setConnected(p, true);
}

void Cluster::join(PlayerId p) {
    if (p == 0 || p >= machines_.size() || machines_[p]) return;
    const uint32_t ce = ++connCounter_;
    spawnMachine(p);
    // anything still queued for the previous incarnation is carried by the old connection epoch and dropped on arrival
    TraceEvent up;
    up.tMs = now_; up.kind = TraceEvent::PeerUp; up.machine = 0; up.from = p; up.connEpoch = ce;
    TraceEvent up2 = up;
    up2.machine = p; up2.from = 0;
    if (oracle_->capture) { oracle_->capture->note(up); oracle_->capture->note(up2); }
    machines_[0]->peerConnected(p, ce);
    machines_[p]->peerConnected(0, ce);
}

void Cluster::leave(PlayerId p) {
    if (p == 0 || p >= machines_.size() || !machines_[p]) return;
    net_.setConnected(p, false);   // drops its queued traffic
    machines_[p].reset();
    TraceEvent down;
    down.tMs = now_; down.kind = TraceEvent::PeerDown; down.machine = 0; down.from = p;
    if (oracle_->capture) oracle_->capture->note(down);
    machines_[0]->peerDisconnected(p);
}

size_t Cluster::saveHost() { saves_.push_back(machines_[0]->hostSave()); return saves_.size() - 1; }

bool Cluster::loadHost(size_t handle) {
    if (handle >= saves_.size()) return false;
    TraceEvent e;
    e.tMs = now_; e.kind = TraceEvent::HostLoad; e.machine = 0; e.data = saves_[handle];
    if (oracle_->capture) oracle_->capture->note(e);
    return machines_[0]->hostLoad(saves_[handle]);
}

void Cluster::setObserver(IEngineObserver* o) { user_ = o; oracle_->next = o; }
void Cluster::setCapture(TraceCapture* c) { oracle_->capture = c; }

void Cluster::inject(PlayerId to, PlayerId from, const Bytes& envelope, uint8_t channel) {
    if (connected(to)) machines_[to]->receiveEnvelope(from, envelope, channel);
}

void Cluster::step() {
    now_ += cfg_.tickMs;
    for (size_t p = 0; p < machines_.size(); ++p) {
        if (!machines_[p]) continue;
        if (oracle_->capture) {
            TraceEvent t;
            t.tMs = now_; t.kind = TraceEvent::Tick; t.machine = static_cast<PlayerId>(p);
            oracle_->capture->note(t);
        }
        machines_[p]->tick(now_);
    }
    pump();
    net_.advance(now_);
    deliverAll();
}

void Cluster::pump() {
    for (size_t p = 0; p < machines_.size(); ++p) {
        if (!machines_[p]) continue;
        for (Outgoing& o : machines_[p]->takeOutbox())
            net_.send(static_cast<PeerId>(p), o.to, std::move(o.data));
    }
}

void Cluster::deliverAll() {
    for (size_t p = 0; p < machines_.size(); ++p) {
        if (!machines_[p]) continue;
        PeerId from;
        Bytes data;
        while (machines_[p] && net_.poll(static_cast<PeerId>(p), from, data)) machines_[p]->receive(from, data);
    }
}

void Cluster::run(uint32_t ms) {
    const uint64_t end = now_ + ms;
    while (now_ < end) step();
}

bool Cluster::quiesce(uint32_t maxMs) {
    const uint64_t end = now_ + maxMs;
    const uint64_t need = static_cast<uint64_t>(cfg_.machine.digestMs) * 3 + 600;
    uint64_t lastHash = 0, stableSince = now_, nextSample = now_;
    for (auto& m : machines_) if (m) m->setAiFrozen(true);
    bool ok = false;
    while (now_ < end) {
        step();
        if (now_ < nextSample) continue;
        nextSample = now_ + 100;
        uint64_t h = 1469598103934665603ull;
        bool pending = false;
        for (auto& m : machines_) {
            if (!m) continue;
            for (int c = 0; c < static_cast<int>(Category::Count_); ++c) { uint64_t d = m->world().digest(static_cast<Category>(c)); h = hash64(&d, sizeof d, h); }
            uint64_t vd = m->view().digest(); h = hash64(&vd, sizeof vd, h);
            if (m->unackedCount() || m->outboxSize() || !m->synced()) pending = true;
            if (m->service()) for (auto& kv : m->world().actors()) if (m->service()->handoffPending(kv.first)) pending = true;
        }
        if (h != lastHash || pending) { stableSince = now_; lastHash = h; continue; }
        if (now_ - stableSince >= need) { ok = true; break; }
    }
    repairBaseline = totalRepairs();
    return ok;
}

void Cluster::freezeAi(bool f) { for (auto& m : machines_) if (m) m->setAiFrozen(f); }

uint64_t Cluster::totalRepairs() const {
    uint64_t n = 0;
    for (auto& m : machines_) if (m) n += m->healCount();
    return n;
}
uint64_t Cluster::totalDoubleApplied() const {
    uint64_t n = 0;
    for (auto& m : machines_) if (m) n += m->stats().doubleApplied;
    return n;
}

std::map<Ref, int64_t> Cluster::itemCensus() const {
    std::map<Ref, int64_t> out = machines_[0]->world().worldItemCensus();
    for (auto& m : machines_) {
        if (!m) continue;
        for (auto& kv : m->world().player(m->self()).inventory) out[kv.first] += kv.second;
    }
    return out;
}
int64_t Cluster::baselineItems(const Ref& base) const { auto it = baseline_.find(base); return it == baseline_.end() ? 0 : it->second; }

// ---- oracles ---------------------------------------------------------------------------------

std::vector<Violation> Cluster::checkAlways() {
    std::set<std::string> have;
    for (auto& v : violations_) have.insert(v.what);
    auto add = [&](const std::string& w) { if (have.insert(w).second) violations_.push_back({now_, w}); };

    if (totalDoubleApplied() > 0) add("an id was applied twice (doubleApplied=" + std::to_string(totalDoubleApplied()) + ")");

    std::map<std::pair<NetId, OwnerEpoch>, PlayerId> ownerAt;
    std::map<NetId, int> drivers;
    std::map<NetId, std::string> who;
    for (size_t p = 0; p < machines_.size(); ++p) {
        if (!machines_[p]) continue;
        const Machine& m = *machines_[p];
        if (m.sess() != machines_[0]->sess() || !m.synced()) continue;   // still on the old session (host load in flight)
        m.view().forEach([&](ActorNetId id, PlayerId owner, OwnerEpoch epoch) {
            if (owner == kNoPlayer) return;
            auto ins = ownerAt.emplace(std::make_pair(id, epoch), owner);
            if (!ins.second && ins.first->second != owner)
                add("actor " + std::to_string(id) + " has two owners at epoch " + std::to_string(epoch) + ": p" + std::to_string(ins.first->second) +
                    " and p" + std::to_string(owner));
        });
        if (m.synced()) for (auto& kv : m.world().actors()) if (kv.second.aiOn && !kv.second.dead && kv.second.enabled) { ++drivers[kv.first]; who[kv.first] += " m" + std::to_string(p) + "(sees owner p" + std::to_string(m.view().ownerOf(kv.first)) + " e" + std::to_string(m.view().epochOf(kv.first)) + (m.handingOff(kv.first) ? " handingOff" : "") + ")"; }
        for (auto& d : m.world().duplicates()) add("machine " + std::to_string(p) + ": duplicate " + d);
    }
    for (auto& kv : drivers) if (kv.second > 1) add("actor " + std::to_string(kv.first) + " is AI-driven on " + std::to_string(kv.second) + " machines:" + who[kv.first]);

    std::vector<Violation> fresh(violations_.begin() + static_cast<std::ptrdiff_t>(reported_), violations_.end());
    reported_ = violations_.size();
    return fresh;
}

std::vector<Violation> Cluster::checkQuiescent() {
    std::set<std::string> have;
    for (auto& v : violations_) have.insert(v.what);
    auto add = [&](const std::string& w) { if (have.insert(w).second) violations_.push_back({now_, w}); };
    const Machine& h = *machines_[0];
    for (size_t p = 1; p < machines_.size(); ++p) {
        if (!machines_[p]) continue;
        const Machine& m = *machines_[p];
        if (!m.synced()) { add("machine " + std::to_string(p) + " never synced"); continue; }
        for (int c = 0; c < static_cast<int>(Category::Count_); ++c) {
            const Category cat = static_cast<Category>(c);
            if (m.world().digest(cat) == h.world().digest(cat)) continue;
            std::string keys;
            auto hk = h.world().keyHashes(cat), mk = m.world().keyHashes(cat);
            int shown = 0;
            for (auto& kv : hk) { auto it = mk.find(kv.first); if ((it == mk.end() || it->second != kv.second) && shown++ < 5) keys += " " + std::to_string(kv.first); }
            for (auto& kv : mk) if (!hk.count(kv.first) && shown++ < 5) keys += " +" + std::to_string(kv.first);
            add(std::string("machine ") + std::to_string(p) + " differs from host in " + toString(cat) + ", keys:" + keys);
        }
        for (auto& kv : h.world().actors()) {
            const ActorState* o = m.world().actors().count(kv.first) ? &m.world().actors().at(kv.first) : nullptr;
            const ActorState& a = kv.second;
            if (!o) continue;   // reported by the digest
            if ((!a.dead && !(o->pos == a.pos)) || o->health != a.health || o->dead != a.dead || o->cell != a.cell || o->limbMask != a.limbMask)
                add("machine " + std::to_string(p) + " actor " + std::to_string(kv.first) + " state differs exactly from host");
        }
        if (m.view().digest() != h.view().digest()) add("machine " + std::to_string(p) + " ownership table differs from host");
    }
    const int64_t want = baselineItems(kGroundItem);
    const int64_t got = itemCensus()[kGroundItem];
    if (checkConservation && want != got) add("item conservation broken: expected " + std::to_string(want) + " of the ground item, found " + std::to_string(got));
    if (totalRepairs() > repairBaseline) add("repairs happened after quiescence (" + std::to_string(totalRepairs() - repairBaseline) + ")");
    std::vector<Violation> fresh(violations_.begin() + static_cast<std::ptrdiff_t>(reported_), violations_.end());
    reported_ = violations_.size();
    return fresh;
}

// ---- replay ----------------------------------------------------------------------------------

ReplayResult replay(const ClusterConfig& cfg, const std::vector<TraceEvent>& events) {
    ReplayResult r;
    std::vector<std::unique_ptr<Machine>> ms(kMaxSlots);
    auto make = [&](PlayerId p, uint32_t ce) {
        MachineConfig mc = cfg.machine;
        mc.self = p;
        mc.seed = machineSeed(cfg.seed, p, ce);
        ms[p] = std::make_unique<Machine>(mc);
    };
    make(0, 1);
    for (const TraceEvent& e : events) {
        ++r.events;
        switch (e.kind) {
            case TraceEvent::PeerUp:
                if (e.machine != 0 && !ms[e.machine]) make(e.machine, e.connEpoch);
                if (ms[e.machine]) ms[e.machine]->peerConnected(e.from, e.connEpoch);
                break;
            case TraceEvent::PeerDown:
                if (ms[e.machine]) ms[e.machine]->peerDisconnected(e.from);
                if (e.from != 0 && e.from < kMaxSlots) ms[e.from].reset();
                break;
            case TraceEvent::Tick: if (ms[e.machine]) ms[e.machine]->tick(e.tMs); break;
            case TraceEvent::Action:
                if (ms[e.machine]) {
                    Archive a = Archive::reader(e.data.data(), e.data.size());
                    Action i;
                    io(a, i);
                    if (a.ok()) ms[e.machine]->doAction(i);
                }
                break;
            case TraceEvent::Msg: if (ms[e.machine]) ms[e.machine]->receiveEnvelope(e.from, e.data, e.channel); break;
            case TraceEvent::HostLoad: if (ms[0]) ms[0]->hostLoad(e.data); break;
        }
        if (e.machine < kMaxSlots && ms[e.machine]) ms[e.machine]->takeOutbox();
        if (e.kind == TraceEvent::PeerDown && ms[0]) ms[0]->takeOutbox();
    }
    for (size_t p = 0; p < ms.size(); ++p)
        for (int c = 0; c < static_cast<int>(Category::Count_); ++c) r.digests.push_back(ms[p] ? ms[p]->world().digest(static_cast<Category>(c)) : 0);
    for (size_t p = 1; p < ms.size(); ++p) {
        if (!ms[p] || !ms[0]) continue;
        for (int c = 0; c < static_cast<int>(Category::Count_); ++c)
            if (ms[p]->world().digest(static_cast<Category>(c)) != ms[0]->world().digest(static_cast<Category>(c)))
                r.violations.push_back("machine " + std::to_string(p) + " differs from host in " + toString(static_cast<Category>(c)));
    }
    return r;
}

}  // namespace coop::fakeworld
