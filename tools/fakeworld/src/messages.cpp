#include "fakeworld/messages.h"
#include "fakeworld/wire.h"

namespace coop::fakeworld {

namespace {
constexpr size_t kMaxList = 4096;
constexpr size_t kMaxBlob = 1u << 22;

void ioLoot(Archive& a, LootList& l) {
    size_t n = l.size();
    if (!a.count(n, 64)) return;
    if (a.reading()) l.assign(n, {});
    for (auto& e : l) { io(a, e.first); a.vari32(e.second); }
}
}  // namespace

void io(Archive& a, OwnMsg& m) {
    uint8_t k = static_cast<uint8_t>(m.kind);
    a.u8(k);
    if (a.reading() && (k < OwnMsg::Grant || k > OwnMsg::Release)) { a.fail(); return; }
    m.kind = static_cast<OwnMsg::Kind>(k);
    a.varu32(m.netId);
    a.varu32(m.epoch);
    a.u8(m.owner);
    a.u8(m.prevOwner);
}

void io(Archive& a, MFullState& m) {
    a.varu32(m.sess); a.varu(m.hostNowMs); a.varu(m.fxSeq); a.bytes(m.world, kMaxBlob);
    size_t n = m.own.size();
    if (!a.count(n, 1u << 20)) return;
    if (a.reading()) m.own.assign(n, {});
    for (auto& o : m.own) io(a, o);
}
void io(Archive& a, MPose& m) { a.u8(m.slot); a.boolean(m.present); a.varu32(m.cell); io(a, m.pos); a.f32(m.yaw); a.varu32(m.seq); a.boolean(m.settle); }
void io(Archive& a, MVitals& m) { a.u8(m.slot); a.vari32(m.health); a.boolean(m.dead); a.varu32(m.seq); }
void io(Archive& a, MActorSnap& m) {
    a.varu32(m.id); a.varu32(m.epoch); a.varu32(m.seq); a.varu32(m.cell); io(a, m.pos); a.f32(m.yaw); a.vari32(m.health);
    a.boolean(m.dead); a.boolean(m.settle); a.boolean(m.final);
}
void io(Archive& a, MActorEvent& m) {
    uint8_t k = static_cast<uint8_t>(m.kind);
    a.varu32(m.id); a.varu32(m.epoch); a.varu(m.eventId); a.u8(k); m.kind = static_cast<ActorEventKind>(k & 1);
    a.vari32(m.health); a.u8(m.limbMask); a.boolean(m.dead); ioLoot(a, m.loot);
    a.varu32(m.seq); a.varu32(m.cell); io(a, m.pos); a.f32(m.yaw);
}
void io(Archive& a, MHit& m) {
    uint8_t l = static_cast<uint8_t>(m.limb);
    a.varu(m.hitId); a.boolean(m.attackerIsPlayer); a.varu32(m.attacker); a.boolean(m.victimIsPlayer); a.varu32(m.victim);
    a.vari32(m.damage); a.u8(l); m.limb = static_cast<Limb>(l % static_cast<uint8_t>(Limb::Count_)); io(a, m.impact); a.f32(m.weaponRange); a.u8(m.hops);
}
void io(Archive& a, MOwn& m) { io(a, m.m); }
void io(Archive& a, MEffects& m) {
    a.varu(m.seq);
    size_t n = m.fx.size();
    if (!a.count(n, kMaxEffectsPerBatch)) return;
    if (a.reading()) m.fx.assign(n, Effect{});
    for (auto& e : m.fx) io(a, e);
}
void io(Archive& a, MDropReq& m) { a.varu32(m.reqId); io(a, m.base); a.varu32(m.cell); io(a, m.pos); }
void io(Archive& a, MItemCreated& m) { a.varu32(m.id); io(a, m.base); a.varu32(m.cell); io(a, m.pos); a.u64(m.origin); }
void io(Archive& a, MPickup& m) { a.u64(m.claimId); a.varu32(m.item); }
void io(Archive& a, MItemTaken& m) { a.varu32(m.item); a.u8(m.by); a.u64(m.claimId); }
void io(Archive& a, MContainerReq& m) {
    uint8_t k = static_cast<uint8_t>(m.op);
    a.u64(m.opId); a.varu32(m.cont); a.u8(k); m.op = static_cast<ContainerOpKind>(k & 3); io(a, m.base); a.vari32(m.count); a.u64(m.closeHash);
}
void io(Archive& a, MContainerRes& m) {
    uint8_t k = static_cast<uint8_t>(m.op);
    a.u64(m.opId); a.varu32(m.cont); a.u8(k); m.op = static_cast<ContainerOpKind>(k & 3); a.boolean(m.ok); io(a, m.base); a.vari32(m.count);
}
void io(Archive& a, MContainerSync& m) { a.varu32(m.cont); io(a, m.base); a.vari32(m.newCount); }
void io(Archive& a, MSpawnRoll& m) { a.varu32(m.site); }
void io(Archive& a, MSpawnActor& m) { a.varu32(m.id); a.varu32(m.site); io(a, m.base); io(a, m.pick); a.varu32(m.cell); io(a, m.pos); }
void io(Archive& a, MSiteState& m) { a.varu32(m.site); a.varu32(m.spawned); a.boolean(m.cleared); a.varu(m.clearedAtMs); a.varu32(m.spawnCount); }
void io(Archive& a, MActivate& m) { a.varu32(m.obj); }
void io(Archive& a, MDigest& m) {
    a.u8(m.cat); a.u64(m.total);
    size_t n = m.keys.size();
    if (!a.count(n, 1u << 20)) return;
    if (a.reading()) m.keys.assign(n, {});
    for (auto& k : m.keys) { a.varu32(k.first); a.u64(k.second); }
}
void io(Archive& a, MRepair& m) {
    a.u8(m.cat);
    size_t n = m.states.size();
    if (a.count(n, kMaxList)) {
        if (a.reading()) m.states.assign(n, {});
        for (auto& s : m.states) { a.varu32(s.first); a.bytes(s.second, 1u << 16); }
    }
    size_t r = m.removes.size();
    if (a.count(r, kMaxList)) {
        if (a.reading()) m.removes.assign(r, 0);
        for (auto& k : m.removes) a.varu32(k);
    }
}

void io(Archive& a, Action& v) {
    uint8_t k = static_cast<uint8_t>(v.kind), l = static_cast<uint8_t>(v.limb);
    a.u8(k);
    if (a.reading() && k >= static_cast<uint8_t>(ActionKind::Count_)) { a.fail(); return; }
    v.kind = static_cast<ActionKind>(k);
    a.varu32(v.a); a.vari32(v.n); io(a, v.pos); a.f32(v.f); io(a, v.base);
    a.u8(l); v.limb = static_cast<Limb>(l % static_cast<uint8_t>(Limb::Count_));
    a.boolean(v.flag);
}

const char* toString(ActionKind k) {
    static const char* names[] = {"Move", "Attack", "Pickup", "Drop", "ContainerOpen", "ContainerClose", "ContainerTake",
                                  "ContainerPut", "Activate", "UseDoor", "Recruit", "Dismiss", "HostSetStage", "HostSetLock",
                                  "HostKill", "Revive"};
    return static_cast<size_t>(k) < sizeof names / sizeof *names ? names[static_cast<size_t>(k)] : "?";
}

const char* bodyName(const Body& b) {
    static const char* names[] = {"FullState", "Pose", "Vitals", "ActorSnap", "ActorEvent", "Hit", "Own", "Effects", "DropReq",
                                  "ItemCreated", "Pickup", "ItemTaken", "ContainerReq", "ContainerRes", "ContainerSync",
                                  "SpawnRoll", "SpawnActor", "SiteState", "Activate", "Digest", "Repair"};
    return names[b.index()];
}

bool reliableByDefault(const Body& b) {
    if (auto* p = std::get_if<MPose>(&b)) return p->settle;
    if (std::holds_alternative<MDigest>(b)) return false;
    if (auto* s = std::get_if<MActorSnap>(&b)) return s->settle || s->final;
    return true;
}

namespace {
template <size_t I = 0>
bool readBody(Archive& a, size_t idx, Body& out) {
    if constexpr (I == std::variant_size_v<Body>) {
        (void)a; (void)idx; (void)out;
        return false;
    } else {
        if (idx == I) {
            std::variant_alternative_t<I, Body> v{};
            io(a, v);
            if (!a.ok()) return false;
            out = std::move(v);
            return true;
        }
        return readBody<I + 1>(a, idx, out);
    }
}
}  // namespace

Bytes encode(const Envelope& e) {
    Archive a = Archive::writer();
    uint8_t ver = kEngineWireVersion, src = e.src, dest = e.dest, idx = static_cast<uint8_t>(e.body.index());
    uint32_t sess = e.sess;
    a.u8(ver); a.varu32(sess); a.u8(src); a.u8(dest); a.u8(idx);
    Body copy = e.body;
    std::visit([&](auto& m) { io(a, m); }, copy);
    return a.finish();
}

bool decode(const Bytes& data, Envelope& out) {
    Archive a = Archive::reader(data.data(), data.size());
    uint8_t ver = 0, idx = 0;
    a.u8(ver);
    if (!a.ok() || ver != kEngineWireVersion) return false;
    a.varu32(out.sess); a.u8(out.src); a.u8(out.dest); a.u8(idx);
    if (!a.ok() || idx >= std::variant_size_v<Body>) return false;
    if (!readBody(a, idx, out.body)) return false;
    return a.ok() && a.atEnd();
}

}  // namespace coop::fakeworld
