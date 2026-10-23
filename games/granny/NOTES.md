# Granny (2018, DVloper) — co-op notes

Owner's target: play the installed Steam copy of **Granny** in co-op with friends.
Status: **working prototype, v0.4.7 (2026-10-23).** Player, Granny, door and item state
sync between two instances on one PC; doors verified at 0 mismatches in 13 paired samples
with one flipper (§23). Open: the item have-flag self-reverts in the game itself, the
catch sequence is not synced, no scene follow, no real-network run yet. §1–§22 are the
chronological log; §23 is the current state. The real install and saves are never
touched.

## 1. Game identity (verified 2026-10-22)

| | |
|---|---|
| Title | Granny |
| Developer / publisher | DVloper |
| Store | Steam, app **962400** |
| Installed build | **buildid 21006637**, `LastUpdated 1764608173` (2025-12-01), size 1,690,565,582 bytes, `StateFlags 4` (fully installed) |
| Branch | public/default (no `branches` key in `appmanifest_962400.acf`) |
| Engine | **Unity 6000.0.58f2** (`92dee566b325`), **IL2CPP**, **global-metadata version 31** |
| Graphics API | D3D12 (`D3D12/` next to the exe, `D3D12Core.dll`) |
| Scripting | no Mono assemblies; `Granny_Data/il2cpp_data/` only |

Hashes of the installed files (SHA-256):

```
Granny.exe         4871133ADA7F623E0CA3128459C99FAF6414F5C0177E9EAB4DBE80EACF2A3E5F
GameAssembly.dll   E260D044CBAAD538DCCC6767E7E4344CC3ABEEBE7C56723ACF1672BA5DD7AF9A
UnityPlayer.dll    78EC69DAB5574F216863EEC417C8DC3D7AC1916E6922B2B6658EFE78FE185456
global-metadata.dat AC9AAE26835E2C949A0A10BD9D385F164C4997DBB0D48653B9A716F7BADCB6CB
```

`Granny.exe` file version `6000.0.58.9625317`; `globalgamemanagers` carries
`6000.0.58f2`, `DVloper`, `Granny PC version`, `dvloperdennisvukanovic`;
`boot.config` build-guid `d4c29881763043abb1d4f3bcfb1eaa5d`; `app.info` = `DVloper` / `Granny`.

Levels present: `level0`..`level4` (5 scenes). Largest asset bundle
`sharedassets3.assets.resS` = 1,301,104,368 bytes.

## 2. Triage answers

1. **Exact game** — Granny (2018), Steam 962400, PC build 21006637, Unity 6 IL2CPP. Not
   the mobile build, not "Granny: Escape Together" (see §3).
2. **Already done?** — No co-op mod for any Granny PC build was found. See §3 for the
   three community projects that *do* exist and why none of them is usable as-is.
3. **Engine** — Unity (IL2CPP). The supported route is a Harmony mod through a Unity mod
   loader (MelonLoader 0.7.3) plus a C# networking library.
4. **Source available** — No source and no decompilation of *this* build. IL2CPP metadata
   gives us full class/field/method names (§5); method bodies need a runtime dump. All of
   the game logic is managed C# compiled to IL2CPP, so naming is intact — this is the
   friendliest possible "closed binary" case for Unity.
5. **Engine multiplayer?** **No netcode**, but **Steamworks.NET is present.** There is no
   Photon/Mirror/Fish-Net/LiteNetLib and no LAN or split-screen mode, so co-op still means
   writing the sync layer from zero: **host-authoritative state sync** (the game is not
   lockstep-friendly — see §6). Steam is a separate story: `ScriptingAssemblies.json`
   lists only two game assemblies (`Assembly-CSharp-firstpass.dll`, `Assembly-CSharp.dll`,
   108 including Unity modules), the whole `Steamworks.*` API is compiled into them, and
   `Granny_Data/Plugins/x86_64/steam_api64.dll` (249,120 bytes) ships with the game.
   `SteamManager` (the standard Steamworks.NET singleton: `s_instance`, `m_bInitialized`,
   `SteamAPIDebugTextHook`) and DVloper's `SteamScript` are in this build. So the process
   already talks to Steam — see §5.1 and §7 for what that does and does not buy us.
6. **Shipping check** — no anti-cheat and no game-specific online service to stay away
   from. The mod can ship as a DLL + loader; game files never need to be redistributed.
   Steam is running in-process as app **962400**, which constrains the connection model
   (§7) but does not block direct-IP play.

## 3. Research log — what exists (and why it doesn't solve this)

| Project | What it actually is | Verdict |
|---|---|---|
| [`FedoraV3/GrannyTAS-MelonLoader-VibeCoded`](https://github.com/FedoraV3/GrannyTAS-MelonLoader-VibeCoded) | MelonLoader v0.7.3 TAS/automation mod, docs target **"Granny Legacy 1.8.9"**. Documents `MobileFPS`, `AI_Granny`, `AI_Grandpa`, `PlayerStatus`, `PickRay`, `SeedManager`, `KeyBindManage` | Great tooling reference (loader version, build setup, deterministic input injection, SyncProbe design). **Type names do not exist in our build** — see §4 |
| [`GDmio/granny-legacy-menu`](https://github.com/GDmio/granny-legacy-menu) (MIT) | C++ trainer, MinHook on `MobileFPS.Update`, `PickRay.Update`, `AI_Granny.SensorPlayer`, `PlayerStatus.Death`; Unity 2022.3, D3D11 | Same "legacy" build, C++/D3D11, not co-op. Hook list is a useful map of *what* to sync, not *where* |
| [`lucaandri007/granny-multiplayer`](https://github.com/lucaandri007/granny-multiplayer) | One Harmony postfix that logs `MobileFPS.transform.position`. No networking at all | Not a starting point |
| [`an666667/granny-coop`](https://github.com/an666667/granny-coop) (GPL-3.0) | A **separate Unity 2020.2 fan game** ("HorrorEyes") with a WebSocket relay at `http://186.241.123.217:8092` | Different game, not code we can run. Design reference only, and untrusted |
| [`CreepyToad/Granny: Escape Together`](https://store.steampowered.com/app/3070520/) (Steam 3070520) | **Different game by a different developer**, released 2024-12-06, ₽385, has Online Co-op / Online PvP | Not this game. Worth mentioning to the owner as the "buy a co-op horror game" option |

Searches run: GitHub repo/code search for `granny coop`, `granny multiplayer`,
`granny melonloader`, `GrannyTAS`; the coop-anything `DIRECTORY.md` and `data/directory.yaml`
(no Granny entry at all); Steam store API for 962400 and 3070520. ModDB/Nexus/Reddit were
not reachable from this environment (bot-blocked) — **not** a proof that nothing exists
there.

## 4. Contradiction (AGENTS.md rule 6)

Every published Granny mod targets a build whose code is organised differently from the
installed one. Cross-checked against the metadata of build 21006637:

| Name in existing mods | Occurrences as a type in this build |
|---|---|
| `MobileFPS` | **0** |
| `AI_Granny`, `AI_Grandpa` | **0** |
| `PlayerStatus`, `PickRay` | **0** |
| `SeedManager`, `HandlePuzzles`, `EnemyController` | **0** |
| `GameInputManager`, `KeyBindManage` | **0** |
| `MainMenu`, `FirstPersonController`, `CharacterControl`, `DoorRay` | **0** |

So the write-ups cannot be followed verbatim. Two plausible readings, both unresolved:

- the installed build is a newer Unity 6 port of Granny and the mods target an older
  Steam build (possibly an opt-in "legacy" branch), or
- the mods target a different distribution entirely.

**Consequence for us:** the game logic classes we must sync have different names. They are
not missing — §5 lists the equivalents — but no existing method-level documentation applies.
Rule 6 says stop and tell the owner, which is done in §8. It does not block building; it
blocks *copying signatures* from the TAS project.

## 5. What this build actually contains (parsed from `global-metadata.dat` v31)

Parsed locally: 10,553 type-definition records (stride 88), 9,448 unique type names, and
73,114 strings from the v31 string heap. Gameplay-relevant types confirmed present:

- **Player:** `FPSControllerNEW` (the only FPS controller class in this build),
  `InventoryController`, `PlayerHealthOwn`, `PlayerFreeze`, `playerCaught`,
  `flashlightController`, `lanternController`, `PlayerWindowController`
- **Enemies:** `EnemyAIGranny`, `EnemyEye`, `MomSpiderHead`, `MomSpiderEyes`,
  `AIfollow`, `AIState`, `AIUpdate`, `NPCMovement`
- **Perception / sound:** `GrannyHear`, `GrannyCloseSensor`, `grannySeePlayerTrigger`,
  `LastPlayerSighting`, `GrannySounds`, `GrannyFootstep`, `GrannyFreeze`, `GrannyBreak`
- **Doors / world:** `OpenCloseDoors`, `OpenCloseDoorV`, `OpenCloseDoorVind`,
  `OpenCloseKitchenDoor1/2`, `OpenCloseOldDoor`, `OpenCloseLeftOldDoor`,
  `OpenCloseSmallDoors`, `OpenCloseBackdoor`, `openCloseVitrindoor*`,
  `openSecretDoorTrigger`, `checkIronDoors`, `lockerdoorscript`, `IfgarderobDoorOpens`
- **Items / traps:** `Item`, `ItemType`, `PickUp`, `PickUpNew`, `BearTrap`,
  `BeartrapTrigger`, `organicBeartrap`, `freezeTrap`, `CoffinsController`,
  `PackageDisappearController`
- **Hiding / scripted:** `hidingSpotTrigger`, `playerCaughtUnderBed`,
  `escapeSpiderCellarTrigger`, `plaskTriggerSpiderCellar`, `startMomSpiderTrigger*`,
  `spiderHuntPlayer`, `spiderHitPlayer`, `LittleSantaController`
- **Flow / persistence:** `BetweenScenesValues`, `GameOverScene4`, `gameOverScenes`,
  `SceneCode`, `SceneFlag`, `ControllerScene`

Absent: any netcode — no `Unity.Netcode`/`Mirror`/`Photon` types, no `Socket`/`TcpClient`
use, nothing named `Network*` in the gameplay namespace. Also absent: any save-system class
with an obvious name (the save format is still unknown, §7).

**Not absent, corrected:** Steamworks is in this build — the `Steamworks.*` API is compiled
into `Assembly-CSharp`, `steam_api64.dll` ships in `Granny_Data/Plugins/x86_64/`, and
`SteamManager`/`SteamScript` exist (§2 answers 5–6, §5.1). It is used only to initialise
Steam as app 962400; no gameplay code calls it.

### 5.1 Method-level map (parsed statically, verified 2026-10-22)

Parser: `local/granny-dump-methods.ps1`, output `local/granny-methods.txt`. Record layouts
for metadata v31 were derived from this file and are now confirmed against known code:

| Record | Stride | Layout used |
|---|---|---|
| TypeDefinition | 88 B | name@0, namespace@4, fieldStart@32, methodStart@36, method_count@64, field_count@68 |
| FieldDefinition | 12 B | name@0, typeIndex@4, token@8 |
| MethodDefinition | 36 B | name@0, declaringType@4, returnType@8, token@24, flags@28, slot@32, **paramCount@34** |

Parameter counts are the check that the layout is right, on methods whose arity is known
from their signatures: `Item.Set` → 2, `SceneCode.EnableMotors` → 2,
`SceneCode.SetMotorState` → 1, every `OnTriggerEnter`/`OnTriggerStay`/`OnTriggerExit` → 1.
Type name → index resolution was also checked for name collisions: none of the classes
below exist twice, so each dump is the only definition of that name in the build.

**`EnemyAIGranny`** — 188 fields, 22 methods. It is the whole Granny AI, not a fragment:
`Start, FixedUpdate, grannyHitByPepper, pepperAnimDone, grannyHitByArrow, grannyHitByCar,
grannyHitByGun, grannyFreeze, dropBearTrap, OnTriggerStay, OnTriggerExit, GrannyDecisions,
facePlayer, facePlayerBed, Playercaught, followPlayer, newNav, disableHeadFollow,
resetLockedDoorSee, cleaning, disableOnlyHeadFollow, .ctor`.

Its fields are a ready-made replication schema for the hardest entity: `myTransform`,
`target`, `playerPos`, `nav1`..`nav16` (waypoint Transforms), `walkSpeed`,
`grannysFollowSpeed`, the state booleans (`seePlayer`, `GrannySearching`, `GrannySearch`,
`GrannyMoving`, `attackingPlayer`, `huntPlayer`, `grannyIsFollow`, `seePlayerTimer`,
`timerSee`, `timerSearch`, `safeTimer`), the stun flags (`hitByArrow`, `hitByGun`,
`hitByCar`, `hitByPepper`, `freeze`, `hitByPepperStart`, `blindTimer`,
`grannyEyeColorTimer`), the hiding flags (`playerHiding`, `playerHidingUnderBed`,
`playerHidingInCoffin`, `playerHidingInCoffinBackyard`, `playerHidingInCar`,
`hidingUnderBed1..3`, `hidingInCoffin4`, `hidingInCoffinBY`, `hidingInCar`), and the
day-5 spine props (`bedCam1..3`, `coffinHead1/2`, `carHead`).

**`FPSControllerNEW`** — 32 fields, 4 methods: `Start, OnEndGame, FixedUpdate, .ctor`.
Movement lives in `FixedUpdate`, and the fields to replicate per player are exactly:
`thisTransform`, `character` (CharacterController), `cameraPivot`, `cameraVelocity`,
`velocity`, `canJump`, `playerCrouch`, `PlayerIsGrounded`, `timeInAir`, `startingPitch`,
plus `day2`, `day3`.

**`InventoryController`** — 253 fields, 8 methods (`Update, CheckInventory,
placePlankaHole, NoText, textTimer1, NoObjectText, PlanktextTimer, .ctor`). Its fields are
one triple per item — `<item>` GameObject, `new<item>` Transform, `have<item>` bool — for
~60 items (`tb1..tb4`, `avbitare`, `hammare`, `vas`, `vas2`, `safeKey`, `exitKey`,
`hanglockKey`, `padlockCode`, `armborst`, `weaponKey`, `screwdriver`, `planka`, `battery`,
`playhouseKey`, `carKey`, `melon`, `teddy`, `kugg1/2`, `message`, `brunnsvev`,
`oldShotgun`, `gunDel1..3`, `topplock`, `carbattery`, `gascan`, `wrench`, `sparkplug`,
`meat`, `specialkey`, `book`, `pepperspray`, `remote`, `birdSeed`, `freezeTrap`,
`wheelCrank`, `rustyPadlockKey`, `woodenStick`, `spiderKey`, `chainCutter`, `deadRat`,
`christmasBall`, `christmasKulaBomb`). That is the shared-inventory sync surface, and it
is a single component per player — cheap to replicate wholesale.

**`OpenCloseDoors`** — 9 fields, 7 methods (`Start, Update, timerDooropen,
timerDoorclosed, timerDoorlocked, OnTriggerEnter, .ctor`). Doors are driven by
`OnTriggerEnter` (the player walks into the trigger) and animated by timers, so the state
to sync per door instance is small: `DoorOpen`, `DoorMoving`, `doorLocked`.

**Perception is split across separate sensor components**, not inside `EnemyAIGranny`:
`GrannyHear` (5 fields, `OnTriggerEnter`, `cooldownDuration`/`nextTriggerTime`),
`GrannyCloseSensor` (`OnTriggerEnter`), `grannySeePlayerTrigger` (`OnTriggerStay`,
`OnTriggerExit`), `LastPlayerSighting` (5 methods). This answers the §7 question: Granny's
*decisions* are in `EnemyAIGranny`, but *triggering* them from a second player needs the
sensor components to see a second player body as well.

**Day / level flow is data-heavy and randomised.** There is **no class named
`GameController`** anywhere in this build — the many `gameController` fields are typed as
`GameObject` (type#14446), i.e. looked up at runtime. Flow lives in three components:

- `startNewDay` — 140 fields, 11 methods (`Start, Update, FadeText1..6, FadeText5LD,
  newDay`). It randomises Granny's spawn (`RandomGrannyPos`, `GrannyStartPos1..4`),
  randomises object placement (`RandomPlaceObjects`, `OP1..OP5`, `RandomNRTavelbitar`,
  `tavelbit1..4`), rolls the day screen (`day1..day6` + `day5LastDay`) and owns
  `daysCounter`.
- `endDay` — 83 fields, 5 methods (`Start, Update, EndDay, beartrapDestroy`).
- `resetDay` — 33 fields, 2 methods (`resetDays`): resets doors `door1..door10`,
  `loda1..loda12`, `kitchenDoor1..6`, `vitrinDoor1..4`.
- `RandomKeyPlace` — 4 fields, 2 methods (`Start` only): picks `keyPlace1..3`.
- `startEndScene` — 28 fields, 6 methods (`startTheEnd1/2, readyToMainMenu`).

`BetweenScenesValues` carries only the three difficulty bools, so anything else that
survives a scene change is not in an obvious carrier type. **unknown.**

**Steam is linked.** `SteamManager` and `SteamScript` (Start only) exist, and the full
`Steamworks.*` surface is in the build with `steam_api64.dll` beside the exe. Consequence
to test, not yet a claim: the process initialises Steam as app **962400**, so a mod cannot
re-initialise the same process as Spacewar (480). Steam P2P/lobby use would therefore be
under 962400, and whether that app has usable Steam networking config is **unknown**
(§7). Direct IP does not care.

**Other confirmed shapes:** `playerCaught` (33 fields, `faceGranny`, `fallingDead`,
`playerExplode`, `faceSpider`, `faceMomSpider`), `PlayerFreeze` (`freezeON`, `freezeOFF`,
5 methods), `BearTrap` (`beartrapOn`, `beartrapShot`, `playerStuck`, `OnTriggerEnter`),
`freezeTrap` (`trapActivated`, `GrannyFreeze`), `PickUpNew` (`playerTaken`, `pickUp`),
`AIfollow` (104 fields, 13 methods — the Slendrina/maniac follower, same shape as Granny),
`MomSpiderHead` (51 fields, `momSpiderEatPlayer`, `OnTriggerStay`, `newNav`),
`BetweenScenesValues` (three `difficulty*` bools), `gameOverScenes`
(`GameOverScenes` int).

**Corrections this dump forced on earlier notes:**

- `PlayerHealthOwn` and `NPCMovement` exist as **empty classes** (0 fields, only `.ctor`).
  Player health is therefore not a self-contained component in this build — it must be
  read from `playerCaught`/`FPSControllerNEW` state or found at runtime. `unknown`.
- `ItemType` (#4028) is **not** the gameplay item enum: its ten fields are
  `EntityRef, CharEntity, SurrogateCharEntity, Whitespace, String, StringChars, Raw,
  RawChars, ValueString, value__`, i.e. an internal XML/text-tokenisation enum that
  happens to carry the name. Item identity is a `string` on `Item.data`, or the
  `have<item>` bools; `Item` itself is `type`/`data` plus `Set(2 params)`.
  **Derived**, not looked up: no gameplay field named `ItemType` was found.
- `UpdateGrannyPosition`, `grannystartposition` and `PlayerPosition` do **not** exist as
  types — drop them from any design that assumed per-frame position broadcast helpers.

Still needs a runtime dump: IL bodies (the static parse gives names, fields and arity
only), the live `Input` path, and the save format.

## 6. Sync surface — what "co-op" has to cover

The game has no determinism contract we can rely on (frame-rate-driven `Update` movement,
`FixedUpdate` AI, per-frame RNG), so lockstep is out. Design: **host-authoritative**, host
simulates everything, clients send input and receive state; entity ownership with epochs for
anything a client can directly drive.

Minimum viable 2-player sync, in dependency order:

1. **Session** — host/join, player slots, scene/level agreement, spawn.
1a. **World seeding** — `startNewDay` and `RandomKeyPlace` randomise Granny's start,
   key placement and object placement at scene start (`RandomGrannyPos`,
   `RandomPlaceObjects`, `OP1..OP5`, `keyPlace1..3`). If both peers roll their own, the
   key is in a different place on each machine and the session is unwinnable. Host rolls,
   clients adopt the host's result — either by replicating the chosen objects/positions or
   by forcing the same seed before those `Start` methods run. This belongs in the
   session handshake, not in the item sync layer.
2. **Players** — position/rotation (camera yaw+pitch), crouch, flashlight on/off, carried
   item, health/downed state, invisibility to Granny, hiding state.
3. **Granny AI** — position, rotation, state machine state, target, stun/freeze, hear/see
   events. Clone a second visual + collider for the remote player so `GrannyHear` /
   `grannySeePlayerTrigger` can perceive both.
4. **Other enemies** — `MomSpiderHead`/`Eyes`, grandpa-equivalents, eyes.
5. **Interactables** — doors open/closed/locked, locks, keys, item pickups and drops
   (shared inventory authority on host), traps armed/triggered, hiding spots occupied.
6. **Level flow** — scene transitions, day counter, win/lose (`GameOverScene4`,
   `gameOverScenes`), restart.
7. **Persistence** — whether a co-op save is needed at all, or session-only.
8. **Transport** — UDP over direct IP (a small C# library such as LiteNetLib), host = the
   peer that also runs the authoritative simulation. Steam P2P under app 962400 is a
   possible upgrade, not a v1 dependency (§8 item 5).

Non-goals for v1: >2 players, dedicated server, mid-session join, mod support, split-screen.

## 7. Open questions (need the runtime dump / a test copy)

- Which class is the *actual* active player controller: `FPSControllerNEW` is the only
  candidate in this build, and its `FixedUpdate` owns movement — confirm at runtime by
  logging instances. **Not yet confirmed.**
- Does the game use the legacy Input Manager or the new Input System? (`KeyBindManage` is
  absent, so input handling moved; `Input.GetKey` hooks may not cover everything.) The
  static dump cannot answer this; `FPSControllerNEW` has no input fields, so input is read
  somewhere else. **unknown.**
- Save system: file location and format, and whether co-op must touch it. The game is
  5 levels with a day counter, so saves matter. No save-system type was found by name in
  the static dump. **unknown.**
- Does `EnemyAIGranny` own all Granny behaviour, or is it split across `GrannyHear`,
  `GrannyCloseSensor`, `grannySeePlayerTrigger` and animation scripts? **Answered, §5.1:**
  decisions and movement are in `EnemyAIGranny`; perception input comes from the separate
  sensor components, which is what a second player body has to be visible to.
- Where is player health stored, given `PlayerHealthOwn` is an empty class (0 fields,
  1 method)? There is no `GameController` type in this build — the `gameController`
  *fields* on several classes are plain `GameObject` references, so the central manager is
  a scene object, not a named class. Needs the runtime dump. **unknown.**
- Where is the central game state (`daysCounter`, difficulty, which day the save is on)?
  `BetweenScenesValues` holds only the three difficulty bools and `daysCounter` is a field
  of `startNewDay`; no persistent manager type was found. How day state survives a scene
  load is **unknown** and matters for a multi-day co-op session.
- **Steam as a transport:** the process is already initialised as app 962400, so the
  Spacewar (480) trick cannot be used in-process. Whether 962400 has usable Steam P2P
  (NAT punchthrough) or Steam Datagram Relay config is **unknown** and can only be tested
  once a loader runs. Design assumption until then: direct IP / virtual LAN for v1, Steam
  lobbies only if the test passes. This is a change from the plan in §8 item 5.
- Is gameplay `Update`-driven on all clients identically, or do we need to drive remote
  entities by interpolation only? Movement is `FixedUpdate` (good — fixed step), Granny AI
  is `FixedUpdate` too, but `InventoryController`, doors and traps are `Update`-driven.
  Mixed; host authority plus interpolation is still the safe design. **Derived from method
  names only.**
- MelonLoader 0.7.3 (released 2026-05-14, Il2CppInterop 1.5.1-ci.845) against Unity
  6000.0.58f2 / metadata v31 — **verified to bootstrap and to run a Melon**; Harmony
  prefixes **verified to fire on native IL2CPP game methods**, see §9. That risk is closed.

## 8. Owner decisions (all six approved 2026-10-22; work started)

Approved, and what has actually been done: (1) install copied — yes, §9; (2) .NET SDK
installed — yes; (3) MelonLoader downloaded — yes; (4) build rather than buy — yes;
(5) direct IP / virtual LAN for v1, Steam P2P to be tested under the existing appid;
(6) 2 players for v1. Details of what each item meant:

1. **Copy the install** (~1.7 GB) into an isolated test folder, so the real install and
   saves are never touched (rule 2). The copy is required for MelonLoader, which writes
   loader files into the game directory.
2. **Install the .NET SDK.** Only the .NET 8 *runtime* is present; compiling a
   MelonLoader mod needs an SDK (`winget install Microsoft.DotNet.SDK.8`). This is a
   machine-wide install and needs admin.
3. **Download MelonLoader 0.7.3** — `MelonLoader.x64.zip`, 20,155,622 bytes, official
   GitHub release. Pinned by URL + SHA-256 recorded at download time. It is extracted into
   the *test copy*, not installed system-wide.
4. **Path decision:** build co-op for this build (the only way to play *this* Granny with
   friends), or buy `Granny: Escape Together` (a different game that already has online
   co-op). Recommendation: build — it is the only route to the game the owner asked for,
   and the engine situation is the friendly one (Unity + managed C#, names intact).
5. **Connection model** (later, but needed for design): **direct IP / virtual LAN
   (ZeroTier, Tailscale) is the v1 plan.** The original idea — Steam Spacewar (480) relay,
   "one click, no port forwarding" — is not available in this process: the game already
   calls `SteamAPI.Init()` as app **962400** through `SteamManager` before any mod code
   runs, and a process cannot be initialised as two apps. Steam P2P may still be usable
   *as* 962400 (the API works for any app that is entitled to call it), but whether lobbies
   and NAT punchthrough are enabled for that appid is **untested** — it needs a runtime
   check, not a static one. Plan: direct IP first, then test Steam P2P under 962400 and
   keep it only if it works.
6. **Player count** for v1: 2 is the realistic first target.

Realistic effort: this is a from-zero co-op mod for a game with no netcode. Phase 2–3
(loader running, runtime dump, player+Granny+door sync visible in two clients) is the
first honest milestone; the full escape-the-house loop with level transitions is a
multi-session project.

## 9. Workspace and loader (phase 2, verified 2026-10-22)

### Observations (things I actually ran and saw)

- **Test copy.** `robocopy` of the installed build into an isolated folder outside the
  user profile: 43 files, 1,690,565,582 bytes, 9 dirs, 0 failed — byte count matches the
  Steam manifest exactly. The real install (`<game dir>`) was read only.
- **Toolchain.** .NET SDK 8.0.426 installed; .NET Desktop Runtime 6.0.36 present (the
  MelonLoader 0.7.3 host is a net6 application). MelonLoader `version.dll` proxy is
  11,057,664 bytes.
- **Loader log, after the fix below** (`<test copy>\MelonLoader\Latest.log`, 10,808 bytes):
  `MelonLoader v0.7.3 Open-Beta`, `Game Type: Il2cpp`, `Game Arch: x64`,
  `Game Name: Granny`, `Game Developer: DVloper`, `Unity Version: 6000.0.58f2`,
  `Game Version: 1.3.4`.
- **Assembly generation finishes.** First launch: `Il2CppAssemblyGenerator` asked the
  RemoteAPI, got `Game Not Found` (no prebuilt assemblies for this game), and generated
  locally with `Cpp2IL 2022.1.0-pre-release.21`, `Il2CppInterop 1.5.1-ci.845`,
  `Unity Dependencies 6000.0.58` — log `Using actual IL2CPP Metadata version 31,1`,
  `Mapping pointers to Il2CppMethodDefinitions...Processed 71566 OK`,
  `Done. Total execution time: 10592ms`. Second launch: `Assembly Generation Successful!`,
  then `Loading Mods... 0 Mods loaded.`, `Support Module Loaded: ...Il2Cpp.dll`, and a
  warning `[Il2CppInterop] Class::Init signatures have been exhausted, using a substitute!`.
  The generated assemblies land in `<test copy>\MelonLoader\Il2CppAssemblies\` (103 DLLs,
  `Assembly-CSharp.dll` 2,897,776 bytes, `UnityEngine.CoreModule.dll` 3,320,320 bytes).
  Note the generated names are namespaced: `Il2Cpp.FPSControllerNEW`, not plain
  `FPSControllerNEW`. `Unity.InputSystem.dll` is among the generated assemblies, which
  makes the new Input System likely — **derived, not confirmed**.
- **A Melon runs.** Built a throwaway `net6.0` class library against
  `MelonLoader\net6\MelonLoader.dll`, `0Harmony.dll`, `Il2Cppmscorlib.dll`,
  `UnityEngine.CoreModule.dll`, `UnityEngine.UIModule.dll`, `UnityEngine.UI.dll` and
  `Assembly-CSharp.dll` from `Il2CppAssemblies`, `dotnet build -c Release` with
  .NET SDK 8.0.426 targeting `net6.0` (the ref pack restored from NuGet). Dropped the DLL
  into `<test copy>\Mods\`: the log shows `[GrannyCoopTest] TESTMELON init`, a heartbeat
  every 2 s on `OnUpdate`, and `UnityEngine.Time.frameCount` advancing (~580 fps in the
  main menu) — managed code calls into IL2CPP and gets real values.
- **Harmony prefixes fire on native IL2CPP game methods (verified 2026-10-23).** This
  closes the open risk in the previous version of this section. Patching
  `Il2Cpp.ClickPlayButton.TaskOnClick`, `Il2Cpp.ClickPlayButton.optionMenu` and the
  compiler-generated coroutine `Il2Cpp.ClickPlayButton+_optionMenu_d__9.MoveNext` with a
  logging prefix, then invoking the menu's Play button from managed code, produced
  `[CoopProbe] CALLED Il2Cpp.ClickPlayButton.TaskOnClick (n=1)` and the two `MoveNext`
  calls in the log, and the game's own native `TaskOnClick` ran (the menu hid). So
  Il2CppInterop's detour is hit by calls made from native game code, not only from
  managed wrappers. (Patching a Unity engine wrapper such as `Time.get_deltaTime` still
  sees nothing from the game — engine calls are native too.)
- **Direct launch needs `steam_appid.txt`.** Starting `Granny.exe` from the test copy
  without one makes the process exit after ~20 s and Steam relaunch it. A
  `<test copy>\steam_appid.txt` containing `962400` fixes it; the game then stays up and
  MelonLoader loads. This is a test-harness detail, not needed when launching through
  Steam.
- **Scene flow.** SplashScreen (`buildIndex 0`, 3 roots) → Menu (`buildIndex 1`, 17 roots:
  `GameController`, `Camera`, `OptionMenuParent`, two lights, `HouseBakedReady`,
  `GrannyParent`, `Halloween_Pumpkin`, `BearTrapOrganic`, `LittleSanta`, `BearTrap`,
  `Teddy`, `optionMenuBackground`, `MenuButtonsHolder`, `BlackscreenHolder`, two
  `EventSystem`s). **The Menu scene already contains the house and Granny** — gameplay
  may be started in place rather than by loading a `level*` scene.
- **Play path (observed).** `MenuButtonsHolder/PlayButton` →
  `ClickPlayButton.TaskOnClick` → `ClickPlayButton.optionMenu` (IEnumerator, type
  `Il2Cpp.ClickPlayButton+_optionMenu_d__9`). In the probe run the coroutine's `MoveNext`
  ran exactly twice (immediately, then ~1.5 s later) and then never again; `InGameVolume.Start`
  fired at the second step, but the scene stayed `Menu`, `FPSControllerNEW` count stayed 0
  (including inactive), and `Time.timeScale` stayed 1 for 60+ s. Why the coroutine ends
  without starting gameplay is **unknown** (difficulty selection? a Steam-gated path? a
  silently skipped step?) — next probe reads the iterator's `<>1__state` and logs the
  build-settings scene list plus `SceneManager.LoadScene*` calls.
- **Melon log buffering.** `<test copy>\MelonLoader\Logs\*.log` stays 0 bytes for the whole
  run — MelonLoader's file logger only flushes when the process exits. `Latest.log` may not
  exist at all. To read a run's log, close the game (or `Stop-Process` it) and *then* open
  the newest file. The Unity `Player.log` in
  `%USERPROFILE%\AppData\LocalLow\DVloper\Granny\` carries no Melon output.
- **Known-benign log noise.** `AccessTools.GetTypesFromAssembly` warnings about
  `ControlOptions`, `UxmlSerializedData`, `Seed4` etc. ("format is invalid") come from
  Harmony enumerating IL2CPP interop assemblies; they did not stop the Melon from loading
  or patching.
- **First launch downloaded tools by itself.** MelonLoader fetched Cpp2IL and the Unity
  dependencies from their official GitHub releases into
  `MelonLoader\Dependencies\Il2CppAssemblyGenerator` on first run. Nothing was piped into
  a shell; these are release assets placed by the loader, not scripts I ran.
- **Non-ASCII install path breaks MelonLoader.** With the copy under a path containing
  Cyrillic characters the loader logged `plthook_open error` into a 219-byte
  `MelonLoader.log` and never generated interop assemblies; the game itself started. The
  message text itself is mojibake in that log (a wide string written through a narrow
  logger) and could not be recovered from any decoding. Moving the identical files to an
  all-ASCII path fixed it on the next launch (log 219 B → 10,808 B). **Workaround for any
  Windows user whose profile name is non-ASCII: put the test copy somewhere like
  `C:\Users\Public\<game>` or `C:\Games\<game>`.**
- **The copy still writes to the shared per-user Unity data folder.** Running it creates
  `%USERPROFILE%\AppData\LocalLow\DVloper\Granny\Player.log` (and `Player-prev.log`).
  So a "copy" isolates game files, not Unity's persistent data. No save files were present
  there after these runs, but a co-op session will produce them and the real install would
  read the same folder — back it up before any test that saves.

### 9.1 Gameplay reached (probe v0.0.10 – v0.0.13, 2026-10-23)

Probe source: `local/testmelon/TestMelon.cs`. Runs read from
`<test copy>\MelonLoader\Logs\26-10-23_0-40-35.log` (v0.0.10, crash),
`26-10-23_0-47-27.log` (v0.0.11), `26-10-23_0-53-8.log` (v0.0.12),
`26-10-23_1-1-16.log` (v0.0.13, 110 s, 0 errors).

#### Observations (what the runs showed)

- **Build scene list** (logged by the probe from `SceneManager`):
  `#0 Assets/1_Scenes/SplashScreen.unity`, `#1 Menu.unity`, `#2 ForestScene.unity`,
  `#3 Scene.unity`, `#4 EndScene.unity`. Root counts: SplashScreen 3, Menu 17, Scene 99.
- **Real play path, in order (v0.0.13 log):**
  `SceneManager.LoadScene arg0=Menu` (01:01:40.221) → `ClickPlayButton.TaskOnClick` →
  `ClickPlayButton.optionMenu` → `_optionMenu_d__9.MoveNext` (01:01:42.03) →
  `ClickContinueButton.TaskOnClick` (01:01:42.215) → `SceneManager.LoadScene arg0=Scene`
  (01:01:42.216) → scene active 99 roots, `buildIndex=3` (01:01:45.689).
  The Continue click was issued by the probe; the Play click opened the difficulty/continue
  panel, which is why the previous run (without a Continue click) never changed scene.
- **The player controller is `FirstPersonController_Egen`**, namespace
  `UnityStandardAssets.Characters.FirstPerson` (not `Il2Cpp.FPSControllerNEW`, which does
  not exist in `Scene`). It lives under the `Player` root with `pushRigidbody`,
  `playerCaught`, `CharacterController`, `Rigidbody`, `Animation`.
- **`Scene` starts with `Player` inactive and `PlayerInBed` active**, and activates the
  player ≈19 s of Unity time after the load: v0.0.13 logged `Player root activated at
  frame=6076 time=35.4` (scene loaded at time≈16). From then on
  `player=True inBed=False`, and `FirstPersonController_Egen.Start/Update/FixedUpdate/
  ProgressStepCycle/UpdateCameraPosition` run every frame.
- **`startNewDay` lifecycle:** `Start` fires once, `Update` fires every frame,
  `FadeText1` fires twice (2.3 s apart); the `newDay` coroutine
  (`Il2Cpp.startNewDay+_newDay_d__149`) **never fires** in the 110 s run — so whatever
  activates `Player` is not `newDay`.
- **`startNewDay` field map** (probe dump): `fadeBlackSpeed`, `fadeTextSpeed`,
  `blackScreenTexture` (= `BlackScreenStart` Image), `bloodscreenImage`, `bloodscreen`,
  `bloodOnBed`, `bloodscreenDay4`, `day1` (= `Day1Text` Image), `day1Text`, …
- **`GameController` components:** `SetTargetFramerate`, `setGameQuality`, `startNewDay`,
  `furnitureControlls`, `endDay`, `playerInBeartrap`, `CheckExitDoor`,
  `playerCaughtUnderBed`, `resetDay`; children `LanternController`, `FlashlightController`.
  `startNewDay` is active=True from the start.
- **The black-screen root is named** `BlackScreenStartParent                 ENABLE INNAN LIVE`
  (padded with spaces). It holds `BlackScreenStartHolder` (Canvas) with `BlackScreenStart`
  (Image), `GameOverText`, `TheEndText`, `MerryChristmasText` (`FadeTheEnd`, CanvasGroup),
  `FPSdisplay`. Across the whole run **every Image alpha stayed 1.00** while the player
  controller was live — whether the rendered frame is actually black was not verified
  (no screenshot was taken).
- **Granny is one `EnemyAIGranny` in `Scene`**, and it moves on its own with no input:
  position went from `-37.3,12.0,-3.9` (load) to `-16.7,20.0,-16.4` (t=97).
- **`Player` never moved** (no input injected): position stayed exactly
  `13.8,21.0,-7.2`, yaw 270 for the whole run.
- **Crash signature:** patching every non-accessor method under `Player` and
  `GameController` (v0.0.10) killed the process ~1 s later with
  `APPCRASH 0xc00000fd` (STACK_OVERFLOW) in `coreclr.dll`. Patch a named handful, never a
  whole type's method list.
- **By-ref parameters break the trampoline.** Patching
  `FirstPersonController_Egen.GetInput(float& …)` produced
  `[Il2CppInterop] During invoking native->managed trampoline / NullReferenceException`
  on *every* call (2,371 lines in one 80 s run). v0.0.13 skips any method with a
  by-ref/pointer parameter and the run logged 0 errors.
- **`GetIl2CppType().Name` is the only reliable type name** for interop objects:
  `obj.GetType().Name` gives the managed wrapper, not the game type.

#### Derived (inference, not directly observed)

- The 19 s gap between scene load and `Player` activation matches the day-1 intro text
  sequence (`FadeText*`), so player activation is likely driven by whichever coroutine ends
  the intro — not by `newDay`, which never ran. Unverified.
- Because Il2CppInterop patches fire on native game calls, a mod can observe and replace
  game logic for the coop layer without a native detour of its own.

#### Unknown / not yet verified

- Which coroutine (if any) activates `Player`, and whether it can be driven by the host so
  two clients start the day at the same moment.
- Whether the visible frame is black during the intro (alpha says yes, gameplay says the
  controller is live).
- Where player input is read (`GetInput` returns by-ref floats; needs an Il2CppInterop-safe
  way to observe it) and whether it can be overridden per-client.
- The save format — no save file was produced in these runs
  (`%USERPROFILE%\AppData\LocalLow\DVloper\Granny\`).

### Derived (my inference, not directly observed)

- Because Cpp2IL mapped 71,566 method pointers using the same metadata v31 header our
  static parser reads (§5.1), the static map and the runtime assemblies describe the same
  build — the type/field names in §5 can be used unqualified in mod code.

### Unknown / not yet verified

- ~~Whether interop generation finishes and Melons actually load.~~ **Answered:** yes,
  103 interop DLLs generated, Melon loaded, `OnUpdate` runs in the game process.
- ~~Whether Harmony patches attach to methods of these generated interop assemblies.~~
  **Answered:** they attach *and fire on native game code* (see Observations above).
- ~~**What actually starts a playable level.**~~ **Answered (see §9.1):** Play →
  difficulty panel → `ClickContinueButton.TaskOnClick` → `LoadScene("Scene")`; the
  `Player` root activates ~19 s after the load. `FPSControllerNEW` is a red herring — the
  controller is `FirstPersonController_Egen`.
- Whether the `NotSupportedException` logged by `SteamManager.OnEnable`
  (`SteamAPIDebugTextHook` marshal) means Steam is not initialised in this process.
  Matters only for the Steam-P2P transport option (§7), not for direct IP.

## 10. Files produced so far (all under `local/`, gitignored)

- `granny-metadata-strings.txt` — 73,114 strings from the v31 string heap
- `granny-types.txt` — 9,448 unique type names parsed from 10,553 records
- `granny-dump-methods.ps1` — parser for type/field/method records, all three layouts
  verified (§5.1)
- `granny-methods.txt` — fields + methods for 26 key gameplay types present in this build,
  plus 3 explicit `NOT FOUND` markers (36 KB)

## 10.1 Sources

- Steam store API, app 962400 and 3070520 (`https://store.steampowered.com/api/appdetails`)
- `appmanifest_962400.acf` in the Steam library, `Granny_Data/globalgamemanagers`,
  `app.info`, `boot.config`, `ScriptingAssemblies.json` in the installed build
- GitHub: FedoraV3/GrannyTAS-MelonLoader-VibeCoded (`docs/game-analysis.md`,
  `docs/game-types.txt`), GDmio/granny-legacy-menu, lucaandri007/granny-multiplayer,
  an666667/granny-coop
- MelonLoader release metadata (v0.7.3, 2026-05-14), Il2CppDumper v6.7.46,
  LiteNetLib 2.1.4 — via `api.github.com`

## 11. Co-op mod v0.1 — first two-instance session (2026-10-23)

Source: `local/grannycoop/` (gitignored, not part of the PR). Build: `net6.0` class
library, MelonLoader 0.7.3, no third-party dependency — the transport is plain
`System.Net.Sockets.UdpClient`.

### What v0.1 does

- **Transport:** host binds `UDP 0.0.0.0:47777`; client binds an ephemeral port and
  dials the host address. Direct IP only (no Steam relay yet).
- **Handshake:** client sends `HELLO` (protocol version + player name) every 0.5 s until
  welcomed. Host accepts the first endpoint and rejects a second one with "session full"
  while the first is alive; peer timeout is 5 s of silence. Message magic `GCP1`, wire
  version 1.
- **State:** both peers send their own player position, yaw, camera pitch and two flags
  (`PlayerValid`, `PlayerInBed`) 20 times a second over unreliable datagrams. No
  retransmission, no reliable channel yet.
- **Scene message:** host announces its scene name on every load; the client only logs a
  mismatch in v0.1 (it does not follow).
- **Remote avatar:** a collider-free capsule with a nose cube, tinted `Standard` (falls
  back to `Unlit/Color`, then `Sprites/Default`), interpolated with
  `1 - exp(-14 * dt)`. The game's `Player` prefab has no body mesh, so v1 draws a
  placeholder.
- **Config:** MelonPreferences category `GrannyCoop` (`Mode`, `Address`, `Port`,
  `PlayerName`, `AutoPlay`), overridden per launch by `--coop-host`, `--coop-join[=ip]`,
  `--coop-off`, `--coop-name=`, `--coop-port=`, `--coop-autoplay`, `--coop-testmove`.
  `--coop-autoplay` and `--coop-testmove` are QA-only (no human input in a headless run).
- **QA driver:** `--coop-testmove` calls `CharacterController.Move` on the local player —
  host walks +X/-X, guest +Z/-Z, 4 s each way at 0.8 m/s.

### Test setup

- Two isolated copies, both real copies (not links): `C:\Users\Public\granny-test` and
  `C:\Users\Public\granny-test2`, 467 files / 1,843,119,668 bytes each, ASCII paths.
  `GrannyCoop.dll` (25,088 B, SHA-256 `F921AB2D7FA7BBF5B9E9C7F74DF98F5AA15D14880BBA41DF3F81AF431D443340`)
  deployed to both `Mods\` folders.
- Each instance launched with its own `-logFile`, so the two processes never interleave
  `%USERPROFILE%\AppData\LocalLow\DVloper\Granny\Player.log`.
- Launcher `local/grannycoop/tools/run-pair.ps1`: refuses to start if any `Granny.exe` is
  already running, starts host, waits 12 s, starts client, sleeps, then stops only the
  pids it recorded (with creation times), and prints matching log lines from this run.

### Run: 2026-10-23 01:28:34–01:31:05 (host pid 12440, client pid 13316)

**Observations (log lines):**

- Host `net listening on udp/47777` (01:28:39.771); client `joined host 'Host' as player 1`
  (01:28:52.704). Host logged `client 'Guest' joined from 127.0.0.1:61663`.
- Both autoplayed Menu → difficulty Continue → `Scene` (buildIndex=3): host scene at
  01:29:01.231, client at 01:29:13.879.
- Local player active: host 01:29:20.261 at `(13.79, 21.07, -7.16)` yaw 270.0; client
  01:29:37.952 at `(13.75, 21.07, -7.14)`.
- Remote avatar created on both sides (host 01:29:38.005 at `(13.75, 21.07, -7.14)`;
  client 01:29:20.311 at `(13.79, 21.07, -7.16)`) and `avVisible=True` in later status
  lines — a camera rendered it.
- **Movement sync matched exactly:** at 01:30:30.854 the client logged local
  `12.1,23.0,-11.1`; at 01:30:31.325 the host logged the guest's remote as
  `12.1,23.0,-11.1`. At 01:30:19.284 client local `13.8,21.1,-4.8` vs host remote
  `13.8,21.1,-4.9` at 01:30:21.322.
- Packet counts at 01:31:01: host sent 2,297 / received 1,776; client sent 1,771 /
  received 2,289. Session stayed connected to the kill.
- No `ERROR`, `Exception`, `CRITICAL` or `Failed` lines in either MelonLoader log.

**Bugs found by this run:**

1. **Welcome field order (fixed).** The host wrote `Byte(version) Byte(clientId)
   Str(name)` but the client read `Str()` first, so the first run logged
   `joined host '' as player 4` (the string length of "Host") and `peer=''` for the whole
   session. Fixed by reading `version`, `playerId`, then `name`; re-run logs
   `joined host 'Host' as player 1`.
2. **Sent counter (fixed).** `_stateSent` incremented even when `SendState` returned
   early (not connected). `SendState` now returns `bool`.

**Observation worth following up:** in the 01:30–01:31 window both instances repeatedly
reported `local=-` and `avVisible=False`, and positions returned to spawn
(`13.5,21.1,-7.2`). The testmove walk likely walked the player into Granny or off the
spawn ledge and the game caught/respawned it. Not confirmed; the status line only says the
`Player` root was inactive or absent.

### Derived (inference, not directly observed)

- Position, yaw and pitch replicate correctly over loopback at 20 Hz with interpolation;
  the remote avatar is a working visual stand-in.
- Because both peers independently reached the gameplay scene and their logs agree on
  spawn position, the two copies are equivalent and the isolated-install approach works.
- The intermittent `local=-` is consistent with normal game behaviour (caught/respawn),
  not with mod interference; no mod code deactivates the `Player` root.

### Unknown / not yet verified

- Whether the avatar is actually inside the local player's view in a normal session
  (`Renderer.isVisible` only says some camera rendered it).
- Whether the remote camera pitch (`CameraShakeAnim/CameraPivot/Main Camera` euler X)
  represents look pitch well enough for the avatar's nose; not checked visually.
- World sync: doors, items/inventory, Granny AI position/state, day progression, catch
  and respawn events — none are replicated in v0.1.
- Behaviour under lag/loss/jitter (no netem-style emulation yet), reconnect, host death,
  late join.
- Both instances share `%USERPROFILE%\AppData\LocalLow\DVloper\Granny` and HKCU
  PlayerPrefs; only the Unity log was separated by `-logFile`. Whether a save written by
  one instance is picked up by the other is unknown.

### Next

Host-authoritative sync for the world: at minimum Granny's transform and AI state, doors,
and the inventory of the shared item set, plus a scene-follow/late-join path. Then the
`coop-qa` matrix (join/leave/rejoin, kill client, kill host, lag/loss, soak).

## 12. Co-op mod v0.2 — Granny replication (2026-10-23)

### What v0.2 adds

- **Protocol:** `MsgType.Granny = 7` (host → client, 20 Hz, unreliable). Payload is
  `Byte flags, Float x, Float y, Float z, Float yaw`; flags are `FlagGrannyValid`,
  `FlagGrannyMoving`, `FlagGrannyAttacking`, `FlagGrannySeesPlayer`. Datagram is 21 bytes.
- **Host:** binds `EnemyAIGranny` lazily (`FindObjectsByType(..., Include, None)`, retried
  every 0.5 s) and sends her transform at 20 Hz, with the three AI booleans
  (`GrannyMoving`, `attackingPlayer`, `seePlayer`) packed into flags.
- **Client:** `RemoteGranny` binds the same object, walks its children for
  `UnityEngine.AI.NavMeshAgent`, disables it and sets `isStopped`, then pins the transform
  every frame to the host's snapshot: `Vector3.Lerp` with `1 - exp(-18·dt)` for position,
  `Quaternion.Slerp` with `1 - exp(-12·dt)` for yaw, and a hard snap when the gap
  exceeds 2.5 m (game-side teleports: bed, car, cellar).
- The client's own `EnemyAIGranny` keeps running. It never moves her (agent off), but it
  still does proximity checks against the pinned transform, so a catch can happen on the
  client at the same spot the host sees her.
- Status line grew `granny=<pos>` (this instance's object), `gRemote=<target>` (last
  snapshot received), `gSent=` and `gRecv=`.
- The mod now references `UnityEngine.AIModule.dll` from `Il2CppAssemblies`.

### Test runs

Four runs of `run-pair.ps1`, all loopback, all with `--coop-testmove`:

| run | window | DLL | result |
|---|---|---|---|
| A | 01:40:41–01:43:21 | 28,672 B, `2405254D…3B4D`, banner still 0.1.0 | Granny tracked host, 2,243 sent / 2,476 recv |
| B | 01:45:08–01:47:45 | 29,184 B, `C7DD7919…8BD47`, v0.2.0 | tracked; component dump showed the `MonoBehaviour` filter misses `NavMeshAgent` |
| C | 01:48:48–01:50:45 | 28,672 B, `36952094…F006`, `Component` filter | tracked; still `0` disabled |
| D | 01:51:45–01:53:48 | `9A6F40C1…1E79`, direct `UnityEngine.AI.NavMeshAgent` | tracked; `1 NavMeshAgent(s) found, 0 disabled` |

**Observations (log lines):**

- Host bound Granny in every run on entering `Scene`: `COOP granny bound at
  (-37.35, 11.97, -3.89) (slave=False)`; client the same at its own scene load, with
  `(slave=True)`.
- Client status lines show local Granny and last host snapshot converging to 0.1
  precision, e.g. run D at 01:53:14: `granny=-13.2,13.2,7.2 gRemote=-13.2,13.2,7.2`;
  run B at 01:46:17: `granny=-18.9,13.2,-7.8 gRemote=-18.9,13.2,-7.8`.
- Packet counts stay steady: run D host `gSent=1689` at 01:53:41, client `gRecv=1741` at
  01:53:44 (the client counts a few more because it also counts messages received while
  its own player was inactive).
- One mid-catch-up sample in run B (01:46:57): `granny=0.4,12.6,7.2` vs
  `gRemote=0.2,7.4,7.1` — a 5.2 m vertical jump (cellar/stairs) that was being glided
  because the old snap threshold was 6 m. Threshold lowered to 2.5 m after that.
- No `ERROR`, `Exception`, `CRITICAL` or `Failed` lines in any of the four runs.
- `COOP granny components:` (run B dump, first 60): `Transform, NavMeshAgent,
  EnemyAIGranny, SphereCollider, Transform, Transform, Animation, GrannyFootstep,
  AudioSource, BoneController, Transform, SkinnedMeshRenderer,
  changeTextureNightmareGranny, Transform, CapsuleCollider ×3, MeshFilter/MeshRenderer
  ×several, Camera, FlareLayer, AudioListener, EnemyEye`. (`GrannySounds` and
  `GrannyCloseSensor` appeared under a `MonoBehaviour`-only dump in run B but not in this
  `Component` dump capped at 60 names.)

### Derived (inference, not directly observed)

- Pinning the transform in `OnUpdate` beats the game's own writes: `FixedUpdate` runs
  before `Update`, so the client always renders the last host snapshot.
- The client's local AI is what makes the puppet behave like Granny (animations, sounds,
  proximity catch) without replicating AI state explicitly.
- `NavMeshAgent` being already `enabled == false` on the client at bind time (runs C/D)
  explains why the earlier `MonoBehaviour`-filtered search found nothing: `NavMeshAgent`
  derives from `Behaviour`, not `MonoBehaviour`.

### Unknown / not yet verified

- Whether the client's AI state (flags) is needed at all; the client currently receives
  them but ignores them.
- Catch, stun, jail and day-change events are not replicated as events. The host player
  went `local=-` intermittently in every run (same behaviour as v0.1). The client player
  stayed active for the whole of runs A–D — its local Granny never reached it, but that is
  not a test of the catch path.
- Granny audio, footsteps and animations on the client come from the client's own AI;
  whether they match the host's state is unchecked.
- Doors, items/inventory, scene-follow/late join, lag/loss, reconnect, host death, soak.

### Next

Doors and the shared item set, then scene-follow/late join, then the `coop-qa` matrix.

## 13. Doors: the full survey (2026-10-23)

### Observation — what is in the scene

Run E: `run-pair.ps1 -Seconds 75 -Probe` (host log
`<test copy>\MelonLoader\Logs\26-10-23_3-12-52.log`, 41,805 bytes). The probe added a
`SurveyDoors()` pass (WorldProbe.cs) that prints one `door-survey` line per component:
type, hierarchy path, active, `DoorOpen`, tag and every clip on the legacy `Animation`
component that drives it. All 55 lines were emitted, 0.035 s for the whole survey.

Census: **55 door-ish components over 23 types**. Two disjoint groups.

**Group A — 48 components that own a legacy `Animation` with clips.** All of them are the
ones that actually appear to move. Type → count → tag prefix at rest:

| type | n | tag at rest | clips |
|---|---|---|---|
| `OpenCloseDoors` | 9 | `innerdoorClosed` (8), `innerdoorLocked` (1: LekhusDoor), `steeldoorLocked` (1: StealDoor) | `InnerdoorOpen/Close/Locked` — LekhusDoor has 0.33 s versions |
| `OpenCloseDoorVind` | 2 | `innerdoorLocked` both | same |
| `OpenCloseGarde1` | 5 | `garde1Closed` all | `Garde1Open/Close` (+ the `Innerdoor*` trio on 3 of them) |
| `OpenCloseKitchenDoor1` | 5 | `kitchenDoor1Closed` all | `KitchenDoor1Open/Close` |
| `OpenCloseKitchenDoor2` | 2 | `kitchenDoor2Closed` both | `KitchenDoor1Open/Close` **and** `KitchenDoor2Open/Close` |
| `OpenCloseKyl` | 1 | `kylClosed` | `KylOpen/Close` |
| `OpenCloseLoda1` | 4 | `loda1Closed` | `Loda1Open/Close` |
| `OpenCloseLoda2` | 8 | `loda2Closed` | `Loda2Open/Close` |
| `openCloseMotorhuv` | 1 | `motorhuvClosed` | `motorhuvOpen/Close` |
| `OpenCloseSmallDoors` | 5 | `smalldoorClosed` (4), `smalldoorLocked` (1) | `SmallDoorOpen/Close` (+ `Innerdoor*` on 4, + `SmallDoorLocked` on the locked one) |
| `openCloseVitrindoorL` | 1 | `vitrindoorLclosed` | `VitrindoorHallOpen/Close`, `VitrindoorHallLOpen/Close` |
| `openCloseVitrindoorR` | 1 | `vitrindoorRclosed` | `VitrindoorHallOpen/Close` |
| `openCloseVitrindoorVRL` | 1 | `vitrindoorVRLclosed` | `VitrindoorVRROpen/Close`, `VitrindoorVRLOpen/Close` |
| `openCloseVitrindoorVRR` | 1 | `vitrindoorVRRclosed` | `VitrindoorVRROpen/Close` |
| `openCloseBaklucka` | 1 | `bakluckaLocked` | `bakluckaOpen/Close` |
| `prisonDoorOpenClose` | 1 | **`Untagged`** | `prisondoorOpen/Close/Locked`; `DoorOpen=True` at rest |

**Group B — 7 components with no `Animation` and no use as a door we can replicate:**
`closePrisonDoorSimple` (a `HideTrigger`), `grannyClosePrisondoor` (a trigger, inactive),
`destroyCarDoor` (`brokencardoor`), `SkjutplattaDoor` (`skjutplatta` collider),
`openDoors` (the player's `Camera/DoorRay` raycast helper, its `DoorOpen` is an
`AudioClip`), `checkIronDoors` and `CheckExitDoor` (both on `GameController`). None of
them is a swinging door; they are gameplay triggers. Nothing to visualise.

Two more facts from the same survey:

- **Clip names are not one scheme.** A door can carry several families of clip
  (`OpenCloseKitchenDoor2` has both `KitchenDoor1*` and `KitchenDoor2*`), and the
  `VitrinDoorL`/`VitrinDoorR` pair does not follow the tag prefix at all (`vitrindoorL` →
  `VitrindoorHall*`). Matching a state to a clip by string similarity alone is therefore
  not reliable (see the L/R tie below).
- **Tag ≠ pose in the first second.** The tag is written by the game's own `Update` some
  time after the animation starts (§12: about 1.8 s), so a door that is opening reads
  `…Closed` while its mesh is already swinging. `DoorOpen`/`DoorMoving` flip earlier.

### Derived (inference, not directly observed)

- Group A is exactly the replicable set, and it is closed: every door a player can see
  swing is in it. Group B can be left alone.
- The state that matters is a three-value enum per door — `Closed`, `Open`, `Locked` —
  read from the tag suffix (`closed`/`open`/`locked`, case-insensitive), with `DoorOpen`
  (bool) as the fallback for `prisonDoorOpenClose`, which has no tag at all.
- A door's open pose is fully described by **one** clip: playing that clip forward goes
  closed → open, and sampling it at `t = 0` is the closed pose. So a single verified clip
  per door is enough; no separate close clip is needed. Closing is the same clip sampled
  backwards.
- The clip can be *chosen by measurement*, not by name: sample each candidate open clip
  at its last frame on the door's hierarchy, keep the one that rotates a descendant the
  most, restore the saved transforms. That resolves the `VitrindoorHallOpen` vs
  `VitrindoorHallLOpen` tie without guessing.

### Unknown / not yet verified

- Which transform inside a door hierarchy each clip actually rotates (not needed — the
  clip is applied to the door's own `GameObject` and Unity resolves the curves).
- Whether writing the tag and `DoorOpen` from the mod confuses the client's own door
  logic (its `Update` may re-run and re-play the animation on top of ours).
- Whether the client's ghost Granny can open doors locally and generate spurious
  requests (mitigated by only sending a request for doors near the local player; not yet
  measured).
- Latency budget: the host's own opens are detected by polling, so the client starts
  about one poll interval plus one RTT late. Not measured against a human.
- Nothing is verified about door *sound*; the clips may carry `AudioSource` curves that
  will fire on both peers.

## 14. Door sync v0.3 — two-instance result (2026-10-23)

### What v0.3 adds (DoorSync.cs, 802 lines)

- **Door table.** `Prepare()` scans the 16 Group A types one concrete type at a time
  (`FindObjectsByType<T>(FindObjectsInactive.Include, SortMode.None)`; the base-typed
  `MonoBehaviour` scan returns proxies and is not usable — §12). Each door gets
  `hash = FNV-1a(hierarchy path)` (32-bit), its state, its chosen clip, the clip length
  and the measured pose delta. Path is the key on the wire, never the index.
- **State.** `Closed` / `Open` / `Locked` / `Unknown`, read from the tag suffix and
  falling back to the `DoorOpen` bool (prison door, §13).
- **Clip.** `ChooseClip` asks for a clip by name, then *verifies by sampling*: every
  candidate is sampled at its last frame on the door's `Animation`, the resulting
  rotation of any descendant is measured, the largest wins, the saved transforms are
  restored. One clip per door, sampled at `s ∈ [0,1]`.
- **Wire** (`MsgType.Door = 8`, host and client both send, unreliable):
  `KindState=0` `(kind, hash, state)`, `KindSnapshot=1` `(kind, count, (hash,state)*)`,
  `KindRequest=2` `(kind, hash, state)` client → host, `KindSnapshotRequest=3` `(kind)`
  client → host. A closed door still travels as an explicit `Closed`, not as "absent".
- **Host authority.** The host applies its own local changes and records them. A client
  `KindRequest` is applied by the host and then broadcast, so the host's state always
  wins; the client never applies its own request.
- **Snapshots.** The client asks once on `Prepare` (`KindSnapshotRequest`), the host
  answers with one `KindSnapshot`, and the host pushes one every 10 s as a drift repair.
  `_gotSnapshot` stops the one-shot from repeating.
- **Anti-echo.** `IgnorePollFor = 2.5 s` per door after any apply we perform, and a
  client only sends a request for a door within reach of its own player, so the game's
  own `Update` re-writing the tag does not start a poll war.
- **QA hook `--coop-doortest`** (`DoorTest`, host only): every 6 s the host flips the
  nearest open/closed door by writing the game's own `DoorOpen`/tag fields — the same
  writes a player's interaction produces — and deliberately does not touch its own
  table, so the normal `Poll` → `SendDoor` path is what the client sees. `Config.cs`,
  `GrannyCoopMod.cs` and `run-pair.ps1 -DoorTest` carry the flag; the client never
  gets it.

### Observation — run F, the long pair

`run-pair.ps1 -DoorTest`, ~110 s of play after the 12 s stagger. Host log
`<test copy>\MelonLoader\Logs\26-10-23_5-3-56.log` (145 lines), client
`<test copy 2>\MelonLoader\Logs\26-10-23_5-4-8.log` (147 lines).

- **Both peers map 48 doors**, `noClip=0`, and the type histogram is identical:
  `OpenCloseDoors=9 OpenCloseLoda2=8 OpenCloseGarde1=5 OpenCloseKitchenDoor1=5
  OpenCloseSmallDoors=5 OpenCloseLoda1=4 OpenCloseDoorVind=2
  OpenCloseKitchenDoor2=2 OpenCloseKyl=1 openCloseBaklucka=1 openCloseMotorhuv=1
  openCloseVitrindoorL/R/VRL/VRR=1 prisonDoorOpenClose=1`.
- The per-door `map` lines match one-for-one on hash, clip, length, state and delta.
  Example, host `05:04:43` / client `05:04:55`: `map 30 OpenCloseLoda2
  hash=2172F94D state=closed clip=Loda2Open len=0.33 openAtEnd=y delta=213.1` at
  `HouseBakedReady/Sovrum1Furniture/Loda2Parent (5)/ByroLoda`.
- Host prepares at `05:04:43.569`, the client at `05:04:55.654` — 12 s later, because
  prepare waits for the local `Player` root to go active.
- **Host QA flips 11 times** on the two nearest cabinets; the client logs 11 applies —
  9 of them match the 9 flips that happened after it prepared, 2 are doors the host's
  player walked through. Every match lands 37–181 ms after the flip:

  | # | host `TEST flip` | client `apply … (msg)` | lag |
  |---|---|---|---|
  | 3 | 05:05:01.506 open→closed | 05:05:01.552 → closed | 46 ms |
  | 4 | 05:07.592 closed→open | 05:07.743 → open | 151 ms |
  | 5 | 05:13.687 open→closed | 05:13.868 → closed | 181 ms |
  | 6 | 05:19.701 open→closed | 05:19.851 → closed | 150 ms |
  | 7 | 05:25.722 closed→open | 05:25.827 → open | 105 ms |
  | 8 | 05:31.731 open→closed | 05:31.830 → closed | 99 ms |
  | 9 | 05:37.749 closed→open | 05:37.789 → open | 40 ms |
  | 10 | 05:43.773 closed→open | 05:43.810 → open | 37 ms |
  | 11 | 05:49.785 open→closed | 05:49.905 → closed | 120 ms |

  The state the client lands on equals the host's `want` state for that flip. Flips #1
  and #2 are absent from the table for the reason in the next bullet.
- Flips #1 (`05:04:49.441`) and #2 (`05:04:55.445`) happened **before** the client
  prepared (`05:04:55.654`), so they have no `apply … (msg)` line. The client's
  `applied` counter is not fooled: at `05:05:04` it reads `applied=3` while only one
  message apply has been logged, and the final counts are
  `applied=13 = 9 flips after prepare + 2 game-driven Innerdoors + 2 from the snapshot`.
- **Doors the host's player actually walked through also synced**, not only the hook:
  client applied `All doors/InnerdoorParent/Innerdoor -> open at 13.7m` and
  `All doors/InnerdoorParent (2)/Innerdoor -> open at 9.4m` (`apply … (msg)`).
- **Counters at the end.** Host `doors=48 applied=0 sent=18 recv=1 req=0 noClip=0`;
  client `doors=48 applied=13 sent=1 recv=19 req=0 noClip=0`. The two lines are 2.1 s
  apart (host `05:05:52.911`, client `05:05:54.999`), so they are samples, not totals;
  one more snapshot in that window explains the client's 19 against the host's 18.
  `sent=1 recv=1` is the one-shot snapshot request.
- **A client counts door datagrams before it is prepared.** The `05:04:54.988` status
  reads `doors=0 applied=0 sent=0 recv=1 req=0` — one message arrived in the second
  before `Prepare`, and was dropped. Anyone reading `recv` as "applied messages" will
  be off by one.
- **No failure lines** in either log: `grep -E "FAILED|Exception|ERROR|CRITICAL"` is
  empty, and neither `DoorOpen write FAILED` nor `tag write FAILED` appears.
- Granny sync is unaffected: client `gRecv` climbs to 1,267, avatar visible, same
  positions as v0.2.

### Observation — run G, the polarity fix

The survey in §13 said the prison door is the only door that loads **open**
(`DoorOpen=True`, tag `Untagged`). `ChooseClip` measures the delta from the *loaded*
pose, so for that door the far end of the clip is the closed pose, not the open one;
the name heuristic (`prisondoorLocked` has no "open"/"close") cannot know. `Prepare`
now reverses `OpenAtEnd` when the observed state is `Open`, and logs it.

`run-pair.ps1 -Seconds 80 -DoorTest`, host log `..._5-9-26.log`, client `..._5-9-38.log`:

- Host at `05:10:12.625` and client at `05:10:24.714`, once per peer:
  `COOP doors: HouseUpdateVind4/PrisonGaller/prisonDoor starts open, sampling clip in reverse`.
- The resulting map line is identical on both peers:
  `map 48 prisonDoorOpenClose hash=4BBB6F4A state=open clip=prisondoorLocked len=0.40
  openAtEnd=n delta=91.9 path=HouseUpdateVind4/PrisonGaller/prisonDoor`.
  With `openAtEnd=n`, `t = (1-s)·len`: applying `Open` samples `t=0`, the pose the door
  loaded in, and `Closed` samples `t=len`.
- Everything else ran as in run F (7 flips → 7 message applies by `05:10:54`,
  `req=0`, no failure lines).

### Derived

- **The path hash is stable across processes.** 48/48 hashes match between a host
  built from one install and a client from another, across two runs. It is a property
  of the scene, so it is safe as the wire key and as the save/join key later.
- **The measured clip is a property of the door, not of the peer.** Clip name, length
  and delta agree on both peers for all 48 doors, so `ChooseClip` can run independently
  on each side and still agree; only the *state* needs the network.
- **Latency is one poll plus one datagram.** 37–181 ms loopback, mean ~93 ms, with a
  poll interval of 0.1 s and a 20 Hz send cap — the spread is the poll phase, not the
  network.
- **Snapshot reconcile covers changes made before a peer was ready.** The two flips
  before the client's prepare arrived as snapshot entries with no log line; the
  `applied` counter (13) is the only place they show. Anyone reading logs alone will
  undercount by exactly the snapshot entries.
- **The game's own door `Update` does not fight our writes.** `req=0` on the client for
  the whole run, and the host's flip counter advances only through the hook (one flip
  per 6 s), so `IgnorePollFor` held and no echo loop formed.
- **Polarity can be derived, not guessed.** "State at prepare + measured delta" is
  enough to fix the open-at-rest case without naming a clip.

### Unknown / not yet verified

- **Client-initiated requests never ran.** `req=0` in every run: the client's own player
  never touched a door, so `KindRequest` → host apply → broadcast is untested. The code
  path is written and compiles; no run has executed it. A client-side `--coop-doortest`
  (or a client flip near its player) is the missing test.
- **Door sound.** Not checked at all.
- **Visual pose.** The deltas prove the transform moved; nothing proves the door looks
  open rather than, say, inside-out. No screenshot comparison has been done.
- **Whether the game's door logic reacts to our tag/bool writes** (proximity prompts,
  "locked" messages, Granny's door pathing) — no observation either way.
- **Tag lag vs pose.** §13 saw the tag trail the animation by ~1.8 s. If a human opens a
  door, the client's `Poll` may read the *previous* tag and send a stale state. Untested.
- **The QA harness's "Continue" button match is too loose.** `ClickByPathPart("Continue")`
  actually invoked
  `OptionMenuParent/OptionsMenu //Disable/DifficultyDropdown/ContinueButton` (a dropdown
  button), yet the gameplay scene still loaded. The autoplay log line says which button
  was pressed, so runs are not ambiguous, but the matcher should be tightened before it
  is used as evidence of anything else.

### Next

Shared item/inventory set, then scene-follow/late join, then the `coop-qa` matrix
(client requests, join/leave/rejoin, kill client, kill host, lag/loss, soak).

## 15. Item sync v0.4 — two-instance result (2026-10-23)

### What v0.4 adds (ItemSync.cs)

- **The inventory surface.** `GameController` carries `Il2Cpp.InventoryController`:
  269 properties, **0 fields**, **50 `have*` boolean properties**, all writable. Each one
  has a sibling GameObject property (`Hammare`) and a Transform property (`newHammare`).
  49/50 GameObject properties resolve through Il2CppInterop and 50/50 Transform properties
  do; the odd one out is not named in the log. Item keys are the property name minus
  `have` (`haveHammare` → `Hammare`).
- **Key.** `hash = FNV-1a(key)`, not the scene path: the inventory lives on the player,
  not in the hierarchy. Both peers compute it locally and it matches (`Hammare`).
- **Wire** (`MsgType.Item = 9`, both directions, unreliable, same envelope as Door):
  `KindState=0` `(kind, hash, value)`, `KindSnapshot=1` `(kind, count, (hash,value)*)`,
  `KindSnapshotRequest=2` `(kind)` client → host.
- **Host authority.** The host polls at 5 Hz, sends one `KindState` per changed flag,
  a full table every 10 s, and answers a `KindSnapshotRequest`. The client asks on
  `Prepare` and re-asks every 5 s until the first table lands (`--coop-itemtest` runs
  showed the host may still be in its menu when the join handshake completes).
- **Apply writes the have-flag only.** It does not touch the item's objects, and it
  deliberately does not call `InventoryController.CheckInventory()` (see run I).
- **QA hook `--coop-itemtest`** (host only): cycles one item (`Hammare`) through
  write-true + visual, an 8 s hold, `CheckInventory()`, write-false + visual, logging
  the flag and both objects at every step. `Config.cs`, `GrannyCoopMod.cs` and
  `run-pair.ps1 -ItemTest` carry the flag; the client never gets it.

### The defect that made items one-way

Item datagrams reached the client's socket and were counted by nothing: `CoopNet.HandlePacket`
had cases for State, Scene, Granny and Door but **none for `MsgType.Item`**, so every
type-9 packet was dropped before `AppMessage`. Doors worked in the same run because they
had a case. The fix is the missing case (`_lastHeardUtc` refresh + `AppMessage`), and it
is worth recording as a trap: **a new message type needs a `case` in `HandlePacket` or it
disappears silently while the sender's counters keep climbing.**

Before the fix (run G-era): host `iSent` climbed to double digits, client `iRecv=0`,
`iApplied=0`. After it, in the first run (host log `26-10-23_5-38-23.log`, client
`26-10-23_5-38-35.log`):

- client `items=50 iSent=1 iRecv=15 iApplied=8` at `05:40:01`, with eight
  `apply Hammare=have/none` lines in the same window;
- host `items=50 iSent=14 iRecv=1 iApplied=0` at `05:39:59` — the single `iRecv` is the
  client's snapshot request. `iApplied=0` on the host is correct: it is the authority
  and never applies.
- `prepared 50 items ... props=269 fields=0 haveProps=50 writable=50 objOk=49 trOk=50
  dupes=0 trueAtStart=0` on both peers.

### Observation — run I (`5-42-46` / `5-42-58`): a flag does not move the model

The item's two objects at rest, on both peers:

- `Hammare` (the GameObject property) → **`HammerHand`** at
  `HandHoldObjects/HammerHand`, `activeSelf=n`, `activeInHierarchy=n`, 0 children.
- `newHammare` (the Transform property) → **`Hammer`**, *root* object,
  `activeSelf=y`, `activeInHierarchy=n`, `pos=(1.29, -1.78, 9.87)`.

The test then wrote `haveHammare=true` and forced the visual by hand (hand active,
`Hammer` instantiated under `HammerHand` as a child, world `Hammer` deactivated):

- immediately after: hand `active=y inHierarchy=y children=1`, `Hammer` `active=n`;
- at +1 s, still `have=have`, same object state;
- `CheckInventory()` then returned `ok` and the flag read **`none`**, the hand
  `active=n`, and the clone was still there (`children=1`).

Meanwhile the client applied every host change (`apply Hammare=have/none`) with its own
`HammerHand` staying `active=n` — an applied flag does not activate the hand either.

### Observation — run J (`5-45-54` / `5-46-06`): the game does not revert the flag

The same cycle with a 1 s check, an **8 s hold with no calls at all**, then
`CheckInventory()`:

| step | host flag | hand | clone children |
|---|---|---|---|
| after write + visual | have | y | 1 |
| +1 s | have | y | 1 |
| **+8 s (untouched)** | **have** | **y** | **1** |
| after `CheckInventory()` | none | n | 1 |
| after write false + visual off | none | n | 0 |

So nothing in the game re-derived the flag during eight seconds of play; the only thing
that ever reverted it was our explicit `CheckInventory()` call. Counters at the end:
host `items=50 iSent=12 iRecv=1 iApplied=0 iRefresh=6/6`, client
`items=50 iSent=1 iRecv=13 iApplied=5`.

### Derived

- **Flag-only sync is viable.** A written `have*` flag survives at least 8 s of normal
  gameplay untouched, so the host's table and the client's writes are not fighting a
  per-frame recompute. `CheckInventory()` is called by us only; it is not a periodic
  game tick (at least not in `Scene` within 8 s).
- **`CheckInventory` has a second source of truth.** It reset a flag whose hand
  contained a cloned model and whose hand was active, so that source is neither the
  have-flag nor the hand hierarchy. Its value is `none` with the clone still parented,
  which rules out "have := hand has children" as well.
- **The item key is stable across processes.** Both peers build the same 50-entry table
  with the same `Hammare` hash and zero `dupes`; the property-name hash is a valid wire
  key, and unlike the door path it needs no scene scan.
- **`new*` is not the live world pickup.** `Hammer` is a root object that is
  `activeSelf=y` yet `activeInHierarchy=n` — it is not in the loaded hierarchy, so
  deactivating it cannot hide a pickup in the scene. What a pickup does to the world
  object is still unobserved.
- **A hand model can be built** (`Instantiate(world, hand.transform)` gives
  `children=1`, `activeInHierarchy=y`) but this proves the transform, not that the model
  is the right one at the right socket.

### Unknown / not yet verified

- **What `CheckInventory()` reads.** Candidates: the save data loaded by "Continue", a
  scene object our scan does not cover, or the `hammare` object property
  (`type#14446`) that we never write. Answering it needs a decompile of
  `InventoryController.CheckInventory` / `PickUpNew.Update`; a decompiler is not
  installed and downloading one needs the owner's approval (AGENTS.md rule 1). Until
  then: **unknown**.
- **Whether gameplay accepts a flag-only "have".** The hammer's use (boarding planks)
  was not exercised: the harness drives the menu and `CharacterController.Move` only, so
  there is no input injection and no interaction test. A flag that reads `have` may or
  may not be enough for the game's own use-checks.
- **Whether the forced hand visual looks right.** No screenshot or human check. Model,
  scale, socket offset and whether the clone animates are all unverified.
- **Duplication.** Each peer has its own copy of the world; with flag-only sync the
  client's world item is still there, and the client can presumably pick it up itself.
  Not tested (needs input).
- **Whether the game ever calls `CheckInventory` on its own** — at scene load, on opening
  a menu, or on using an item. Only the 8 s gameplay window is covered, and that window
  had no interaction.
- **HUD text and sound.** `hammerText`/`NeedhammerText` exist in the metadata; nothing
  observes whether a synced flag drives them.
- **`iSent=1` on the client in every run** is the snapshot request; the counter does not
  distinguish kinds, so a client can never show a non-zero "changes sent" by accident —
  but a reader must not take `iSent>0` on the client as evidence of an item change.

### Next

Scene-follow/late join, then the `coop-qa` matrix (client-initiated door requests —
`req=0` in every run — join/leave/rejoin, kill client, kill host, late join, lag/loss,
soak). Item visuals stay open until the `CheckInventory` source can be decompiled with
the owner's approval.

## 16. Late join and client→host door requests (2026-10-23)

### What changed

- `GrannyCoopMod.OnConnectionChanged` (GrannyCoopMod.cs): the host re-announces its
  scene when a peer connects. The scene message used to be sent only from
  `OnSceneWasLoaded`, so a guest that joined after the host had loaded `Scene` never
  heard it at all. The client's handler still only logs "v1 does not follow".
- `DoorSync.Tick` (DoorSync.cs:153): the QA door flip is no longer host-gated
  (`if (DoorTest) TickDoorTest(...)`, was `role == CoopRole.Host && DoorTest`).
  Test-only change; without it a guest can never originate a flip, so `Poll` never sets
  `ReqState/ReqLeft` and `TickRequests` never sends `KindRequest` — which is why `req=0`
  in every earlier run.
- `tools/run-pair.ps1`: new `-ClientDoorTest` switch that passes `--coop-doortest` to the
  guest. `-DoorTest` still goes to the host only.

### Observations — run K, late join (`buqvz69xa`: pair 06:02:18 / 06:02:56, 240 s, host `-DoorTest`)

- Host prepared 48 doors at 06:03:04.345, i.e. with no peer connected.
- Guest connected at 06:03:02.520 and logged the re-announced scene 1 ms later:
  `COOP host is in scene 'Scene', we are in 'SplashScreen' (v1 does not follow)`.
- Guest sat in splash/menu/intro for 56 s and only prepared its door table at
  06:03:58.460 (items 06:03:58.504). `SettleDelay` gates on the player being spawned,
  so a late joiner's table starts ~1.5 s after its own intro.
- First door line on the guest is 06:03:58.708, a live broadcast. `applied=3` by
  06:04:02.837 while only one per-door line exists: **snapshot applies are silent**
  (`KindSnapshot` increments `Applied` without a log line, DoorSync.cs:746–748). Counters
  and log lines do not correspond 1:1; read them together.
- Subsequent host flips reached the guest in ~40–150 ms; guest `recv` tracked host
  `sent` for the rest of the run.

### Observations — run L, guest-driven flips (`b1ac90635`: pair 06:14:29 / 06:15:07, 230 s, guest `-ClientDoorTest`, host `doortest=False`)

- 27 guest `TEST flip` lines between 06:16:16.232 and 06:18:57.731.
- 26 of them reached the host as `COOP doors: apply … (request)` lines; host
  `applied=26` equals the number of `(request)` lines exactly.
- Latency flip → host apply: 0.0 s (1), 0.1 s (5), 0.2 s (8), 0.3 s (3), 0.4 s (1),
  0.5 s (4), 0.6 s (2), 0.9 s (1), 1.2 s (1). Median ~0.2 s.
- The one flip with no host apply (#27, 06:18:57.731, 5.7 m) landed at the run boundary —
  the guest was killed at ~06:18:57.7. Not a defect, but do not reuse this run as
  evidence for "every flip arrives".
- Guest `req=75` for 26 delivered flips ≈ 2.9 per flip. `TickRequests`
  (DoorSync.cs:580–592) only decrements `ReqLeft` and re-arms on `RequestRetry` (0.4 s);
  it has no "state already matches, stop retrying" check, so all `RequestTries=3`
  datagrams go out even when the host answered the first one. The host dedupes by state
  (`if (observed == d.State) continue`), so duplicates are dropped with no apply and no
  echo.
- No fight and no loop: host `req=0` for the whole run (the host never requests), guest
  `applied` stayed at 11 (it does not count its own flips; the host's echo of a request
  matches the guest's local state and is skipped), and neither side's request counter
  ran away.
- The guest sent state at ~5.5 Hz (`sent=1214` over 219 s) against the host's ~18.6 Hz
  (`sent=4278` over 230 s). The send loop is frame-driven (`OnUpdate` + 0.05 s
  accumulator), so a guest that renders at ~5 fps during the intro also polls doors and
  retries requests at ~5 Hz. QA runs need to allow for that when interpreting latencies.
- Granny's own door use on the host still crosses the wire: guest logged 9
  `apply … (msg)` lines at 8.0–28.4 m, including `All doors/BackyardDoorParent/Backdoor
  -> locked` at 06:18:11.337 — a state the guest cannot produce by walking.

### Derived

- The guest → host request path works end to end: guest flips locally, `Poll` notices,
  `KindRequest` goes out, the host applies it and echoes `KindState`, the guest's door
  stays where it was put.
- Host authority holds for doors: the host applies requests but never accepts an
  instruction it would have to undo, and it forces locked doors back to `Locked`
  (DoorSync.cs:769).
- All retries going out every flip is wasteful but safe. Three datagrams of ~20 bytes per
  flip is not worth an ack scheme in v1.
- Re-announcing the scene on connect is necessary and sufficient for the host to tell a
  late joiner where it is; the client deliberately does not follow (scene-follow is still
  unimplemented).
- A guest whose table is prepared late still catches up fully from one snapshot: state,
  not events, is what crosses the wire.

### Unknown / not yet verified

- **Whether the door looks right after an apply.** `open`/`closed`/`locked` are read from
  the game's own component; whether the animation, the collider and the sound match is
  not observable from a log. No screenshot.
- **Whether a guest can force open a locked door and be reverted** — only the host-driven
  `-> locked` transition was observed; no guest request for a locked door was generated.
- **Item catch-up under a real change.** `changed=0`/`iApplied=0` in all runs, so the
  item apply path is still unexercised; see §15.
- **Whether the 1.2 s outlier has a cause other than frame pacing.** One sample; a
  frame-time log would settle it.
- **Reconnect/idempotence of the snapshot.** Not tested: a guest that reconnects gets a
  new `DoorSync.Reset()` state, but no run has reconnected the same process.

### Next

The rest of the `coop-qa` matrix, in order: join/leave/rejoin, kill client, kill host,
lag/loss, soak. Item visuals stay open until the `CheckInventory` source can be
decompiled with the owner's approval.


## 17. Kill client, rejoin (2026-10-23)

### What changed

- `tools/run-pair.ps1`: `-KillClientAfter`, `-RelaunchClientAfter`, `-KillHostAfter` (seconds
  from that process's own launch; `-1` = never). The fixed `Start-Sleep` became a 0.5 s event
  loop, and every process the script starts — including a relaunched client — goes into
  `$tracked` and is stopped by pid at the end. `Stop-Tracked` prints the process start time,
  so a log line can be matched to the exact process instance.
- No mod code changed for this run. The point was to exercise the paths that only exist
  when a peer disappears without a `Bye`.

### Observations — run M (`kill-client-rejoin.txt`: `-Seconds 250 -ClientDelay 38 -DoorTest -KillClientAfter 70 -RelaunchClientAfter 95`)

- Host pid 32452 started 06:26:18.997, guest pid 30596 started 06:26:57.034. Guest killed at
  its own +70 s (≈06:28:07); a new guest pid 29208 launched at its predecessor's +95 s
  (06:28:32, log `26-10-23_6-28-32.log`).
- First session: guest joined 06:27:03.194, host prepared 48 doors 06:27:04.678 and 50 items
  06:27:04.714. Guest's own player became active 06:27:55.513 and it prepared its tables
  06:27:56.910 / 06:27:56.947. Its last status before the kill (06:28:03.136) read
  `doors=48 applied=5 … items=50 iApplied=0`. Host's last connected status (06:28:04.218)
  read `remote=13.8,21.1,-7.5 avVisible=True`.
- **Timeout fired 5 s after the last packet, not earlier and not later:**
  `06:28:11.940 COOP peer 'Guest' timed out after 5s of silence` → `06:28:11.940 COOP
  disconnected`. `PeerTimeoutSec = 5.0`, and a Force kill sends no `Bye`, so this is the
  timeout path, not the graceful one.
- While disconnected the host kept running its own game: Granny went from
  `-18.9,13.2,-7.8` (06:28:14) to `-8.6,13.2,8.7` (06:28:34), and `local=` kept changing.
  Its `sent`/`recv` counters **froze** at `1332`/`594` across the three status lines
  06:28:14, :24, :34 — `SendState`/`SendGranny` return `false` with no peer, so nothing is
  queued and nothing is retried.
- The host kept its door and item tables across the gap (`doors=48`, `items=50`,
  `applied=0`, `req=0` throughout). Reconnect needed no re-preparation.
- Reconnect was accepted from a **new endpoint and a new name**: `06:28:38.199 COOP client
  'Guest2' joined from 127.0.0.1:56035` (old session was `:57131`), host
  `COOP connected to 'Guest2'` 06:28:38.200; the guest logged `COOP joined host 'Host' as
  player 1` 06:28:38.244. This is CoopNet's "new Hello from another endpoint while
  `!Connected`" branch (CoopNet.cs:208–230) — the old `_peer` is simply replaced.
- Scene re-announce on reconnect still works: guest logged
  `06:28:38.247 COOP host is in scene 'Scene', we are in 'SplashScreen'` — 1 ms after join.
- Guest2 launched 06:28:32, player active 06:29:23.529, door/item tables prepared
  06:29:25.519 / 06:29:25.556 (≈53 s of splash+menu+intro, same as the late join in §16).
  Host `sent` resumed growing immediately after the join (1448 at 06:28:44 vs 1332 frozen).
- Catch-up after rejoin: at 06:29:31.260 the guest already read `applied=5` while only one
  `(msg)` apply had been logged (06:29:29.727) — the same silent-snapshot accounting gap
  documented in §16. `applied` then went 5 → 6 → 11 → 14 → 17 → 20 through 06:30:22 while
  the host's own `-DoorTest` flips kept crossing the wire (`doors … sent=47`, `applied=0`,
  `req=0` on the host). Live door messages were 0.0–0.3 s behind the host.
- Items after rejoin: the guest logged `items: snapshot n=50 changed=0` on its usual ~10 s
  cadence (06:29:25.565 onward) and stayed at `iApplied=0`. No item change was generated.
- **0** matches for `error|exception` in all three MelonLoader logs (host `6-26-19.log`
  26162 B, old guest `6-26-57.log` 17517 B, new guest `6-28-32.log` 21229 B).

### Derived

- A hard kill is detected by the 5 s silence timeout and handled as a normal disconnect; the
  host neither crashed nor stalled, and its own single-player simulation (Granny, doors)
  kept running.
- Counter freeze during the gap is by design (`Send*` is gated on `Connected && _peer !=
  null`), so "sent stopped growing" is a reliable signal that the host knows it has no peer.
- A rejoin is a fresh session in the protocol's eyes even though the host keeps its state:
  new endpoint, new name, new process. The guest's own DoorSync/ItemSync state is rebuilt
  by `OnSceneWasLoaded → Reset()` and then filled from a snapshot, exactly like a late join.
- Because the guest re-requests a snapshot as a matter of course on every fresh
  `OnSceneWasLoaded`, rejoin-after-crash is covered by the same code path as late join.
  There is nothing reconnect-specific to fix in the guest.

### Unknown / not yet verified

- **Graceful quit (`Bye`).** This run only exercised the timeout. `Dispose()` sends `Bye`
  on `OnApplicationQuit`, but no run has closed a guest cleanly to see whether the host
  drops the peer immediately instead of waiting 5 s.
- **Host death.** Not run yet; `-KillHostAfter` is implemented but unexercised. In
  particular, what the guest does after the host's 5 s timeout (keeps waiting? retries
  `Hello` every 0.5 s forever? reconnects if the host comes back?) is unmeasured.
- **Same-process reconnect to a restarted host.** `DoorSync._gotSnapshot` and the item
  equivalent are reset only in `OnSceneWasLoaded`, not on `OnConnectionChanged`, so a client
  that stays in-process and reconnects to a fresh host would never re-request a door
  snapshot. Unreachable in this run (the guest was a new process); whether it is reachable
  in practice depends on the host-restart behaviour above.
- **Door state across the gap.** The host kept flipping its test door while no peer was
  connected; the guest's first snapshot was the only thing that could have carried those
  changes and snapshot applies are silent, so whether the guest's doors matched the host's
  at 06:29:31 cannot be read from the log — only from the counters.
- **Item apply** is still unexercised (`changed=0`, `iApplied=0` in every run). Item visuals
  remain open as in §16.

### Next

Kill host (the `-KillHostAfter` path), then lag/loss, then soak. Record what the guest does
after its host times out, and whether a restarted host can be re-joined by the same process.
Item visuals stay open until the `CheckInventory` source can be decompiled with the owner's
approval.

## 18. Kill host (2026-10-23)

### What changed

- No code change. `tools/run-pair.ps1 -KillHostAfter` (added in §17) is exercised here for
  the first time: the host is Force-killed at a fixed offset from its own launch while the
  guest keeps running to the end of the run.

### Observations — run N (`kill-host.txt`: `-Seconds 250 -ClientDelay 38 -DoorTest -KillHostAfter 120`)

- Host pid 32636 started 06:33:59.586, guest pid 27612 started 06:34:37.619. Host killed at
  its own +120 s ≈ 06:35:59.6.
- Guest joined 06:34:43.588 (scene re-announced 06:34:43.590), reached `Menu` 06:34:59.672,
  `Scene` 06:35:12.986, player active 06:35:37.064, door/item tables prepared
  06:35:38.868 / 06:35:38.905. `applied=5` at 06:35:44.327 with one `(msg)` apply logged
  (06:35:39.801) — the same silent-snapshot gap as §16/§17.
- Host's `-DoorTest` produced 6 flips between 06:34:51 and 06:35:39; guest `recv` tracked
  host `sent` (`gRecv=1175` vs host `gSent=1182` at 06:35:44).
- **Guest timeout exactly 5 s after the host died:** `06:36:04.614 COOP peer 'Host' timed
  out after 5s of silence` → `06:36:04.614 COOP disconnected`.
- After the disconnect the guest **kept running in `Scene`** for the remaining ~2 minutes
  (run end 06:38:09). Its counters froze for good: `sent=688 recv=1472 doors=48 applied=5
  items=50 iSent=1 iRecv=4` identical on every 10 s status line from 06:36:04 to 06:38:04.
  No reconnect, no exception, no extra log line, no crash.
- The guest's own Granny started moving on her own after the disconnect:
  `0.5,7.4,0.7` (06:35:54) → `13.0,20.0,-5.4` (06:36:14) → `-18.9,13.2,-7.8` (06:36:24) →
  `29.0,11.7,-11.3` (06:37:14). At bind time the mod logged
  `COOP granny: 1 NavMeshAgent(s) found, 0 disabled on the client` — the agent was already
  disabled when `Slave()` ran, and nothing re-pins her once `ClearTarget()` fires.
- The guest's player kept its own local movement (`local=13.8,21.1,-8.8` etc. from the
  `--coop-testmove` driver), i.e. the single-player simulation continues normally with no
  peer.
- **0** matches for `error|exception` in either MelonLoader log (host `6-33-59.log`
  19369 B, guest `6-34-37.log` 21906 B).

### Derived

- Host death is symmetric with guest death: 5 s silence timeout, clean disconnect, no
  crash, no repeated log spam. The mod is a passive observer of its own traffic, so losing
  the peer degrades to single-player rather than breaking the game.
- The guest does **not** exit or stall when the host disappears. It stays in the gameplay
  scene with its last known door/item table and keeps simulating its own world.
- `SendHello` re-runs every `HelloIntervalSec` (0.5 s) while a joining client is
  disconnected (CoopNet.cs:114–119), so the same process would accept a host that comes
  back on the same port. That is code reading; no run has yet restarted a host under a live
  guest (next test).
- Client-side "slave" Granny is enforced by per-frame pinning, not by keeping the
  NavMeshAgent disabled: the log shows the agent was already disabled at bind and still
  something moves her once the target is cleared. Any future "pause when disconnected"
  feature has to keep pinning or re-disable the agent on `ConnectionChanged(false)`.

### Unknown / not yet verified

- **Host restart under a live guest.** The guest has no exit path; whether it auto-rejoins
  and how long its door/item state stays stale after a restarted host prepares its tables
  is unmeasured. This is the next run.
- **Same-process reconnect and the snapshot flag.** `DoorSync._gotSnapshot` /
  `ItemSync._gotSnapshot` are cleared only in `Reset()` (called from `OnSceneWasLoaded`),
  not on reconnect. The guest therefore does not re-*request* a table when it reconnects.
  Mitigation in the code: the host broadcasts a full door snapshot every
  `SnapshotInterval = 10 s` unconditionally (DoorSync.cs:142–148), and the item equivalent
  likewise, so a reconnecting guest should be repaired within ~10 s of the host preparing.
  Not yet observed.
- **Granny after a disconnect** — whether the client's own AI can catch and kill the local
  player while no host is present. Her transform moves; no player-Granny interaction was
  logged in either direction.
- **Graceful host quit (`Bye`).** Same gap as §17: only the Force-kill path has run.

## 19. Host restart under a live guest (2026-10-23)

### What changed

- `tools/run-pair.ps1`: `-RelaunchHostAfter` (seconds from the run's start, like
  `-KillHostAfter`), plus `$host2Log` / `$hostRelaunched`. A relaunched host is started from
  `$Test1` with `--coop-host --coop-name=Host2 --coop-autoplay --coop-testmove
  --coop-port=$Port -logFile unity-host2.log` and appended to `$tracked`, so it is stopped by
  pid at the end like every other process this script starts. The header comment lists the new
  switch.
- No mod code changed. The run answers §17/§18's open question: what a guest that never leaves
  the gameplay scene does when its host dies and a fresh host comes back on the same port.

### Observations — run O (`host-restart.txt`: `-Seconds 300 -ClientDelay 38 -DoorTest -KillHostAfter 120 -RelaunchHostAfter 140`)

- Host1 pid 30648 started 06:40:58.404 (mod init 06:41:03.353, listening 06:41:03.355); guest
  pid 32676 started 06:41:36.447 (init 06:41:42.023). Host1 killed at its own +120 s
  ≈06:42:58.4; Host2 pid 29324 started at run +140 s, 06:43:18.472 (init 06:43:23.885,
  listening 06:43:23.887).
- First session: host in `Scene` 06:41:23.482, guest joined 06:41:42.526
  (`COOP client 'Guest' joined from 127.0.0.1:63685`), host prepared 48 doors / 50 items
  06:41:44.283 / 06:41:44.319 (its player became active between the 06:41:33 and 06:41:43
  status lines; `DoorSync.Tick` returns until `player.activeSelf`, then waits `SettleDelay`).
  Guest reached `Scene` 06:42:12.566, prepared its own tables 06:42:38.538 / 06:42:38.575 and
  applied the host's snapshot at 06:42:38.651 (`applied=3` by 06:42:43.925). Host1 ran six
  `-DoorTest` flips 06:41:50.204 → 06:42:38.556.
- **Guest timeout exactly 5 s after the host died:** `06:43:03.433 COOP peer 'Host' timed out
  after 5s of silence` → `COOP disconnected` (same path as §18).
- Between the kill and the restart the guest stayed in `Scene`, its door/item counters froze
  (`sent=708 recv=1472 doors=48 applied=3 items=50 iSent=1 iRecv=5` identical at 06:43:04,
  :14, :24), and its unbound Granny walked on her own: `-13.2,20.0,-7.2` (06:43:04) →
  `-17.6,20.0,-10.9` (06:43:14) → `0.5,7.4,0.7` (06:43:24). `local=` alternated between a
  position and `-` as in §17/§18, and kept changing whenever it was present.
- **The same client process auto-rejoined 500 ms after Host2's socket opened:**
  `06:43:24.387 COOP client 'Guest' joined from 127.0.0.1:63685` on the host — the *same*
  endpoint as the first session (host1 line 26: `:63685`) — and `06:43:24.394 COOP joined host
  'Host2' as player 1`, `COOP connected to 'Host2'` 06:43:24.395 on the guest. That is exactly
  one `HelloIntervalSec = 0.5` re-dial tick after `listening on udp/47777` (06:43:23.887), so
  the reconnect loop described from code in §18 is now observed.
- Host2 needed 06:43:18.472 → 06:43:44.435 to reach `Scene` (25.9 s), then 19 s more until its
  local player became active (06:44:03.434), then the 1.5 s `SettleDelay`: door and item tables
  prepared at 06:44:04.956 / 06:44:04.990, **40.6 s after it accepted the join**. The guest saw
  the scene mismatch lines `host is in scene 'SplashScreen'` (06:43:24.395) and `'Menu'`
  (06:43:40.506) but does not follow (v1).
- While Host2 was still in splash/menu the guest's door `req` went 0 → 3 (06:43:24.190
  `req=0`, 06:43:34.194 `req=3`): its `Poll` saw local door changes and queued three
  `KindRequest`s (`RequestTries = 3`, `RequestRetry = 0.4 s`). `KindRequest` is ignored unless
  the host is `_prepared` (DoorSync.cs:760), so all three were dropped, and `req` stayed 3 for
  the rest of the run — nothing was re-sent after 06:44:04.956. Same for item changes, had
  there been any (ItemSync.cs:549).
- **The guest never re-requested a door snapshot.** Its `sent` counter was 1 on both sides of
  the restart and stayed 1 to the end of the run: `_gotSnapshot` was still `true` from the
  first session, exactly as predicted in §17/§18. Recovery came from the host instead.
- First door traffic after the restart: `06:44:11.020 apply …/ByroLoda -> open (msg)` — 137 ms
  after Host2's own flip #1 at 06:44:10.883 — and `06:44:11.340 apply All
  doors/SmallDoorParent (1)/SmallDoor -> open at 27.1m (msg)`, a door no test flip touched,
  sent by the host's `Poll` because its observed pose differed from its table (DoorSync.cs:565
  sends `KindState` on the host). By 06:44:14.289 the guest read `recv=13 applied=5`.
- Counters then grew normally for the rest of the run: guest `applied` 5 → 7 → 9 → 11 → 13 →
  15 → 17 → 18 → 21 → 24 and `recv` 13 → 40 against host `sent` 2 → 29. Three flip pairs
  measure live latency: 06:44:35.224 → 06:44:35.328 (104 ms), 06:44:47.226 → 06:44:47.302
  (76 ms), 06:45:35.241 → 06:45:35.346 (105 ms).
- Item channel: the guest logged `COOP items: snapshot n=50 changed=0` at 06:44:33.5, 43.8,
  54.1, 06:45:04.3, 14.6, 24.9, 35.2, 45.5, 55.7 — Host2's periodic item snapshots arriving on
  their ~10 s cadence. `iApplied` stayed 0 throughout (no item ever changed).
- The guest never left `Scene` (last `scene ->` line is 06:42:12.566), never re-prepared its
  own tables (`doors=48 items=50` constant), and resumed Granny pinning once remote packets
  came back (`gRemote` non-`-` from 06:43:54.222).
- **0** matches for `error|exception` in all three logs (host1 `26-10-23_6-40-58.log` 19396 B,
  Host2 `26-10-23_6-43-18.log` 22046 B, guest `26-10-23_6-41-36.log` 27352 B).

### Derived

- A restarted host is re-joined by the same client process with no user action, at the first
  0.5 s `Hello` tick after its socket opens, over the same UDP socket — so both sides see the
  same endpoint and name (`Guest`), unlike the fresh-process rejoin in §17 (`Guest2`, new port).
- The gap's length is set by the restarted host, not the guest: 40.6 s here (boot → `Scene` →
  first active player → 1.5 s settle). During that entire window the guest has no door/item
  authority to talk to and its changes are silently dropped, not queued.
- The 10 s host-snapshot mitigation from §17/§18 is consistent with what happened — both
  channels resumed on their own, the door within ~6 s of the host preparing and the items on
  the regular 10 s cadence — but door snapshot applies are not logged, so this run cannot
  separate the snapshot's contribution from the host Poll's `KindState` broadcasts.
- Because the guest's table and `_prepared` survive the restart (no scene load, so no `Reset`),
  the reconnected session reuses all 48 door entries and needs no `SettleDelay`; only the
  values had to catch up.

### Unknown / not yet verified

- **Stale pose repair is unproven.** If the guest holds a pose the restarted host never
  broadcasts (host observation equals its own table), only the periodic full snapshot can fix
  it, and snapshot applies produce no log line. In this run the two sides agreed after the
  host's first poll, so no persistent mismatch was observed.
- **Whether the periodic door snapshot actually reached the guest.** Host `sent` mixes
  `KindState` (poll observations and test flips) with snapshots (DoorSync.cs:567, :148), so the
  counter cannot tell them apart.
- **Request loss during host startup** has a real worst case: a door the guest changes while
  the host is in `SplashScreen`/`Menu` is dropped and never retried. Whether the host's 10 s
  snapshot corrects it depends on the host having a different value — unmeasured.
- **Granny across a restart** — she was pinned again after the rejoin, but no player–Granny
  interaction is logged in either direction, so "slave again" rests on the position stream,
  not on behaviour.
- **Graceful quit (`Bye`)** is still untested for either side.

### Next

Lag/loss, then soak, per §16. Item visuals and item deltas stay open until the `CheckInventory`
source can be decompiled with the owner's approval.

## 20. Lag and loss through the chaos relay (2026-10-23)

### What changed

- `tools/chaos-stats.py`: the packet log's `t` is milliseconds since the relay process
  started and `copies` are per-copy delivery delays in ms (`tools/chaos/relay.py:104-105`),
  so the gap section now divides by 1000 and the earlier seconds-based figures are void.
  Added per-packet door and item dumps so a packet log can be lined up with both game logs.
- `tools/chaos-rpc.ps1`: `Invoke-Chaos` no longer returns the parsed result, only prints the
  one-line summary; the return value made the harness log print formatted objects again.
- No mod code changed.

### Observations — run P (`bad-wifi.txt`; `-Seconds 240 -ClientDelay 38 -DoorTest -Relay -RelayProfile bad_wifi -RelaySeed 1003 -ChaosPlan 120:stats`)

- Relay `127.0.0.1:51407`, RPC 64603, artifacts `runs/chaos-20261023-070410/`. Host launched
  07:04:10.095, guest 07:04:48.975. Guest joined 07:05:07.873, loaded `Scene` 07:05:28.270,
  prepared its tables 07:05:49.143. Host flipped 10 doors; the game itself moved two more.
- **Final relay counters** (RPC `stats`, 07:08:0x): c2s seen 3347 / forwarded 3216 / dropped
  131 (loss 63, burst 68) / duplicated 18; s2c seen 6441 / 6172 / 269 (loss 129, burst 140) /
  21. The packet log, which keeps running a little past that call, has c2s 3355/3224/131/18
  and s2c 6450/6181/269/22. So **3.9 % c2s and 4.2 % s2c were actually dropped**, against the
  profile's 2 % loss plus bursts — this is not a profile that dropped nothing.
- One-way delay over delivered copies: c2s n=3242 mean 34.6 p50 34.9 p95 57.9 max 59.9 ms;
  s2c n=6203 mean 34.6 p50 35.0 p95 57.6 max 60.0 ms. Consistent with `bad_wifi`'s 35±25 ms
  clamped at 60.
- Drops by message type: c2s 131 `State`; s2c 146 `State`, 122 `Granny`, 1 `Door`.
  Delivered types — c2s: Hello 1, State 3221, Door 1, Item(`SnapRequest`) 1; s2c: Welcome 1,
  State 3274, Scene 2, Granny 2875, Door 22, Item(`Snapshot`) 7.
- **Join-time requests arrived exactly once**: one door `SnapshotRequest` (relay t=98.516 s),
  answered 15 ms later at 98.531 s; one item `SnapRequest` (98.553 s), answered by a snapshot
  at 98.605 s. No retries: guest `sent=1`, `iSent=1` from 07:05:58 to the end.
- **Door channel**: host `sent=22` counted sends (final status) plus one snapshot reply that
  `sent` does not count = 23 datagrams; the relay delivered 22 and dropped 1 at t=86.677 s
  (07:05:37.33). The guest had not prepared yet (tables at 07:05:49.143), so the loss had no
  guest-visible effect; the next door datagram arrived 1.76 s later.
- **Guest door bookkeeping**: 22 s2c door datagrams delivered, one of them (t=64.095 s,
  07:05:14.7) before the guest's scene load, so 21 received after `Reset` — final status
  `doors=48 applied=14 sent=1 recv=21`. 13 of those were logged applies and one was a silent
  snapshot apply (07:05:49.16, the reply to the join request), which accounts for 14.
- **Flip → guest apply**, host log to guest log: #3 106 ms, #4 107, #5 86, #6 104, #7 98,
  #8 65, #9 146, #10 100 (median ≈ 101). The direct-LAN baseline in §16 is 104/76/105, so the
  relay's 35 ms is not the dominant term at this sample size.
- Game-side door changes also crossed: door (5) closed 07:06:02.962 (guest applied 02.991);
  `All doors/SmallDoorParent (1)/SmallDoor` at 26.1 m opened 07:06:10.785; at 07:06:29.403,
  .405 and .420 three door states — (4) and (5) at 5.7/5.5 m and `SmallDoor` at 26.1 m — were
  sent in one host frame (three datagrams reached the relay in the same millisecond, with
  different impairment delays 12.0/14.1/29.1 ms) and applied in order, 30–31 ms after delivery.
- **Item channel**: host sent 6 periodic snapshots + 1 request reply = 7; relay delivered 7/7;
  guest `iRecv=7`. The first (07:05:37.385) arrived while its inventory handle was null and
  was discarded without setting `_gotSnapshot`, so the guest still issued its own
  `SnapRequest` at 07:05:49.15. The other six logged `snapshot n=50 changed=0`; `iApplied=0`
  throughout, because nothing changed in this run (no `-ItemTest`).
- **Snapshot cadence is a host-side artifact, not loss**: item snapshots applied at 07:06:06.127,
  :34.913, :45.255, :55.639 and 07:07:24.349 — gaps 28.8, 10.4, 10.3, 28.7 s. The host's
  `local=-` (its `Player` root null or inactive) in 13 of its 20 ten-second status samples, and
  both `DoorSync.Tick` and `ItemSync.Tick` early-return in that state, so the snapshot
  accumulators only advance inside active windows.
- **Granny channel reconciles exactly**: host `gSent=2997` = 2875 Granny datagrams delivered
  + 122 dropped; guest `gRecv=2887` = 2875 + the 12 duplicate copies the relay made of them.
- **Silence budget**: longest s2c delivery gap 0.31 s (at t=223.6 s); longest c2s gap between
  send attempts 3.58 s, from relay t=74.1 s — 07:05:24.73 to 07:05:28.31, exactly the guest's
  Menu → `Scene` load (scene line 07:05:23.871, scene load 07:05:28.270). `PeerTimeoutSec = 5.0`.
- **Session health**: every post-join status line on both sides says `connected=True`
  (20 host, 11 guest); grep for `timed out|disconnect|timeout|error|exception` over both logs
  returns nothing. At 07:07:48.752 the host entered `Menu`; the guest logged the `Scene`
  message 17 ms later and stayed in `Scene`, as v1 intends.

### Derived

- **The pass is not hollow.** 131 c2s and 269 s2c packets were dropped, 40 of them duplicates
  created, and the door channel lost one of its 23 datagrams — convergence was reached with a
  genuinely impaired link, not an unimpaired one.
- **Both reliable paths survived 4 % loss without retries.** A single request in each direction
  was enough because the answer came back well inside the retry interval; the retry logic
  (`RequestTries = 3`, `SnapshotRequestInterval`) never had to fire. Exactly-once held in the
  sense that no request was ever re-sent, so no duplicate request was ever possible.
- **Ordering held.** The three simultaneous door states in one host frame arrived with
  impairment delays spanning 12–29 ms and were applied in host order, so the relay's
  independent per-copy scheduling did not reorder messages from the same send burst.
- **Convergence after the only door loss was trivially safe.** The lost datagram predated the
  guest's table, so this run does **not** exercise "a periodic snapshot was lost"; what it does
  show is that the guest's table matched the host's at the end (14 applies, no counter
  divergence) and that a loss before prepare is harmless.
- **Latency measured end to end is game-loop dominated.** Flip → apply medians under
  `bad_wifi` (≈101 ms) overlap the direct-LAN baseline; adding 35 ± 25 ms one-way is lost in
  the ~50 ms polling granularity on each side.
- **The item cadence gap is explained by the `Tick` guards**, not by snapshots being dropped:
  relay delivered all 7 item datagrams, so every stretched interval is a host that was not
  ticking. This also confirms the `local=-` samples are real inactive periods, not status noise.
- **Time alignment**: six independent item snapshot applies pin the relay's t0 at
  ≈07:04:10.65 (agreement within ~5 ms), which is what let every relay line above be quoted
  in wall-clock time.
- **Reconnect/timeout accounting**: the host's `gSent` window starts at the host's scene load
  and the guest's `gRecv` at the guest's, several seconds apart; 2875 delivered + 122 dropped
  = 2997 exactly, so there is no counting gap to explain.

### Unknown / not yet verified

- **Why the host's `Player` root is null/inactive in 13 of 20 status samples under
  `--coop-testmove` is unknown.** The consequence is measured (snapshots stretch to 28.8 s),
  the cause is not. This also affects how representative any latency figure is.
- **A lost periodic door snapshot was not exercised.** The one door loss was pre-prepare. The
  next periodic snapshot should repair a dropped one within one interval, but no run has
  observed a snapshot being dropped and the repair that follows.
- **Whether a 3.58 s load silence would trip `PeerTimeoutSec = 5.0`** on a slower disk or a
  slower machine is unmeasured. The margin here is ~1.4 s; the guest reached `Scene` in 20.4 s
  from launch and was already connected throughout.
- **Item deltas and visuals** remain unverified: no item changed in this run, `iApplied=0`
  throughout, and the `CheckInventory` source is still not decompiled (needs the owner's
  approval).
- **Door snapshot requests and replies are not individually logged** (the reply applies
  silently), so their arrival is inferred from `applied` jumping at the moment of preparation,
  not from a log line.

### Next

The timed outage variant, then soak, per §16: `-ChaosPlan` with `disconnect:<ms>` and
`partition:c2s:<ms>` / `partition:s2c:<ms>`, expecting the 5 s peer timeout to fire and
checking that both sides converge on heal. Item visuals and item deltas stay open until the
`CheckInventory` source can be decompiled with the owner's approval.

## 21. Timed outage — attempt 1 aborted by the host machine (2026-10-23)

- Run Q: `run-pair.ps1 -Seconds 240 -ClientDelay 38 -DoorTest -Relay -RelayProfile bad_wifi
  -RelaySeed 1003 -ChaosPlan "110:stats;115:partition:c2s:6000;150:stats;160:disconnect:8000;200:stats"`,
  started 07:37:11. Host pid 29708, guest pid 28720, relay `127.0.0.1:57455` (RPC 49152).
- The harness process was killed 62 s into the run (07:38:14) because the machine was low on
  memory — not by the mod, the relay or a test assertion. Nothing in the plan had fired yet
  (first entry was at +110 s). Relay packet log stops at t=62.0 s; host had prepared 48 doors
  and 50 items (07:37:57) and run two test flips; the guest was in `Menu` (07:38:12.839),
  connected since 07:37:56.848.
- Both game processes and the relay died with the harness; `Get-Process Granny,python`
  afterwards is empty, so no orphan was left running against the owner's install.
- No outage observation exists from this run. Artifacts kept for the record at
  `runs/outage.txt` and `runs/chaos-20261023-073711/` (packets.jsonl 175 815 B).
- The run has **not** been restarted: it needs the owner's go-ahead after the memory pressure
  clears. §21 stays open.

### Attempt 2 — one-way partition (run Q2, completed)

- Same command, started 09:35:00.796, 240 s, exit 0. Host pid 32700, guest pid 4500,
  relay `127.0.0.1:62863` (RPC 60103), profile `bad_wifi`, seed 1003.
  Artifacts `runs/outage2.txt`, `runs/chaos-20261023-093500/`.
- **Observations (log lines):**
  - 09:35:46.049 host: `COOP client 'Guest' joined from 127.0.0.1:51041`; 09:35:46.087 guest:
    `COOP joined host 'Host' as player 1`.
  - Partition `c2s` started at ≈09:36:55.8 (plan entry at +115 s).
  - 09:37:00.952 host: `COOP peer 'Guest' timed out after 5s of silence` then
    `COOP disconnected` — exactly `PeerTimeoutSec = 5.0` after the last delivered c2s packet.
  - 09:37:05.981 guest: `COOP peer 'Host' timed out after 5s of silence` then
    `COOP disconnected` — the guest, still receiving nothing (host stopped sending after
    its own timeout) and sending Hellos into the blackhole, timed out 5 s later.
  - Partition healed at ≈09:37:01.8 (6000 ms from 09:36:55.8), so the guest's immediate
    re-Hello after its timeout was accepted: host `COOP client 'Guest' joined from
    127.0.0.1:51041` at 09:37:06.040, guest `COOP joined host 'Host' as player 1` at
    09:37:06.121, both `COOP connected` within 140 ms of each other.
  - Service outage: 09:37:00.95 → 09:37:06.04 = **10.2 s** (5 s host detect + 5 s guest
    detect, then instant rejoin). No user action needed.
- **Derived:** rejoin works after a one-way partition without clearing `_peer` (the host
  accepted the same endpoint 51041 again), and the join is symmetric — each side considers
  itself connected at its own timeout+rehello, and the host's `Welcome` makes them agree.
  Door/item tables and `_gotSnapshot` persist across the drop (they are only cleared in
  `OnSceneWasLoaded`), so no snapshot re-request was expected; the log confirms no new
  `item Snapshot` burst after rejoin.
- **Relay counters at the stats call (+150 s):** `partition:19` calls recorded, s2c
  `disconnect:2` — see the bug below.

### Harness bug: `disconnect:<ms>` collapsed to a single character

`run-pair.ps1` parsed `disconnect:8000` into `duration_ms = 56`, i.e. the ASCII code of
`'8'`. Cause: `$rest = if (...) { @($f[2..($f.Count-1)]) } else { @() }` — PowerShell
enumerates the if-statement's output, and a one-element array unrolls to a bare string;
`$step.Args[0]` then indexed a `System.Char`. `partition` was unaffected because it has
two arguments (a real 2-element array). Fixed by building `$rest` as `@()` and storing
`Args = @($rest)`; verified in isolation: `disconnect` → `duration_ms = 8000`,
`partition` → `['c2s','6000']`, `stats` → 0 args.

**Consequence: the full 8 s two-way outage has still not been tested.** Run Q2 exercised
only a 56 ms total blackout (2 s2c packets), which is not an outage. The Q2 partition
result stands on its own. A rerun with `110:partition:c2s:6000;150:disconnect:8000;200:stats`
was run as Q3 — see §22.

## 22. Timed outage — full 8 s disconnect (run Q3, 2026-10-23)

Closes the gap at the end of §21: the two-way blackout is now tested, and so is a repeat
`partition:c2s` run with the fixed harness.

- Run Q3: `run-pair.ps1 -Seconds 240 -ClientDelay 38 -DoorTest -Relay -RelayProfile bad_wifi
  -RelaySeed 1003 -ChaosPlan "110:partition:c2s:6000;150:disconnect:8000;200:stats"`,
  started 09:43:22.830, exit 0. Host pid 30292, guest pid 30508, relay `127.0.0.1:61328`
  (RPC 51147). Artifacts `runs/outage3.txt` (258 lines) and
  `runs/chaos-20261023-094322/packets.jsonl` (8183 packet rows; `start_wall`
  1792737803.404 → relay t=0 is 09:43:23.404). Every wall time below is a relay tick
  translated with that offset.
- The plan fired as written: `partition {"dir":"c2s","duration_ms":6000}` → `true` at
  t=109.759 (09:45:13.163) and `disconnect {"duration_ms":8000}` → `true` at t=149.757
  (09:45:53.161). The one-argument parsing bug from §21 stays fixed.

### Observations — relay timeline

- 09:44:08.197 host `COOP client 'Guest' joined from 127.0.0.1:50904`; guest
  `COOP joined host 'Host' as player 1` at 08.243. The host `local player active` at
  09:44:07.298 (scene `Scene` first seen at 09:43:48.388); doors prepared 08.858 (48),
  items 08.892 (50).
- 09:44:38.624 guest scene `Scene` (menu until then).
- **Partition:** last c2s delivered 09:45:13.039; partition on 13.163; 16 c2s datagrams
  attempted inside the 6.000 s window, all dropped (`partition:16`, all `State` — the
  guest had not timed out yet, so it sent no Hellos). Host `peer 'Guest' timed out after
  5s of silence` + `disconnected` at 09:45:18.070 = 5.031 s after the last delivery; its
  last s2c datagram was 09:45:18.020. Partition healed at 09:45:19.164 (6000.2 ms measured).
- The link was whole again at 19.164 but neither side used it: the host had already dropped
  the peer and never re-initiates (s2c `Hello` count for the whole run is **0**), and the
  guest still believed it was connected until its own timeout at 09:45:23.079 (5.059 s
  after 18.020). The guest's first Hello went out 3 ms later (23.082), the host accepted at
  23.110 from the **same** `127.0.0.1:50904`, and the guest rejoined at 23.174. Host-side
  input gap 09:45:13.039 → 23.110 = **10.07 s** for a 6 s partition.
- 09:45:24.354 guest `local player active` — 45.7 s after its scene load. Guest doors
  prepared 09:45:32.086 (48), items 32.123 (50); door `SnapshotRequest` (kind 3) at 32.090.
- **Blackout:** last s2c delivered 09:45:53.147; disconnect on 53.161. In the 8.000 s
  window (heal 09:46:01.161, exactly 8000 ms measured) the relay dropped
  **22 c2s** (16 `State` + 6 `Hello`) and **185 s2c** (91 `State`, 91 `Granny`, 2 `Door` —
  the periodic snapshot at 54.605 and flip #9's state at 57.858 — and 1 `Item` snapshot at
  55.228).
  - Host `timed out` at 09:45:57.876 = 5.040 s after its last c2s (52.836).
  - Guest `timed out` at 09:45:58.206 = 5.059 s after its last s2c (53.147); first Hello
    58.211, retried 58.727 / 59.233 / 59.734 / 60.247 / 60.766 (~0.5 s, `HelloIntervalSec`),
    all dropped, then 09:46:01.284 — 123 ms after the heal — the first delivered packet of
    any kind. Host `joined` 01.300, s2c `Welcome` 01.303, `Scene` 01.304, first `State` and
    `Granny` 01.319; guest `joined` 01.349.
  - Service gap: host 8.464 s (52.836→01.300), guest 8.202 s (53.147→01.349); the two sides
    agreed on connection state within 49 ms.
- Both rejoin paths used the guest's existing socket port (50904 three times in the host
  log), so reconnects do not need a new endpoint.
- **No errors:** zero `ERROR` / `Exception` / `CRITICAL` / `Failed` lines in either log for
  the whole 240 s.

### Observations — door and item convergence

- 23 host TEST flips, 09:44:14.779 → 09:47:21.830 (6 s period, `DoorTestPeriod`).
- Host counters: `doors=48 applied=0 sent=45 recv=2 req=0`. `recv=2` is the guest's single
  `SnapshotRequest` arriving twice — the relay duplicated it (`copies=[58.9, 41.8]`), and
  the host replied twice (s2c snapshots at 09:45:32.134 and 32.148). The second reply
  applied nothing; the request path is idempotent.
- Guest counters: `doors=48 applied=23 sent=1 recv=39` at 09:47:21.190, and one more
  `(msg)` apply at 21.919 after the last status line. 19 `(msg)` applies are logged in the
  run; the **5 silent ones** are `KindSnapshot` (DoorSync.cs:736-751 increments `Applied`
  without logging, DoorSync.cs:720-733 logs):
  - 3 in the first snapshot pair after the join (32.134 / 32.148), which is why `applied`
    was already 7 at 09:45:52.552 against 4 logged applies;
  - 1 in the periodic snapshot at 09:46:04.774;
  - 1 in the periodic snapshot at 09:47:15.861 — this is how flip #22
    (`Loda2 (5) closed -> open`, 15.829) reached the guest. There is no `(msg)` apply line
    for it, and the state packet the host sent at 15.860 *was* forwarded, so this is a
    receive-side miss, not a relay drop.
- Matching that: by 09:47:21.190 the relay had forwarded 43 door packets s2c, but the guest's
  `recv` (incremented in `Handle`, DoorSync.cs:714) was 39 — **4 forwarded door packets were
  never handled**. The guest's status lines show frame stalls (44:58.821 → 45:15.514 →
  45:52.552 against a 10 s period), so a receive queue overflowing during a stalled frame is
  the likely cause; which packets were lost apart from 15.860 is unknown.
- Consequence: the host's 10 s periodic full snapshot (DoorSync.cs:142-149) is not
  decoration — in this run it repaired the one state the guest missed after the blackout
  rejoin, without the guest having to ask.
- **Item channel:** 13 s2c item snapshots delivered (1 dropped in the blackout), median gap
  10.278 s, max 28.645 s across the rejoin. Guest `iRecv=13` matches the relay exactly.
  Every applied one logged `COOP items: snapshot n=50 changed=0` (9 lines); `iApplied=0` on
  both sides. The item apply path still has not run.

### Observations — relay counters and rates (whole run)

- c2s: seen 885, forwarded 813, dropped 72, dup 8 — `loss` 14 `State`, `burst` 20 `State`,
  `partition` 16 `State`, `disconnect` 16 `State` + 6 `Hello`.
- s2c: seen 7298, forwarded 6802, dropped 496, dup 24 — `loss` 69 State / 81 Granny / 1 Door,
  `burst` 76 / 83 / 1, `disconnect` 91 / 91 / 2 Door / 1 Item.
- One-way delay over delivered copies: c2s mean 34.9, p95 57.5, max 59.9 ms; s2c mean 34.6,
  p95 57.6, max 60.0 ms.
- s2c `State` and `Granny` both 19.4 Hz; s2c `Door` 33 state attempts (31 delivered) and
  15 snapshot attempts (13 delivered), snapshots ~10.1 s apart. c2s `State` median gap
  184 ms (p10 52, p90 376) — the guest instance's frame rate (~5 Hz), not the 20 Hz design
  rate.
- Duplicated packets by type: c2s 7 State + 1 Door, s2c 17 State + 7 Granny.

### Derived

1. Recovery after a full outage is entirely guest-driven. The host stops sending the moment
   it times out (`s2c Hello = 0` all run) and only listens; the guest's 0.5 s Hello timer is
   what closes the gap. Time to reconnect after the heal is therefore 0–0.5 s (123 ms here)
   plus one RTT for `Welcome`/`Scene`.
2. A partition longer than `PeerTimeoutSec` costs about **2 × 5 s** of service, not the
   partition length: the host detects at +5 s, and nothing can happen until the *guest* also
   reaches its own +5 s and starts sending Hellos — even if the link is already healed
   in between (Q2: 10.2 s outage for a 6 s partition; Q3: 10.07 s). A partition shorter than
   5 s never trips a timeout and costs only the packets inside it (Q2's earlier 56 ms run).
3. Door convergence after a drop has three independent paths — per-change `State`, the 10 s
   full snapshot, and the guest's correction request — and the snapshot alone was sufficient
   here. Duplicate requests and duplicate snapshot replies are no-ops on the receiving side.
4. `applied` is a superset of the `(msg)` log lines; counting from the log undercounts by the
   silent snapshot applies (5 here). Use the counter, not the log, for convergence claims.

### Unknown

- Why the guest's player took 45.7 s to become active after its scene loaded (the host took
  18.9 s for the same step), and why the guest's status cadence stalled 17 s then 37 s.
  Both processes plus the relay ran on one machine; load is the plausible cause, not proven.
- Why the guest's prep came 7.7 s after `local player active` when `SettleDelay` is 1.5 s and
  the host's prep came 1.56 s after its own activation.
- Which 4 forwarded door packets the guest's handler never saw, and whether any of them
  carried a state change that the 10 s snapshot did not later repair. The applied count is
  consistent with only flip #22 having been missed.
- Whether a guest whose Hello timer is mid-wait at the moment of the heal takes the full
  extra 0.5 s. Not measured (this run's Hello landed 123 ms after the heal).

### Tooling notes from this run

- `tools/chaos-stats.py` now separates control-plane rows (`{"event": …}`) from packet rows
  and prints an event timeline; before that, any `partition`/`disconnect` call crashed it
  with `KeyError: 'dir'`. Two log facts the tool relies on: `t` is **milliseconds** since the
  relay started, and `copies` holds the per-copy delivery delay in ms.
- The wire header is 9 bytes — magic(4) + type(1) + seq(4) (`Protocol.cs`, `MsgWriter`) — so
  the message type sits at hex offset 8 of `head` and a payload kind byte at offset 18. Both
  `chaos-stats.py` and the ad-hoc queries above use those offsets.

### Next

- Soak run (last item of §16): 30–60 min of scripted play with periodic comparisons and
  memory sampling (`-MemSample 30`), report mismatches and growth. The outage matrix itself
  is now covered for both one-way (Q2) and two-way (Q3) outages.
- The item apply path is still unexercised (`iApplied=0` in every run so far); item visuals
  and item deltas stay blocked on the `CheckInventory` decompile with the owner's approval.

## 23. The inactive-Player window, live snapshots, and a soak-ready status line (2026-10-23)

### What changed

- **v0.4.1** (`sanity-v041.txt`, 100 s, no relay): added `COOP player active ->` edge
  logging and the item-test refresh probe. First run that showed the Player root leaving
  the scene for long stretches.
- **v0.4.2** (`diag-v042-180s.txt`): added an `inBed` field to the edge line. The field was
  wrong (see Derived 4) and was replaced in v0.4.3.
- **v0.4.3** (`diag-v043-180s.txt`): edge line now prints the root position; snapshots log
  `stale=<n>` (cached state vs live `Observe`); sync ticks still early-returned on
  `!player.activeSelf`.
- **v0.4.4** (`diag-v044-180s.txt`): three fixes in `DoorSync.cs` / `ItemSync.cs` —
  1. `BuildSnapshotBody()` reads the world live (`Observe` / `Have.GetValue`) instead of the
     cached `d.State` / `e.Last`;
  2. `case KindRequest` refreshes `d.State` from the world before replying;
  3. the `!player.activeSelf` gate now protects only `Prepare` (discovery), not Poll or the
     periodic snapshot.
- **v0.4.5** (`sanity-v045.txt`, 100 s, `good_wifi`): the QA status line fires on
  wall-clock 10 s boundaries (`DateTime.UtcNow` second % 10) instead of a per-process
  10 s accumulator. Build `42B87FC9…`.
- **v0.4.6**: `StateFingerprint()` returns `not-ready` until `_prepared`, so two blind ends
  cannot compare equal as `0:811C9DC5`. Build `C9FB5AF4560259C018EB0AC100B9E800EE860569E1AED437BC11EC750FB4AB85`,
  deployed to both test installs; used for the soak.
- Earlier hashes for the record: 0.4.2 `CF9D1A6A…`, 0.4.3 `D4CBFE4D…`, 0.4.4 `86F070A8…`.

### Observations — the inactive window (v0.4.2–v0.4.6 logs)

- Host, `diag-v044-180s.txt`: `False 11:14:59.178 → True 11:15:18.411` (19.23 s),
  `True → False 11:15:27.257` (8.85 s), `False → True 11:15:45.619` (18.36 s),
  `True → False 11:15:53.786` (8.17 s), `False → True 11:16:12.142` (18.36 s),
  `True → False 11:16:21.136` (9.00 s), `False → True 11:16:39.488` (18.35 s),
  `True → False 11:16:48.023` (8.54 s), `False → True 11:17:06.377` (18.35 s),
  `True → False 11:17:15.133` (8.76 s).
- Guest, same file: inactive stretches 19.43 s, 18.17 s, 18.36 s; active stretches 31.5 s,
  14.35 s, 9.03 s.
- Across `diag-v042`, `diag-v043`, `diag-v044` the inactive stretch never left
  18.3–19.4 s; the active stretch varied 8.2–33.0 s.
- The root position alternates between `21.0/21.1` and `20.4` on the Y axis at every edge;
  everything else is unchanged. It is the same player at the same spot, not a respawn
  elsewhere.
- v0.4.3 (gate still on): host sends at `11:08:08.666` and `11:08:10.202`, the game reverts
  doors at the `11:08:12.838` edge, and the host emits no door traffic until `11:08:31.247`
  — the whole 18.4 s window. The guest meanwhile held `closed` for doors the host had
  reverted to `open`.
- v0.4.4 (gate moved): the host publishes 17 ms after the `11:16:21.136` edge
  (`... (5)/ByroLoda -> closed` and `... (1)/SmallDoor -> closed` at `11:16:21.153/.154`),
  applies a guest request while inactive (`11:16:29.366`), and emits snapshots at
  `11:15:30.167`, `11:16:01.080`, `11:16:11.445`, `11:16:21.770`, `11:16:32.109`,
  `11:16:42.510`, all `stale=0`.
- Door fingerprints, v0.4.4, guest sample paired with the host sample ~0.7 s later (the
  status cadence was not yet aligned): `CF01F842` (11:16:08.390/09.217), `988EE1A0`
  (11:16:18.394/19.222), `CF01F842` (11:16:28.789/29.224), `EC1DB641`
  (11:16:38.562/39.224), `EC1DB641` (11:16:48.399/49.230), `CF01F842`
  (11:17:08.504/09.238), `EC1DB641` (11:17:18.517/19.239), `EC1DB641`
  (11:17:28.581/29.246). Eight of eight match, several of them inside host-inactive windows.
- Item fingerprints matched in 8 of 9 pairs **but not at 11:16:58.503** (guest
  `D810CB9F` = none) vs **11:16:59.237** (host `D710CA0C` = have). The host's item test
  wrote `have=true` at `11:16:51.996` and the guest applied it at `11:16:52.192`; no
  further item message was exchanged, and the guest's own fingerprint read `none` 6.3 s
  later. `iApplied=8` at the guest sample is consistent with the apply happening once.
- v0.4.5 alignment check, `sanity-v045.txt`: host `11:21:50.002 dh=48:D21AACE8
  ih=50:D810CB9F`, guest `11:21:50.010` the same two values — 8 ms apart, first time the
  two ends printed a comparable sample.
- `run-pair.ps1`'s `stopping pid … (<time>)` prints the process **StartTime**, not the stop
  time (line 69). The "runs end minutes before their last log line" oddity in earlier
  sections is that label, nothing else; the label now says `started …`.

### Derived

1. The Player root's inactive window is the game's own sequence, not a mod effect: it
   happens with the mod's per-frame work outside the gate, on a regular ~18 s + ~9 s cycle,
   and the door table reverts at the same moment.
2. Any sync work gated on `player.activeSelf` is withheld exactly when the world changes.
   Discovery still needs an active player (the scene is not ready before it), so the gate
   belongs in `Prepare` only. This was the cause of the v0.4.2/v0.4.3 door mismatches.
3. Item `have` flags are not durable on the guest: the guest's game clears a flag the mod
   wrote while no item message is in flight, and the host never re-asserts it. Host
   `iApplied=0` and `ItemSync.cs:560` show the item channel is one-way host → guest by
   design, so the guest has no corrective path. This is a game-logic problem (the
   `CheckInventory` path), not a transport loss.
4. `FindRoot("PlayerInBed")` found a root present whether or not the player was in bed, so
   `inBed=True` on every edge in `diag-v042-180s.txt` proved nothing. Replaced by position.
5. The two `stopping pid` timestamps that looked like early termination are start times;
   there is no evidence of a run ending before its last log line.

### Guess

- The inactive window is the caught/respawn (or day-transition) sequence: the test player
  stands still, Granny reaches them, the root is hidden for ~18 s, and the game resets
  doors and inventory flags. The Y position dropping from `21.1` to `20.4` fits a scripted
  "put the player in bed" step. Not proven — it would need a decompile of the catch
  routine or a run with the player moved away from Granny.

### Unknown

- Why the active stretch varies from 8 s to 33 s for the same idle player.
- Whether `CheckInventory` runs every frame, on a timer, or only on day transitions, and
  which of the 50 flags it owns. Blocked on the decompile the owner has not yet approved.
- Whether the guest's cleared `have` flag is ever visible to the player as a missing item
  in the hand. The flag and the hand object are separate; `Refresh()` is only called by
  the item test.

### Review — stale-packet ordering on the client (code reading, not a test)

- The wire header carries a per-direction `seq` (`Protocol.cs:67-72`, parsed at
  `Protocol.cs:102`), but nothing reads it on receive: `CoopNet.cs` only uses `NextSeq()`
  when sending. So datagrams are applied in arrival order, not send order.
- The three client apply paths in `DoorSync.OnMessage` — `KindState` (`DoorSync.cs:744`),
  `KindSnapshot` (`:760`) and the `KindRequest` reply (`:806`) — all do the same thing: if
  the incoming state differs from `d.State`, `Apply` it and set
  `IgnorePollUntil = now + 2.5`. None of them can tell whether the state in the packet was
  read before or after the state the client already holds.
- Consequence, worst case: the client flips door D, `Poll` takes it as a local change
  (`:577`) and queues a `KindRequest`. The host applies it and answers `KindState`. A
  `KindSnapshot` generated just *before* the request landed still carries the old state;
  if the relay's jitter or its 2 % reorder delivers that snapshot after the reply, the
  snapshot branch reverts D on the client. `IgnorePollUntil` then stops the client from
  noticing its own world was rewritten, so the door stays wrong until the next snapshot
  (≤ 10 s, `SnapshotInterval`) re-asserts the host's state.
- So the failure is transient and self-healing, but it *is* visible to the soak's 10 s
  fingerprint comparison. A door mismatch that appears in one paired sample, disappears in
  the next, and is accompanied by an `applied` increment on the guest with no matching
  `COOP doors: send` on the host is consistent with this mechanism — not with a lost
  message.
- Fix, not applied yet: give each door a seq watermark and skip a snapshot entry whose
  packet seq is older than the last host message applied for that door (signed
  `seq - watermark < 0`). `r.Seq` is already parsed, so this needs no protocol change.
  Worth doing only if the soak shows the pattern above.
- The item path has the same gap and a narrower shape: items only ever travel host → join
  (`ItemSync.cs:528` `KindState` applies unconditionally, `:539` `KindSnapshot` applies
  when the live value differs). There is no item request/reply, so the race is only
  between a host `KindState` and a host snapshot generated earlier and delivered later —
  the same per-sender seq watermark would cover both tables.
- Code reading only; no run has shown a door or item reverting on its own yet.

### Observations — the v0.4.6 soak, aborted (2026-10-23 11:24–11:33)

- Stopped at t=583 s of 1800 s. Both ends sat on `scene='Menu'`, `doors=0`,
  `dh=not-ready` from 11:27:34 (host) / 11:28:54 (guest) to the end, so the remaining
  ~25 min would have compared two menu screens.
- The game returns to the menu on its own when the idle test player is caught: host
  `COOP scene -> 'Menu'` at 11:27:34.912, guest at 11:28:54.061 — 80 s apart, although
  `granny=` matched on both ends (2.9,12.3,-23.3 at 11:26:40). The mod reset its tables
  on the scene change (host `doors=0`, `dh=not-ready`) but nothing drove the menu again:
  `AutoPlayTick` returned early once `_autoPlayed` was set.
- Gameplay measured: host 11:24:56–11:27:34 (158 s), guest 11:25:16–11:28:54 (218 s).
- Paired comparison over that window (`soak-v046-aborted-window.txt`, built from the two
  MelonLoader logs): 20 paired samples, **12 door mismatches**, 8 item mismatches — and
  all 8 item mismatches are against a host `not-ready`, i.e. the guest simply kept
  playing 80 s after the host was already on the menu. Nothing about items here.
- The door mismatches alternate between a handful of values (`48:EC1DB641`,
  `48:CF01F842`, `48:35DFEF8B`, `48:E5D00C52`) and differ by one door:
  `HouseBakedReady/SovrumFurniture/Loda2Parent (5)/ByroLoda`, the nearest door to the
  player and the one both QA flippers drive.
- Timeline of the mismatch at 11:27:00/11:27:10 (guest `applied` 12→13, `req` 39→48):

  | time | end | event |
  |---|---|---|
  | 11:26:30.008/009 | both | status: `dh=48:E5D00C52` — the two worlds agree |
  | 11:26:32.606 | guest | TEST flips ByroLoda closed→open, locally |
  | 11:26:32.759 | host | applies `open (request)` from the guest |
  | 11:26:33.365 | host | TEST flips ByroLoda open→closed |
  | 11:26:33.775 | host | `refresh open -> closed (request)` then applies `open (request)` |
  | 11:26:38.129 | guest | TEST flips open→closed |
  | 11:26:38.301 | host | applies `closed (request)` |
  | 11:26:39.370 | host | TEST flips closed→open — **not sent until 11:26:40.854** |
  | 11:26:40.001/.003 | both | status: host `CF01F842`, guest `EC1DB641` — opposite |
  | 11:26:40.854 | host | `send ByroLoda -> open` |
  | 11:26:40.894 | guest | `apply ByroLoda -> open at 5.5m (msg)` |

### Derived — from the aborted soak

1. `-DoorTest` together with `-ClientDoorTest` makes both ends flip the same nearest door
   every 6 s (`DoorTestPeriod`). Each flip has to cross the relay and be applied, so the
   door is in flight for a large share of the time and a 10 s sample catches the two ends
   on opposite sides of a flip that is still in progress. That is a race between two QA
   flippers, not a convergence failure — both ends did apply each other's flip
   (`applied=15` guest, `sent=29` host; every mismatch is followed by the matching
   apply). The soak must drive the flip on **one** end.
2. `IgnorePollFor=2.5` is applied after every `Apply`, so a genuine local change made
   within 2.5 s of an applied state is not sent until the grace ends: the host's flip at
   11:26:39.370 was sent at 11:26:40.854. This is a real input-to-send latency on a
   recently synced door, distinct from the ping-pong above. Not changed yet — a shorter
   grace risks the echo the grace exists to prevent, and the single-flipper run has to
   show what the echo actually costs first.
3. The catch→menu cycle is the game's own behaviour, not a mod effect: the mod keeps the
   connection up across the scene change and both ends keep exchanging position and
   Granny state on the menu (host `sent` keeps rising).
4. Memory over 583 s: relay pid 8224 flat at 19 MB ws / 5 MB priv; host pid 33476
   740→693 MB ws, 2663→943 MB priv (startup release, not growth); guest pid 33136
   9→675 MB as Unity loaded its assets. Nothing to conclude about leaks from a run that
   spent its second half on the menu.

### Harness error — the backup DLL was loaded as a second mod

- v0.4.7 was copied into `Mods\GrannyCoop.dll` and v0.4.6 was kept as
  `Mods\GrannyCoop-0.4.6.dll`. MelonLoader loads every `.dll` in `Mods`, so both the
  current and the backup build were installed; the run's banner says
  `COOP initialised v0.4.6`, i.e. the **old** build was the live one. The 420 s
  re-entry run therefore tested v0.4.6 and its one-shot autoplay, which is exactly the
  behaviour it then showed (no autoplay after the return to the menu).
- Fix: backups moved to `<install>\CoopBackup\` on both test installs; `Mods\` now holds
  one `GrannyCoop.dll` with SHA-256 `C076A097…36CEB0B` on both. Check the version in the
  `COOP initialised vX` line at the start of every run, not the file copy.

### Observations — the 420 s run that actually loaded v0.4.6 (11:37–11:44)

- Both ends reached the gameplay scene (host 11:38:06, guest 11:38:47) and the host was
  caught at 11:41:13, the guest at 11:41:57; no autoplay followed on either end (the
  v0.4.6 latch), so the last ~3 min are menu time. The scene-change reset itself works:
  the host's `doors=0 applied=0 … dh=not-ready` from 11:41:20.
- Paired status samples: 17 comparable, 5 door mismatches, of which 4 are the guest
  still in `Scene` against a host already on the menu (`not-ready`). The one genuine
  world comparison, 11:40:50, has the guest on `48:988EE1A0` and the host on
  `48:EC1DB641`; both ends had applied each other's door (5) changes at 11:40:46.34–.39,
  and at 11:40:54.223 the host applied `(5) -> closed (request)` — so the guest's table
  and its own world disagreed for a few seconds and it asked for the state back.
- With a **single** door-test flipper (host only) there is no ping-pong, and the
  mismatches drop to this one. Not enough samples to call it convergence: the run was
  the wrong build.
- Item mismatches: 6, all against `not-ready` or the transient `50:D710CA0C` ↔
  `50:D810CB9F` pair seen at 11:39:50/11:40:20 — the guest's item table drifting by one
  flag and coming back. Same shape as before; unexplained at the time. (The `iRefresh`
  reading in this line was wrong: `Refresh()` is called only by the QA item test, never
  by `Apply` — see the v0.4.7 section below.)
- Packet log for this run (`tools/runs/chaos-20261023-113740/packets.jsonl`, 50-door
  snapshots every ~10.4 s): host→client door/item packets show **3** delivery-order
  inversions in 97 packets, each off by ≤2 seq and ≤4 ms. The seq-ordering hazard is
  real but rare at this profile, and no inversion so far spans a changed value. New
  `tools/door-order.py` prints them.

### What changed (v0.4.7)

- `AutoPlayTick` now re-drives the menu on every visit instead of once per process
  (`GrannyCoopMod.cs:377`): the per-visit state (`_menuFrames`, `_afterClick`,
  `_autoContinued`) is cleared whenever the scene leaves `Menu`, and if the menu has not
  advanced 4 s after the Continue click the Play/Continue pair is tried again every
  4 s, logged as `COOP autoplay: menu still up, retry #N`. The `_autoPlayed` latch is
  gone.
- Built as v0.4.7, SHA-256 `C076A097…36CEB0B`, deployed to both test installs.

### Observations — v0.4.7 run, cut short by the harness (11:46:36–11:50:16)

Same arguments as the failed re-entry run: `-Seconds 420 -ClientDelay 38 -DoorTest
-ItemTest -Relay -RelayProfile bad_wifi -RelaySeed 1042 -MemSample 30`.

- **The banner says v0.4.7 on both ends** (`COOP initialised v0.4.7 role=Host …
  doortest=True itemtest=True` 11:46:41.511; `role=Join … doortest=False itemtest=False`
  11:47:20.810), and `Mods\` holds one `GrannyCoop.dll` (83,968 bytes). The build
  question from the previous run is closed.
- The harness stopped the run at 11:50:16, 220 s into 420 s, because the machine ran
  low on free memory (Claude Code reaped the background task). `run-pair.ps1` did not
  reach its own stop-and-dump path, so the run file
  (`tools/runs/v047-reentry-420s-b.txt`) holds only the banner and 7 memory samples.
  Both `Granny.exe` processes and the relay had exited by 11:50:36 (no processes left).
  The two MelonLoader logs are complete up to the kill; I joined them plus the `mem`
  lines into `tools/runs/v047-reentry-420s-b-partial.txt` for `analyze-soak.py`.
- Menu entry worked first try on both ends: host `Menu` 11:46:58 → `Scene` 11:47:01,
  guest `Menu` 11:47:37 → `Scene` 11:47:42. **No catch happened before the cut**, so
  the v0.4.7 re-entry fix was *not* exercised; only the first entry was.
- Doors mapped: host 48 doors at 11:47:22, guest 48 at 11:48:02. The **enumeration
  order differs between the two installs**: host #1 is `All doors/InnerdoorParent
  (3)/Innerdoor` (`82963D81`) while guest #1 is `…/BastuDoor` (`59EC2878`), which is
  host #3. Same set of hashes, different order. Nothing on the wire carries the index
  (`BuildDoorBody` sends the hash, and `Protocol.Fingerprint` sorts by hash), so this is
  cosmetic — but it means door indices in logs are per-process, and no test may compare
  them.
- Paired status samples: 13, **0 door mismatches**. Two fingerprints other than the
  test door's two states showed up (`48:3ADFB417`, `48:5702B8E6`), but each appeared on
  *both* ends inside the same pair, i.e. the ends agreed while the world itself was
  moving; neither is a divergence.
- With the host as the only flipper the guest follows each Loda2 flip within ~1.5–3 s
  and there is no ping-pong. 13 pairs over ~130 s of steady state is a clean signal but
  still a short window.
- Items: 2/13 mismatches, both explained without a protocol fault:
  - 11:48:30 guest `D710CA0C` (have) vs host `D810CB9F` (none). The host flipped to
    none at 11:48:26.674 and the guest logged no apply until the 11:48:35.287 snapshot
    (`snapshot n=50 changed=1`) — one lost `State` delta, repaired by the 10 s
    snapshot.
  - 11:49:20 guest none vs host have. The guest applied have at 11:49:12.852 and was
    back to none by the 11:49:20.000 sample **with no apply line in between**; the host
    shows the same self-revert (`itemtest: +8s hold have=none` at 11:49:38.669, before
    its `Refresh`). So the have flag reverts on its own ~7–8 s after a write, on an end
    that never calls `CheckInventory`. This is the item problem, not packet loss.
- Correction to the previous section: `Refresh()` is called only from the QA item test
  (`ItemSync.cs:444`, `:464`); `Apply` never calls it (`:271-286`). So the host's
  `iRefresh=n/n` counts its own test cycles and the guest's `iRefresh=0/0` is expected,
  not evidence of a host re-applying its snapshot.
- Memory over the 7 samples (t=39→220 s): relay pid 31472 flat 19 MB ws / 5 MB priv;
  host 33196 755→760 MB ws, 2690→1262 MB priv (startup release); guest 35644
  16→769 MB ws, 9→2720 MB priv (Unity loading assets). Each instance settles at ~2.7 GB
  private, so a pair asks for ~5.4 GB of commit — the likely reason the run tripped the
  machine's memory pressure rather than any mod growth.

### Next

- The 420 s re-entry check is **unfinished**: (a) v0.4.7 banner — confirmed; (c) door
  convergence with a single flipper — 0/13 mismatches, clean but short; (b) re-entry
  after a catch — **untested**, and 420 s is marginal for it (earlier catches came at
  ~4.5 and ~5.5 min). Do not start it again unprompted: the harness stopped it for
  memory pressure, so re-run only when the machine has headroom, and prefer a longer
  `-Seconds` so one run covers both the catch and the re-entry.
- Then the 30 min single-flipper `bad_wifi` soak, with `-DoorTest` only.
- Still open: the ~7–8 s self-revert of the item have-flag (now observed without
  `CheckInventory`), `IgnorePollFor=2.5` delaying a genuine local flip, and the seq
  watermark (implement only if a soak shows an inversion that spans a changed value).

## 24. Guest scene follow (v0.4.8, 2026-10-23)

**What changed.** The host was already the authority for the scene (`MsgType.Scene`, sent on
every load and every guest connect), but a guest that received it only logged "v1 does not
follow". v0.4.8 makes the guest follow:

- On `Scene`, a **Join** that is connected and is not already in that scene sets
  `_pendingScene`. The deadline is set only when the requested name changes, so a host that
  re-announces at 10 Hz cannot starve the load.
- `TickSceneFollow()` runs from `OnUpdate`. It loads with `SceneManager.LoadScene(name)`
  outside the socket callback (a load inside the callback would run the whole
  `OnSceneWasLoaded` chain on the socket stack), retries up to 3 times 4 s apart, verifies
  success in `OnSceneWasLoaded`, and is cleared on disconnect.
- **Boot guard.** A real host announces its scene the moment a guest connects, which is
  while the guest is still in `SplashScreen`. Following then would skip the game's own boot
  and menu initialisation, so the request is held while `_scene` is empty or
  `SplashScreen`; it fires on the transition out of SplashScreen.

**A broken interop method, found the hard way.** The first version pre-checked the name
with `Application.CanStreamedLevelBeLoaded(scene)`. On this IL2CPP build
(Unity 6000.0.58f2) every call throws
`MissingMethodException: Method not found: '!0 ByRef Il2CppSystem.ReadOnlySpan'1.GetPinnableReference()'`
— the interop wrapper for that overload is broken, not the game. The check was removed;
`SceneManager.LoadScene(string)` works fine and the `try/catch` around it is the real guard.
Do not re-add that pre-check.

**Single-instance QA path.** A pair of game clients needs ~5.4 GB of private bytes and the
owner asked not to launch one without a go-ahead, so scene follow was tested with one real
client against a scripted host: [`tools/fake-host.py`](../tools/fake-host.py) speaks
protocol v1 (answers `Hello` with `Welcome`, `version=1 playerId=1`), sends `Scene` inside
scheduled windows, and keeps the 5 s peer timeout away with an idle `State` every 1 s. It is
Python 3, standard library only, binds `127.0.0.1`, and prints every event with a relative
timestamp:

```
python -I tools/fake-host.py --port 47778 --scene Menu --scene-at 10 --scene-until 20 \
    --scene Scene --scene-at 25 --scene-until 40 --seconds 60
```

**Runs (one client, `Host=Loopback`, port 47778).**

- *Follow Menu → Scene.* After Welcome and a `Scene` window in `Menu`, the guest logged
  `scene follow: loading 'Scene' (try 1/3)` ~0.5 s after the announcement, reached the scene
  in ~3 s, then built the world as a Join: `Player` root present, Granny bound
  (`1 NavMeshAgent(s) found, 0 disabled`, slave), 48 doors and 50 items prepared. Follow
  counter incremented once.
- *Follow Scene → Menu.* The reverse window brought the guest back to `Menu` the same way.
- *Boot guard.* With the first `Scene` window starting while the guest was still in
  `SplashScreen`: request accepted at 12:54:48.426 ("host entered 'Scene', loading it"),
  **no load happened**, the guest reached `Menu` at 12:55:03.935, and the queued load ran
  7 ms later, completing at 12:55:07.938. The whole boot was preserved.

**Observations / open questions.**

- Loading `Scene` directly from `Menu` skips the Play → difficulty → Continue path. The
  world still builds and the local player became active ~20 s later, but a real pair has not
  been run this way: whether skipping the difficulty selection changes anything the game
  later reads is **unknown**. A real host normally reaches `Scene` through Play, and the
  guest then mirrors that load; this is the path to test in the next pair run.
- Both runs used the scripted host, not a real host process. A real host's `Scene`
  announcement on connect is covered by the boot guard, but has not been observed live.
- A two-instance pair run is still the missing confirmation. Do not start one without the
  owner's go-ahead (memory pressure, §23).

## 25. Harness: the test install must be the working directory

**Observation.** Launching `Granny.exe` from the copied test install with `Start-Process` and
no `-WorkingDirectory` made the process quit at `Menu` every time, with the mod loaded and
`SplashScreen -> Menu` in the log and nothing after it. The same build launched as

```
Start-Process -FilePath 'C:\Users\Public\granny-test\Granny.exe' `
    -WorkingDirectory 'C:\Users\Public\granny-test'
```

plays normally.

**Derived.** `Steamworks.SteamAPI.RestartAppIfNecessary` compares the process's working
directory with the install directory Steam knows for app 962400 (still the owner's F:
install). When they differ it hands the launch to Steam as
`steam://run/962400//<args>`, the test process exits, and Steam starts the *install* copy,
which has no MelonLoader. So every QA run needs the working directory set to the test copy.
This is the reason earlier single-instance runs could look like "the mod does nothing".

**Side effect worth knowing.** The relaunched copy is launched through Steam, so if a stale
test process ever survives, two game processes can exist. Check for `Granny` in
`Get-Process` before starting a run.

## 26. Spectating through the death sequence (v0.5.0–0.5.3, 2026-10-23)

**What changed.** `CatchSync` used to spectate only while `playerCaught`'s flags were set.
Those flags drop about a second after the grab, while the death sequence keeps running:
Granny carries the body to the bed and only then does `playerDead.startEndScene` fire, which
was 30–60 s later in the QA runs. The spectator view ended before the player was even off
screen. v0.5.0 latches the dead state and holds it.

**Reading the script API without a decompiler.** `ReflectionOnlyLoadFrom` on the interop
stubs in `MelonLoader\Il2CppAssemblies` returns zero usable types, so the members were read
straight out of the metadata with `System.Reflection.Metadata` (`PEReader` +
`MetadataReader` + an `ISignatureTypeProvider` that prints type names). The throwaway tool
is `work/mdread` (`dotnet run --project work/mdread -- <assembly.dll> <needle>...`); it lists
fields with decoded types, methods and properties for every type whose full name contains a
needle, and never loads the assembly. That is how `playerDead` was found — it is not in the
public modding docs for this build.

`Il2Cpp.playerDead` (on the `GameController` object) has the durable death signal:

```
field Boolean endSceneRunning, endSceneRunning2, endSceneRunning3, endSceneRunning4
field UnityEngine.UI.Image blackScreenTexture, gameOverTexture
field UnityEngine.GameObject gameOverText, bloodscreenEnd
method startEndScene, startEndScene2, startEndScene3, startEndScene4, gameOverNoGranny
```

**What the mod does now.**

- Binds `playerCaught` on `Player` and `playerDead` on `GameController` (the latter through
  `GetComponentInChildren` then `FindObjectsByType(..., Include)`), logged once:
  `COOP death: bound playerDead on 'GameController' end=(False,False,False)`.
- Dead = a catch flag, or `endSceneRunning || endSceneRunning2 || endSceneRunning3`. The
  state is latched on the rising edge, so the one-second flag window is enough to enter it.
- The latch is held while the end sequence runs and for `DeathHoldSec = 45f` of continuously
  clear flags after that, unless the game revives the player first (`v0.5.1`, see below).
  A scene change (`Reset()` in `OnSceneWasLoaded`) clears it, so the Menu is not spectated.
- While dead the mod disables `blackScreenTexture.enabled` and `bloodscreenEnd` — they are
  screen-space UI and drew over the spectator camera. `StopSpectating` restores the image.
- The status line reports `catch=dead` when the end sequence, not a grab flag, started it.

**Observations — single client + `tools/fake-host.py` (`--state-circle 3 --scene Scene
--scene-at 15 --scene-until 300`, 200 s; log `26-10-23_14-16-8.log`).** The synthetic catch
came from the mod's own `catchtest` option, which walks the player onto Granny:

```
[14:16:54.830] COOP death: hid the death overlay so the spectator view is visible
[14:16:54.831] COOP catch: local player caught (granny)
[14:16:54.833] COOP spectate: watching the other player
[14:16:54.850] COOP spectate: using camera 'Camera'
[14:16:55.824] COOP spectate: cam='PlayerEyes' at (-1.7,1.5,2.5) target=(-1.9,0.0,2.4) yaw=321.7 pitch=0.0
...
[14:17:41.402] COOP catch: local player is alive again
```

That is 46.6 s of continuous spectating, through the game's own camera swaps
(`Camera -> PlayerEyes -> PlayerCamInBed`), with the camera at the remote player's eye
height (`y = 1.5`, remote `y = 0`) and yaw following the fake host's 3 m circle to within
a degree. The cycle then repeated three more times (releases at 14:18:29.718 and
14:19:18.908, fresh catches 1.2 s later), including one cycle where
`COOP player active -> False` at 14:19:28 while the spectator camera kept tracking — so the
view survives the local player becoming inactive as well.

**Follow-up: the game revives in place (v0.5.1).** A later run
(`26-10-23_14-31-1.log`) showed that the sequence is not always a trip to the game-over
screen: 26 s after the grab the game activated the Player root again and play continued.
The 45 s hold therefore kept the client spectating for another ~19 s after the player was
playable again. v0.5.1 releases on the revive itself: if the Player root went inactive while
the latch was up and is active again, and the game's death flags stay clear for
`ReviveGraceSec = 2f`, the latch drops. The 45 s hold stays as the fallback for a death
whose end scene has not started yet. The check is `GameObject.activeSelf` on the same
`Player` root the mod already uses (`NotePlayerActive`).

Re-run, same harness (`26-10-23_14-41-12.log`):

```
[14:41:59.643] COOP catch: local player caught (granny)
[14:42:07.549] COOP player active -> False
[14:42:25.912] COOP player active -> True
[14:42:27.947] COOP catch: local player is alive again
```

The view covered the grab and the whole carry (28.3 s, through `Camera -> PlayerEyes ->
PlayerCamInBed -> Camera`) and stopped 2.0 s after the game revived the player.

**The survivor side of the same edge (v0.5.2).** The other half of a catch is the peer's
view: a `Catch` message saying the other player is dead hides that player's standing avatar
(`RemoteAvatar.RemoteDead`), because the body is with Granny or in the bed and the capsule
would otherwise stand in the room as a ghost; the alive edge shows it again. Checked with the
scripted host (`tools/fake-host.py --catch-at 30 --revive-at 55 --state-circle 3`,
`26-10-23_14-48-52.log`):

```
[14:49:28.780] COOP catch: remote player caught (granny)
[14:49:28.781] COOP avatar: hidden, the peer is dead
[14:49:53.814] COOP catch: remote player is alive again
[14:49:53.815] COOP avatar: shown again, the peer is alive
```

The status samples straddle it cleanly: `avVisible=True` at 14:49:20 with `rcatch=?`, then
`avVisible=False ... rcatch=granny` for the 14:49:30–14:49:50 samples, and `avVisible=True`
with `rcatch=alive` at 14:50:00.

**Death → Menu: the scene change ends it (v0.5.2).** The death path that leaves the scene
instead of reviving was exercised by having the scripted host request `Menu` while the local
player was dead (`26-10-23_14-58-6.log`):

```
[14:59:22.064] COOP catch: local player caught (granny)
[14:59:22.743] COOP catch: scene change ends the death, local player is alive again
[14:59:22.744] COOP spectate: stopped
[14:59:22.745] COOP scene -> 'Menu' (buildIndex=1)
```

The spectator teardown and the overlay restore run before the old scene unloads, and the
alive edge goes out on the wire: `fake-host` logged `catch from guest: alive cause=1` 0.7 s
after the `Menu` request. Before v0.5.2 `Reset()` only cleared local state, so the peer would
have kept this player's avatar hidden until its own next scene change.

The same run re-verified the revive path: catch 14:58:52.085, `player active -> True`
14:59:18.414, `alive again` 14:59:20.415 (2.0 s), 28.3 s of spectating.

Note on the harness: after the guest reached `Menu`, `--coop-autoplay` kept driving
Play → difficulty → `Scene`, and scene follow pulled it back to the host's `Menu` every ~5 s.
That flip-flop is the test flag fighting the scripted host, not a game or mod behaviour; a
real guest sits in `Menu` and waits.

**Unknown / not tested.**

- Death → `Menu` was never observed: the QA runs were stopped while still in `Scene`, so
  `Reset()` clearing the latch on the game-over screen has not been seen on a real death.
- The two halves of a catch have only been checked separately, each against the scripted host
  (one client spectating, one client hiding its peer's avatar). Nobody has watched a real pair
  where one player is dead: whether the dead peer's position stream (frozen at the grab) and
  the hidden avatar behave together is unverified.
- Re-entry after a catch (guest back to Menu, then Play again) is still unverified, as in §23.

**Next.** Watch one full death in a real pair (needs the owner's go-ahead) and confirm the
Menu transition stops spectating and restores the overlay.

## 27. The item flag does not revert by itself — `CheckInventory` reverts it (v0.5.4–0.6.0, 2026-10-23)

§15/§23 blamed an unknown game updater for a have-flag write reverting after 7–8 s. A probe
(`--coop-itemprobe`, HarmonyX prefix/postfix on every non-accessor method of
`InventoryController`, entry/exit flag snapshots compared and logged) settled it.

- `InventoryController` declares exactly seven patchable methods — `Update`,
  `CheckInventory`, `placePlankaHole`, `NoText`, `textTimer1`, `NoObjectText`,
  `PlanktextTimer` — plus 506 property accessors, which HarmonyX cannot patch on IL2CPP
  ("field accessor, it can't be patched"), so those are skipped. A whole-assembly scan for
  `pickup` in a method name found 37 candidates on other components (`PickUp`, `PickUpNew`,
  bed/coffin buttons) and none on the controller: there is no "give item" entry point.
- A flag written from outside **holds**: still `have` at +60 s in a QA cycle with no
  intervening call. The game's `Update` never touches it.
- The flag alone moves **nothing**: at +1 s the hand object was inactive with zero children
  and the world model was still active. The game does not read the flag to build the hand.
- The only reverter is an explicit `CheckInventory()`, which re-derives every flag from the
  world objects. The game called it zero times in normal play; the caller was the QA refresh
  itself — so the "self-revert" was our own measurement undoing its subject.

**Fix (v0.6.0).** `Apply()` writes the flag, then `ApplyVisual()` activates the hand slot,
clones the world model into it (`Coop<Item>`, zero local transform) and deactivates the
world object; a drop reverses all three. Each item owns its replica (`Entry.Spawned`), and
`Reset()` destroys them. `CheckInventory` is never called in the sync path. The poll now
runs on both ends (a joining guest stays quiet until its first host table, so its default
flags are not broadcast), and `Item/State` is accepted on either role.

**Verified.** Two instances on one PC (port 47778, `--coop-itemtest` on the guest):
guest wrote `Hammare=have` at 15:35:51.439, host logged
`COOP items: apply Hammare=have was=none now=have obj=HammerHand(active=y ...)
tr=Hammer(active=n)` at 15:35:51.448 — 9 ms. The host→guest direction had been confirmed by
the same apply line on the guest in the run before. The shipped 0.6.0 zip was then re-run as
a pair: both ends logged `initialised v0.6.0` and the guest→host apply fired again.

**Unknown.**

- A **human** pickup (looking at an item, pressing the key) has not been exercised; the QA
  cycle writes the flag directly. Whether `PickUp`/`PickUpNew` write these same flags is not
  mapped — the main reason the item path is still "network-verified, gameplay-unverified".
- The hand model is a clone of the world model and may differ from the game's own hand
  prefab; no pickup animation or sound plays.

## §28 The pickup scan found no action method to call (v0.6.0, 2026-10-23)

`--coop-itemprobe` scanned every type in the game assembly for methods with "pickup" in the
name (declared-only, all visibilities). 37 hits, **all property accessors**: `get_/set_pickupButton`
on the bed/coffin/car buttons, `PickUp.get_/set_pickUp(Boolean)`, `get_/set_readyPickUp`,
`get_/set_pickUpGascan(AudioClip)`, `PickUpNew.get_/set_pickUpButton`, and
`InventoryController.get_/set_textPickUpOn`. No action method carries a "pickup" name, so a
bot has nothing to invoke; the interaction lives in the player's input path and the
component's `Update`/trigger. A human pickup therefore cannot be tested by the bot harness
(input injection or a person is needed). The item poll already reads any flag the game
writes, so a flag-writing pickup syncs unchanged; a hand-only pickup would not. Still
**unknown**. Next: patch `PickUp.Update`/`PickUpNew.Update` and log the flags around one
manual pickup.
