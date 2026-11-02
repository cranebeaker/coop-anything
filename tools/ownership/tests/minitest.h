// Tiny self-contained test framework: TEST(group, name), CHECK, REQUIRE. Run: <test exe> [--group g] [--name n] [--list]
#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace minitest {
struct Case { const char* group; const char* name; void (*fn)(); };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
struct Reg { Reg(const char* g, const char* n, void (*f)()) { registry().push_back({g, n, f}); } };
struct State { int checks = 0, failures = 0; };
inline State& state() { static State s; return s; }
struct Abort {};
}  // namespace minitest

#define TEST(group, name) \
    static void test_##group##_##name(); \
    static minitest::Reg reg_##group##_##name(#group, #name, &test_##group##_##name); \
    static void test_##group##_##name()

#define CHECK(cond) do { ++minitest::state().checks; if (!(cond)) { ++minitest::state().failures; \
    std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_MSG(cond, msg) do { ++minitest::state().checks; if (!(cond)) { ++minitest::state().failures; \
    std::printf("    FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__, #cond, std::string(msg).c_str()); } } while (0)
#define REQUIRE(cond) do { ++minitest::state().checks; if (!(cond)) { ++minitest::state().failures; \
    std::printf("    REQUIRE FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); throw minitest::Abort{}; } } while (0)
