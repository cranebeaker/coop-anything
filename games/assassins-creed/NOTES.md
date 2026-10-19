# Assassin's Creed 1 (2007) — session notes

Agent session started 2026-10-19. Owner wants campaign co-op with friends.

## Phase 0: Triage answers

1. **Which exact game?** *Assassin's Creed* (2007), **Director's Cut Edition**, PC. Steam
   copy (owner's answer), installed at `<game dir>` on drive D. Two 32-bit exes:
   `AssassinsCreed_Dx9.exe` and `AssassinsCreed_Dx10.exe`, version **1.0.2.1**.
   Identity hashes (see below) match EaglePatch's "Confirmed supported exes (GOG/Steam)".
2. **Is it already done?** No co-op mod for AC1 found. Details under Phase 1.
3. **Which engine?** Ubisoft **Scimitar** (the original Anvil; renamed AnvilNext from AC2
   on). Not in `playbook/engine-families.md` → no same-family co-op framework to reuse.
4. **How much source?** Nothing scriptable and no decompilation. But the exe ships full
   MSVC RTTI with **4084 `scimitar::` classes** (2575 top-level), so class names/vtables
   are recoverable. Native hooks proven by EaglePatch.
5. **Does the engine have multiplayer?** No. Confirmed by string scan and by product
   history (series MP arrived in Brotherhood, 2010). A sync layer must be built.
6. **Shipping check.** No anti-cheat, no live online service in gameplay (only Ubisoft
   telemetry, removable). Ship as an ASI/DLL package; no game files redistributed.

## Phase 1: Research — what exists

No public co-op work for AC1, and no netcode to reuse. Observations:

| Find | What it gives | Source |
|---|---|---|
| `Sergeanur/EaglePatch` (C++, ~368★, last release v1.1 2025-12-02) | **ASI plugin** for AC1 & AC2: `DllMain` + `InjectHook`/`PatchByte`, hardcoded addresses per DX9/DX10 build, wraps the engine's own allocator (`ac_allocate`, `ac_getNewDescriptor`, `ac_delete`), reads engine classes (`scimitar::Pad`, `PadProxyPC`, `PadXenon` with vtables/sizes). Proves native in-process patching works on AC1. | https://github.com/Sergeanur/EaglePatch |
| `ThirteenAG/Ultimate-ASI-Loader` | The loader EaglePatch uses (`dinput8.dll` proxy + `scripts/` folder). Reusable foundation for a co-op ASI mod. | (linked from EaglePatch readme) |
| `gentlegiantJGC/ACExplorer` (Python, archived) | `.forge` archive explorer/exporter (Unity first, designed to extend). The local install is `DataPC_*.forge` archives (~6 GB). | https://github.com/gentlegiantJGC/ACExplorer |
| Turfster ".forge extractor/replacer" | Extract/replace AC1 + AC2 (Scimitar) `DataPC_*.forge` incl. textures. | moddb.com/downloads/forge-extractorreplacer-by-turfster |
| `Sev7nOfNine/Universal-Modder` knowledge note (2026-10-05) | Another agent's family-wide survey. Verdict on AC1: "no MP, no framework, tiny community — expect to build tooling from scratch"; "AC1 is a trap for feature work". Also: no public co-op mod for **any** AC game. | github.com/Sev7nOfNine/Universal-Modder (knowledge/games/assassin-s-creed/…) |
| `bloxtbc/SubtitleSynchAC1` | Only other AC1-specific tool named in the survey (subtitles). | (survey) |
| Reddit r/assassinscreed 14ksmrc | Community consensus: AC titles are "notoriously difficult to be modded", mostly retexture/re-shade. | reddit.com/r/assassinscreed/comments/14ksmrc/ |
| Nexus "Origins Together" (AC Origins, in development) | Not AC1; shows demand for AC co-op. | nexusmods.com/assassinscreedorigins/images/377 |

## Phase 2: Local workspace state (owner's PC — private details in `local/`)

- Install: `<game dir>` (Steam library on D:).
- **The install is already patched** by a third-party `ac1-fix` package that was placed in
  the game folder (files `RUN-AC1-FIX.cmd`, `ac1-fix.sh`, created/copied 2026-08 — run
  ~2026-10-19 22:24). Provenance unknown; not audited by us and **not run by us** (Ground
  Rules 8–9). What it did, read from its own script:
  - Zeroed the 16-byte string `gconnect.ubi.com` inside both exes (kills the logo-freeze /
    telemetry), keeping backups in `.ac1-fix-backup/`.
  - Added outbound Windows Firewall blocks for the three exes.
  - Wrote `Launch_AssassinsCreed_{Dx9,Dx10}_31_threads.cmd` (CPU affinity for 32+ thread CPUs).
  - Menu option to open the EaglePatchAC1 PCGamingWiki link (EaglePatch **not** installed).
- Consequence: the exes **on disk are modified**; the hashes below are of the clean
  originals preserved in `.ac1-fix-backup/*.original`.
- No `scripts/` folder and no `dinput8.dll` in the game dir → no ASI loader/EaglePatch yet.
- Config: `%APPDATA%\Ubisoft\Assassin's Creed\Assassin.ini` (present, currently **0 bytes**)
  and `DARE.INI` (DARE audio middleware: DS3D options, `NB_AUDIBLE_VOICES=48`).
- Save files: **none found** under `%APPDATA%`, `%LOCALAPPDATA%` or `Saved Games` → the
  story has not been played here yet, and the save path is **unknown** (Phase 3 task).

### Game identity (recorded, Ground Rule 5)

| File | Version | SHA-256 (clean original) | MD5 | SHA-1 |
|---|---|---|---|---|
| `AssassinsCreed_Dx9.exe` | 1.0.2.1 | `2FA8D50C60398CC249C6D908DE46B15F48E445DD0C605B5956E94FA2F445A713` | `8E72C3333743780E43BC2C34BBF625F9` | `A9EA476E70F076709CCA59A802E26EC894432120` |
| `AssassinsCreed_Dx10.exe` | 1.0.2.1 | `576A04841C37BE5A819974ABD59EA3ECD818F0BC36ACAB71C62EE0313DCA0129` | `CA87753255E2D14B1F18BB737C643792` | `601475FD5EFA02BBAE80D1C06DFA25A0A1E7FD2F` |

MD5/SHA-1 match the EaglePatch readme's "Confirmed supported exes (GOG/Steam)" exactly →
this is the standard Director's Cut build the community tools target.

## Phase 3 (light): Engine facts mined this session (read-only)

From RTTI and a raw string scan of the clean DX9 exe:

- 4084 classes in namespace `scimitar`, 2575 top-level. Gameplay-relevant names seen:
  `Human`, `HumanDecision`, `HumanNPCDecision`, `HumanData`/`HumanDeadData`/`HumanPoleData`/
  `HumanRopeData`, `PDeAssassin`, `AssassinFirstPersonCamera`, `AIGlobalManager`,
  `AISerializedManager`, `AIWorldSpecificManager`, `FuNPCNav`, `Scene`, `SceneHandler`,
  `WorldTransitionManager`, `GamePlayGroupHandler`, `AssassinationConditionClip`,
  `MissionItemSceneSequencer`, `AltairRanker`, `AnimTrack*` (animation track types).
  → A scene/event-driven single-player engine with heavy `EventCallback`/`EventAccumulator` use.
- Networking: `WSAStartup` ×1, `socket` ×1, `gconnect` ×1 (the removable telemetry);
  `DirectPlay`/`NetDriver`/`Replication`/`Udp` = 0. **No gameplay netcode.**
- Input stack already carries 8 pad sets (`Keyboard1..4`, `Joy1..4`) and console pad classes
  (`PadXenon`, `PS2Pad`) — a console-derived port. Not split-screen, but note the breadth.

## Phase 5: M0 started (2026-10-19)

Owner chose the from-zero build. First step taken, in the separate mod repo layout
(`ac1-coop/`, kept out of this hub per rule 7):

- Isolated test install created by copying the game folder (`<game dir>` → a workspace
  copy, ~6.7 GB), with the **clean** exes restored from `.ac1-fix-backup/`.
- Wrote `src/dllmain.cpp`: ASI plugin skeleton that writes `ac1coop.log` next to the exe
  and reports the detected build using EaglePatch's two signature bytes. **Compiles**
  (MSVC 14.51, x86) and the DLL/ASI loader are installed into the test copy.
- **Verified in game 2026-10-19** (M0 done). Launched the test copy's
  `AssassinsCreed_Dx9.exe`; `ac1coop.log` next to the exe contains:

  ```
  [2026-10-19 22:58:48] AC1Coop M0 loaded: module=6BDF0000 gameBase=00400000 build=DX9 imageBase=00400000
  ```

  So our own code loads through the ASI loader and the DX9/DX10 signature check works.
- Toolchain on the owner's PC: MSVC 14.51 (x86/x64) + MSBuild, Python 3.11, git.
  CPU is 24 logical processors → the 32+ thread launch bug does not apply here.

## Phase 6: M1 started — engine class map from RTTI (2026-10-19)

M1's question is whether a second, driver-controllable `Human` can exist. First concrete
step: recover the engine's class / vtable / virtual-function map from the exe's own RTTI,
so a decompiler — and our own runtime code — can name things.

- Wrote `ac1-coop/tools/rtti_map.py` (pure Python, no dependencies). It parses the PE,
  then walks MSVC RTTI: TypeDescriptors → CompleteObjectLocators → vtables → virtual
  function addresses, plus the class-hierarchy descriptors.
- **Important detail:** this exe stores RTTI / vtable pointers as **absolute VAs**
  (image_base + RVA), not RVAs. Naive RVA handling misreads them, because some RVAs are
  ≥ image_base (e.g. `0x0178FD7C` sits inside `.rsrc`'s RVA range). Reads must prefer the
  VA interpretation.
- Result on the clean DX9 exe (v1.0.2.1, SHA-256 `2FA8…A713`):
  - 4,323 type descriptors, 4,544 CompleteObjectLocators
  - **1,645 vtables**, 29,291 virtual slots, **13,037 distinct functions named**
- Recovered inheritance chains (derived ← base):
  - `Human` ← `AIActor` ← `Object` ← `IHuman` (Human: 87 virtuals)
  - `PlayerSpawner` ← `CharacterSpawner` ← `Spawner` ← `Component` ← `ManagedObject` ← `Object` (45)
  - `NPCSpawner` ← `CharacterSpawner` ← … (45)
  - `HumanUpperBody` ← `VHuman::DataContext` ← `ActorContext` ← `FSMStaticState`
- `PDeAssassin` is real but has **no vtable** — a pure data descriptor class (like
  `PDeHayStack` / `PDeScaffold`), seen only through `EventCallback<VPDeAssassin,…>`
  templates. So the player *entity* class to watch is **`Human`**, not `PDeAssassin`.
- Deliverables in `ac1-coop/tools/out/`: `rtti_classes.txt`, `rtti_classes.json`, and
  `rtti_symbols.csv` (`address,type,name`). `ac1-coop/tools/ghidra_apply_rtti.py` applies
  the CSV in Ghidra to name all 1,645 vtables and the ~13k functions.

Next: decompile around `PlayerSpawner` / `CharacterSpawner` to find the spawn path and the
global that holds the local player, then prove a second `Human` exists at runtime.

### M1 recon v1: two bugs worth remembering (2026-10-19)

The first runtime scan reported 5 "objects" at consecutive addresses
(`…FEB0/FEB4/FEB8/FEBC/FEC0`), in the exact order of the scanner's own target array.
Two independent bugs:

1. **vptr is COL-slot + 4.** `rtti_map.py` records the address of the RTTI
   CompleteObjectLocator pointer, which sits immediately *before* the vtable
   (`col_rva`). A live object's first word points at the **first virtual function**,
   i.e. `col_rva + 4`. Searching for `col_rva` finds nothing real.
2. **The scan matched itself.** The array of target addresses lived on the scanning
   thread's stack, so the scanner found its own copy of those pointers.

Fixes: a `SelfCheck` that verifies, for all 1,645 vtables, that the COL pointer is
present at `col_rva` at runtime before scanning (proves the address map); the scan
skips its own module and its own stack region; and the target table is a static array
in the plugin image (never on the scan stack).

### M1 recon v2: object census

`src/dllmain.cpp` is now a census rather than a 5-target match. For every dword in
committed readable memory it checks membership in a sorted table of all 1,645 vptr
values (`tools/gen_vtable_table.py` → `src/rtti_vtables.inc`). A hit means a live
object of that class; `object = vptr_slot - col_offset`. Output: totals
(private/image/mapped), the top 30 classes by live object count, a spotlight for
Human/AIActor/PlayerSpawner/NPCSpawner/Assassin/Guard, and the image-resident
pointers that reference the first `Human` (candidate player global).

### M1 recon v2: results in the running game (2026-10-19/20)

Run in Masyaf, Steam Director's Cut data, our clean DX9 exe, plugin
`ac1-test\scripts\AC1Coop.asi`. Log kept at `ac1-test\ac1coop.log`.

- `M1 RTTI self-check: 1645/1645 vtable addresses verified` — the whole RTTI address
  map is correct at runtime (COL pointer present at every recorded address). The
  plugin refuses to scan if this is not 100%.
- Census: 4,003–5,889 committed readable regions, 905–1,024 MB, **136,048–175,121
  class-instance matches** (96% private/heap, ~5,392 in the image — constant, i.e.
  static/global instances), **1,040–1,162 distinct classes live at once**.
- Counts move between scans (`magma::Keyframe` 58,776 → 28,241 → 28,241,
  `scimitar::SoundBao` 11,864 → 28,035) = real allocation/streaming, not noise.
- Top live classes seen: `magma::Keyframe` (animation), `scimitar::SoundBao`,
  `magma::Element`, `scimitar::SimpleCustomAction`, `scimitar::ContactEvent`,
  `scimitar::JumpLinkSurrogate`, `magma::ScaleStateProperties`,
  `scimitar::GameStateData`, `scimitar::Entity` (~3.2k), `scimitar::ActionTransition`,
  `hkRigidBody` (~2.5k), `hkCapsuleShape`, `hkMoppCode`, `hkConstraintInstance`,
  `scimitar::RigidBody`, `scimitar::Bone`, `scimitar::Mesh` / `CompiledMesh` (~1k).
- Engine layering is now visible by name: `magma::` = data/asset/tool layer,
  `scimitar::` = game layer, `hk*` = **Havok** physics.
- Spotlight (Masyaf, walking around): Human-family objects **1,284–1,816 across 47
  classes**, `scimitar::PlayerSpawner` **3–11**, `scimitar::NPCSpawner` **5–12**,
  Assassin-family **186–234 across 27 classes**, Guard-family 0–4. `AIActor` has 0
  live objects even though its class is present in the image.
- Class-map note: `scimitar::Human` (87 virtuals) is the only entity class; every
  other `Human*` is a state (`HumanGround`, `HumanLedge`, `HumanClimb`, …), a `*Data`
  descriptor, an `IHuman*` interface, or a `VHuman::DataContext` FSM context.
- Early `PlayerSpawner` decompilation (Ghidra, see below): its instances are **0xa0
  bytes**, allocated through the engine object factory `FUN_007a4510(kind=2, size,
  classinfo, …)` with `classinfo` from the pool allocator `FUN_00924070(size,
  align=0x10, …)`. The Spawner vtable itself only does clone/serialize/construct; the
  actual character creation is elsewhere (to find).

### M1 tooling: Ghidra 12 has no Jython (2026-10-19)

Ghidra 12 removed the bundled Jython, so a `.py` post-script fails with
`JythonStubScriptProvider$JythonStubException` **before the program is saved**, which
then leaves the project without the imported program (a follow-up `-process` run fails
with `Requested project program file(s) not found`). Use Java scripts instead:
`tools/ghidra_apply_rtti.java` names the map, `tools/ghidra_dump_class.java` decompiles
every `<Class>__vfN` of one or more classes to a `.c` file for reading. Auto-analysis of
the 6.8 MB exe takes ~590 s; the RTTI analyzer 44 s.

### M1 recon v3: the reference graph (2026-10-20)

The census says *what exists*; to find *the player* we need to know who holds a
`Human`. `src/dllmain.cpp` now collects every live `scimitar::Human` object, then scans
memory for pointers landing inside any of them, and walks backwards from each such
pointer to the nearest vtable pointer to name the holder's class. Owners that live in
the game image (`.data`) are printed as `image/global` — a singleton that holds exactly
one `Human` is the player. Runs on the first census and on **F8**, not every 60 s.

## Phase 4: Chosen path

**Path 2 — build from zero** (no mod to revive). Recommendation and plan are in
`DESIGN.md`: an ASI plugin + host-authoritative state sync, starting with a puppet player.
This is a multi-week reverse-engineering project, not a session task. Owner chose
"research + design doc first".

## Sources

- https://github.com/cranebeaker/coop-anything (AGENTS.md, DIRECTORY.md, playbook/)
- https://github.com/Sergeanur/EaglePatch (+ its `readme.md`, `src/ac1.cpp`, `EaglePatchAC1.ini`)
- https://github.com/ThirteenAG/Ultimate-ASI-Loader
- https://github.com/gentlegiantJGC/ACExplorer
- https://github.com/Sev7nOfNine/Universal-Modder (AC family survey, 2026-10-05)
- https://www.reddit.com/r/assassinscreed/comments/14ksmrc/any_coop_mod_to_any_assassins_creed_campaign/
- https://www.nexusmods.com/assassinscreedorigins/images/377

## Unknowns

- Where AC1 stores save games and what the save format is.
- Whether a second `Human`/player entity can be spawned and driven (the gate for any co-op).
- Provenance/author of the local `ac1-fix` package.
- Whether `PDeAssassin` is the local player class (name suggests it; unverified).
- Nexus/ModDB site search + Reddit search not run with site tools (web search API returned 403).
