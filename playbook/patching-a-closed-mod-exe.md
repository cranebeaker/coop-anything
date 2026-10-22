# Patching a bug inside a mod's own rebuilt exe

Sometimes the bug is not in the game but in the mod, and the mod ships as a rebuilt
executable whose source is only partly public. You can read the mod's source for the
subsystem you care about, but the binary the player runs was compiled with the closed
game code linked in, so you cannot just rebuild it. You still have everything you need: the
public source is a map of the exe, and a small freestanding patch can carry a fix into the
binary without its build system.

This page is the route that worked on Outlast (OpenOL): a one-line gameplay rule added to a
rebuilt `OLGame.exe` whose multiplayer source is public but whose engine is not. Worked
example and all the code: [tools/outlast-coop/exe-patch](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/exe-patch) in an own fork of OpenOL.
Read [reverse-engineering.md](reverse-engineering.md) first for the decompilation tools.

## When this applies

- A mod rebuilds the game exe (or a DLL) and the bug lives in the mod's own logic.
- Part of the mod's source is public (the subsystem with the bug), the rest is not.
- Rebuilding the whole exe is out of reach: missing engine source, closed static libs, a
  build you cannot reproduce byte for byte.
- The fix is small and local: a rule, a guard, a redirected call, not a redesign.

If none of the source is public, this is ordinary reverse engineering; if all of it is,
rebuild the exe instead. This page is for the middle case.

## 1. Use the public source as a map

Read the mod's source for the broken subsystem before you open the decompiler. Note the
function names, the types, the field names and the order of operations. You are not going to
compile this source; you are going to find the same functions and the same struct fields
inside the binary and change one thing. The source tells you what the binary should look
like, so every address and offset you recover can be checked against it instead of guessed.

On Outlast the broken subsystem was enemy replication in OpenOL's public
`Development/Src/Multiplayer/Src/EnemyChannel.cpp`: every client ran the AI of each
map-placed enemy and sent it, and every client turned what it received into a second puppet,
so players saw two enemies at one spot. The fix is one rule (the lower player id keeps the
real enemy), added at two points the source names: `ApplySpawn` and `TickSend`.

## 2. Find the functions by anchoring on unique strings

Addresses move between builds and the binary has no symbols, so do not hunt for a function
by its shape. Anchor on a string only that function uses: a debug or log format string, an
error message, a `Tag`/name literal, a class name the code compares against. Search the
image for the string, then list what references it. One good anchor usually lands you in the
function or one call away from it.

Run this headless so it is repeatable. The worked example uses three tiny Ghidra scripts
([ghidra/](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/exe-patch/ghidra)): `FindStr` (find a
substring as ASCII and UTF-16 and print the functions that reference it), `RefsTo` (list the
functions that reference an address), and a `Decomp`/`Dis` pair to read one function. On
Outlast, the `MultiplayerDummyEnemy` tag literal set on the puppet led straight to
`ApplySpawn`; from there the only direct caller was the packet dispatcher, and the enemy sync
globals (`LocalPlayerID`, `SyncEnemies`) were found the same way and used to gate the new
code.

## 3. Confirm every struct field offset against the source

The decompiler will show you field accesses as raw offsets (`+0xeb8`, `+0x44`, `+0x18`).
Do not trust one in isolation. For each field you touch, match the offset to the source:
the element size of an array, the order of members, a nearby field whose meaning you already
know. Read a function whose job you understand (a getter, a constructor) to pin an offset,
then reuse it. A single wrong offset writes into the wrong field and corrupts state in a way
no SHA-256 check will catch, so this is the step to be slow on.

The Outlast notes record each offset next to the source line it came from: `RemoteEnemies`
at controller `+0xeb8` (count `+0xec0`, element stride `0x44`), `CachedEnemies` at `+0xe78`,
`DummyEnemy +0x10`, `OwnerID +0x18`, `UObject::Class +0x50` (confirmed because `Index +0x38`
and `Name +0x48` were visible in `GetName()`). Write them down this way so the next reader
can re-check them after a mod update.

## 4. Write the fix as freestanding C

Write the patch as C with no runtime behind it: no libc, no imports, no relocations. It has
to run as a blob dropped into the exe at an address the linker never saw, so it cannot
import a function or reference a global by a relocated address. Practical rules that keep the
compiled blob position independent:

- **Read the image base from the PEB**, then reach everything (the mod's own functions, its
  globals) as *image base + known RVA*. This makes address randomisation (ASLR) irrelevant:
  the blob computes real addresses at run time. On x64 Windows:
  `mov rax, gs:[0x60]` (the PEB), then `mov rax, [rax+0x10]` (`ImageBaseAddress`).
- **Call the mod's own functions through the image base**, cast to a function pointer
  (`((Fn)(ib + RVA_DESTROYACTOR))(...)`), rather than linking them.
- **Give floats as integer immediates.** A float constant would land in `.rdata` and need a
  relocation. Encode it as its 32-bit pattern and move it into an xmm register
  (`150*150` becomes the immediate `0x46afc800`), so the compiler emits no constant pool.
- **Build with the libc and unwinding stripped off:** `-ffreestanding
  -fno-asynchronous-unwind-tables -fno-stack-protector -fno-ident -mno-red-zone`, then
  check the object has no `REL32`/`ADDR64` relocations before you trust it. The Outlast
  `build.py` fails the build if any relocation survives.

Keep the blob small (the Outlast one is about 1 KB) and `used`/`noinline` on the entry
functions so nothing is dropped or inlined away.

## 5. Append a section and redirect one call site

Put the blob in a new PE section you append after the last one (the Outlast tool calls it
`.olfix`), fix up `SizeOfImage` and the section count, and zero the checksum (an exe's
checksum is not checked by the loader). Then redirect the smallest number of call sites that
carries the fix — ideally one:

- **One direct call:** overwrite the `E8 rel32` with a new `rel32` to your hook. Your hook
  does its work and calls the original through the image base.
- **One virtual call:** overwrite the indirect call (`FF 90 <disp32>`, 6 bytes) with
  `E8 rel32` plus a `NOP` to keep the length, pointing at your hook; the hook reads the
  vtable itself to reach the original. Do this only when a direct call will not do.

On Outlast two sites were redirected because the rule has two halves (drop our copy when we
receive the other client's, and drop it when we are about to send ours): a direct `call
ApplySpawn` in `OnBinaryPacket`, and a vtable `call [rax+0x268]` (`SendSpawn`) in `TickSend`.
Each hook runs the original and then applies the lower-id rule.

## 6. Ship it as a script that checks SHA-256 and never overwrites its input

Deliver the patch as a tool, not as a prebuilt binary (you cannot redistribute the mod's
exe — see [AGENTS.md](../AGENTS.md)). The tool:

- Reads the input and **refuses any file whose SHA-256 is not the exact build** the patch
  was written for. A wrong build means wrong offsets, so refuse rather than corrupt.
- **Never writes to its input.** Take an output path, refuse if it equals the input, and
  leave the player's original exe untouched. Re-running on the output is refused by the same
  SHA-256 gate.
- Guards every byte it overwrites with an `expect` on the current bytes, so a wrong address
  stops the patch instead of smashing a random instruction.

The Outlast example ships `apply_patch.py` with a pure-Python PE helper (`pe.py`) and a
`build.py` that rebuilds the blob from `olfix.c`; the committed `olfix.json` is the exact
build used in the tests, and a PowerShell port ([`play/apply-native-patch.ps1`](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/play/apply-native-patch.ps1)) does the same
for players without Python. Record the expected input hash and the expected patched-output
hash so anyone can confirm they produced the same file.

## 7. Prove it with a twin run

A patch that passes a test proves nothing until the test is known to fail without it. Run
the **same test suite against the unmodified exe in the same setup**: it must still show the
bug. If the twin passes too, the test is not exercising the fix (an earlier case consumed the
precondition, the mode silently fell back, the assert was hollow — see
[automated-qa.md](automated-qa.md)). Only a patched-PASS next to an unpatched-FAIL counts.

On Outlast, suite group A ran both exes side by side: the patched exe was 10 of 10 PASS,
including A4 (one owner), A4b (the higher id loads first) and A4c (both game processes still
alive after the handover); the twin — the original exe in the same run — was A4/A4b XFAIL,
one spot with the enemy sent by both clients, exactly as every earlier run had shown.

## Snags we hit

- **Ghidra headless breaks on a path with spaces.** `analyzeHeadless.bat` mangles a project
  or script path that contains a space. Work from a path with no spaces: make a junction to
  the workspace (`New-Item -ItemType Junction`) and run everything from there. The Outlast
  launcher `ghidra/gh.ps1` assumes this.
- **Walk a string hit back to the string's start.** A substring search lands in the middle
  of a longer string, and the references point at the string's *start address*, not at your
  hit. Step backwards until the byte before is a terminator (two zero bytes for UTF-16), then
  list references to that start address. `FindStr` does this before printing refs.
- **"Called from" is not always a caller.** Entries the decompiler lists as referencing a
  function can be slots in exception-handler or other native tables, not real call sites.
  Confirm a caller by reading the instruction (an actual `call`), not by trusting the
  cross-reference list.
