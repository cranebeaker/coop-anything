// Machine: NPC AI for owned actors, snapshots, combat (hits, damage, death), poses, vitals.
#include <algorithm>
#include <cmath>
#include "fakeworld/machine.h"

namespace coop::fakeworld {

namespace {
constexpr uint64_t kTagHit = 1ull << 56, kTagEvent = 2ull << 56;
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

// ---- pose / vitals ---------------------------------------------------------------------------

void Machine::refreshAi(NetId id) {
    ActorState* a = world_.findActor(id);
    if (a) a->aiOn = view_.ownerOf(id) == cfg_.self && !handingOff_.count(id) && !a->dead && a->enabled;
}

void Machine::refreshAiFlags() {
    for (auto& kv : world_.actors()) refreshAi(kv.first);
}

void Machine::publishPose(bool force) {
    if (!synced_) return;
    bool settle = false;
    if (!force && !poseDirty_) {   // stopped: one reliable copy of the final pose, so a lost datagram cannot leave others behind
        if (!poseUnsettled_ || now_ < lastPoseChangeMs_ + cfg_.settleMs) return;
        settle = true;
    } else if (!force && now_ < nextPoseMs_) {
        return;
    }
    nextPoseMs_ = now_ + cfg_.poseMs;
    if (!settle) { poseDirty_ = false; poseUnsettled_ = true; lastPoseChangeMs_ = now_; }
    else poseUnsettled_ = false;
    PlayerState& p = world_.player(cfg_.self);
    MPose m;
    m.settle = settle;
    m.slot = cfg_.self; m.present = true; m.cell = p.cell; m.pos = p.pos; m.yaw = p.yaw;
    m.seq = seqBase() + ++poseCounter_;
    p.poseSeq = m.seq;
    sendBody(m, kNoPlayer);
}

bool Machine::onPose(const Envelope& e, const MPose& m) {
    if (m.slot == cfg_.self || m.slot >= kMaxSlots) return false;
    if (isHost() && (e.src != m.slot || !links_.count(m.slot))) return false;
    PlayerState& p = world_.player(m.slot);
    if (m.seq <= p.poseSeq) return false;
    p.slot = m.slot; p.present = m.present; p.poseSeq = m.seq;
    if (m.present) { p.cell = m.cell; p.pos = m.pos; p.yaw = m.yaw; }
    return true;
}

bool Machine::onVitals(const Envelope& e, const MVitals& m) {
    if (m.slot == cfg_.self || m.slot >= kMaxSlots) return false;
    if (isHost() && e.src != m.slot) return false;
    PlayerState& p = world_.player(m.slot);
    if (m.seq <= p.vitalsSeq) return false;
    p.vitalsSeq = m.seq; p.health = m.health; p.dead = m.dead;
    return true;
}

// ---- owner side: AI and snapshots ------------------------------------------------------------

void Machine::aiStep(ActorState& a, uint32_t dtMs) {
    lastOwnedEpoch_[a.id] = view_.epochOf(a.id);
    const float dts = static_cast<float>(dtMs) / 1000.0f;
    bool moved = false;
    auto cellIt = world_.cells().find(a.cell);
    auto clampToCell = [&](Vec3& p) {
        if (cellIt == world_.cells().end() || cellIt->second.interior) return;
        const float x0 = static_cast<float>(cellIt->second.gx) * kCellSize, y0 = static_cast<float>(cellIt->second.gy) * kCellSize;
        p.x = clampf(p.x, x0 + 50.0f, x0 + kCellSize - 50.0f);
        p.y = clampf(p.y, y0 + 50.0f, y0 + kCellSize - 50.0f);
    };
    auto moveToward = [&](const Vec3& target, float speed, float stopAt) {
        const float d = distance2d(a.pos, target);
        if (d <= stopAt || d < 1e-3f) return;
        const float step = std::min(d - stopAt, speed * dts);
        a.pos.x += (target.x - a.pos.x) / d * step;
        a.pos.y += (target.y - a.pos.y) / d * step;
        a.yaw = std::atan2(target.y - a.pos.y, target.x - a.pos.x);
        clampToCell(a.pos);
        moved = moved || step > 0;
    };

    if (aiFrozen_) {
        // no decisions
    } else if (a.recruiter != kNoPlayer && a.recruiter < kMaxSlots && world_.player(a.recruiter).present) {
        // follower: goes wherever the recruiter goes
        const PlayerState& r = world_.player(a.recruiter);
        if (r.cell != a.cell) { a.cell = r.cell; a.pos = r.pos; a.pos.x += 80; moved = true; cellIt = world_.cells().find(a.cell); }
        else moveToward(r.pos, cfg_.npcSpeed, 250.0f);
    } else if (a.hostile) {
        const PlayerState* tgt = nullptr;
        float best = cfg_.aggroRange;
        for (uint8_t s = 0; s < kMaxSlots; ++s) {
            const PlayerState& p = world_.player(s);
            if (!p.present || p.dead || p.cell != a.cell) continue;
            const float d = distance2d(p.pos, a.pos);
            if (d < best) { best = d; tgt = &p; }
        }
        if (tgt) {
            if (best > cfg_.meleeRange) moveToward(tgt->pos, cfg_.npcSpeed, cfg_.meleeRange * 0.8f);
            else if (now_ >= lastAttackMs_[a.id] + cfg_.npcAttackMs) {
                lastAttackMs_[a.id] = now_;
                MHit h;
                h.hitId = nextId(); h.attackerIsPlayer = false; h.attacker = a.id; h.victimIsPlayer = true; h.victim = tgt->slot;
                h.damage = cfg_.npcDamage; h.impact = tgt->pos; h.weaponRange = cfg_.meleeRange;
                sendHit(h, tgt->slot);
            }
        }
    }
    if (!moved && !aiFrozen_) {
        const bool engaged = a.hostile && lastAttackMs_.count(a.id) && now_ < lastAttackMs_[a.id] + 500;
        Wander& w = wander_[a.id];
        if (now_ >= w.nextMs) {
            w.nextMs = now_ + 1000 + rng_.range(0, 2000);
            if (rng_.chance(0.5)) { w.dx = w.dy = 0; }
            else { const float ang = static_cast<float>(rng_.uni()) * 6.2831853f; w.dx = std::cos(ang); w.dy = std::sin(ang); }
        }
        if ((w.dx != 0 || w.dy != 0) && dtMs > 0 && !engaged) {
            a.pos.x += w.dx * cfg_.wanderSpeed * dts;
            a.pos.y += w.dy * cfg_.wanderSpeed * dts;
            a.yaw = std::atan2(w.dy, w.dx);
            clampToCell(a.pos);
            moved = true;
        }
    }

    if (moved) {
        lastMoveMs_[a.id] = now_;
        bool near = false;
        for (uint8_t s = 0; s < kMaxSlots && !near; ++s) near = world_.player(s).present && world_.player(s).cell == a.cell;
        const uint32_t interval = near ? cfg_.snapshotMs : cfg_.farSnapshotMs;
        if (now_ >= lastPublishMs_[a.id] + interval) publishActor(a, false, false);
    } else if (lastMoveMs_.count(a.id) && now_ >= lastMoveMs_[a.id] + cfg_.settleMs && settledAt_[a.id] <= lastMoveMs_[a.id]) {
        settledAt_[a.id] = now_;
        publishActor(a, true, false);
    }
}

void Machine::publishActor(ActorState& a, bool settle, bool final) {
    OwnerEpoch epoch = 0;
    if (!view_.mayPublish(a.id, epoch)) return;
    lastPublishMs_[a.id] = now_;
    MActorSnap s;
    s.id = a.id; s.epoch = epoch; s.seq = ++a.snapSeq; s.cell = a.cell; s.pos = a.pos; s.yaw = a.yaw;
    s.health = a.health; s.dead = a.dead; s.settle = settle; s.final = final;
    sendBody(s, kNoPlayer);
}

bool Machine::onActorSnap(const Envelope& e, const MActorSnap& m) {
    ActorState* a = world_.findActor(m.id);
    if (!a) { ++stats_.snapshotsDropped; return false; }
    const UpdateVerdict v = view_.check(m.id, e.src, m.epoch);
    if (obs_) obs_->onActorUpdate(cfg_.self, now_, m.id, e.src, m.epoch, v);
    if (v != UpdateVerdict::Accept) {
        ++stats_.snapshotsDropped;
        if (rejectLog_.size() < 64) rejectLog_.push_back(std::string("snap ") + std::to_string(m.id) + " from " + std::to_string(e.src) + ": " + toString(v));
        if (v == UpdateVerdict::FutureEpoch && isHost()) {   // the sender is ahead of us or we are behind: tell it the truth
            OwnMsg cur;
            if (svc_->current(m.id, cur)) { ++stats_.resyncs; sendBody(MOwn{cur}, e.src); }
        }
        return false;
    }
    if (!m.final && m.seq <= a->snapSeq) { ++stats_.snapshotsDropped; return false; }
    a->cell = m.cell; a->pos = m.pos; a->yaw = m.yaw; a->health = m.health;
    a->snapSeq = std::max(a->snapSeq, m.seq);
    ++stats_.snapshotsApplied;
    if (m.final) {
        ++stats_.finalSnaps;
        if (isHost()) ackAfterRelay_.push_back(m.id);   // the host acks (and may commit) only after relaying this state onward
        else maybeAck(m.id);
    }
    return true;
}

// ---- loot and death --------------------------------------------------------------------------

LootList Machine::rollLoot(Rng& r) const {
    LootList l;
    const int n = static_cast<int>(r.range(0, 2));
    for (int i = 0; i < n; ++i) {
        Ref f("world.pack", 0x200001u + r.range(0, 2));
        const int32_t c = static_cast<int32_t>(r.range(1, 3));
        bool merged = false;
        for (auto& e : l) if (e.first == f) { e.second += c; merged = true; }
        if (!merged) l.push_back({f, c});
    }
    return l;
}

void Machine::makeCorpse(ActorState& a, const LootList& loot) {
    ContainerState& c = world_.containers()[a.id];
    const PlayerId holder = c.inUseBy;
    c = ContainerState{};
    c.id = a.id; c.cell = a.cell; c.pos = a.pos; c.corpse = true; c.inUseBy = holder;
    for (auto& e : loot) c.items[e.first] += e.second;
}

void Machine::applyDamage(ActorState& a, int32_t damage, Limb limb, bool) {
    a.health -= damage;
    if (limb != Limb::Torso && damage >= 25) a.limbMask = static_cast<uint8_t>(a.limbMask | (1u << static_cast<unsigned>(limb)));
    bool kill = a.health <= 0;
    if (kill && a.named && !engineFaults().namedActorKillable) { a.health = 1; kill = false; }   // essential: only the story may kill
    OwnerEpoch epoch = 0;
    view_.mayPublish(a.id, epoch);
    MActorEvent ev;
    ev.id = a.id; ev.epoch = epoch; ev.eventId = nextId(); ev.health = a.health; ev.limbMask = a.limbMask;
    ev.seq = ++a.snapSeq; ev.cell = a.cell; ev.pos = a.pos; ev.yaw = a.yaw;
    if (kill) {
        a.health = 0; a.dead = true;
        ev.kind = ActorEventKind::Death; ev.dead = true; ev.health = 0;
        ev.loot = rollLoot(rng_);
        makeCorpse(a, ev.loot);
    }
    applied_.insert(kTagEvent | ev.eventId);
    sendBody(ev, kNoPlayer);
    if (kill && isHost()) onActorDiedHost(a.id);
}

bool Machine::onActorEvent(const Envelope& e, const MActorEvent& m) {
    ActorState* a = world_.findActor(m.id);
    if (!a) return false;
    const UpdateVerdict v = view_.check(m.id, e.src, m.epoch);
    if (obs_) obs_->onActorUpdate(cfg_.self, now_, m.id, e.src, m.epoch, v);
    if (v != UpdateVerdict::Accept) {
        if (rejectLog_.size() < 64) rejectLog_.push_back(std::string("event ") + std::to_string(m.id) + " from " + std::to_string(e.src) + ": " + toString(v));
        if (v == UpdateVerdict::FutureEpoch && isHost()) {
            OwnMsg cur;
            if (svc_->current(m.id, cur)) { ++stats_.resyncs; sendBody(MOwn{cur}, e.src); }
        }
        return false;
    }
    if (!applyOnce(kTagEvent | m.eventId)) { ++stats_.eventsDuplicate; return false; }
    a->health = m.health; a->limbMask = m.limbMask;
    if (m.seq > a->snapSeq) { a->snapSeq = m.seq; a->cell = m.cell; a->pos = m.pos; a->yaw = m.yaw; }
    ++stats_.eventsApplied;
    if (m.dead) {
        a->dead = true; a->health = 0;
        LootList loot = m.loot;
        if (engineFaults().lootPerMachine) loot = rollLoot(rng_);
        makeCorpse(*a, loot);
        if (isHost()) diedAfterRelay_.push_back(a->id);   // untrack only after the event itself went out (ordering: event, then Release)
    }
    return true;
}

// ---- hits ------------------------------------------------------------------------------------

void Machine::sendHit(const MHit& h, PlayerId dest) {
    if (dest == cfg_.self) {
        Envelope e;
        e.sess = sess_; e.src = cfg_.self; e.dest = cfg_.self; e.body = h;
        onHit(e, h);
        return;
    }
    sendBody(h, dest);
}

bool Machine::validateHit(const MHit& h, const ActorState& victim, PlayerId from, std::string& why) {
    if (engineFaults().hitNoValidation) return true;
    if (victim.dead) { why = "victim already dead"; return false; }
    if (h.damage <= 0 || h.damage > cfg_.maxDamage) { why = "damage out of bounds"; return false; }
    if (h.weaponRange > cfg_.maxWeaponRange) { why = "weapon range out of bounds"; return false; }
    if (!h.attackerIsPlayer) { why = "npc attacker against an npc"; return false; }
    if (h.attacker != from || h.attacker >= kMaxSlots) { why = "attacker is not the sender"; return false; }
    const PlayerState& p = world_.player(static_cast<PlayerId>(h.attacker));
    if (!p.present || p.dead) { why = "attacker absent or dead"; return false; }
    if (!world_.adjacent(p.cell, victim.cell)) { why = "attacker in another cell"; return false; }
    if (distance(p.pos, victim.pos) > h.weaponRange + cfg_.rangeSlack) { why = "out of weapon range"; return false; }
    if (!(h.impact == Vec3{}) && distance(h.impact, victim.pos) > cfg_.impactSlack) { why = "impact point not near victim"; return false; }
    auto& q = hitTimes_[static_cast<PlayerId>(h.attacker)];
    while (!q.empty() && q.front() + 1000 <= now_) q.pop_front();
    if (q.size() >= cfg_.maxHitsPerSec) { why = "hit rate"; return false; }
    q.push_back(now_);
    return true;
}

void Machine::onHit(const Envelope& e, MHit h) {
    if (isHost() && e.dest != cfg_.self) {
        // routing: the host knows the current owner, so a hit aimed at a stale owner is redirected
        if (h.victimIsPlayer) { relay(e); ++stats_.hitsForwarded; return; }
        const PlayerId owner = view_.ownerOf(h.victim);
        if (owner != kNoPlayer && owner != cfg_.self) {
            Envelope f = e;
            f.dest = owner;
            relay(f);
            ++stats_.hitsForwarded;
            return;
        }
    }
    if (!applyOnce(kTagHit | h.hitId)) { ++stats_.hitDuplicates; return; }
    if (h.victimIsPlayer) { applyHitToSelf(e, h); return; }
    ActorState* a = world_.findActor(h.victim);
    if (!a) return;
    const PlayerId owner = view_.ownerOf(h.victim);
    if (owner != cfg_.self && !engineFaults().hitAppliedByNonOwner) {
        ++stats_.hitsRejected;
        if (rejectLog_.size() < 64) rejectLog_.push_back("hit on " + std::to_string(h.victim) + ": not the owner");
        return;
    }
    if (handingOff_.count(h.victim)) {   // already giving this actor away: its new owner decides, not us
        ++stats_.hitsRejected;
        if (rejectLog_.size() < 64) rejectLog_.push_back("hit on " + std::to_string(h.victim) + ": handoff in progress");
        return;
    }
    std::string why;
    if (!validateHit(h, *a, e.src, why)) {
        ++stats_.hitsRejected;
        if (rejectLog_.size() < 64) rejectLog_.push_back("hit on " + std::to_string(h.victim) + ": " + why);
        return;
    }
    ++stats_.hitsApplied;
    applyDamage(*a, h.damage, h.limb, true);
}

void Machine::applyHitToSelf(const Envelope& e, const MHit& h) {
    PlayerState& me = world_.player(cfg_.self);
    if (h.victim != cfg_.self || me.dead) return;
    if (!engineFaults().hitNoValidation) {
        if (h.damage <= 0 || h.damage > cfg_.maxDamage) { ++stats_.hitsRejected; return; }
        if (!h.attackerIsPlayer) {
            const ActorState* npc = world_.findActor(h.attacker);
            if (!npc || npc->dead || view_.ownerOf(npc->id) != e.src) { ++stats_.hitsRejected; return; }   // only the npc's owner may report it
            if (npc->cell != me.cell || distance(npc->pos, me.pos) > cfg_.meleeRange + cfg_.rangeSlack + 400.0f) { ++stats_.hitsRejected; return; }
        }
    }
    me.health -= h.damage;
    if (me.health <= 0) { me.health = 0; me.dead = true; }
    ++stats_.hitsApplied;
    MVitals v;
    v.slot = cfg_.self; v.health = me.health; v.dead = me.dead; v.seq = seqBase() + ++vitalsCounter_;
    me.vitalsSeq = v.seq;
    sendBody(v, kNoPlayer);
}

}  // namespace coop::fakeworld
