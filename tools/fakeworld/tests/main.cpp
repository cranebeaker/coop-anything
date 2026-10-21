#include <cstring>
#include "minitest.h"

int main(int argc, char** argv) {
    const char* group = nullptr; const char* name = nullptr; bool list = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--group") && i + 1 < argc) group = argv[++i];
        else if (!std::strcmp(argv[i], "--name") && i + 1 < argc) name = argv[++i];
        else if (!std::strcmp(argv[i], "--list")) list = true;
    }
    int ran = 0, failed = 0;
    for (auto& c : minitest::registry()) {
        if (group && std::strcmp(group, c.group)) continue;
        if (name && std::strcmp(name, c.name)) continue;
        if (list) { std::printf("%s.%s\n", c.group, c.name); continue; }
        int before = minitest::state().failures, checksBefore = minitest::state().checks;
        try { c.fn(); } catch (const minitest::Abort&) {}
        catch (const std::exception& e) { ++minitest::state().failures; std::printf("    EXCEPTION %s\n", e.what()); }
        bool ok = minitest::state().failures == before;
        std::printf("[%s] %s.%s (%d checks)\n", ok ? "PASS" : "FAIL", c.group, c.name, minitest::state().checks - checksBefore);
        ++ran; if (!ok) ++failed;
    }
    if (list) return 0;
    std::printf("== %d tests, %d passed, %d failed, %d checks ==\n", ran, ran - failed, failed, minitest::state().checks);
    if (ran == 0) { std::printf("no tests matched\n"); return 1; }
    return failed ? 1 : 0;
}
