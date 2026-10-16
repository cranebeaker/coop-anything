# Native runtime isolation test

This original harness tests a bounded runtime boundary in the pinned
[Oracles native port](https://github.com/kirby-letsgo/oracles-decomp/tree/533860de269e0e9b338e12f3484ce8497367e6ed).
It does not implement multiplayer. Two separate original single-Link simulations
alternate complete frames on one OS thread and must match two isolated input
references. Each stream covers 9,500 bootstrap steps and 30 independent movement
steps, comparing sampled RAM, rendered pixels and renderer history.

## Prerequisites and fixture identity

Use Linux x86-64, a C compiler, CMake, and a separate native-port checkout at
`533860de269e0e9b338e12f3484ce8497367e6ed`. Supply your own valid Seasons
USA/Australia revision 0 ROM, named exactly:

```text
Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc
```

The ROM, boot state and input movie remain outside this playbook. The two
upstream TAS fixtures must be the matching console-verified pair:

| Input | SHA-256 |
|---|---|
| Original Seasons ROM | `862a51368fb30539279d336b3fe193b43876d2cb15c87a36f5da517804ab3971` |
| `tas/seasons-consoleverified-boot.state` | `f16b3c969d4103d8a8d355cee069ac8dad2cb39aa0f1d5dcfaabbd5197c4ca21` |
| `tas/seasons-consoleverified.inputs` | `2f500cb406432804599dd7caf431b34360375252734876a1f53112d584e1c5b1` |

Do not combine the console-verified boot state with `seasons-play.inputs`.
The harness uses the upstream state loader and native fibers; it does not need
the CGB boot ROM or the Ages game ROM.

## Standalone build and run

Set these paths to your separate checkouts, own ROM directory, and a writable
output directory. `tas_dir` names the upstream fixture directory. The compiler
receives the ROM and TAS directories as explicit arguments through its defines.

```sh
native_root=/absolute/path/to/oracles-decomp
playbook_root=/absolute/path/to/coop-anything
rom_dir=/absolute/path/to/own-rom-directory
tas_dir="$native_root/tas"
artifact_dir=/absolute/path/to/test-output
mkdir -p "$artifact_dir"

git -C "$native_root" rev-parse HEAD
# Must print: 533860de269e0e9b338e12f3484ce8497367e6ed
```

Build the existing upstream native static-library target. SDL is unnecessary
for this headless harness. This uses the upstream configured source/object set,
including its generated game code; do not substitute a hand-selected list of
upstream C files.

```sh
cmake -S "$native_root" -B "$native_root/build" -DORACLES_SDL=OFF
cmake --build "$native_root/build" --target oraclesnative -j2

cc -std=gnu11 -O2 \
  -I"$native_root/src" -I"$native_root/tests" \
  "-DGAME_ROM_DIR=\"$rom_dir\"" "-DTAS_DIR=\"$tas_dir\"" \
  "$playbook_root/games/oracle-of-seasons/coop/native-tests/test_coop_runtime.c" \
  "$native_root/build/liboraclesnative.a" \
  -o "$artifact_dir/test_coop_runtime"

"$artifact_dir/test_coop_runtime"
```

Both include directories are required: `src` supplies native APIs and `tests`
supplies upstream `unit.h`. The compile/link command follows the verified native
scratch-harness invocation, with the additional `tests` include needed by this
exported source location. The exported file is byte-identical to the reviewed,
passing native test; the standalone exported command itself has not been rerun.

Expected success is exit 0 with `PASS native runtime isolation`. A missing
Seasons ROM exits 77; malformed or missing required TAS/state fixtures fail.
The recorded fixture is sample frame 9688, group `00`, room `c4`, Link `(72,79)`.
After 30 held frames, Right reaches `(102,79)` and Left reaches `(61,83)`;
both must move and differ while each matches its isolated reference.

Run the negative control separately. It disables only the second candidate's
movement input, leaving the isolated Left reference intact. Expected exit is 1,
with an isolation mismatch on stream 1 at complete-frame index 9500.

```sh
if "$artifact_dir/test_coop_runtime" --disable-second-input; then
  echo 'FAIL: disabled-second-input control unexpectedly passed' >&2
  exit 1
else
  negative_status=$?
  test "$negative_status" -eq 1
fi
```

To write a private gameplay checkpoint at the parked native-thread boundary:

```sh
COOP_FIXTURE_OUT="$artifact_dir/gameplay.state" "$artifact_dir/test_coop_runtime"
```

The recorded checkpoint was independently reloaded into native runtimes and
reproduced both movement outcomes and state hashes. Keep generated states private;
the native state format contains process pointers that the upstream loader
replaces, so raw state-file hashes identify artifacts rather than canonical game
state. Loading into a reused runtime also requires upstream `fibers_reset`.

## Evidence limits

The reviewed native target passed, and its disabled-input control failed as
expected. No production isolation correction was needed. This tests same-title,
sequential, complete-frame scheduling with four long-lived runtime allocations.
It does not establish thread safety, nested execution, mixed Ages/Seasons use,
renderer-slot reuse, unlimited runtimes, shared room state, two interactive Links,
networking, or campaign compatibility. Upstream renderer metadata has eight
GB-address-keyed slots, while hook/symbol selection and some feature bookkeeping
remain process-global.

The original complete baseline had 13 passes, 7 skips and 2 failures out of 22
cases: the `mods` timeout-message assertion and `app_menus` scripted invocation
timeout failed before co-op changes. Ages-first suites skipped because the Ages
ROM was absent; `wide` and `room` reached their Ages portion after Seasons work.
`sync_http` and `sync_flow` also depend on external services. The CGB boot-ROM
fixture was absent. The baseline is not wholly passing, and this gate does not
resolve those gaps.
