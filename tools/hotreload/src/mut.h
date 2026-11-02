// Runtime mutation twins. A twin is a deliberately broken variant of one safety rule. The tests must notice it.
//
// Test builds define HOTRELOAD_RUNTIME_MUT, and the variant is picked when a process starts:
//     set HOTRELOAD_MUT=4        (environment variable, read once)
// or in-process by the unit tests with hotreload_mut::set(n).
// The sources say  if (HR_MUT(4)) { ...broken variant... }  or  if (!HR_MUT(6)) { ...code the twin removes... }.
// Production builds (no HOTRELOAD_RUNTIME_MUT) get HR_MUT(n) == false: the broken variants are dead code and nothing
// reads the environment.
//
// Integration twins (loader or core in the test host, run by tests/scenarios.py):
//    1 drain_off             the drain says "nobody is inside" without looking
//    2 adopt_ignored         adoptThread() does nothing, so an owned library thread blocks the drain
//    3 managed_ids_ignored   the drain does not ignore ManagedThreads
//    4 no_cache_copy         the loader maps the original core file (the build can no longer overwrite it)
//    5 stale_build_id        the loader keeps the previous build id after a reload
//    6 version_ignored       a state blob with an older layout version is read as the current layout
//    7 no_state_blob         the old core never hands its state to the loader
//    8 save_failure_ignored  a participant whose save fails no longer vetoes the reload
//    9 no_rollback           the loader stays dead when the new core fails to start
//   10 veto_ignored          the loader unloads the core even when it refuses
// Unit twins (tests/unit_tests.cpp, run with --twin N):
//   14 crc_unchecked         a corrupt payload is handed to restore()
//   15 forward_shutdown      shutdown runs in registration order instead of reverse
//   16 thread_not_joined     ManagedThread detaches instead of joining
//   17 drain_ip_only         the drain looks only at the instruction pointer, not at return addresses on the stack
//   18 drain_timeout_ok      a drain timeout counts as success
//   19 preflight_ignored     canUnload() refusals do not veto
#pragma once

#ifdef HOTRELOAD_RUNTIME_MUT
#include <cstdlib>

namespace hotreload_mut {
inline int& slot() {
    static int v = [] {
        const char* e = std::getenv("HOTRELOAD_MUT");
        return (e && *e) ? std::atoi(e) : 0;
    }();
    return v;
}
inline int current() { return slot(); }
inline void set(int n) { slot() = n; }
}  // namespace hotreload_mut

#define HR_MUT(n) (::hotreload_mut::current() == (n))
#else
#define HR_MUT(n) (false)
#endif
