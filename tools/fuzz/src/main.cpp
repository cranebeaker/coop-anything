// coopfuzz: property-based co-op fuzzer CLI. Normally driven by tools/fuzz/run.py.
//   --minutes M | --count K     how long / how many seeds (default 200 seeds)
//   --seed S                    first seed
//   --players P                 machines per session (2..4); 0 = vary per seed
//   --duration MS               chaos phase length per session (default 15000)
//   --target NAME               which registered target (default: the first one)
//   --bug NAME                  inject a bug class (repeatable); a run with a bug is expected to fail
//   --replay FILE               run one scenario file; exit 1 and print VIOLATION lines when an invariant breaks
//   --profile NAME              link preset: lan good_wifi bad_wifi mobile hell
//   --out DIR                   where failures (shrunk scenario + report) are written (default out/coopfuzz)
//   --fixture BUG               find a failing session for BUG, shrink it and write <out>/regress_BUG.json (a saved regression)
//   --no-shrink, --max-fail N, --list-bugs, --list-targets, --quiet
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "coopfuzz/fuzz.h"

using namespace coopfuzz;

static std::string readFile(const std::string& p, bool* ok) {
    std::ifstream f(p, std::ios::binary);
    *ok = static_cast<bool>(f);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// The most specific broken invariant: a convergence failure is usually only the symptom of one of the others.
static std::string mainInvariant(const RunResult& r) {
    static const char* order[] = {"setup", "double_apply", "multi_owner", "stale_applied", "echo_adopted", "dup_thing", "repairs_after_quiescence", "not_quiescent", "converge"};
    for (const char* o : order)
        for (auto& v : r.violations) if (v.invariant == o) return o;
    return r.violations[0].invariant;
}

static std::string detailOf(const RunResult& r, const std::string& inv) {
    for (auto& v : r.violations) if (v.invariant == inv) return v.detail;
    return r.violations[0].detail;
}

int main(int argc, char** argv) {
    double minutes = 0;
    int count = 200, players = 0, maxFail = 3;
    uint64_t seed = 1;
    uint32_t duration = 15000;
    std::string out = "out/coopfuzz", replay, fixture, profile, targetName;
    std::vector<std::string> bugs;
    bool shrinkOn = true, quiet = false, countGiven = false, listBugs = false;
    for (int i = 1; i < argc; ++i) {
        auto a = [&](const char* n) { return !std::strcmp(argv[i], n) && i + 1 < argc; };
        if (a("--minutes")) minutes = std::atof(argv[++i]);
        else if (a("--count")) { count = std::atoi(argv[++i]); countGiven = true; }
        else if (a("--seed")) seed = std::strtoull(argv[++i], nullptr, 10);
        else if (a("--players")) players = std::atoi(argv[++i]);
        else if (a("--duration")) duration = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (a("--target")) targetName = argv[++i];
        else if (a("--bug")) bugs.push_back(argv[++i]);
        else if (a("--replay")) replay = argv[++i];
        else if (a("--fixture")) fixture = argv[++i];
        else if (a("--profile")) profile = argv[++i];
        else if (a("--out")) out = argv[++i];
        else if (a("--max-fail")) maxFail = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--no-shrink")) shrinkOn = false;
        else if (!std::strcmp(argv[i], "--quiet")) quiet = true;
        else if (!std::strcmp(argv[i], "--list-bugs")) listBugs = true;
        else if (!std::strcmp(argv[i], "--list-targets")) {
            for (auto* t : allTargets()) std::printf("%s\t%s\n", t->name.c_str(), t->description.c_str());
            return 0;
        } else { std::fprintf(stderr, "unknown or incomplete argument: %s\n", argv[i]); return 2; }
    }
    if (listBugs) {
        for (auto& b : knownBugs(targetName)) std::printf("%s\n", b.c_str());
        return 0;
    }
    if (!findTarget(targetName)) { std::fprintf(stderr, "no such target: '%s' (use --list-targets)\n", targetName.c_str()); return 2; }
    if (!profile.empty()) {
        LinkParams tmp;
        if (!linkProfile(profile, tmp)) { std::fprintf(stderr, "unknown --profile %s\n", profile.c_str()); return 2; }
    }
    const std::string resolvedTarget = findTarget(targetName)->name;
    RunOptions opt;
    opt.target = targetName;

    if (!fixture.empty()) {
        for (uint64_t sd = 1; sd <= 600; ++sd) {
            Scenario s = generate(sd, 2 + static_cast<int>(sd % 3), 8000);
            s.bugs = {fixture};
            s.target = resolvedTarget;
            RunResult r = run(s, opt);
            if (r.ok) continue;
            std::string inv = mainInvariant(r);
            if (inv == "setup") { std::fprintf(stderr, "%s\n", r.violations[0].detail.c_str()); return 2; }
            Scenario small = shrink(s, opt, inv);
            small.expect = inv;
            std::error_code ec;
            std::filesystem::create_directories(out, ec);
            std::string path = out + "/regress_" + fixture + ".json";
            std::ofstream f(path, std::ios::binary);
            f << toJson(small);
            std::printf("fixture %s: seed %llu, %s, %zu ops -> %s\n", fixture.c_str(), static_cast<unsigned long long>(sd), inv.c_str(), small.ops.size(), path.c_str());
            return f ? 0 : 2;
        }
        std::fprintf(stderr, "no failing session found for %s\n", fixture.c_str());
        return 1;
    }

    if (!replay.empty()) {
        bool ok;
        std::string text = readFile(replay, &ok);
        Scenario s;
        std::string err;
        if (!ok || !fromJson(text, s, &err)) { std::fprintf(stderr, "cannot read scenario %s: %s\n", replay.c_str(), err.c_str()); return 2; }
        for (auto& b : bugs) s.bugs.push_back(b);
        if (!profile.empty()) s.profile = profile;
        RunOptions ro = opt;
        if (out != "out/coopfuzz") { ro.reportDir = out; ro.reportName = "replay"; }
        RunResult r = run(s, ro);
        for (auto& v : r.violations) std::printf("VIOLATION %s %s\n", v.invariant.c_str(), v.detail.c_str());
        std::printf("replay %s: %zu ops, %llu virtual ms, %s\n", replay.c_str(), s.ops.size(), static_cast<unsigned long long>(r.virtualMs), r.ok ? "all invariants held" : "FAILED");
        return r.ok ? 0 : 1;
    }

    auto t0 = std::chrono::steady_clock::now();
    auto elapsedMin = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 60.0; };
    int ran = 0, failed = 0;
    std::map<std::string, uint64_t> total;
    std::map<std::string, int> byInvariant;
    for (uint64_t k = 0;; ++k) {
        if (minutes > 0 && !countGiven) { if (elapsedMin() >= minutes) break; }
        else if (static_cast<int>(k) >= count) break;
        uint64_t sd = seed + k;
        int mc = players > 0 ? std::max(2, std::min(4, players)) : 2 + static_cast<int>(sd % 3);
        Scenario s = generate(sd, mc, duration);
        s.bugs = bugs;
        s.target = resolvedTarget;
        s.profile = profile;
        RunResult r = run(s, opt);
        ++ran;
        for (auto& kv : r.stats) total[kv.first] += kv.second;
        if (r.ok) continue;
        ++failed;
        for (auto& v : r.violations) ++byInvariant[v.invariant];
        std::string inv = mainInvariant(r);
        std::printf("FAIL seed %llu (%d machines): %s: %s\n", static_cast<unsigned long long>(sd), mc, inv.c_str(), detailOf(r, inv).c_str());
        if (inv == "setup") return 2;
        std::string name = "fail_seed" + std::to_string(sd) + "_" + inv;
        RunOptions full = opt;
        full.reportDir = out;
        full.reportName = name + ".full";
        run(s, full);
        if (shrinkOn) {
            int runs = 0;
            Scenario small = shrink(s, opt, inv, &runs);
            RunOptions so = opt;
            so.reportDir = out;
            so.reportName = name + ".min";
            so.reportExtra = "\"shrunk_from\": {\"ops\": " + std::to_string(s.ops.size()) + ", \"machines\": " + std::to_string(s.machines) + ", \"durationMs\": " +
                             std::to_string(s.durationMs) + "}, \"shrink_runs\": " + std::to_string(runs);
            RunResult sr = run(small, so);
            std::printf("  shrunk %zu -> %zu ops, %d machines, %u ms in %d runs; still fails: %s\n", s.ops.size(), small.ops.size(), small.machines, small.durationMs, runs,
                        sr.ok ? "NO (bug in shrinker)" : sr.violations[0].invariant.c_str());
            std::printf("  minimized scenario: %s/%s.min.scenario.json  (hash %s)\n", out.c_str(), name.c_str(), fnvHex(toJson(small)).c_str());
            std::printf("  report:             %s/%s.min.report.json\n", out.c_str(), name.c_str());
            std::printf("  replay: coopfuzz --replay %s/%s.min.scenario.json\n", out.c_str(), name.c_str());
        } else {
            std::printf("  scenario: %s/%s.full.scenario.json\n  report:   %s/%s.full.report.json\n", out.c_str(), name.c_str(), out.c_str(), name.c_str());
        }
        if (failed >= maxFail) { std::printf("stopping after %d failures\n", failed); break; }
    }
    if (!quiet) {
        std::printf("coverage:");
        for (auto& kv : total) if (kv.first.rfind("reject_", 0) != 0) std::printf(" %s=%llu", kv.first.c_str(), static_cast<unsigned long long>(kv.second));
        std::printf("\nrejected (correctly):");
        for (auto& kv : total) if (kv.first.rfind("reject_", 0) == 0) std::printf(" %s=%llu", kv.first.c_str() + 7, static_cast<unsigned long long>(kv.second));
        std::printf("\n");
    }
    std::printf("coopfuzz: %d sessions, %d failed, target %s, %zu injected bug classes\n", ran, failed, resolvedTarget.c_str(), bugs.size());
    for (auto& kv : byInvariant) std::printf("  %s: %d\n", kv.first.c_str(), kv.second);
    if (failed == 0 && ran > 0 && bugs.empty()) std::printf("RESULT: ALL OK\n");
    return failed ? 1 : 0;
}
