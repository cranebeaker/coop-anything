// Ownership scenarios without a network: the service and views driven by hand, with reordered, duplicated messages.
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include "scenarios.h"
#include "rng.h"

namespace coop::testing {

namespace {
void note(std::string* why, const std::string& s) { if (why && why->size() < 600) *why += s + "; "; }
Presence P(PlayerId p, bool inCell, bool adj, float d) {
    Presence x; x.player = p; x.inCell = inCell; x.adjacent = adj; x.distance = d; return x;
}
}  // namespace

bool churnHolds(uint64_t seed, int steps, std::string* why) {
    Rng r(seed * 977 + 13);
    constexpr int kPlayers = 3, kActors = 6;
    OwnershipService svc;
    OwnershipView views[kPlayers] = {OwnershipView(0), OwnershipView(1), OwnershipView(2)};
    bool connected[kPlayers] = {true, true, true};
    for (int p = 0; p < kPlayers; ++p) svc.playerJoined(static_cast<PlayerId>(p));
    struct Pending { int to; OwnMsg m; int at; };
    std::vector<Pending> q;
    struct Ack { OwnMsg m; PlayerId from; int at; };
    std::vector<Ack> acks;
    std::map<std::pair<ActorNetId, OwnerEpoch>, PlayerId> history;
    std::map<ActorNetId, OwnerEpoch> lastEpoch;
    bool ok = true;
    int t = 0;

    auto emit = [&](const OwnershipService::Out& out) {
        for (const OwnMsg& m : out) {
            views[0].apply(m);   // the host's own view is applied at once
            if (m.kind == OwnMsg::Grant || m.kind == OwnMsg::Commit || m.kind == OwnMsg::Release) {
                auto ins = history.emplace(std::make_pair(m.netId, m.epoch), m.kind == OwnMsg::Release ? kNoPlayer : m.owner);
                if (!ins.second && ins.first->second != (m.kind == OwnMsg::Release ? kNoPlayer : m.owner)) {
                    ok = false; note(why, "two owners at one epoch for actor " + std::to_string(m.netId));
                }
            }
            for (int p = 1; p < kPlayers; ++p) {
                q.push_back({p, m, t + static_cast<int>(r.range(0, 4))});
                if (r.chance(0.2)) q.push_back({p, m, t + static_cast<int>(r.range(0, 9))});   // duplicate
            }
            if (m.kind == OwnMsg::Transfer && m.owner == 0) acks.push_back({m, 0, t});        // the host acks itself at once
        }
    };

    for (ActorNetId id = 1; id <= kActors; ++id) {
        OwnershipService::Out out;
        svc.track(id, {P(0, r.chance(0.5), true, 100.0f + 50 * id), P(1, true, true, 90.0f + 10 * id), P(2, false, true, 500)}, 0, out);
        emit(out);
    }
    for (; t < steps; ++t) {
        const int now = t * 100;
        // players come and go (never the host)
        if (r.chance(0.04)) {
            const int p = static_cast<int>(r.range(1, 2));
            OwnershipService::Out out;
            if (connected[p]) {
                connected[p] = false;
                auto orphans = svc.playerLeft(static_cast<PlayerId>(p), now, out);
                emit(out);
                (void)orphans;
            } else {
                connected[p] = true;
                views[p].clear();
                svc.playerJoined(static_cast<PlayerId>(p));
                OwnershipService::Out snap;
                svc.snapshot(snap);
                for (auto& m : snap) q.push_back({p, m, t + 1});
            }
        }
        OwnershipService::Out out;
        svc.tick(now, out);
        for (ActorNetId id = 1; id <= kActors; ++id) {
            std::vector<Presence> who;
            for (int p = 0; p < kPlayers; ++p) {
                if (!connected[p]) continue;
                Presence x = P(static_cast<PlayerId>(p), r.chance(0.4), r.chance(0.7), static_cast<float>(r.range(10, 3000)));
                who.push_back(x);
            }
            if (r.chance(0.02)) svc.setRecruiter(id, static_cast<PlayerId>(r.range(0, 2)), who, now, out);
            else svc.evaluate(id, who, now, out);
        }
        emit(out);
        // deliver due messages in random order
        std::vector<Pending> due, keep;
        for (auto& p : q) (p.at <= t ? due : keep).push_back(p);
        q = keep;
        for (size_t i = due.size(); i > 1; --i) std::swap(due[i - 1], due[r.range(0, static_cast<uint32_t>(i - 1))]);
        for (auto& d : due) {
            if (!connected[d.to]) continue;
            views[d.to].apply(d.m);
            if (d.m.kind == OwnMsg::Transfer && d.m.owner == d.to && !r.chance(0.15))   // 15% of acks are lost: the timeout path
                acks.push_back({d.m, static_cast<PlayerId>(d.to), t + static_cast<int>(r.range(0, 3))});
        }
        std::vector<Ack> dueAcks, keepAcks;
        for (auto& a : acks) (a.at <= t ? dueAcks : keepAcks).push_back(a);
        acks = keepAcks;
        OwnershipService::Out aout;
        for (auto& a : dueAcks) {
            OwnMsg ack = a.m; ack.kind = OwnMsg::Ack; ack.owner = a.from;
            svc.onAck(ack, a.from, now, aout);
        }
        emit(aout);

        // invariants on the host
        for (ActorNetId id = 1; id <= kActors; ++id) {
            if (svc.epochOf(id) < lastEpoch[id]) { ok = false; note(why, "epoch went backwards for actor " + std::to_string(id)); }
            lastEpoch[id] = svc.epochOf(id);
            if (views[0].ownerOf(id) != svc.ownerOf(id) || views[0].epochOf(id) != svc.epochOf(id)) {
                ok = false; note(why, "host view differs from service for actor " + std::to_string(id));
            }
        }
        // random actor updates arrive at the host: only the current owner at the current epoch may be accepted
        for (int k = 0; k < 4; ++k) {
            const ActorNetId id = static_cast<ActorNetId>(r.range(1, kActors));
            const PlayerId from = static_cast<PlayerId>(r.range(0, 2));
            OwnerEpoch e = svc.epochOf(id);
            if (r.chance(0.5)) e = static_cast<OwnerEpoch>(r.range(0, e + 1));
            const UpdateVerdict v = views[0].check(id, from, e);
            if (v == UpdateVerdict::Accept && (from != svc.ownerOf(id) || e != svc.epochOf(id))) {
                ok = false; note(why, "accepted a stale or non-owner update for actor " + std::to_string(id));
            }
        }
        if (!ok) return false;
    }
    // drain: everything still queued arrives, then every connected view must equal the host's
    for (auto& d : q) if (connected[d.to]) views[d.to].apply(d.m);
    for (int p = 1; p < kPlayers; ++p) {
        if (!connected[p]) continue;
        // views of players that joined late got a snapshot, others got every message: all must agree with the host
        for (ActorNetId id = 1; id <= kActors; ++id) {
            // a handoff still pending at the end legitimately shows the old owner in a view that has not seen the commit
            if (svc.handoffPending(id)) continue;
            if (views[p].ownerOf(id) != svc.ownerOf(id) || views[p].epochOf(id) != svc.epochOf(id)) {
                ok = false; note(why, "view of p" + std::to_string(p) + " differs from host for actor " + std::to_string(id));
            }
        }
    }
    return ok;
}

bool staleEchoRejected(std::string* why) {
    bool ok = true;
    OwnershipView host(0), a(1), b(2);
    OwnMsg g; g.kind = OwnMsg::Grant; g.netId = 9; g.epoch = 1; g.owner = 1;
    OwnMsg t; t.kind = OwnMsg::Transfer; t.netId = 9; t.epoch = 2; t.owner = 2; t.prevOwner = 1;
    OwnMsg c; c.kind = OwnMsg::Commit; c.netId = 9; c.epoch = 2; c.owner = 2; c.prevOwner = 1;
    for (OwnershipView* v : {&host, &a, &b}) { v->apply(g); v->apply(t); v->apply(c); }
    // the old assignment is echoed back (a stale client re-sending what it believed)
    for (OwnershipView* v : {&host, &a, &b}) {
        v->apply(g);
        if (v->ownerOf(9) != 2 || v->epochOf(9) != 2) { ok = false; note(why, "an echoed old Grant overwrote the new owner"); }
    }
    // the previous owner keeps publishing at its old epoch
    if (host.check(9, 1, 1) == UpdateVerdict::Accept) { ok = false; note(why, "update from the previous owner at the old epoch was accepted"); }
    if (host.check(9, 1, 2) == UpdateVerdict::Accept) { ok = false; note(why, "update from the previous owner at the new epoch was accepted"); }
    if (host.check(9, 2, 2) != UpdateVerdict::Accept) { ok = false; note(why, "the real owner was rejected"); }
    // the new owner sees its own publish reflected back
    if (b.check(9, 2, 2) == UpdateVerdict::Accept) { ok = false; note(why, "own echo accepted"); }
    return ok;
}

bool rejoinEpochsMonotonic(std::string* why) {
    bool ok = true;
    OwnershipService s; s.playerJoined(0); s.playerJoined(1);
    OwnerEpoch last = 0;
    std::vector<OwnMsg> seen;
    for (int round = 0; round < 4; ++round) {
        OwnershipService::Out out;
        s.playerJoined(1);
        s.track(5, {P(1, true, true, 10)}, round * 1000u, out);
        s.evaluate(5, {P(1, true, true, 10)}, round * 1000u + 1, out);
        s.playerLeft(1, round * 1000u + 500, out);
        for (auto& m : out) {
            if (m.epoch <= last) { ok = false; note(why, "epoch did not rise across leave/rejoin"); }
            last = m.epoch;
            seen.push_back(m);
        }
    }
    OwnershipView v(0);
    for (auto& m : seen) v.apply(m);
    for (auto& m : seen) if (v.apply(m) == ApplyResult::Applied) { ok = false; note(why, "a replayed message from an earlier incarnation applied"); }
    return ok;
}

// Players 1 and 2 stand about equally near an actor and swap which one is slightly closer every 300 ms. The host is
// far away. With hysteresis the first owner keeps the actor; without it every swap is a handoff.
bool noFlap(std::string* why) {
    bool ok = true;
    OwnershipService s; s.playerJoined(0); s.playerJoined(1); s.playerJoined(2);
    OwnershipService::Out out;
    s.track(7, {P(0, false, false, 9000), P(1, true, true, 500), P(2, true, true, 545)}, 0, out);
    int transfers = 0;
    for (int i = 1; i < 28; ++i) {
        const bool flip = (i & 1) != 0;
        const uint32_t now = static_cast<uint32_t>(i) * 300u;
        out.clear();
        s.evaluate(7, {P(0, false, false, 9000), P(1, true, true, flip ? 545.0f : 500.0f), P(2, true, true, flip ? 500.0f : 545.0f)}, now, out);
        for (auto& m : out) {
            if (m.kind != OwnMsg::Transfer) continue;
            ++transfers;
            OwnMsg ack = m; ack.kind = OwnMsg::Ack;
            OwnershipService::Out o2;
            s.onAck(ack, m.owner, now, o2);
        }
    }
    if (transfers > 2) { ok = false; note(why, "ownership flapped: " + std::to_string(transfers) + " transfers in 8 s"); }
    return ok;
}

// The host owns a companion. Player 1 recruits it and walks far away; the host stays next to it.
bool followerStaysWithRecruiter(std::string* why) {
    bool ok = true;
    OwnershipService s; s.playerJoined(0); s.playerJoined(1);
    OwnershipService::Out out;
    const std::vector<Presence> far1 = {P(0, true, true, 10), P(1, false, false, 9000)};
    s.track(7, far1, 0, out);
    if (s.ownerOf(7) != 0) { ok = false; note(why, "the host should own it first"); }
    out.clear();
    s.setRecruiter(7, 1, far1, 5, out);
    if (out.size() != 1 || out[0].kind != OwnMsg::Transfer || out[0].owner != 1) {
        ok = false; note(why, "recruiting did not start a handoff to the recruiter");
    } else {
        OwnMsg ack = out[0]; ack.kind = OwnMsg::Ack;
        out.clear();
        s.onAck(ack, 1, 10, out);
    }
    out.clear();
    for (uint32_t t = 10000; t < 60000; t += 5000) s.evaluate(7, far1, t, out);
    if (s.ownerOf(7) != 1) { ok = false; note(why, "the companion left its recruiter"); }
    // the recruiter disconnects: the companion becomes an ordinary actor again
    out.clear();
    s.playerLeft(1, 70000, out);
    out.clear();
    s.evaluate(7, {P(0, true, true, 10)}, 70001, out);
    if (s.ownerOf(7) != 0) { ok = false; note(why, "the companion did not return to normal rules"); }
    return ok;
}

// While a handoff is pending the old owner is still the authority. Only the Ack, then the Commit, moves the actor.
bool handoffKeepsLastState(std::string* why) {
    bool ok = true;
    OwnershipService s; s.playerJoined(0); s.playerJoined(1);
    OwnershipView host(0), oldOwner(1);
    OwnershipService::Out out;
    s.track(7, {P(1, true, true, 50)}, 0, out);
    for (auto& m : out) { host.apply(m); oldOwner.apply(m); }
    out.clear();
    s.evaluate(7, {P(0, true, true, 10), P(1, true, true, 50)}, 2000, out);   // the host walks up: it takes over
    for (auto& m : out) { host.apply(m); oldOwner.apply(m); }
    bool sawCommit = false;
    for (auto& m : out) sawCommit = sawCommit || m.kind == OwnMsg::Commit;
    if (sawCommit) { ok = false; note(why, "the handoff committed before the new owner acknowledged"); }
    if (s.ownerOf(7) != 1) { ok = false; note(why, "the service moved the owner before the Ack"); }
    if (host.check(7, 1, 1) != UpdateVerdict::Accept) { ok = false; note(why, "the old owner's last state was refused during the handoff"); }
    OwnerEpoch pe = 0;
    if (!host.handoffIncoming(7, pe) || pe != 2) { ok = false; note(why, "the new owner does not see the handoff as incoming"); }
    OwnMsg ack; ack.kind = OwnMsg::Ack; ack.netId = 7; ack.epoch = 2; ack.owner = 0;
    out.clear();
    if (!s.onAck(ack, 0, 2100, out)) { ok = false; note(why, "a valid Ack was refused"); }
    for (auto& m : out) { host.apply(m); oldOwner.apply(m); }
    if (host.ownerOf(7) != 0 || oldOwner.ownerOf(7) != 0) { ok = false; note(why, "the Commit did not move the owner"); }
    if (oldOwner.check(7, 1, 1) == UpdateVerdict::Accept) { ok = false; note(why, "the old owner is still accepted after the Commit"); }
    OwnerEpoch e = 0;
    if (oldOwner.mayPublish(7, e)) { ok = false; note(why, "the old owner may still publish after the Commit"); }
    return ok;
}

}  // namespace coop::testing
