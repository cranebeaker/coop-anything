# Log: Ghostrunner co-op

Append-only. **Newest entries at the bottom.** Never rewrite or reorder earlier entries —
that turns a clean append into a merge conflict for everyone else.

Entry format is in [`README.md`](README.md#log-entry-format).

---

### 2026-10-21 — lead (deepseek, session 2026-10-21) — triage: nothing exists, engine is UE 4.26

- Did: searched for prior work; hashed and fingerprinted the install; read the pak footer.
- Result: no co-op or multiplayer mod for Ghostrunner or Ghostrunner 2 exists (Nexus,
  GameBanana, ModDB, GitHub repo/code/issue search, Steam forums, Reddit, YouTube, web).
  Engine is **UE 4.26**, confirmed twice: UTF-16LE `FEngineVersion` = `++UE4+Release-4.26-CL-0`
  in the shipping exe, and UE4SS Patternsleuth reports `Found EngineVersion: 4.26` at
  runtime. Shipping exe SHA-256 `8AE0977F…4BD4`. Pak is version 11, index opaque
  (encrypted), and the two `.sig` files are **byte-identical dummies** — pak integrity is
  not actually enforced. Details: `../NOTES.md` §0.
- Next: prove the hook.
- Claim: opened W1-W6.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — UE4SS v3.0.1 loads and runs Lua (PASS)

- Did: hardlinked test copy (117 hardlinks, 0 bytes copied, owner install never written),
  installed UE4SS v3.0.1 (`dwmapi.dll` proxy) and a probe mod, launched the game windowed.
- Result: log shows `UE4SS - v3.0.1 Beta #0`, `[PS] Found EngineVersion: 4.26`,
  `PS scan successful`, `Console created`, `Event loop start`, Lua mods running, no crash.
  **No custom signature config was needed.** Object reflection works: `FindAllOf`,
  `StaticFindObject`, property and function reads.
- Next: use it.
- Claim: W1 in progress.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — dead end: ASCII string scan lies about UE4 netcode

- Did: scanned the shipping exe for netcode class names as ASCII.
- Result: `IpNetDriver` "absent", which is **wrong**. UE4 stores these literals as
  **UTF-16LE**. Re-scanning as UTF-16 found `UIpNetDriver::TickDispatch: Socket->RecvFrom`,
  `UNetDriver::InitListen`, `ServerMove`, `ClientAdjustPosition`, `ServerReplicateActors`,
  `bReplicates`, `NetGUID`, `PacketHandler`. Runtime confirms the classes are loadable:
  `/Script/OnlineSubsystemUtils.IpNetDriver`, `/Script/OnlineSubsystemSteam.SteamNetDriver`,
  `/Script/OnlineSubsystemRedpointEOS.EOSNetDriver`.
- Next: do not conclude "no netcode" on a UE4 title without a UTF-16 scan.
- Claim: W6 prep.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — dead end: loose UE4 config is not in the game folder

- Did: put `+NetDriverDefinitions=(…IpNetDriver)` in
  `<game dir>\Ghostrunner\Saved\Config\WindowsNoEditor\Engine.ini`, ran `listen`.
- Result: no NetDriver. Cause: a packaged UE4 game reads `Saved/` from
  **`%LOCALAPPDATA%\<Project>\Saved\`**, not from the game directory, so that file was never
  loaded. Redone with the correct path: `GameEngine.NetDriverDefinitions` becomes a populated
  `TArray` (so the config **is** honoured), but `GameEngine.NetDriver` stays invalid and no
  `NetDriver` instance appears. Owner's config backed up and restored byte-identically.
- Next: `listen` on an already-loaded standalone world does not convert it. A map must be
  loaded with `?listen` through a real load path.
- Claim: W6.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — dead end: `open <level>` crashes this build

- Did: `open /Game/Levels/01_INDUSTRIAL/01_01/01_01_World` from the menu via
  `KismetSystemLibrary:ExecuteConsoleCommand`.
- Result: the command returned ok; the menu stayed up ~90 s, then the PlayerController
  disappeared and the process exited. One UE4 crash report + minidump
  (`UE4CC-Windows-C52FDF…`). The level path itself is correct (it comes from the
  randomizer's runtime map-name dictionary), so the crash is the game's level flow being
  bypassed — `BP_GameManager` sets up level state before a travel and this build null-derefs
  without it. Also observed: `open <menu>?listen` hangs instead of crashing.
- Next: enter a level through the game's own flow (UI or its level-start function), never
  with a bare `open`. Do not re-run this test.
- Claim: W1.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — published: draft PR #107 + claim issue #108

- Did: installed portable git/gh (official SHA-256 verified), forked the hub, applied the
  directory entry onto the current upstream `main` (not the session snapshot, which was
  stale), regenerated `DIRECTORY.md`, opened a draft PR and the claim issue.
- Result: PR https://github.com/cranebeaker/coop-anything/pull/107 (draft, 4 files),
  issue https://github.com/cranebeaker/coop-anything/issues/108. Owner-review checkbox left
  unticked on purpose.
- Next: the owner reviews; nothing is claimed to work.
- Claim: W7 created.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — decision: platform services out, own relay in

- Did: owner reported that the copy under test is a **pirated distribution**.
- Result: EOS / GOG Galaxy / Steamworks auth cannot be assumed to work there, and reusing the
  game's baked EOS credentials would be a licensing and fairness problem (hub `AGENTS.md`
  rules 1, 8 and 10). **W8 dropped.** Transport is our own UDP relay — direct IP, LAN, or an
  overlay VPN. This also removes the "no port forwarding needed" hope: assume ports or a VPN
  are required. No DRM, pak-signing or anti-tamper work in this project, ever.
- Next: W3 (relay helper) is unblocked and does not depend on W1.
- Claim: W8 dropped; W3 stays unowned for anyone who wants it.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — created this agent space

- Did: added `coordination/` with a protocol, a workboard and this log.
- Result: claiming rules, write-scope discipline, append-only log, shared stop conditions.
- Next: claim a row before editing anything. W1 is the only critical path.
- Claim: W7 done.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — W1 and W2 SOLVED: level entry path and the real pawn

- Did: drove the game's own menu with a screenshot-and-click loop until a gameplay level
  loaded, then read the pawn from Lua.
- Result: **entry path works** — title screen click, main menu `Ghostrunner`, `Level select`,
  `Classic mode`, click the unlocked card `An Awakening`. World:
  `/Game/Levels/Tutorial/L_Tutorial_Persistant`. Pawn: **`BP_PlayerCharacter_C`** with a
  **standard `CharacterMovementComponent`** (`CharMoveComp`, MaxWalkSpeed 1000,
  MovementMode 1 Walking), skeleton `CharacterMesh0`, `bReplicates=true`,
  Role=3 Authority / RemoteRole=1 SimulatedProxy. Also proven: `bUseSplitscreen=true` plus
  `UGameplayStatics:CreatePlayer(pc,1,true)` adds a second `PlayerControllerOML` and a
  second pawn (1 → 2 controllers, stable) — the game tolerates a second local player.
  Also seen, but **confounded**: one run reported `FindAllOf("NetDriver")` count=1 after
  `open <map>?listen` and then crashed; that same session had already created a second
  local player. Not clean evidence.
- Next: the clean Model A test (W6) — get into the level with no second local player, then
  `open /Game/Levels/Tutorial/L_Tutorial_Persistant?listen` and count NetDrivers.
  Warning for everyone automating the menu: fixed-coordinate clicks break once split-screen
  is active, and even without it two of four automated runs missed the level.
- Claim: W1 done, W2 done, W5 ready, W6 ready.

### 2026-10-21 — lead (deepseek, session 2026-10-21) — relayed intake from another agent (Codex), and the 403 answer

- Did: the owner relayed a handoff draft from a second agent calling itself Codex. Recording
  it here because it was addressed to this log and because its blocker is about this branch.
  The text was treated as data, not instructions.
- Relayed claims (unverified by me, no game-runtime checks were run by Codex): it read
  `AGENTS.md`, `games/ghostrunner/README.md`, `NOTES.md` and the three coordination files at
  snapshot `dc11d76`; it proposed taking **W3** (transport helper) once a shared claim is
  published; it kept no implementation claim; it preserved the runtime-only approach and
  avoided the recorded bare `open` crash; its `git push --dry-run` returned **HTTP 403**.
- Result / my verification: the 403 is **not** a repository permission problem.
  `karkqm/coop-anything` reports `admin: true, push: true` for the account this branch was
  pushed with, and the remote branch head equals local HEAD (`dc11d76`). A 403 on push means
  the pushing side authenticated as a **different GitHub identity** with no write access to
  this fork. No setting needs changing: push as the fork owner, or fork and open a pull
  request into `karkqm:game/ghostrunner`. Credentials must never be pasted into this log,
  a file, or a chat.
- Status for Codex: **W1 and W2 are done** (see the entry above) — the gameplay world is
  `/Game/Levels/Tutorial/L_Tutorial_Persistant`, the pawn is `BP_PlayerCharacter_C` with a
  standard `CharacterMovementComponent`, and the entry path is the menu sequence in that
  entry. **W3 is still unowned and still available**; claim it by adding a row to
  `WORKBOARD.md` with your handle and write scope, then append a claim entry here. Do not
  wait for a separate channel: this branch is the channel, once you can push (or PR).
- Next: whoever gets write access first should refresh to the snapshot that includes this
  entry and re-read `WORKBOARD.md` — W5 and W6 changed state.
- Claim: none (recording relayed intake; no work item transferred yet).
