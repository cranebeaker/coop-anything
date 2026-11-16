# Step 4 — Gate 3: runtime actor and object model (static half)

This file belongs to one agent step. Do not edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`
or `data/` — the integrator folds this file into them once no other agent is running
(see `../PARALLEL.md`).

- Prompt: `prompts/04-object-model.md` (static half only, this run)
- Agent: step 4 static subagent (spawned by the parent session)
- Branch / worktree: `game/south-park-the-stick-of-truth-step04-objects` at `D:\sp-step04`
- Launch lock held: none — no game launch in this run (static on-disk parse only)
- Test copy verified: SHA-256 `54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6`,
  11,468,656 bytes (matches `PLAN.md`)

## 2026-11-15 — step 4 — static RTTI/vtable index (step04 subagent)

Scope of every line below: the on-disk test-copy image only. Every class is **observed in MSVC
RTTI**. Nothing here says anything about runtime layout, field offsets, the vtable's method
list, or behaviour. `.text` is never decoded.

## Observed (ran and saw)

- Hash gate: `Get-FileHash` on `D:\coop-work\game-test\South Park - The Stick of Truth.exe` gave
  `54A83D055F97CAD8BEEB5AA027C96C0C2AA4A6C36394634270297DD47856A0D6`, size 11,468,656 bytes.
  The tool refuses any other file: `py -3.14 -B tools\rtti_index.py --exe tools\rtti_index.py`
  → `refusing: exe does not match …`, exit 2.
- PE header (pefile): i386 (0x14c), ImageBase `0x400000`, SizeOfImage `0x1e5b000`. Sections:
  `.text` rva `0x1000`, `.rdata` `0x738000`, `.data` `0x8a5000`, `.rsrc` `0x1d4b000`, `.bind` `0x1df2000`.
- Self-test: `py -3.14 -B tools\rtti_index.py --self-test` → `8/8 demangler cases passed`.
- Full run: `py -3.14 -B tools\rtti_index.py --pylibs D:\coop-anything\local\pylibs` (run from the
  game folder) → 937 type descriptors; 929 x86 Complete Object Locators verified (the class
  hierarchy descriptor, the base array and entry 0 = the class itself all check out); 929 vtables;
  878 classes with at least one vtable; 59 type-descriptor-only classes; 0 duplicate names; 0
  base-list mismatches between COLs; 0 vtables rejected. Output `tools/rtti_index.json`, 835 KB.
- Reproducibility: two consecutive runs gave the same SHA-256 for `rtti_index.json`
  (`33CB898B7AEB46B4ECE846ADAC30E84AF4770817A060ECF4072B7AEFE6B8F320`).
- Completeness check, byte level: the whole file contains exactly 937 `.?A[UVTW]` occurrences, and
  937 NUL-terminated names of at most 153 characters. The tool accepted all 937 — none was dropped
  by the type-descriptor check.
- Bare-name search (raw bytes, file offsets): `Player_Object` occurs 4 times, all inside
  `.?AV?$Host/Ghost_Object_Controller@U…Player_Object_Net_Data…` names (file offsets
  `0x98e7bf`, `0x98e839`, `0x98f10e`, `0x98f188`); `Secondary_Player_Object` 2 times and
  `Remote_Camera_Object` 2 times, both only inside Host/Ghost names; `Companion` appears as
  `.?AV…Companion_Basic/Companion_Follow…` names and as text; `Creature` and `Actor` as parts of
  other names and as text; `NetConnection` only as text
  (`instance::NetConnection::…is not implemented`); `Level_State` as `Component_Level_State`-style
  names and text.
- Focus results from the index (vtable RVAs; COL RVA in brackets where useful):

| Focus name | Result | Vtable RVA(s) |
|---|---|---|
| `Player_Object` | **not found** as a class name. Only as the template argument of Host/Ghost controllers. | Host `<Player_Object_Net_Data>` `0x7f7330`; Ghost `<Player_Object_Net_Data>` `0x7f7100` |
| `Secondary_Player_Object` | **not found** bare. Only as a Net-data template argument. | Host `0x7f734c`; Ghost `0x7f711c` |
| `Remote_Camera_Object` | **not found** bare. Closest bare class: `OEI::Game_Component::Component_Remote_Camera`. | Host `0x7f74d4`; Ghost `0x7f72a4`; `Component_Remote_Camera` `0x7b8434` |
| `Companion` | **not found** bare. AI-state classes exist: `OEI::AI::State::Package::Follower::Companion_Basic`, `…Plan_Level::Follower::Companion_Follow`, `…Companion_Command`, `…Companion_Command_Hint`. | `0x7f88b4`, `0x7fa46c`, `0x7fa4a8`, `0x7fa4d8` |
| `Creature` | **not found** bare. Host/Ghost for `Creature_Object_Net_Data`; `OEI::Game_Component::Component_RPG_Creature`. | Host `0x7f73a0`; Ghost `0x7f7170`; `Component_RPG_Creature` `0x7a19e4` |
| `Actor` | **not found** bare (the string `Actor` is text). `OEI::Game_Component::Component_Actor`. | `0x7a0474` |
| `Game_State` | `OEI::Game::State::Game_State`, no bases. Host/Ghost for `Game_State_Object_Net_Data`. | `0x7b9a3c`; Host `0x7f74b8`; Ghost `0x7f7288` |
| `Level_State` | **not found** bare. `OEI::Game_Component::Component_Level_State`. Host/Ghost for `Level_State_Object_Net_Data`. | `0x7b8314`; Host `0x7f750c`; Ghost `0x7f72dc` |
| `Component_Network` | `OEI::Game_Component::Component_Network`, base `OEI::Game_Component::Component`. | `0x7b2f3c` (COL `0x890edc`) |
| `Host_Object_Controller<…>` | 20 instances (table below). | see table |
| `Ghost_Object_Controller<…>` | 20 instances (table below). | see table |
| `Session_Func` | `OEI::Net::Session_Func`, no bases. | `0x7d6720` (COL `0x89407c`) |
| `Client_Node` | `OEI::Net::Client_Node`, bases `OEI::Net::Game_Node`, `OEI::Net::Node_Process_Func`, `OEI::Net::Node_Receive_Func`. **Three** vtables (multiple inheritance, this-offsets 0 / 304 / 308). | `0x7bb448` (this 0), `0x7bb488` (this 304), `0x7bb47c` (this 308) |
| `NetConnection` | **not found** as a class. Only text strings. | — |

- Host/Ghost controllers (vtable RVAs, the Net-data argument type in brackets):

| Argument | Host vtable | Ghost vtable |
|---|---|---|
| Creature_Object_Net_Data | `0x7f73a0` | `0x7f7170` |
| Destructible_Object_Net_Data | `0x7f742c` | `0x7f71fc` |
| Door_Object_Net_Data | `0x7f73f4` | `0x7f71c4` |
| Game_State_Object_Net_Data | `0x7f74b8` | `0x7f7288` |
| Hazard_Object_Net_Data | `0x7f7480` | `0x7f7250` |
| Inventory_Object_Net_Data | `0x7f7528` | `0x7f72f8` |
| Item_Object_Net_Data | `0x7f7448` | `0x7f7218` |
| Level_State_Object_Net_Data | `0x7f750c` | `0x7f72dc` |
| Openable_Object_Net_Data | `0x7f7410` | `0x7f71e0` |
| Path_Blocker_Object_Net_Data | `0x7f749c` | `0x7f726c` |
| Player_Equipment_Object_Net_Data | `0x7f7368` | `0x7f7138` |
| Player_Object_Net_Data | `0x7f7330` | `0x7f7100` |
| Player_Secondary_Equipment_Object_Net_Data | `0x7f7384` | `0x7f7154` |
| Remote_Camera_Object_Net_Data | `0x7f74d4` | `0x7f72a4` |
| Secondary_Inventory_Object_Net_Data | `0x7f7544` | `0x7f7314` |
| Secondary_Player_Object_Net_Data | `0x7f734c` | `0x7f711c` |
| Sound_Effect_Object_Net_Data | `0x7f7464` | `0x7f7234` |
| Test_Conversation_Object_Object_Net_Data | `0x7f73bc` | `0x7f718c` |
| Test_Generic_CSA_Object_Net_Data | `0x7f73d8` | `0x7f71a8` |
| Visual_Effect_Object_Net_Data | `0x7f74f0` | `0x7f72c0` |

- Component hierarchy (RTTI base lists): `OEI::Game_Component::Component` has vtable `0x79dd04`.
  137 `OEI::Game_Component::*` classes exist, all with a vtable. All 80 plain `Component_*` classes
  (for example Component_Player, Component_RPG_Player, Component_Actor, Component_Party_Member) list
  `Component` as their only direct base. The other 57 (`DS_Component_*`, `NY_*` variants, and
  `Component`, `Sub_Component_Combat_Cinematic`, `LevelObjectClipYChangedFunc`, `UpdateRigFramesFunc`)
  were not checked for base lists. Vtable RVAs are in `tools/rtti_index.json`
  (for example Component_Player `0x7af064`, Component_RPG_Player `0x7a1c0c`,
  Component_Party_Member `0x7b89ec`, Component_Game_State `0x7ae86c`).
- Reading: `NY_`, `DS_` (Dungeon Siege III) and plain `Component_*` classes sit side by side.

## Derived (concluded from evidence, with the evidence named)

- The index is internally consistent: every accepted COL is confirmed by its class hierarchy
  descriptor and by entry 0 of its base array, and every vtable's first slot points into an
  executable section. Evidence: the tool's own checks, the 937/937 byte count. Confidence: high for
  the RTTI structure; the executable-section check is not the same as proof that a slot is a method.
- The 59 type-descriptor-only classes are all bases of other classes (non-polymorphic bases), so the
  missing vtables are not parser misses. Evidence: the base-list cross-check. Confidence: high.
- The replicated per-type record names (`Player_Object_Net_Data`, `Secondary_Player_Object_Net_Data`,
  `Remote_Camera_Object_Net_Data`, `Creature_Object_Net_Data`, `Level_State_Object_Net_Data`,
  `Game_State_Object_Net_Data`, `Item_Object_Net_Data`, `Door_Object_Net_Data`) exist as template
  arguments of the Host/Ghost controllers. They have no standalone RTTI type. Evidence: the byte-level
  search and the table above. Confidence: high for "no standalone RTTI"; the record role is inferred
  from the names only.
- The bare focus names `Player_Object`, `Secondary_Player_Object`, `Remote_Camera_Object`, `Creature`,
  `Companion`, `Actor`, `Level_State`, `NetConnection` have no RTTI type name in this build. This
  contradicts the "RTTI for Player_Object, Secondary_Player_Object, Remote_Camera_Object, …" line in
  `prompts/04-object-model.md` and in the `NOTES.md` "Still unknown" section, which should be read as
  "the replication records for these exist", not "these classes exist". Confidence: high for the
  string search.
- Name-based candidates for the controllable body (not verified, RTTI names only):
  `OEI::Game_Component::Component_Player` `0x7af064`, `Component_RPG_Player` `0x7a1c0c`,
  `Component_Party_Member` `0x7b89ec` (companion/party role), `Component_Remote_Camera` `0x7b8434`
  (the camera-owner role). Confidence: low, names only.
- `Companion` is present in the AI state tree (`OEI::AI::State::…Follower…`), i.e. a follower
  behaviour, not a body class in RTTI. Confidence: medium, names only.
- Multiple inheritance is real: `Client_Node` and 29 other classes have more than one vtable (30
  classes in total). Confidence: high (observed COL this-offsets).

## Unknown

- Field layouts and offsets of any class: the static index gives none. Runtime work must find them.
- Whether the vtable slots are the methods of the named class: the first slot is only checked for
  pointing into `.text`. Method content is not read, because `.text` is encrypted.
- Whether a live object of any of these classes exists in a given state: static RTTI says nothing.
- 44 names are not fully demangled (template integer arguments such as `$0CF@`, and MSVC
  back-references such as `2`/`3`). They are stored with their mangled name as the key. None is a
  focus class. Writing a back-reference-aware demangler would be needed if one of them matters.
- The `instance::NetConnection::…` strings are not an RTTI class; what they belong to is unknown.
- Whether the test copy and the retail install have identical RTTI: only the test copy was read.
- Whether the Net-data records (and so the Host/Ghost controllers) are used at all without a live
  session. Gate 0 must answer that.

## Deliverable

- Files (this worktree, branch `game/south-park-the-stick-of-truth-step04-objects`):
  - `games/south-park-the-stick-of-truth/tools/rtti_index.py` — the static parser (tool name differs
    from the `PARALLEL.md` plan of `tools/object_model.py`, on the parent's instruction).
  - `games/south-park-the-stick-of-truth/tools/rtti_index.json` — output: 937 classes, sha256 pinned
    exe, sections, `counts`, `focus`, `classes` (each with `evidence`, `vtables`, `col_rvas`,
    `base_classes`).
  - `games/south-park-the-stick-of-truth/notes/04-object-model.md` — this file.
- Reproduce: `cd games/south-park-the-stick-of-truth` then
  `py -3.14 -B tools\rtti_index.py --pylibs <folder with pefile.py>` (or set `SP_PYLIBS`), and
  `py -3.14 -B tools\rtti_index.py --self-test`. `--exe` picks another copy, but it must still match
  the pinned SHA-256.
- Not done (`MODEL.md` and `tools/object_model.py` are the runtime half, not written here):
  no `MODEL.md`, no live object finder, no player list, no position/health reading. No launch.
- Commands that ran (all on this run): the two `py -3.14 -B tools\rtti_index.py …` runs above, the
  self-test, the refusal check, two reproducibility runs, the byte-level count, the bare-name raw search
  and the component/vtable queries. Nothing else was executed.

## Handoff notes for the integrator

- `README.md` / `NOTES.md`: "Static RTTI index of the test copy (step 4, 2026-11-15): 937 type
  descriptors, 929 verified x86 COLs, 929 vtables, 878 classes with a vtable. Tool
  `games/south-park-the-stick-of-truth/tools/rtti_index.py`, output `tools/rtti_index.json`, exe
  hash-checked. RTTI only; no runtime layout claimed."
- `NOTES.md` / `prompts/04-object-model.md` correction: "The exe contains Onyx RTTI for `Player_Object`,
  `Secondary_Player_Object`, `Remote_Camera_Object`, `Companion`, `Creature`, `Item`, `Door`,
  `Level_State`, `Game_State`" is only half true. The Host/Ghost replication records exist for all of
  them as template arguments; the bare classes are not RTTI types, except `Game_State` and
  `Component_*`. Propose: "RTTI has `Host_Object_Controller<X_Object_Net_Data>` and
  `Ghost_Object_Controller<X_Object_Net_Data>` for X in 20 types including `Player`, `Secondary_Player`,
  `Remote_Camera`, `Creature`, `Level_State`, `Game_State`, `Item`, `Door`; the bare classes are not present."
- `PLAN.md` gate 3 (static half): done with the RTTI index. Runtime half still needed (see below).
- `NOTES.md` 2026-11-15 "Still unknown": "whether a second `Player_Object` can exist" stays open; the
  RTTI result is only that a player-replication record exists, not a second class instance.
- Candidate names for the body/camera role to check at runtime (names only, not verified):
  `Component_Player` `0x7af064`, `Component_RPG_Player` `0x7a1c0c`, `Component_Remote_Camera` `0x7b8434`,
  `Component_Party_Member` `0x7b89ec`.
- Runtime step (needs the launch lock and a guarded launch; not done here): hold the lock, launch the
  isolated test copy through the guard, locate the module base at runtime (do not assume `0x400000`),
  then scan committed private read/write memory for dwords equal to `module base + vtable RVA` for the
  candidates above. Validate hits by reading them, not by name: a real player object should change
  its three position floats when the character walks; a hit that stays still is not the player. Count
  the live instances of each class, which answers the player-list question. Write nothing to the process
  until the read-only model is stable. Record addresses as module+offset only.
- Tool gaps to note in the write-up: 44 names kept mangled (none is a focus class); `near_matches`
  in `focus` are capped at 30 and alphabetical, so use `classes` for a full search.

## Runtime half (appended 2026-11-15, repository-hygiene pass)

The runtime half exists as `tools/object_model.py`. Its scan runs in a Clyde's Fort save and found
`DS_Component_Player` at three stable addresses. The position triple at offset `0x5c` turned out to
be a cached copy: writing to it changed the value but did not move the character.

No runtime claim beyond those lines is made here; the static sections above are unchanged.
