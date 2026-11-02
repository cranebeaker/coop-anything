// Scenario runner, delta-debugging shrinker and the JSON failure report.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {

RunResult run(const Scenario& s, const RunOptions& opt) {
    RunResult r;
    std::string err;
    auto t = makeTarget(!opt.target.empty() ? opt.target : s.target, s, &err);
    if (!t) {
        r.ok = false;
        r.violations.push_back({"setup", err, 0});
        return r;
    }
    for (const Op& op : s.ops) {
        t->advance(op.t);
        t->apply(op);
        ++r.opsRun;
    }
    t->advance(std::max<uint64_t>(s.durationMs, t->now()));
    t->heal();

    // Wait for quiescence; one more second must stay quiet too (ownership re-evaluation and heartbeats can still fire right after the heal).
    uint64_t start = t->now();
    bool quiet = false;
    while (t->now() - start < opt.quiesceCapMs) {
        while (!t->quiescent() && t->now() - start < opt.quiesceCapMs) t->advance(t->now() + 100);
        if (!t->quiescent()) break;
        t->advance(t->now() + 1000);
        if (t->quiescent()) { quiet = true; break; }
    }
    if (!quiet) r.violations.push_back({"not_quiescent", "traffic still moving " + std::to_string(opt.quiesceCapMs) + " ms after the link was healed", t->now()});
    if (quiet) t->markQuiescent();
    checkConverged(*t, r.violations);

    uint64_t repairsBefore = 0;
    for (int m = 0; m < t->machines(); ++m) repairsBefore += t->repairs(m);
    t->advance(t->now() + opt.afterQuiesceMs);
    uint64_t repairsAfter = 0;
    for (int m = 0; m < t->machines(); ++m) repairsAfter += t->repairs(m);
    if (quiet && repairsAfter > repairsBefore)
        r.violations.push_back({"repairs_after_quiescence", std::to_string(repairsAfter - repairsBefore) + " repairs in the " + std::to_string(opt.afterQuiesceMs) +
                                " ms after quiescence (must be 0)", t->now()});
    checkLogs(*t, r.violations);
    for (auto& v : t->nativeViolations()) r.violations.push_back(v);

    r.ok = r.violations.empty();
    r.stats = t->stats();
    r.virtualMs = t->now();
    if (!r.ok && !opt.reportDir.empty()) writeReport(*t, s, r, opt.reportDir, opt.reportName, opt.reportExtra);
    return r;
}

static bool hasInvariant(const RunResult& r, const std::string& inv) {
    for (auto& v : r.violations) if (v.invariant == inv) return true;
    return false;
}

Scenario shrink(const Scenario& s0, const RunOptions& opt0, const std::string& invariant, int* runsOut) {
    RunOptions opt = opt0;
    opt.reportDir.clear();
    int runs = 0;
    const int kMaxRuns = 4000;
    auto fails = [&](const Scenario& c) {
        if (runs >= kMaxRuns) return false;
        ++runs;
        return hasInvariant(run(c, opt), invariant);
    };
    Scenario best = s0;
    // ddmin over ops
    std::vector<Op> ops = best.ops;
    size_t n = 2;
    while (ops.size() >= 2 && runs < kMaxRuns) {
        size_t chunk = (ops.size() + n - 1) / n;
        bool reduced = false;
        for (size_t start = 0; start < ops.size(); start += chunk) {
            std::vector<Op> comp;
            comp.reserve(ops.size());
            for (size_t i = 0; i < ops.size(); ++i) if (i < start || i >= start + chunk) comp.push_back(ops[i]);
            Scenario c = best; c.ops = comp;
            if (fails(c)) { ops = comp; n = std::max<size_t>(n - 1, 2); reduced = true; break; }
        }
        if (!reduced) {
            if (n >= ops.size()) break;
            n = std::min(n * 2, ops.size());
        }
    }
    // a lone op may also be removable (needs the empty list to fail on its own)
    if (ops.size() == 1) { Scenario c = best; c.ops.clear(); if (fails(c)) ops.clear(); }
    best.ops = ops;
    // simplify the environment
    auto tryScenario = [&](Scenario c) { if (fails(c)) { best = c; return true; } return false; };
    { Scenario c = best; c.link = LinkParams(); tryScenario(c); }
    { Scenario c = best; c.link.corrupt = 0; tryScenario(c); }
    { Scenario c = best; c.link.duplicate = 0; tryScenario(c); }
    { Scenario c = best; c.link.loss = 0; tryScenario(c); }
    { Scenario c = best; c.link.maxLatencyMs = c.link.minLatencyMs; tryScenario(c); }
    for (int m = best.machines; m > 2; --m) {
        bool used = false;
        for (auto& o : best.ops) if (o.m >= m - 1 || (o.kind == OpKind::Stale && o.a >= m - 1)) used = true;
        if (used) break;
        Scenario c = best; c.machines = m - 1;
        if (!tryScenario(c)) break;
    }
    { Scenario c = best; c.durationMs = best.ops.empty() ? 1 : best.ops.back().t + 1; tryScenario(c); }
    if (runsOut) *runsOut = runs;
    return best;
}

// ------------------------------------------------------------------ report
bool writeReport(const ITarget& t, const Scenario& s, const RunResult& r, const std::string& dir, const std::string& name, const std::string& extra) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::string base = dir + "/" + name;
    { std::ofstream f(base + ".scenario.json", std::ios::binary); f << toJson(s); if (!f) return false; }

    std::ofstream f(base + ".report.json", std::ios::binary);
    if (!f) return false;
    f << "{\n \"format\": \"coopfuzz-report-1\",\n \"seed\": " << s.seed << ",\n \"machines\": " << t.machines() << ",\n \"target\": \"" << jsonEscape(s.target)
      << "\",\n \"profile\": \"" << jsonEscape(s.profile) << "\",\n \"mutations\": [";
    for (size_t i = 0; i < s.bugs.size(); ++i) f << (i ? ", " : "") << "\"" << jsonEscape(s.bugs[i]) << "\"";
    f << "],\n \"ops\": " << s.ops.size() << ",\n \"virtualMs\": " << r.virtualMs << ",\n";
    if (!extra.empty()) f << " " << extra << ",\n";
    f << " \"replay\": \"coopfuzz --replay " << jsonEscape(name) << ".scenario.json\",\n \"violations\": [";
    for (size_t i = 0; i < r.violations.size(); ++i)
        f << (i ? ", " : "") << "{\"invariant\": \"" << jsonEscape(r.violations[i].invariant) << "\", \"detail\": \"" << jsonEscape(r.violations[i].detail)
          << "\", \"tMs\": " << r.violations[i].tMs << "}";
    f << "],\n \"stats\": {";
    bool first = true;
    for (auto& kv : r.stats) { f << (first ? "" : ", ") << "\"" << jsonEscape(kv.first) << "\": " << kv.second; first = false; }
    f << "},\n";
    // The two logs the oracles read. Capped so a runaway failure does not write hundreds of megabytes.
    const size_t kCap = 5000;
    f << " \"ownership_log\": [";
    first = true;
    size_t n = 0;
    for (auto& o : t.ownLog()) {
        if (++n > kCap) break;
        f << (first ? "\n  " : ",\n  ") << "{\"t\": " << o.tMs << ", \"ord\": " << o.ord << ", \"kind\": " << int(o.kind) << ", \"ref\": " << o.ref << ", \"owner\": " << o.owner
          << ", \"prev\": " << o.prevOwner << ", \"epoch\": " << o.epoch << "}";
        first = false;
    }
    f << "\n ],\n \"apply_log\": [";
    first = true;
    n = 0;
    for (auto& e : t.applyLog()) {
        if (++n > kCap) break;
        f << (first ? "\n  " : ",\n  ") << "{\"t\": " << e.tMs << ", \"ord\": " << e.ord << ", \"m\": " << e.machine << ", \"kind\": \"" << applyKindName(e.kind) << "\", \"ref\": " << e.ref
          << ", \"epoch\": " << e.epoch << ", \"seq\": " << e.seq << ", \"origin\": " << e.origin << ", \"idem\": " << e.idem << ", \"accepted\": " << (e.accepted ? "true" : "false")
          << ", \"why\": \"" << jsonEscape(e.why) << "\"}";
        first = false;
    }
    f << "\n ],\n \"logs_capped_at\": " << kCap << "\n}\n";
    return static_cast<bool>(f);
}

}  // namespace coopfuzz
