# Town action probe analysis

Source: the owner's read-only `coop-action-probe.log`, captured 2026-10-31 from the supported Steam
executable (`4d78fbfaa65b7d002d2208d02da809200f4a5723119521180e30100d6f1a75f2`). The log successfully
advanced from group 0 to group 1 and group 2. It contains no hero-placement call.

## Confirmed call data

- `buy_upgrade`: request bytes at `RCX` use the already documented 12-byte `{tree, code, hero_id}` form.
- `hero_recruit`: `RCX` is a wrapper whose first pointer is the stagecoach model; `RDX` is a local hero
  object, with roster id at `hero + 0x130c`; `R8` is the candidate index. Never send either pointer over
  the network.
- `hero_dismiss`: `RCX` is the local roster-panel object and `RDX` is the hero id. The panel pointer is
  process-local.
- `provisions_buy`: the observed values identify a local store context and a slot. The same slot was
  invoked repeatedly, so requests must represent one purchase per call or carry a measured count.
- `provisions_return`: `RCX` is an item-stack object and `RDX` is the count. The object pointer cannot be
  reused on another game process; identify the local stack by stable item/slot data.
- `wagon_buy`: like provisions purchase, the arguments include a local store context and selected slot.
- `wagon_sell`: `RCX` points to a temporary local request containing an inventory pointer, sale amount,
  and index. The inventory pointer is process-local; replicate stable values and resolve the local item.
- `heirloom_exchange`: `RCX` is the local exchange context. Its pointer graph is process-local. The action
  data is source type, source amount, destination type, and destination amount; host-side balance and rate
  validation are still required because the game's wallet deduct helper can overdraw.

## Runtime constraints

All game calls must run on the game thread through `ddcoop_poll`. Guest callbacks must suppress the
original local mutation before queuing a town request. Host callbacks may broadcast only after confirming
that the game applied the local action. TOWN_APPLY calls use each process's own game objects; pointers from
one peer must never be serialized. The action context log confirms call frames, but it does not prove that
the host can resolve an unopened or inactive screen context. That lookup must be implemented and checked
before enabling the corresponding remote action.

## Follow-up static analysis (2026-10-31)

Inspected the supported executable with `objdump` and the local RTTI cross-reference tool. These are
additional implementation facts, not yet runtime-confirmed:

- `hero_recruit` is a UI wrapper, not a context-free roster operation. Its RCX object points to a
  StageCoach model; the model's candidate pointer vector is at `+0x350/+0x358`, and its roster pointer is
  at `+0x80`. The function scans that candidate vector for the selected hero pointer before firing
  `HeroRecruitedEvent` (`EventManager` at `0x14117d8a8`, dispatch helper `0x1406b9120`). It then clears
  the matching roster candidate flag at `entry+0x1538`, increments campaign `+0x1708`, and runs save/
  refresh paths. A network payload containing only the stable hero ID is appropriate, but replay requires
  resolving the local StageCoach model and candidate record first.
- `hero_dismiss` similarly dispatches `HeroDismissedEvent` through local listeners and uses its roster
  panel object after the event to locate/update the hero row. The `RosterList` UI object has RTTI vftable
  `0x140EB1800`; its constructor begins at `0x1406a6840`. Do not call it with a peer pointer or a null
  placeholder.
- `StageCoach` model RTTI vftable is `0x140E8EB38`; its constructor/factory is around `0x140404590` and
  initializes a `0xd8`-byte model. Its pointer is not yet resolved from the live campaign town model.
- The supported-build address table already contains all seven action targets from the probe, but
  presence in the table does not provide a safe per-process context resolver. Each action must remain
  disabled or queued until that resolver and its own game-state checks are available.
- Town controller states are registered in a per-thread state list rooted at global `0x14117d900`;
  the helper at `0x14043a720` returns the current thread's list. Its entries are `0x18` bytes and
  include a pointer to the `state+0x10` subobject. `ProvisionsTownControllerState` is constructed at
  `0x1404308f4`, has vftable `0x140E72848`, and the parent controller stores it at `+0x70`. This gives a
  concrete, game-owned way to identify the active provisions screen on the game thread. It does not yet
  prove which state fields can safely reconstruct the `provisions_buy` callback context.

Next reverse-engineering target: resolve persistent model access (town/stagecoach, roster, provisions,
and Nomad Wagon) from the live campaign/UI state manager, then add explicit apply-readiness checks and a
FIFO host request queue. Keep guest mutation hooks fail-closed until apply is verified for that action.

## Town-state probe follow-up (2026-10-31, v9b DLL)

The latest test-copy log now shows the registered object's first-word vtable changing from zero to
`Darkest.exe+0xED2B48` (runtime `0x00007FF6E5432B48`) after campaign load. This is inside the expected
image, so the earlier apparent random pointer was an initialization-time observation. The active flag
was 1 while the vtable was zero, then 0 by the time the vtable became readable. This address is not in
the currently monitored state-vtable list, and the probe's five-second sampling did not capture any
recognized building-state class. The log also confirms the campaign/roster load, but contains no
screen-transition evidence. Next static step: identify the RTTI/vtable at VA `0x140E72B48` and trace
which constructor/registration path installs it; do not infer it is a provisions state from nearby
addresses alone.

Further static check: RTTI scanning finds no class vftable rooted at `0x140E72B48`; it is one pointer
past a small `std::function` lambda vftable at `0x140E72B40`, whose RTTI name refers to a focus-element
callback. That supports the stale/deactivated-object explanation, but does not identify which state was
active earlier. A v10 read-only probe now puts a hardware execute breakpoint on the verified registry
insertion helper `0x14043A7A0` and logs its RDX state subobject and `state-0x10` base vtable at insertion.
This should catch state entry even if it lasts less than the former five-second polling interval.

## Correcting the state-registry interpretation (2026-10-31)

The v10 log contains only two `TOWNREG` events, during campaign initialization. RTTI identifies the
first registered subobject vtable (`0x140E8EA00`) as `QueryListener<FindNonRosterHeroesQuery>`; its caller
is `0x140547FD5`, in the Stagecoach hero-recruit setup. The second registration is also an event-listener
path. The helper at `0x14043A7A0` is therefore a generic per-thread listener registry, not a town-screen
state stack. In these two events, subtracting `0x10` from the registered listener pointer reaches an
object with no vtable, confirming that the derived pointer is not a controller base. The earlier v9/v10
scan cannot establish which building screen is open, and the absence of more registrations is not
reliable evidence that the Provisions screen was not opened.

Do not use this registry to resolve active town screens. Next static target is the actual navigation and
state-selection path owned by `TownController`, then verify it against the known `TownControllerState`
constructors/vtables. No additional owner run is needed until that path provides a specific observable
or a better targeted probe.

## Correct TownController state source (2026-10-31)

Static analysis found the actual controller and transition function in the supported executable:

- Global `0x14117D8E0` points to the `TownController` instance.
- `TownController + 0x18` is the active state ID; `+0x90` points to an array of state objects indexed by that ID.
- `TownController::SetState` is `0x140430FE0`. It calls the old state's virtual exit at vtable `+0x18`, stores the new ID, then calls the new state's virtual enter at `+0x10`.
- The names table at `0x140DC9D50` maps IDs 0–16: HeirloomExchange, ChooseItem, ChooseHero, InspectHero, UpgradeFacility, Statue, Graveyard, Stagecoach, TownEventPurchaseItem, Nomad, ActivityFacility, HeroFacility, ActivityLog, ChooseBuilding, Embark, Provisions, Null.

This supersedes the earlier use of the thread-local listener registry. The v11 read-only probe reads the selected ID/object and breaks on `SetState`, so it can establish whether the requested screen was entered and identify its actual state object. Static findings are build-specific and remain gated by the known executable hash.

## Provision screen mismatch and direct UI probe (2026-10-31)

The owner confirms they reached the expedition Provision screen during the v11 run. The log nevertheless
shows only `TownController` state `Null` and no `SetState` calls. Therefore `TownController` is not a
reliable indicator for the visible ProvisionSelect screen in this flow; this is a probe-target mismatch,
not evidence that the owner skipped the screen. RTTI identifies `ProvisionSelect@Panel@TownUI` at vftable
`0x140EAF410`, and its vtable method at `0x140688A50` is the `PreChildrenShowInternal` override (the
adjacent lambda RTTI names also identify that function). v12 logs this panel method directly, along with
the TownController ID only as comparison. Do not infer the screen from the controller field alone.

The owner's v12 log confirms the direct hook: it captured two `PROVISION_SHOW` entries at `Darkest.exe+0x1407ea28d`,
both with panel vtable `0x140EAF410`. The supported executable hash matched. Campaign loading also found eight
activity objects and eight roster entries (IDs 2, 16, 29, 8, 1, 7, 17, 31). The later `Roster: 0 heroes` is a
subsequent unload/menu transition and does not invalidate the earlier campaign observation. No `TOWNSET` was
expected from this probe. No repeat Provision screen run is needed.

## Normal Nomad model and purchase transaction (2 Nov, test10)

Static checks against the pinned Steam EXE provide a persistent model route. Campaign buildings manager
is at +0x1138, map head pointer +0xc0 (save/iteration
0x1405877e0), MSVC tree node value +0x28. Normal NomadWagon has vftable RVA 0xe8db78 and child pointer
vector +0xa8/+0xb0. Its normal TrinketSupply child has vftable RVA 0xe8dac8; stock Inventory is +0x68.
The shard supply is excluded. Bounded read-only resolution lives in src/dd_shop.c and fails closed.

The normal panel's PreChildrenShowInternal (0x140686a50) creates context {stock Inventory*, estate
Inventory*} and a std::function callback at context+0x48. Estate inventory is Campaign+0xe88 only when
the alternate-mode flag RVA 0x2a5c08b is zero. The callback vftable RVA 0xeaf150 invokes 0x140688150;
its invocation ignores `this`, checks normal wallet gold, and calls the same adjusted-price function
0x140591560 as the purchase. A temporary captureless callback/context can therefore call the original
0x140686cf0 without a live panel. No game model fields are written directly by the integration.

Stock items are 0x5a8 bytes, qty +0, type hash +0x44, definition ID +0x88. The game accepts a visible
ordinal, skipping empty stacks; the wire carries physical slot, type, ID and quoted gold instead, and
each copy resolves its current visible ordinal. Require qty=1 and matching price before calling the
game. Afterwards require one added estate item, stock zero, and exactly one price deducted. The normal
game function dispatches its own inventory event. The owner confirmed two-way purchases in test10a
at 23:16, with matching gold/item/stock changes and verified guest restoration. Closed-peer shop timing
was not separately described.

Recruit correction: 0x1406b7860's first parameter is a capture whose first pointer is the genuine
BaseHeroRecruitDisplay, not the persistent StageCoach model. See the test11 route below. Do not synthesize
that display from a building pointer or mistake launcher roster sharing for live game instantiation.

## Test10 purchase refusal (2 Nov, 23:04; test10a correction)

Both runtimes connected and resolved the normal Nomad model. Host clicks and all 16 guest orders were
refused before the original game purchase function (no transaction or post-purchase logs). Test10's
combined guard did not identify the failing precondition, so these logs alone cannot name that field.

Static review found a concrete invalid bound: dd_shop_owned allowed 2048 estate slots. The normal game's
inventory/base.inventory.system_configs.darkest configures trinket_storage with max_slots 9999, including
reserved empty slots. The capacity function 0x1405d4980 uses Inventory+8/+0x10 vector length, minus occupied
count at +0x24, to calculate free slots. The bound must support this capacity, rather than just the few
populated items in the save. This is unrelated to the store's much smaller stock vector.

Test10a uses a named, bounded DD_SHOP_MAX_STORAGE=9999. The fixture now reserves all 9999 slots with only
two populated, and checks an item in the final slot, corrupt quantity and refusal of a 10000-slot vector.
Against the previous 2048 implementation this fixture fails three assertions; the correction passes.
Failure logs now distinguish wallet, estate storage, stock, identity/price, affordability and game refusal.
Host and guest receive a local refusal notice; a pre-call log records counts/gold before invoking the
original game function. The later successful owner test is recorded above and in HANDOFF-CURRENT.md.

## Normal Stagecoach integration (2 Nov, test11; first live test pending)

Pinned-EXE disassembly distinguishes two operations. Roster::AddEntry at 0x14057fd70 clones the source
Entry into a new 0x15c8-byte live roster wrapper, appends it and notifies roster listeners. The source
callback at 0x1406b7860 does NOT perform that addition: it consumes the candidate, updates the controller,
fires the normal recruit event, increments the recruit counter and requests saves/town refresh.
Remote apply must perform AddEntry and then this completion, exactly once. No explicit analytics or
PvP function is called by DDCoop. The local host follows the ordinary game UI path and broadcasts the
observed completion instead of recruiting twice.

The early guest hook is 0x1406a7b30, the real roster UI drop acceptance (target, position, dragData;
dragData+8 = source Entry). It requests host approval and returns 0 before the ordinary drop can add
the hero. Source IDs are Entry+0x130c. Live roster vector elements are wrappers with Entry at +8,
so their IDs are wrapper+0x1314; tests deliberately distinguish these offsets.

Campaign+0x11f8 is the buildings-map head pointer; nodes carry the model at +0x28. Normal StageCoach
vtable RVA is 0xe8eb38, children vector +0xa8/+0xb0, normal HeroRecruit store vtable 0xe8ea80, candidate
vector +0x68/+0x70 with stride 0x1540. Available flag is Entry+0x1538. Alternate mode and shard store
0xe8ebe0 are refused. Count/capacity are read through the normal game functions 0x1405814b0/0x1405815a0.
Candidates with Entry+0x13dc != 0 use a special roster-limit rule; test11 refuses these before mutation.

Genuine display vtable RVA 0xeb2410, model +0x80, row vector +0x350/+0x358 (stride 16, bound 64).
Show/hide hooks are 0x1406b5400/0x1406b56c0. Capture after show, settle 150ms, clear before hide destroys
rows. Row vtable 0xeae348, active flag +0x58, controller +0x158; controller vtable 0xed1eb8 and Entry
pointer +0x28. Match the exact persistent candidate pointer, not only its ID. After AddEntry, revalidate
the display before finishing; if a game listener closed it, mark resync rather than dereference stale UI.
Only the callback's one-pointer capture is reconstructed; all its referenced UI/model objects are real.

The main-thread FIFO peeks before popping. Recruit requests/applies wait for a normal live Stagecoach;
a local notice asks the player to open it. Later ordered actions remain behind the recruit. A consumed
duplicate can be rejected without a panel. Queue overflow and partial results request a new session.
The host can still perform a normal local recruit while the guest has closed its panel; the guest
catches up after opening. For the first test, keep both panels open and perform one action at a time.

Validation: bounded snapshot tests plus actual runtime hook/apply/FIFO code with fake game callbacks;
guest makes no local addition, host and guest each add then complete once, duplicate/full roster refuse,
panel invalidation never calls stale completion, and queue overflow is detected. Both new tests pass
MSVC AddressSanitizer. All 26 address signatures match uniquely. This proves the isolated behavior;
it does not prove the real game calling conventions or visible refresh until the owner runs test11.

## Normal estate trinket sale (3 Nov, test12; first live test pending)

Pinned-EXE static review: final sale callback 0x14069b400 takes one 16-byte capture:
{Inventory* at +0, signed quotedGold at +8, physicalSlot u32 at +0xc}. It clears the entire source stack
through 0x1405d2ed0, recounts occupied slots, adds quoted gold through the normal wallet function and
notifies wallet listeners. The callback does not use a UI object. Normal click/confirmation routes
0x14069aeb0 and wrappers 0x1406a2090/0x1406a20f0 converge here, before the first inventory mutation.
Original 13-byte prologue ends before the first LEA; no RIP-relative instruction is copied.

0x14069aeb0 obtains the sale base value from item definition +0xa4 and applies the existing adjusted-price
function 0x140591560 with modifier RVA 0x2a6db78. Do not substitute the purchase base value (+0x9c) or
purchase modifier (+0x2a6ba34). Source must be the campaign's normal estate inventory at +0xe88,
physical slot below the bounded vector/9999 limit, quantity exactly 1, type equal to normal trinket
type at RVA 0x2a5f7f4 and a known definition with positive sale value. Whole-stack removal makes any
larger quantity unsupported. Unsellable definitions remain eligible for normal purchases.

Guest confirms through normal UI, queues an identity/quote request, suppresses local callback. Host
checks current source and adjusted quote, then uses a temporary capture holding its own real Inventory.
Postconditions: exact gold increase, total matching estate item count minus 1, source quantity 0.
Partial/unreadable results mark resync and show a local instruction to close/rejoin. No direct wallet or
item writes, synthetic UI, extra Steam/PvP calls or changes to save recovery. Closed-peer inventory UI
does not block apply; visual refresh still needs the first live test.

DD_PROTO_VERSION=3: WAGON_SELL now has physicalSlot/count/type/ID/quotedGold like the buy identity envelope;
old slot/count-only sale packets cannot be decoded as current orders. Launcher LR_VERSION stays 2,
with the new pinned runtime kit ID ensuring matching peers. This was the initial sale checkpoint; see the expanded test12 work below.

The actual runtime bridge fixture covers guest suppression before mutation, serialized request and
host/guest apply, local host path, repeat competing sale (one credit/removal), stale identity/quote,
unsupported multi-quantity and source, gold overflow, and a partial game result requiring a notice/resync.
The bounded snapshot adapter tests normal estate definitions and slot limits. Native tests and MSVC
AddressSanitizer pass. These are isolated callbacks; first real sale is not yet validated.

Additional test11 logs from 02:53: host/guest connected around 02:41, guest ran with -nosave and restored
32 files / 172916 bytes at 02:53:04.641. No new town request/apply lines in that interval. Host launcher
was restarted before collecting logs, so its current launcher log contains no session actions. The
owner's eight missing-feature observations remain the evidence for those gaps; silence is not proof.

## Expanded test12: Hamlet transactions and F9 (4 Nov; first real game test pending)

49 unique 48-byte signatures now pass the known executable gate. Hook preparation uses a five-byte
near jump to a register-neutral fourteen-byte indirect relay; trampoline returns preserve RAX/AL.
An executable-buffer CPU fixture checks actual calls and restoration. No copied prologue has a RIP-relative
instruction. See dd_hamlet_bridge.inc for per-hook lengths. All hooks prepare before any are applied.

dd_hamlet validates the normal-mode flag, embedded Hero versus Entry identity, learned/selected skills,
UTF-8 names, wallet/rates, real registered panels, party ranks and real inventory objects. Commands carry
stable IDs and before-state quotes. Apply depth suppresses nested upgrade/party/UI rebroadcast.

Native normal paths: combat/camping toggles 5c9b10/5c9500; bounded name input through 7f1950; roster dismissal
6a8300 on a genuine roster panel; wallet add/deduct/notify for the twelve normal heirloom exchange rates;
normal QuestSelectHeroSlot SetHero 7d6f70 plus roster ChangeLocation 5817c0. Party guests chain intended
snapshots during pickup/drop/cancel before ACK. Changed native UI pointers are re-resolved, not reused.

Trinket drags capture both physical endpoints before mutation. Apply constructs a valid native Item via
3ca5f0, clears it with 5d2ed0, performs three native Inventory::SwapSlot calls (5d3eb0), destroys the temporary
with the full native Item destructor 3ca790 and refreshes affected heroes through 47ca10. Never memcpy a live
C++ Item. Validate class restrictions, duplicate equipped IDs and both final endpoints. Source finish/cancel
cannot restore a stale drag copy, including repeated callbacks. Unsupported recognized trinket sources are
blocked. Bulk auto-sort/unequip-all are guarded; individual manual drags are the test12 path.

The F9 panel is rendered into the game's GL framebuffer before SDL_GL_SwapWindow; the bootstrap filters
panel-owned SDL input. GDI text is drawn into an owned bitmap and uploaded as a texture. Shader/VAO/texture,
framebuffer, viewport, blend/depth/cull/scissor/stencil/raster/sRGB and upload states are restored. The launcher
publishes bounded local snapshots with sequence counters; hero commands retain stable IDs. Host/Join and save
preparation remain before launch. No PvP, PlayFab, monster-control or save-format patch is involved.

Automated evidence: nine runtime tests, six launcher tests; runtime bridge with native-operation doubles;
MSVC ASan protocol (one million fuzz inputs), Hamlet, recruit and bridge; genuine launcher UI/publisher at
96/144 DPI; SDL wrapper and real GL fixture (NVIDIA GL4.6); safe-bootstrap-repair fixture. These fixtures
exercise our dispatch and invariants but cannot prove native game calling conventions or visible refresh.
All eight new game paths and the F9 integration still require the owner's two-PC run. Test11's owner-confirmed
recruits/Survivalist and automatic verified guest restoration remain the regression baseline.


## 4 Nov 2026: test12 log regression and test12a static correction

The supplied host/guest logs show matching applied selections through sequence 38, including camping
seq20. This proves model changes for those actions, not visible widget refresh. The ordinary
CharacterDisplay callbacks call toggle at 7f774c (combat, return 7f7751) and 7f72f3 (camping,
return 7f72f8); AL=false skips their native UI update. Guest interception returns false while pending.
Replayed Hero toggles therefore need a deferred normal CharacterDisplay rebind, not a second toggle.
7f3500(display, Hero) rebuilds widget vectors +500/+518 (stride130), reads current Hero selected lists,
and updates selection alpha/counts. It is deferred to the next event pump so no live callback's widgets
are destroyed on its own stack. Require visible +60, displayed Hero +140, stable roster identity,
both vector bounds, normal Town root and CharacterDisplay primary vtable ed3f00. Hidden/other heroes
refresh through the next ordinary OnEnter. No synthetic widget/context is passed to this function.

Test12's flat screen registry at image+117d9c0 was the wrong discovery route. Normal TownDisplay lives
at image+117dbe0, primary vtable eb4e68. Constructor 6ce2c0 calls 6ce970(this+17a8). That constructs
RosterList at sub+17a0 => Town+2f48 (ctor6a6840, primaryvt eb1800), EmbarkParty at sub+1e98 =>
Town+3640 (ctor66d6c0, vt eabdd0), CharacterDisplay at sub+2118 => Town+38c0 (ctor7ea320, vt ed3f00).
The normal dismiss callback7eb740 also reads that Town global, adds2f48, then calls6a8300.
Resolve these genuine embedded panels afresh; reject wrong root/child vtables and mode. No scanning,
Arena/PartySetupDisplay path or copied UI objects. Regression fixtures now use this constructor layout
with the old flat registry absent; the former production resolver failed four assertions before fixing.

Guest launcher logs member-left then FAILED at00:17:25.430; runtime logs host disconnected. The host
later times out. The cause of the Steam leave is unknown. No Guild/party command is logged during the
connected interval, so Guild's reported regression is not isolated by these logs. Dismissal at00:21:07
was after connection loss and also had the real bad-resolver defect. Test12a latches live-stream loss,
stops shared edits on both roles and requires both games to restart; it does not claim to repair Steam.
No silent stream reconnection with stale ordered state. Initial save-load UI setup remains native.
Host/guest upgrade fixtures cover nested combat/camping toggles under apply-depth, one BUY only,
guest wait-before-mutation and deferred refresh. Add host-native upgrade result logs for the retest.


## 4 Nov: test12a owner confirmation and deferred dismissal (test12b)

Host/guest005644 bundles: matched skill selections seq1..16, upgrades17..19, party20..37 (one stale
request16 refused under concurrent changes), trinket operations38..42, rename43, recruit44 and exchange45.
Owner confirms selection/learning, party ranks/add/remove, trinkets/equipment, rename, exchange, recruit,
and ownership UI. Guest applied all45 with no sequence failure. This is now the broad live baseline.
Host-local dismissal hero19 at00:52:11.947 reported roster6->6 present1, so the old immediate post-check
returned false and emitted NO ordered APPLY. The host launcher subsequently saw roster5 at00:52:19.625;
the guest retained that hero. Later recruit had host5->6 vs guest6->7: real roster drift despite equal
ordered sequences. Connection remained live until host exit00:56:35.883 / guest disconnect00:56:39.253
(clock offset~3.4s). Guest original restored/verified; journal cleared00:56:42.362.

Static function6a8300 fires HeroDismissedEvent and marks RosterList+1e4 for later update (6a84d6),
not a synchronous Roster::RemoveEntry. Test12a's fake native operation removed immediately, concealing
this integration defect. Changed fixture schedules an event, later explicitly completes the game tick;
old production fails seven assertions. Test12b calls the normal callback exactly once, retains a bounded
snapshot of ALL roster IDs and campaign identity, and waits for target absence plus exact survivor set.
Host-local actions broadcast observed only AFTER completion; remote request/APPLY stays at FIFO head
until local completion, then existing dd_town authority commits its original request/sequence once.
Check guest sequence before ANY native side effect. Shared local mutations wait; genuine unchanged party
UI refreshes remain native. Timeout5s, changed campaign, missing/wrong survivors, lost session or reset
require resync; never declare a pure refusal for a queued event that may still execute later.
Only ordinary spare heroes outside party/buildings are admitted in this kit. The native event route,
not a copied Entry/UI model or Arena path, owns deletion and cleanup. Wire protocols remain unchanged.
New fixtures cover host-local and guest-requested deletion, async guest apply, subsequent purchase FIFO,
single native event, stale/replayed/gapped messages, wrong removal, timeout and disconnection. Still
requires the owner to test host and guest dismissal in the real game. Fresh session repairs prior drift.


## Ode to Mods / build28385 port (5 Nov 2026)

Official 4 Nov announcement: https://steamcommunity.com/app/262060/announcements/
Public feature list: https://docs.google.com/document/d/1zkAFFAiRS-q11wW06DiSHo5QSGyzACnXgr8CAxW1dfY/edit
The list mainly adds data-driven effects, buffs, targeting, skill/trinket options; no co-op transport API
was identified. It does not replace our native/network/save work. Native structures really changed.
Read legitimate old EXE4d78... from the owner's test copy and updated Steam EXEf9d914ca...; no game ran.
Local Steam manifest build26437048;50 old signatures yielded only5 unchanged unique raw matches.
Candidate ranking was research only; never a runtime fallback. Verified counterparts with normalized
native instructions/callers, MSVC RTTI COL/type names and RIP-relative references. New50 patterns unique.

Normal Campaign global118e138, StorageManager1185248, TownDisplay118e360, item library118e0f8,
normal-mode flag2a6d4bb. Gold-type cache2bb41f0, trinket purchase modifier2a7b404, sale2a7dca4,
trinket type cache2a6f868 (static hash initializer133670 of literal trinket). Trinket definitions2c3c570.
Hero/Entry late fields shift+70: ID137c, roster-wrapper ID1384/location15b0; roster wrapper allocation1638;
candidate stride15b0/available15a8; special-limit144c. Hero trinket inventory1280, class1320; camping
selected1398/13a0, learned13b0/13b8; combat selected13c8/13d0, learned13e0/13e8. Name+8 and live Entry+8
embedding unchanged. Native recruit/show/location functions confirm the shift; native equipment helper
7ed2d0 confirms definition stride488 and allowed-class vector2d8/2e0. Item stack remains5a8; native buy/sale
quote functions confirm definition type40/id84/price9c/sale-a4, unchanged ordinary wallet/campaign fields.

Real Town constructor6da6e0 ->6dad90 retains embedded offsets2f48/3640/38c0; RTTI maps roster ec00a0,
EmbarkParty eba728, CharacterDisplay ee2ae8, Town ec37a8; HeroSlot ebcc00, quest slot ee0838.
Stagecoach display ec0cb0/recruit slot ee0990; buildings Nomad e9c338/supply e9c2e0, coach e9d088/store e9cfd0.
Nomad captureless affordability function6944f0 and its std::function vtable ebda18 are the same normal path.
Combat toggle5d4f70 has a new prologue: safe copied length13, old14 splits the next instruction. All28
hook regions verified complete and free of PC-relative operands. Normal selection callers return8045f1
(combat call8045ec) and8040c8 (camp call8040c3). Rebind8002d0. Quest input range7e3500..7e3f60;
roster input6b3ee0..6b44b0. Dismiss6b46b0 retains the normal event/deferred-row semantics;12b fix retained.

New test_game_build.py independently inspects real new EXE fields/RTTI/prologues/callers; no test-game load.
Initial bridge fixture retained three absolute fake old vtables, caught8 assertions; corrected to new RTTI
values, bridge then passed. Static-audit mode-flag sample initially used a wrong helper window; corrected
to the actual ordinary roster-count reference. Source mode RVA was already correct. Do not mistake source
unit/ASan/static successes for live validation; the new owner test is pending. No PvP/PlayFab path used.
