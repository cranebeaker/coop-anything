// Invariant oracles. They read only what an ITarget exposes: its apply log, ownership log and per-machine views.
#include <algorithm>
#include <cstdio>
#include <set>
#include <tuple>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {

const char* applyKindName(ApplyKind k) {
    static const char* n[] = {"pos", "playerpos", "hp", "owner", "holder", "door", "spawn", "remove", "reset", "intent"};
    return n[static_cast<int>(k)];
}

bool Thing::operator==(const Thing& o) const {
    return netId == o.netId && type == o.type && ident == o.ident && x == o.x && y == o.y && cell == o.cell && hp == o.hp &&
           dead == o.dead && owner == o.owner && epoch == o.epoch && holder == o.holder && open == o.open;
}

std::string Thing::str() const {
    char b[200];
    std::snprintf(b, sizeof b, "#%x t%d id%u pos(%d,%d)c%d hp%d%s own%d e%u hold%d%s", netId, type, ident, x, y, cell, hp, dead ? " dead" : "",
                  owner, epoch, holder, open ? " open" : "");
    return b;
}

namespace {
void add(std::vector<Violation>& out, const char* inv, const std::string& detail, uint64_t t) {
    // Keep the first few per invariant; a broken run produces thousands of the same thing.
    int same = 0;
    for (auto& v : out) if (v.invariant == inv) ++same;
    if (same >= 3) return;
    out.push_back({inv, detail, t});
}
std::string ev(const ApplyEvent& e) {
    char b[200];
    std::snprintf(b, sizeof b, "m%d t=%llu %s ref=#%x epoch=%u seq=%u origin=%d idem=%llx wepoch=%u", e.machine, static_cast<unsigned long long>(e.tMs),
                  applyKindName(e.kind), static_cast<unsigned>(e.ref), e.epoch, e.seq, e.origin, static_cast<unsigned long long>(e.idem), e.worldEpoch);
    return b;
}
}  // namespace

void checkLogs(const ITarget& t, std::vector<Violation>& out) {
    const auto& log = t.applyLog();
    const auto& own = t.ownLog();

    // 1. double_apply: the same (machine, message id) accepted more than once.
    {
        std::map<std::pair<int, uint64_t>, const ApplyEvent*> seen;
        for (auto& e : log) {
            if (!e.accepted || e.idem == 0) continue;
            auto key = std::make_pair(e.machine, e.idem);
            auto it = seen.find(key);
            if (it != seen.end()) add(out, "double_apply", "applied twice: " + ev(e), e.tMs);
            else seen[key] = &e;
        }
    }

    // 2. multi_owner: each actor has one owner per epoch, and epochs only grow.
    {
        std::map<int, std::pair<uint32_t, int>> last;           // ref -> (epoch, owner)
        for (auto& o : own) {
            if (o.kind == OwnEvent::WorldEpoch) { last.clear(); continue; }   // a host load replaces the ownership table
            auto it = last.find(o.ref);
            if (it != last.end()) {
                if (o.epoch < it->second.first)
                    add(out, "multi_owner", "ownership epoch went backwards for #" + std::to_string(o.ref) + ": " + std::to_string(it->second.first) + " -> " + std::to_string(o.epoch), o.tMs);
                else if (o.epoch == it->second.first && o.owner != it->second.second)
                    add(out, "multi_owner", "two owners in epoch " + std::to_string(o.epoch) + " for #" + std::to_string(o.ref) + ": m" +
                        std::to_string(it->second.second) + " and m" + std::to_string(o.owner), o.tMs);
            }
            last[o.ref] = {o.epoch, o.owner};
        }
    }

    // 3 and 4. stale_applied and echo_adopted. Merge both logs by order of occurrence.
    {
        struct Item { uint64_t ord; const ApplyEvent* a; const OwnEvent* o; };
        std::vector<Item> merged;
        merged.reserve(log.size() + own.size());
        for (auto& e : log) merged.push_back({e.ord, &e, nullptr});
        for (auto& o : own) merged.push_back({o.ord, nullptr, &o});
        std::stable_sort(merged.begin(), merged.end(), [](const Item& x, const Item& y) { return x.ord < y.ord; });
        std::map<int, int> ownerNow;                        // ref -> owner as the service has it
        std::map<int, uint32_t> epochNow;
        uint32_t worldEpoch = 0;
        std::map<std::pair<int, int>, uint32_t> maxEpoch;   // (machine, ref)
        std::map<std::tuple<int, int, uint32_t>, uint32_t> lastSeq;
        for (auto& it : merged) {
            if (it.o) {
                if (it.o->kind == OwnEvent::WorldEpoch) { worldEpoch = it.o->epoch; ownerNow.clear(); epochNow.clear(); maxEpoch.clear(); lastSeq.clear(); }
                else { ownerNow[it.o->ref] = it.o->owner; epochNow[it.o->ref] = it.o->epoch; }
                continue;
            }
            const ApplyEvent& e = *it.a;
            if (e.kind == ApplyKind::Reset) {   // a full state reset on one machine forgets what it knew
                for (auto i = maxEpoch.begin(); i != maxEpoch.end();) i = i->first.first == e.machine ? maxEpoch.erase(i) : std::next(i);
                for (auto i = lastSeq.begin(); i != lastSeq.end();) i = std::get<0>(i->first) == e.machine ? lastSeq.erase(i) : std::next(i);
                continue;
            }
            if (!e.accepted) continue;
            if (e.kind == ApplyKind::Pos || e.kind == ApplyKind::Owner) {
                auto& me = maxEpoch[{e.machine, e.ref}];
                if (e.epoch < me) add(out, "stale_applied", "older ownership epoch applied (have " + std::to_string(me) + "): " + ev(e), e.tMs);
                me = std::max(me, e.epoch);
            }
            if (e.kind == ApplyKind::Pos && e.seq) {
                auto& ls = lastSeq[{e.machine, e.ref, e.epoch}];
                if (e.seq <= ls) add(out, "stale_applied", "update sequence did not advance (last " + std::to_string(ls) + "): " + ev(e), e.tMs);
                ls = std::max(ls, e.seq);
            }
            if (e.machine == 0 && e.kind == ApplyKind::Pos) {   // the authority itself must only accept the current owner at the current epoch
                auto oi = ownerNow.find(e.ref);
                if (oi != ownerNow.end() && oi->second != e.origin)
                    add(out, "stale_applied", "update from m" + std::to_string(e.origin) + " accepted while m" + std::to_string(oi->second) + " owns it: " + ev(e), e.tMs);
                auto ei = epochNow.find(e.ref);
                if (ei != epochNow.end() && e.epoch != ei->second)
                    add(out, "stale_applied", "update at epoch " + std::to_string(e.epoch) + " accepted, current is " + std::to_string(ei->second) + ": " + ev(e), e.tMs);
            }
            if (e.kind == ApplyKind::Intent && e.worldEpoch != worldEpoch)
                add(out, "stale_applied", "request from world epoch " + std::to_string(e.worldEpoch) + " accepted in epoch " + std::to_string(worldEpoch) + ": " + ev(e), e.tMs);
            // a machine adopting the echo of its own request
            if (e.machine != 0 && (e.kind == ApplyKind::Pos || e.kind == ApplyKind::PlayerPos) && e.origin == e.machine)
                add(out, "echo_adopted", "machine applied the echo of its own update: " + ev(e), e.tMs);
        }
    }
}

void checkConverged(const ITarget& t, std::vector<Violation>& out) {
    int host = 0;
    std::vector<Thing> ref = t.view(host);
    for (int m = 0; m < t.machines(); ++m) {
        if (!t.connected(m)) continue;
        std::vector<Thing> v = t.view(m);
        // dup_thing: no duplicates inside one machine's world
        std::set<uint32_t> ids;
        std::map<uint32_t, uint32_t> idents;
        for (auto& th : v) {
            if (!ids.insert(th.netId).second) add(out, "dup_thing", "m" + std::to_string(m) + " has two things with net id " + th.str(), t.now());
            if (th.ident) {
                auto it = idents.find(th.ident);
                if (it != idents.end()) add(out, "dup_thing", "m" + std::to_string(m) + " has two copies of identity " + std::to_string(th.ident) + " (#" + std::to_string(it->second) + " and " + th.str() + ")", t.now());
                else idents[th.ident] = th.netId;
            }
        }
        if (m == host) continue;
        // converge: compare to the host, thing by thing, then the digests
        size_t i = 0, j = 0;
        std::string first;
        while ((i < ref.size() || j < v.size()) && first.empty()) {
            if (j >= v.size() || (i < ref.size() && ref[i].netId < v[j].netId)) { first = "host has " + ref[i].str() + ", m" + std::to_string(m) + " lacks it"; ++i; }
            else if (i >= ref.size() || v[j].netId < ref[i].netId) { first = "m" + std::to_string(m) + " has extra " + v[j].str(); ++j; }
            else { if (!(ref[i] == v[j])) first = "host " + ref[i].str() + " vs m" + std::to_string(m) + " " + v[j].str(); ++i; ++j; }
        }
        if (!first.empty()) add(out, "converge", first, t.now());
        for (int c = 0; c < kDigestCats; ++c)
            if (first.empty() && t.digest(m, c) != t.digest(host, c))
                add(out, "converge", "digest category " + std::to_string(c) + " differs on m" + std::to_string(m), t.now());
    }
}

}  // namespace coopfuzz
