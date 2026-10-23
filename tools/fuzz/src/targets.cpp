// Target registry and the shared logging helpers of TargetBase.
#include <map>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {

namespace {
std::map<std::string, TargetInfo>& registry() {
    static std::map<std::string, TargetInfo> r;   // function-local so registration order between files never matters
    return r;
}
}  // namespace

void registerTarget(TargetInfo info) {
    std::string name = info.name;
    registry()[name] = std::move(info);
}

const TargetInfo* findTarget(const std::string& name) {
    auto& r = registry();
    if (r.empty()) return nullptr;
    if (name.empty()) return &r.begin()->second;
    auto it = r.find(name);
    return it == r.end() ? nullptr : &it->second;
}

std::vector<const TargetInfo*> allTargets() {
    std::vector<const TargetInfo*> v;
    for (auto& kv : registry()) v.push_back(&kv.second);
    return v;
}

std::unique_ptr<ITarget> makeTarget(const std::string& name, const Scenario& s, std::string* err) {
    const TargetInfo* t = findTarget(name);
    if (!t) {
        if (err) *err = "target '" + name + "' is not built into this executable";
        return nullptr;
    }
    return t->make(s, err);
}

std::vector<std::string> knownBugs(const std::string& target) {
    const TargetInfo* t = findTarget(target);
    return t ? t->bugs : std::vector<std::string>();
}

void TargetBase::logApply(int machine, uint64_t idem, int ref, uint32_t epoch, uint32_t seq, int origin, ApplyKind k, bool accepted, uint32_t worldEpoch,
                          const std::string& why) {
    ApplyEvent e;
    e.machine = machine; e.tMs = now(); e.ord = ++ord_; e.idem = idem; e.ref = ref; e.epoch = epoch; e.seq = seq; e.origin = origin; e.kind = k;
    e.accepted = accepted; e.worldEpoch = worldEpoch; e.why = why;
    apply_.push_back(std::move(e));
    if (!accepted) ++counters_["reject_" + why];
}

void TargetBase::logOwn(OwnEvent::Kind k, int ref, int owner, int prev, uint32_t epoch) {
    OwnEvent o;
    o.tMs = now(); o.ord = ++ord_; o.kind = k; o.ref = ref; o.owner = owner; o.prevOwner = prev; o.epoch = epoch;
    own_.push_back(o);
}

}  // namespace coopfuzz
