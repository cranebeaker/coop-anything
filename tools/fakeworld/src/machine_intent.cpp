// Machine: local actions. Every action is an Action so a recorded stream can be replayed exactly.
#include <algorithm>
#include "fakeworld/machine.h"

namespace coop::fakeworld {

bool Machine::doAction(const Action& i) {
    if (!synced_) return false;
    if (obs_) obs_->onAction(cfg_.self, now_, i);
    PlayerState& me = world_.player(cfg_.self);
    switch (i.kind) {
        case ActionKind::Move: {
            if (me.dead || !world_.cellKnown(i.a)) return false;
            const bool cellChanged = me.cell != i.a;
            me.cell = i.a; me.pos = i.pos; me.yaw = i.f;
            poseDirty_ = true;
            if (cellChanged) loadCellsFor(i.a);
            return true;
        }
        case ActionKind::Attack: {
            ActorState* v = world_.findActor(i.a);
            if (!v || v->dead || me.dead) return false;
            MHit h;
            h.hitId = nextId(); h.attackerIsPlayer = true; h.attacker = cfg_.self; h.victimIsPlayer = false; h.victim = v->id;
            h.damage = i.n; h.limb = i.limb; h.impact = i.pos; h.weaponRange = i.f;
            PlayerId owner = view_.ownerOf(v->id);
            if (owner == kNoPlayer) owner = kHostPlayer;
            sendHit(h, owner);
            return true;
        }
        case ActionKind::Pickup: {
            auto it = world_.items().find(i.a);
            if (it == world_.items().end() || it->second.taken || me.dead) return false;
            ItemState& item = it->second;
            const uint64_t claim = nextId();
            item.taken = true; item.takenBy = cfg_.self;
            me.inventory[item.base] += 1;
            if (isHost()) {
                ++stats_.pickupsWon;
                sendBody(MItemTaken{item.id, cfg_.self, claim}, kNoPlayer);
            } else {
                pendingClaims_[claim] = item.id;
                sendBody(MPickup{claim, item.id}, kHostPlayer);
            }
            return true;
        }
        case ActionKind::Drop: {
            auto it = me.inventory.find(i.base);
            if (it == me.inventory.end() || it->second <= 0 || me.dead) return false;
            if (--it->second <= 0) me.inventory.erase(it);
            MDropReq r;
            r.reqId = ++dropCounter_ + connEpoch_ * 100000u; r.base = i.base; r.cell = me.cell; r.pos = me.pos;
            if (isHost()) {
                Envelope e;
                e.sess = sess_; e.src = cfg_.self; e.dest = kHostPlayer; e.body = r;
                onDropReq(e, r);
            } else {
                sendBody(r, kHostPlayer);
            }
            return true;
        }
        case ActionKind::ContainerOpen:
        case ActionKind::ContainerClose:
        case ActionKind::ContainerTake:
        case ActionKind::ContainerPut: {
            auto it = world_.containers().find(i.a);
            if (it == world_.containers().end() || me.dead) return false;
            MContainerReq r;
            r.opId = nextId(); r.cont = i.a; r.base = i.base; r.count = i.n;
            r.op = i.kind == ActionKind::ContainerOpen ? ContainerOpKind::Open
                 : i.kind == ActionKind::ContainerClose ? ContainerOpKind::Close
                 : i.kind == ActionKind::ContainerTake ? ContainerOpKind::Take : ContainerOpKind::Put;
            if (r.op == ContainerOpKind::Close) r.closeHash = world_.keyHash(Category::Containers, i.a);
            if (r.op == ContainerOpKind::Put) {
                auto inv = me.inventory.find(i.base);
                if (inv == me.inventory.end() || i.n <= 0 || inv->second < i.n) return false;
                if ((inv->second -= i.n) <= 0) me.inventory.erase(inv);
                pendingPuts_[r.opId] = {i.base, i.n};
            }
            if (isHost()) {
                Envelope e;
                e.sess = sess_; e.src = cfg_.self; e.dest = kHostPlayer; e.body = r;
                onContainerReq(e, r);
            } else {
                sendBody(r, kHostPlayer);
            }
            return true;
        }
        case ActionKind::Activate:
        case ActionKind::UseDoor: {
            auto it = world_.objects().find(i.a);
            if (it == world_.objects().end() || me.dead) return false;
            const WorldObject o = it->second;
            if (o.locked || o.destroyed || !o.enabled) return false;
            if (isHost()) {
                Envelope e;
                e.sess = sess_; e.src = cfg_.self; e.dest = kHostPlayer;
                MActivate a{o.id};
                e.body = a;
                onActivate(e, a);
            } else {
                sendBody(MActivate{o.id}, kHostPlayer);
            }
            if (i.kind == ActionKind::UseDoor && o.targetCell && world_.cellKnown(o.targetCell)) {
                const bool cellChanged = me.cell != o.targetCell;
                me.cell = o.targetCell; me.pos = o.targetPos;
                poseDirty_ = true;
                if (cellChanged) loadCellsFor(o.targetCell);
            }
            return true;
        }
        case ActionKind::Recruit: {
            const ActorState* a = world_.findActor(i.a);
            if (!isHost() || !a) return false;
            CompanionOwner c;
            c.npc = a->ref; c.owner = static_cast<uint8_t>(i.n); c.active = true;
            return commitEffects({Effect{c}});
        }
        case ActionKind::Dismiss: {
            const ActorState* a = world_.findActor(i.a);
            if (!isHost() || !a) return false;
            CompanionOwner c;
            c.npc = a->ref; c.owner = a->recruiter == kNoPlayer ? 0 : a->recruiter; c.active = false;
            return commitEffects({Effect{c}});
        }
        case ActionKind::HostSetStage: {
            SetStage s;
            s.quest = i.base; s.stage = static_cast<uint32_t>(i.n);
            return commitEffects({Effect{s}});
        }
        case ActionKind::HostSetLock: {
            auto it = world_.objects().find(i.a);
            if (!isHost() || it == world_.objects().end()) return false;
            SetLock l;
            l.ref = placedRef(i.a); l.locked = i.flag; l.level = i.n;
            return commitEffects({Effect{l}});
        }
        case ActionKind::HostKill: {
            const ActorState* a = world_.findActor(i.a);
            if (!isHost() || !a || a->dead) return false;
            KillActor k;
            k.actor = a->ref;
            return commitEffects({Effect{k}});
        }
        case ActionKind::Revive: {
            if (!me.dead) return false;
            me.dead = false; me.health = me.maxHealth;
            MVitals v;
            v.slot = cfg_.self; v.health = me.health; v.dead = false; v.seq = seqBase() + ++vitalsCounter_;
            me.vitalsSeq = v.seq;
            sendBody(v, kNoPlayer);
            return true;
        }
        default: return false;
    }
}

bool Machine::move(CellId cell, Vec3 pos, float yaw) {
    Action i; i.kind = ActionKind::Move; i.a = cell; i.pos = pos; i.f = yaw;
    return doAction(i);
}
bool Machine::attackActor(NetId victim, int32_t damage, Limb limb, Vec3 impact, float weaponRange) {
    Action i; i.kind = ActionKind::Attack; i.a = victim; i.n = damage; i.limb = limb; i.pos = impact; i.f = weaponRange;
    return doAction(i);
}
bool Machine::pickup(NetId item) { Action i; i.kind = ActionKind::Pickup; i.a = item; return doAction(i); }
bool Machine::drop(const Ref& base) { Action i; i.kind = ActionKind::Drop; i.base = base; return doAction(i); }
bool Machine::openContainer(NetId c) { Action i; i.kind = ActionKind::ContainerOpen; i.a = c; return doAction(i); }
bool Machine::closeContainer(NetId c) { Action i; i.kind = ActionKind::ContainerClose; i.a = c; return doAction(i); }
bool Machine::containerTake(NetId c, const Ref& base, int32_t n) {
    Action i; i.kind = ActionKind::ContainerTake; i.a = c; i.base = base; i.n = n; return doAction(i);
}
bool Machine::containerPut(NetId c, const Ref& base, int32_t n) {
    Action i; i.kind = ActionKind::ContainerPut; i.a = c; i.base = base; i.n = n; return doAction(i);
}
bool Machine::activate(NetId obj) { Action i; i.kind = ActionKind::Activate; i.a = obj; return doAction(i); }
bool Machine::useDoor(NetId obj) { Action i; i.kind = ActionKind::UseDoor; i.a = obj; return doAction(i); }
bool Machine::recruit(NetId actor, PlayerId owner) { Action i; i.kind = ActionKind::Recruit; i.a = actor; i.n = owner; return doAction(i); }
bool Machine::dismiss(NetId actor) { Action i; i.kind = ActionKind::Dismiss; i.a = actor; return doAction(i); }
bool Machine::setStage(const Ref& quest, uint32_t stage) {
    Action i; i.kind = ActionKind::HostSetStage; i.base = quest; i.n = static_cast<int32_t>(stage);
    return isHost() && doAction(i);
}
bool Machine::setLock(NetId obj, bool locked, int32_t level) {
    Action i; i.kind = ActionKind::HostSetLock; i.a = obj; i.flag = locked; i.n = level; return doAction(i);
}
bool Machine::killByStory(NetId actor) { Action i; i.kind = ActionKind::HostKill; i.a = actor; return doAction(i); }

// Cell load by proximity: the engine would roll spawn sites of newly loaded cells locally. Here the roll is switched
// off and reported to the host, which arbitrates one winner per site.
void Machine::loadCellsFor(CellId cell) {
    loaded_ = world_.loadedAround(cell);
    for (auto it = rolledSites_.begin(); it != rolledSites_.end();) {
        auto s = world_.sites().find(*it);
        const bool stillLoaded = s != world_.sites().end() && std::find(loaded_.begin(), loaded_.end(), s->second.cell) != loaded_.end();
        it = stillLoaded ? std::next(it) : rolledSites_.erase(it);
    }
    for (auto& kv : world_.sites()) {
        const SpawnSiteState& s = kv.second;
        if (std::find(loaded_.begin(), loaded_.end(), s.cell) == loaded_.end()) continue;
        if (!rolledSites_.insert(s.id).second) continue;
        if (isHost()) {
            Envelope e;
            e.sess = sess_; e.src = cfg_.self; e.dest = kHostPlayer;
            MSpawnRoll r{s.id};
            e.body = r;
            onSpawnRoll(e, r);
        } else {
            sendBody(MSpawnRoll{s.id}, kHostPlayer);
        }
    }
}

}  // namespace coop::fakeworld
