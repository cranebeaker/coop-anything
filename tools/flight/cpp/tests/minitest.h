// Tiny self-contained test framework: TEST(group, name), CHECK, REQUIRE.
// Run: cofr_tests [--group g] [--name n] [--list]. Include this header in exactly one .cpp file: it defines main().
#pragma once
#include <cstdio>
#include <cstring>
#include <exception>
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
#define REQUIRE(cond) do { ++minitest::state().checks; if (!(cond)) { ++minitest::state().failures; \
    std::printf("    REQUIRE FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); throw minitest::Abort{}; } } while (0)

#define MINITEST_MAIN() \
    int main(int argc, char** argv) { \
        const char* group = nullptr; const char* name = nullptr; bool list = false; \
        for (int i = 1; i < argc; ++i) { \
            if (!std::strcmp(argv[i], "--group") && i + 1 < argc) group = argv[++i]; \
            else if (!std::strcmp(argv[i], "--name") && i + 1 < argc) name = argv[++i]; \
            else if (!std::strcmp(argv[i], "--list")) list = true; \
        } \
        int ran = 0, failed = 0; \
        for (auto& c : minitest::registry()) { \
            if (group && std::strcmp(group, c.group)) continue; \
            if (name && std::strcmp(name, c.name)) continue; \
            if (list) { std::printf("%s.%s\n", c.group, c.name); continue; } \
            int before = minitest::state().failures, checksBefore = minitest::state().checks; \
            try { c.fn(); } catch (const minitest::Abort&) {} \
            catch (const std::exception& e) { ++minitest::state().failures; std::printf("    EXCEPTION %s\n", e.what()); } \
            bool ok = minitest::state().failures == before; \
            std::printf("[%s] %s.%s (%d checks)\n", ok ? "PASS" : "FAIL", c.group, c.name, minitest::state().checks - checksBefore); \
            ++ran; if (!ok) ++failed; \
        } \
        if (list) return 0; \
        std::printf("== %d tests, %d passed, %d failed, %d checks ==\n", ran, ran - failed, failed, minitest::state().checks); \
        if (ran == 0) { std::printf("no tests matched\n"); return 1; } \
        return failed ? 1 : 0; \
    }
