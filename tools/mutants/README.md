# mutants: mutation twins you can switch on at run time

A **twin** is a deliberately broken copy of one piece of your code, for example `a - b` where the code says `a + b`.
The test that guards that code must fail when the twin is on. A twin that no test notices means the test proves
nothing about that code, and you find that out before the bug comes back.

The old way compiles the code once per twin: 40 twins, 40 builds. This tool builds once. The broken variant sits in the code
behind a switch, and you pick the twin when the test program starts:

```
set COOP_MUT=add_wrong_operator && my_tests.exe        (a name or a number)
```

| Part | What it does |
|---|---|
| [`coop_mut.h`](coop_mut.h) | Header-only C++ switch. `COOP_MUT_IF("name")` is false and costs nothing unless you build with `COOP_RUNTIME_MUT`. |
| [`coop_mut.py`](coop_mut.py) | The same idea for Python: `coop_mut.mut("name")`. |
| [`run_twins.py`](run_twins.py) | Reads a twin table (twin name to the test that must fail), runs every test once without a twin, then each twin, and reports caught or missed. |
| [`twins.example.json`](twins.example.json) | A table you can run as it is. |

The Python parts use the standard library only, and the C++ header uses only the C++ standard library. The tests build the header as C++17 with Visual Studio 2022 on Windows.

## Quick start

**C++.** Put the header on your include path and wrap the broken variant:

```cpp
#include "coop_mut.h"

int add(int a, int b) {
    if (COOP_MUT_IF("add_wrong_operator")) return a - b;   // the twin: a broken variant
    return a + b;
}
bool isEven(int x) {
    if (!COOP_MUT_IF("even_check_removed")) return x % 2 == 0;   // a twin that removes code
    return true;
}
```

Build the code under test **for your tests** with `COOP_RUNTIME_MUT` defined, and your shipped build without it:

```cmake
add_library(core_rt STATIC core.cpp)                 # the test build
target_compile_definitions(core_rt PRIVATE COOP_RUNTIME_MUT)
add_library(core STATIC core.cpp)                    # the shipped build: COOP_MUT_IF(x) is the constant false
```

A test `main` can also accept `--mut NAME`: call `coop_mut::init_from_args(argc, argv);` first.

**Python.**

```python
import coop_mut

def add(a, b):
    if coop_mut.mut("add_wrong_operator"):
        return a - b
    return a + b
```

`COOP_MUT=add_wrong_operator python my_tests.py` switches it on. `coop_mut.active("name")` is a context manager for in-process
tests, and `coop_mut.ENABLED` is False when no twin is chosen.

**The table.** One JSON file says which test must fail for which twin:

```json
{
  "label": "core",
  "cmd": ["build/core_tests.exe", "{test}"],
  "real_regex": ", 0 failed",
  "caught_regex": ", [1-9][0-9]* failed",
  "scan": ["src/**/*.cpp"],
  "twins": {
    "add_wrong_operator": "test_add",
    "even_check_removed": ["test_even", "test_range"]
  }
}
```

```
python -I tools/mutants/run_twins.py core_twins.json
python -I tools/mutants/run_twins.py tools/mutants/twins.example.json --verbose
```

The last line is `<label>: real tests pass, all N twins caught - ALL OK` only when every twin was caught. The exit code is 0 for that, 1 when a
twin was missed or the real run fails, 2 when the table is wrong. Every column of the table is described at the top of
[`run_twins.py`](run_twins.py) (`--help` prints it).

## Add a twin for every fix

Do this each time you fix a bug. It takes two minutes and it is what keeps the fix fixed.

1. Write the test that fails on the bug. Run it and watch it fail.
2. Make the fix.
3. Keep the old behavior behind a twin: `if (COOP_MUT_IF("short_name_of_the_bug")) { old, broken code } else { fixed code }`.
   For a fix that adds a check, write `if (!COOP_MUT_IF("name")) { the new check }`.
4. Add the row to your twin table: `"short_name_of_the_bug": "the_test_from_step_1"`.
5. Run `run_twins.py`. It must say `caught`. Read the other results:

| Result | What it means | What to do |
|---|---|---|
| `MISSED` | The named test passed with the twin on. The test does not guard that code, or the twin is not wired in. | Make the test check the thing the fix changed, or fix the twin name. |
| `WRONG TEST` | The named test passed, but another test failed. The twin breaks something else. | Fix the table (name the test that fails) or narrow the twin. |
| `BUILD CRASH` | A compile time twin did not build (see below). | A twin must compile and then fail its test. |
| `UNLISTED` / `STALE` | With `"scan"`: a twin used in the sources is not in the table, or a table row has no call site. | Add the row, or delete the dead row. |

A twin that fails because the program crashed is not counted as caught when you give `caught_regex`, so a crash is not mistaken
for a failing check. A loader error (missing DLL, bad image) is never counted as caught. A hang counts as caught.

Keep twin names short and about the bug, not the line. Numbers work too (`COOP_MUT_IF(7)`, `"7": "test_x"`).
One twin is on at a time. Never leave `COOP_RUNTIME_MUT` defined in your shipped build.

## Compile time twins

Some breakage cannot sit behind an `if`, for example a different type or a removed member. Give the table a `twin_build` command, which
runs once per twin with the environment variable `COOP_MUT` set to the twin (your build reads it and passes a `-D` flag), then the test.
When a twin does not compile the result is `BUILD CRASH`, and after the last twin the runner builds again without a twin so your tree
is left clean. Twins with a build run one at a time.

## How it is tested

```
python -I tools/mutants/run_tests.py       # prints ALL OK only if every check passes and every twin is caught
```

20 checks and 28 twins; it takes about a minute, most of it the C++ builds. `--no-cpp` skips the C++ part and says so;
`--build-dir DIR` (or `COOP_BUILD_DIR`) picks the build folder, which defaults to a folder under the system temp directory (keep
it short: Visual Studio fails on paths longer than 260 characters).

- **The runner**, against fake test programs: all twins caught; a twin that changes nothing is reported as `MISSED` with no `ALL OK`;
  a twin that breaks the wrong test is `WRONG TEST`; a twin that crashes the build is `BUILD CRASH` and the tree is rebuilt without it;
  a failing real run stops everything; a loader error, a crash without the expected text and a hang are judged correctly; a list of
  tests where one is enough; the parent environment never leaks a twin into the real run; the source scan; table errors and exit
  code 2; parallel runs; `--only`, `--list`, `--json`; the example table.
- **`coop_mut.py`**: off by default, `0` and empty mean off, names and numbers match exactly, `set`, `active` (also after an exception),
  `--mut` forms, a child process reads its own environment.
- **`coop_mut.h`**: built twice with CMake. With `COOP_RUNTIME_MUT` all four twins are caught by the tests named for them, the
  `--mut NAME`, `--mut=NAME` and `--mut 4` forms work, `COOP_MUT=0` and a partial name are no twin. Without it the program
  ignores `COOP_MUT` and `--mut`, and the binary contains neither the twin names nor the string `COOP_MUT`.
- **Twins of the tools themselves**, each applied from outside: a caught test that always says yes or never, loader errors counted as
  caught, a skipped real run, a leaking environment, no wrong-test detection, a build crash ignored, no rebuild, a scan that finds
  nothing, no timeout handling, a report that is never written, no table checks, `--only` ignored; in `coop_mut.py` the environment
  ignored, prefix matching, `active` not restoring, `0` as a twin, arguments not removed; in `coop_mut.h` a name that never
  matches, a number that never matches, a flag that is ignored, prefix matching, and a shipped build that can still be switched.

## Files

`coop_mut.h`, `coop_mut.py`, `run_twins.py`, `twins.example.json`, `run_tests.py`, `tests/` (`cpp_under_test.cpp`, `CMakeLists.txt`,
`py_under_test.py`, `py_mut_checks.py`, `fake_suite.py`, `fake_build.py`).

Generalised from fnv-coop commit e85460f (github.com/cranebeaker/fnv-coop): `tools/mut/fnvcoop_mut.h` and `run_twins.py`. What changed: the
macro and the variable are `COOP_MUT_IF` and `COOP_MUT`, a twin has a name as well as a number, the table names the test that must
fail instead of one command for the whole suite, a twin that breaks another test or does not build is reported on its own, the
sources can be scanned against the table, and there is a Python switch.
