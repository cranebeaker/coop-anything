// A tiny test harness: TEST(group, name) { CHECK(...); }. No dependencies.
#pragma once
#include <cstdio>
#include <string>
#include <vector>

namespace minitest {

struct Case { const char* group; const char* name; void (*fn)(); };
struct State { int failures = 0, checks = 0; };
struct Abort {};

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline State& state() { static State s; return s; }
struct Registrar { Registrar(const char* g, const char* n, void (*f)()) { registry().push_back({g, n, f}); } };

inline bool check(bool ok, const char* expr, const char* file, int line, const std::string& msg = "") {
    ++state().checks;
    if (!ok) {
        ++state().failures;
        std::printf("    CHECK FAILED %s:%d: %s%s%s\n", file, line, expr, msg.empty() ? "" : "  -- ", msg.c_str());
    }
    return ok;
}

}  // namespace minitest

#define TEST(group, name) \
    static void test_##group##_##name(); \
    static minitest::Registrar reg_##group##_##name(#group, #name, &test_##group##_##name); \
    static void test_##group##_##name()
#define CHECK(c) minitest::check((c), #c, __FILE__, __LINE__)
#define CHECK_MSG(c, m) minitest::check((c), #c, __FILE__, __LINE__, (m))
#define REQUIRE(c) do { if (!minitest::check((c), #c, __FILE__, __LINE__)) throw minitest::Abort(); } while (0)
