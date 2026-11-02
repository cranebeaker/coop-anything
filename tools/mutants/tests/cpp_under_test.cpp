// Code under test for the coop_mut.h tests: four tiny functions, each with one twin, and four named tests.
//
//   cpp_under_test.exe                run every test
//   cpp_under_test.exe test_add       run one test
//   cpp_under_test.exe --mut NAME ... pick a twin on the command line (same as COOP_MUT=NAME)
//
// Built twice: with COOP_RUNTIME_MUT (twins can be switched on) and without (the shipped shape: COOP_MUT_IF is false).
// Output ends with "<n> checks, <k> failed"; the exit code is 1 when k is not 0.
#include <cstdio>
#include <cstring>
#include <string>

#include "coop_mut.h"

namespace {
int g_checks = 0, g_failed = 0;
#define CHECK(c)                                                          \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(c)) {                                                       \
            ++g_failed;                                                   \
            std::printf("[FAIL] line %d: %s\n", __LINE__, #c);            \
        }                                                                 \
    } while (0)

int add(int a, int b) {
    if (COOP_MUT_IF("add_wrong_operator")) return a - b;  // twin: wrong operator
    return a + b;
}

bool isEven(int x) {
    if (!COOP_MUT_IF("even_check_removed")) return (x % 2) == 0;  // twin: the check is removed
    return true;
}

const char* label(bool flag) {
    return COOP_MUT_IF("label_always_off") ? "off" : (flag ? "on" : "off");
}

int clampTo(int x, int lo, int hi) {
    if (COOP_MUT_IF(4)) return x;  // twin 4 (a number): no clamping
    return x < lo ? lo : (x > hi ? hi : x);
}

void test_add() {
    CHECK(add(2, 3) == 5);
    CHECK(add(-1, 1) == 0);
}
void test_even() {
    CHECK(isEven(4));
    CHECK(!isEven(7));
}
void test_label() {
    CHECK(std::string(label(true)) == "on");
    CHECK(std::string(label(false)) == "off");
}
void test_clamp() {
    CHECK(clampTo(50, 0, 10) == 10);
    CHECK(clampTo(-5, 0, 10) == 0);
    CHECK(clampTo(5, 0, 10) == 5);
}
}  // namespace

int main(int argc, char** argv) {
#ifdef COOP_RUNTIME_MUT
    coop_mut::init_from_args(argc, argv);
    std::printf("runtime twins: on, twin = '%s'\n", coop_mut::current().c_str());
#else
    std::printf("runtime twins: compiled out\n");
#endif
    const char* only = nullptr;
    for (int i = 1; i < argc; ++i)
        if (argv[i][0] != '\0') only = argv[i];
    struct T {
        const char* name;
        void (*fn)();
    } tests[] = {{"test_add", test_add}, {"test_even", test_even}, {"test_label", test_label}, {"test_clamp", test_clamp}};
    bool ran = false;
    for (const T& t : tests) {
        if (only && std::strcmp(only, t.name) != 0) continue;
        t.fn();
        ran = true;
    }
    if (!ran) {
        std::printf("unknown test %s\n", only);
        return 2;
    }
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
