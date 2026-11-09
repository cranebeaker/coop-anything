# Test52 / duo35: real refusal capture, including no food (Codex, 8 Nov 2026)

Owner duo34 run18:31: both copies detected Hunger prompt1 in area43726f63 with
same Rules checkpoint1c172bf1, but Eat nothing produced no order. The refusal
filter incorrectly interpreted lambda-data+8 as a RaidDisplay pointer. Native
ShowHunger actually stores a FOOD FLOAT there, followed by an unused copied
32-bit field. The existing fixture had copied this wrong pointer assumption.

Correct refusal capture: overlay+0, food float+8, ignored trailing word+12.
Validate its food value against the prompt, retaining cached display/area checks;
replay the same native layout on the other copy. The eat callback still takes
the full std::_Func (overlay+8, food+10). Added chosen/cost-mismatch logging.
No wire/protocol change (23), no hook-address/prologue change; address comment fixed.

A new regression supplies real-shaped captures with non-pointer trailing bytes
and insufficient food: both roles failed before the fix, pass after. Fixtures
now model the native layout and verify zero/two-food refusal from both roles,
host approval/replay, unchanged food and one native HP/stress consequence. Existing
feeding/double-click/delayed-prompt/ownership/loading tests remain included.
The EXE proof now independently verifies BOTH native lambda builders' overlay
and float stores at7123bd/7123c2 and7125e2/7125e7, instead of trusting the fixture.
Duo's16ms pacing is unchanged; the latest screenshot shows about61fps. No claim
that the prior NVIDIA TDR is fixed. Owner's live refusal check is still pending.

# Test51 / duo34: hunger choices; lower duo render load (Codex, 8 Nov 2026)

Owner's duo33 screenshot: guest had eaten (8 food vs host12, one healed HP), host
still had the hunger prompt. These native callbacks were not hooked. Hunger now
uses DD_ITEM_OP_HUNGER=19, runtime protocol23. Capture normal ShowHunger's per-prompt
epoch, overlay/display identity, area, native food type and max(0,1+hero+bc4) sum.
Eat/refuse are host-authoritative ordered choices; a guest waits for approval,
either side can choose, late/crossed clicks cannot consume twice. Application waits
up to30s for the same prompt, validates cost/food, then calls the original native
callback with its documented captures. No direct mutation of food/HP/stress or RNG.
Other campaigns/single player stay native; curio props sharing the overlay refuse.

New functions on the exact pinned EXE: ShowHunger712080 (14-byte complete prologue),
eat713bf0 (std::_Func overlay+8, float food+10;13 bytes), refuse712800 (lambda data
containing overlay+0/display+8;15 bytes). gen_address_table now116 unique48-byte
patterns. tests/test_hunger_layout.py verifies callback vtables, forwarding thunk,
prologues, cost field, native food global and dispatch. Native fixtures cover both
choices and directions, delayed UI, first-choice/duplicates, food modifiers/split
stacks, reject/unlock, curio distinction, malformed payloads and session loss.

Owner's later logs17:42 showed exit0xc0000409. Read-only local Windows records and
both crash dumps confirm NVIDIA OpenGL TDR, not a proven mod exception: Display4101
17:42:07.842; NVIDIA Application1 error7 for both games; fault nvoglv64+10b30a9,
fast-fail info7 on NVIDIA worker threads. User's desktop went black and Wallpaper
Engine also crashed. Exact trigger unresolved. Duo34 paces frames with a16ms
minimum (~60FPS) only in DD_DUO; no busy-spin/catch-up, normal two-PC runtime unchanged.
This reduces load as a mitigation; it is not a proven driver fix. No driver/registry
or system setting changed. Existing Rules and ownership repairs remain in this build.

# Test50 / duo33: bind restored-raid Rules before the save starts (Codex, 8 Nov)

Owner's duo32 run17:12-17:14 confirms guest ownership and request/apply replay.
Loot differs at Victory. Battle stream was reseeded equally25794603, but Rules
was clock-seeded491712938 vs965984767. No Departure restored seed appeared: the
warm native load entered RaidStart before the slot-watch-enabled town hooks.
The first logged Rules checkpoints already differed1d4ef1aa vs3993c1ff.

At the validated normal save row, after authentication but before advertising
LOAD readiness, bind the known Rules stream and install the existing RaidStart
hook once. Arm that single configured-slot load, allowing its restored Rules
seed even while the campaign watcher still reports inactive. Other inactive
calls pass natively. Defer all other town/map hooks as before. The later full
hook set excludes RaidStart and refuses a collision with its dedicated target.
The same Rules binder is reused by the later rand_s binding. No new addresses,
wire messages or protocol changes. A binding failure leaves the save unloaded
and does not advertise readiness. Manual save clicks still bypass auto-load
coordination; the owner test uses automatic loading.

Regression: fast load reaches RaidStart synchronously with both campaign_active
and hooks_installed false; same seed before native start on both roles, retained
through inactive polling. Failed binding/no session/multiple slots/other mode
must not arm/load. Existing departure, ownership and duplicate-target checks stay.
This fixes timing before generation; it does not copy loot or repair loot already
generated in an earlier divergent run. Live test remains pending.

# Test49 / duo32: restored party ownership (Codex, 8 Nov 2026)

Owner's duo31 logs16:42-16:47: both copies loaded slot3 within16ms, used Rules
seed92843430 and logged26 identical combat snapshots. No Heroes table was set;
seven guest picks were dropped as another player's hero. The launcher's file
correctly assigned IDs1 and2 to the guest, including revisions2..8. A saved raid
skips departure ownership setup; this affects normal and duo builds alike.
The first ownership table is now host-only, ordered, validated against live IDs,
and deferred until the guest slot and party are ready. Canonical IDs stay fixed
through formation changes. Small blocks show the owner at each current rank,
independently of the active actor; unknown ownership is gray. The large strip
still shows the turn. Tests cover delayed loading/slot-watch, guest request/host
acceptance/replay, shuffled formation, both front heroes, turn-independent blocks,
invalid membership, host defaults and normal embark/live-owner behavior.
A new real-game run is pending; test evidence is shipped with the kit.

# Test34 corrections (Codex, 6 Nov 2026, based on dd977f1)

Owner's test33 run 18:35-18:46 (host 184835, guest184659): two fights reach the same outcomes; initial
owner mask is zero, and the later host INI revision3 assigns heroes1/2 to the guest. The old runtime
never rereads it. First take-all reaches the guest while battle state34 is still finishing; it is logged
as applied before the battle-over line. Host then crashes with access violation (exit3221225477) after
an unsupported-trinket-source notice while equipping secret-room loot. No exception stack is supplied,
so the exact faulting instruction is unproven; the unsupported source path is independently reproduced.

Test34 changes:
- The actual hook table had duplicate addresses: bag_drop/source_a/source_b replaced trinket drop/cancel/finish.
  These are now single dispatch hooks, with test/install duplicate-target guards. Native fixtures formerly missed this.
- Native trinket transactions recognize the local raid bag (+218) and loot (+5240), using stable wire
  endpoint tokens, never process pointers. Class/duplicate/stale-state checks and native Item lifetime remain.
  Secret-room fixture independently equips one of two trinkets on both copies and preserves the other.
- Loot waits for the actual native input condition in715610: state2, +170 enabled, no display+4118 blocker
  or queued presentation +40e0/+40e8, and combat finished. Earlier frames clear+218/+219 at717014.
  The guest's60-second unshared-click fallback is removed; missing readiness fails rather than claiming success.
- Host ownership changes are ordered at a skill/target input boundary or outside a fight, every500ms at most.
  The standalone bounded parser enforces written_by=host. Invalid/missing live updates retain the agreed table.
  This explicitly supersedes the older frozen-owner design at the owner's request.
- Normal provisions callback6e7a70 (capture+8=localTownDisplay) is now DD_ACT_PREPARE. It opens the genuine
  panel and chooses app state0x0a. Local quest index/key and normal panel identities are checked. An already
  opening/open panel is idempotent and never refilled. No Circus callback is invoked or hooked.

Native fixtures are not a live game run. The owner's new test must confirm actual loot completion,
secret-room equip, mid-fight F9 handoff and following the normal provisions screen. Earlier queued party
orders still require genuine expedition slots to exist; those are not fabricated by this opener.

# Dungeon foundation

Live since test16: only "Departure, step 1" at the end of this file. Everything else here is still source-only.

The portable coordinator, ownership reader and proposed packet codec are implemented. They are not
linked into the live runtime, launcher, F9 panel or installer. Test13 remains the build28385 Hamlet
retest kit. It does not synchronize mission departure, walking or combat. Do not start a co-op raid
with that kit. No game, Steam session or real save was launched/modified for this foundation.

## What exists

- `src/dd_dungeon.h/.c`: departure/readiness, frozen owners, authenticated-sender authorization,
  ordered commands, explicit native completion, per-peer acknowledgements, release barriers,
  timeouts and a latched resync state. The module never calls a game function itself.
- `src/dd_dungeon_owners.c`: bounded reader for the agreed launcher INI v1. Missing/non-host table
  means all host; malformed host table blocks departure. Unlisted heroes default to host. Only the
  host adapter will read this file. Guests use the host's copied departure table.
- `src/dd_dungeon_wire.h/.c`: strict, explicit little-endian DNG1 proposal. No native object layout,
  pointers or padding. It does not replace DDCP or add a live message router.
- `tests/test_dungeon.c`: three simulated copies, the real proposed codec, canonical fixture-state
  HMAC-SHA256 digests, separate Park-Miller streams and deliberately delayed native completion.

Core capacity is four players. The test uses three to exercise multiple owners; the current launcher,
runtime transport and ownership INI still support TWO. INI v1 still rejects owners outside 0..1.
Nothing in this step adds a third live player or changes the launcher interface.

## Expedition flow and adapter obligations

1. On the game thread, quiesce shared Hamlet/provision mutations and drain their ordered FIFO.
   Read the actual selected quest, dungeon, ranked party IDs, provisions and current gameplay state.
   Parse the host ownership file once and copy the resulting table into the departure offer.
   Missing or unreadable/malformed data is a refusal, never an invented default quest or hero.
2. Host generates a fresh nonzero 64-bit expedition ID using the existing OS random service.
   Freeze participants, owners/revision, party order, map seed, Rules/Battle/quest states, content
   digest and complete town digest. There are at most32 provision entries; larger input is refused.
   Map seed is an independent uint32, not a Park-Miller state. Its meaning/restoration is unverified.
3. Each copy calls `dd_dungeon_local_ready` with its OWN freshly measured town/content digests before
   sending READY. Host checks every authenticated peer's READY. LAUNCH requires all connected peers
   and the host's own local readiness. Guests reject LAUNCH before their own successful verification.
4. Native adapter loads the same expedition and validates the generated map/party/provisions/RNG.
   Only after actual load completion does the host publish the first checkpoint record. Guests
   measure their own state, compare it, then ACK. Host RELEASE enables the first dungeon action.
5. A command quotes the last sequence and gameplay digest. The host validates the sender's ownership,
   scene/turn, entity identities and a read-only native rules callback. An accepted request permits
   ONE native operation; it is not a completion or a broadcast. No validator means refusal.
6. Keep NATIVE_PENDING while animations, event queues and automatic gameplay consequences finish.
   Host completion produces one ordered EVENT containing the chosen command and resulting checkpoint.
   Guests check their own read-only native rules, execute once, wait for real completion, measure and
   compare their state, then ACK. Every connected peer must ACK before host RELEASE permits more input.
7. Returning uses the same barrier: RETURNING, normal unload/host save/guest host-state preparation,
   a verified TOWN checkpoint, all ACKs, RELEASE, then `finish_return`. Only then are shared Hamlet
   edits allowed. Do not restore the guest's original campaign between raids in one session; existing
   verified borrow/restore recovery still runs at final session exit.

The coordinator's 'checkpoint' is metadata plus a digest, NOT a restorable world snapshot. An actual
canonical reader, native loader and resync payload are still needed. Setting a hash equal to the host's
without measuring local state is invalid. Never copy a host's live process pointers or overwrite save
folders while a game is running. Guest loading stays `-nosave`; host progress stays authoritative.

Normal food has item type `provision` and an empty ID (hash0), as defined in the original supply data.
Departure items preserve the complete `(type,id)` pair and positive count. A zero item ID is not an
empty slot. The native adapter must verify the pair in the loaded item library before offer/readiness;
portable shape validation and proposed wire encoding cannot decide catalogue membership. The source-only
coordinator/codec regression covers zero-ID food through a host/guest offer and byte-exact round trip.
Hero/monster identity, type/count bounds, owner/readiness and scene barriers remain unchanged. This
correction does not link dungeon code or change the live DDCP protocol.

Adapter code must re-measure the actual pre-state immediately before a native operation and stop if
it differs from the committed checkpoint. Pause/gate autonomous gameplay, AI and input at safe native
boundaries while waiting; a network barrier by itself does not pause the game engine. UI preview or
target-selection functions must not be assumed read-only or RNG-free. Validate on the game thread
without consuming gameplay RNG, scheduling events or changing native state.

## Authorization and identity

Sender index comes from the authenticated connection/router, never a user-controlled payload field.
Only host messages may offer/launch, publish events/checkpoints, release barriers or remove participants.
The event's origin is an audit of the host's accepted request, not a way for clients to claim an identity.

| Action | Authority / phase |
|---|---|
| Hero skill or pass | That hero's frozen effective owner, active battle turn |
| Item use | Hero owner; exploring/camping, or that hero's active battle turn |
| Camping skill | Hero owner, camping |
| Curio / trap | Acting hero owner, exploring |
| Walk / choose room / start camp | Host, exploring |
| End camp | Host, camping |
| Loot decision | Host, loot phase |
| Enemy AI choice | Host only, active enemy turn; no player-controlled monsters |
| Retreat | Host, exploring/battle |
| Finish quest | Host, exploring/loot; native validator checks quest completion |
| Return to town | Host, returning |

Heroes use persistent roster IDs, never row positions or names. Enemies need an encounter-bound stable
spawn ID; a corpse, transformation or summon must not inherit a stale target identity. Commands carry
typed actor/target IDs and an action-specific argument (for example a skill identity or destination).
The adapter resolves the exact current skill/level/mode and native targets; ambiguous/colliding IDs,
stale ranks, invalid targets, unavailable items/uses or unsupported content must be refused.
Native rules decide the actual allowed effect, cost, targeting and RNG, not a second homemade ruleset.

Ownership changes count only at the next offer. Host-default ownership for an absent player is
implemented in this isolated core. Removing participants is host-only, never adds them back mid-raid,
and refuses an in-flight native operation by entering resync. There is no host election. Existing
live connection-loss behavior is unchanged: stop shared edits and rejoin a fresh session.
Future F9 integration must show the frozen expedition owners separately from launcher changes queued
for the next expedition; changing a checkbox mid-raid must not imply immediate control transfer.

## Digests, RNG and new mod data

Earlier project battle traces showed that matching initial state AND actions can reproduce outcomes.
They also showed that one seed alone does not suffice: Rules draws can occur outside battle, while
Battle, quest loading and map generation have separate state/seed paths (private NOTES.md, tests6/7).
The coordinator checks Rules/Battle/quest states in addition to the digest. Restoring them in the
new game build, locating other relevant generators, and sharing the actual map remain native work.

A real canonical digest must include party order and persistent identities; HP/stress/death/statuses,
buffs, cooldowns/uses, selected/learned skill identities and levels; equipped items and shared inventory,
gold/provisions/loot; map/location/orientation/torch; encounter/spawn identity, enemy state, initiative
and active turn; quest/camping state and all relevant RNG states. Use explicit deterministic scalar
encoding and sorted stable keys. Exclude addresses, allocator order, padding, frame counters, cosmetic
animation and local UI selection. This step only implements that idea for the SMALL fixture world.

The new [modding feature document linked by Red Hook](https://docs.google.com/document/d/1zkAFFAiRS-q11wW06DiSHo5QSGyzACnXgr8CAxW1dfY/edit)
adds targeting/effect options, per-skill selection rules and support for more than two trinket slots.
Consequently the future reader must enumerate actual skills/equipment/effects rather than assume
exactly four equipped skills or two trinkets. Native validation must resolve the effective loaded data.
These data features do not supply our network protocol or guarantee deterministic replay.

Before departure, the future content digest should cover the exact EXE, game mode/difficulty, relevant
enabled DLC, ordered enabled mods and actual gameplay-affecting content bytes. Equal mod names are
insufficient. The core compares supplied digests; this fingerprint producer is NOT implemented yet.
Base-game support comes first; overflow (including >8 explicit targets) blocks an unsupported action
instead of truncating it or silently claiming mod compatibility.

## Proposed DNG1 interface (not a live protocol change)

Header24 bytes: u32 magic0x31474e44, u16 version1, u16 type, u32 body length, u32 sequence, u64 expedition.
Maximum1024 bytes including header. OFFER with32 provisions occupies527 bytes, exceeding the existing
DDCP v3 body limit256. Do not put it in that buffer or send these packets through the current router.
Future integration needs an explicitly versioned routing/body-limit change and matching builds on
both PCs, then runtime bridge/launcher agreement tests. Existing authenticated AES-GCM framing may
carry it only after that integration; DNG1 itself supplies no encryption or authentication.

| Message | Direction / body |
|---|---|
| OFFER | Host -> all; departure, frozen owners, provisions and digests; sequence0 |
| READY | Guest -> host; its measured town/content digests; sequence0 |
| LAUNCH | Host -> all; empty body, sequence0 |
| CHECKPOINT | Host -> all; first loaded metadata/digest, sequence0 |
| COMMAND | Client/local host -> host; request ID, action, typed IDs, argument, pre-digest |
| EVENT | Host -> all; accepted origin/command and post-checkpoint; next sequence |
| ACK | Guest -> host; completed sequence/digest |
| RELEASE | Host -> all; confirmed sequence/digest |
| REJECT | Host -> requester; request ID and bounded refusal reason |
| MEMBERS | Host -> remaining peers; connected bitmask |

Decode validates exact length, magic/version/type, bounds and structural identities before output is
usable. Failed decode zeroes output. The session then checks role, epoch, order, ownership and actual
gameplay state. Transport callbacks must route only permitted message directions. Old sessions/inputs,
replays and future/gapped events cannot invoke a second native operation. Thirty-second deadlines
cover load/native/ACK waits using the caller's monotonic milliseconds; ordinary human turn time is
not limited. Missing ACK, hash/RNG disagreement, uncertain partial effects or loss enters RESYNC.
No automatic rollback, fabricated success or automatic state overwrite is implemented.

## Existing mods and research provenance

The public [DD Together FAQ/changelog](https://www.nexusmods.com/darkestdungeon/mods/2282) describes
desync/recovery issues. It reinforces treating room/loot/combat transitions as explicit barriers.
This is a design inference, not evidence that its implementation can be reused; no closed DLL was
downloaded or copied. Its PvP/monster-control features are outside our project.

[Blindest Dungeon](https://github.com/Vicorin/Blindest-Dungeon) exposes an MIT accessibility-mod source.
Its normal action-bar reader separates target/rank/use checks and observed native completion. Treat
those concepts as leads only: its published layout differs from our pinned build28385. No code or
offsets were copied, no mod binaries run, and its arena/network features will not be followed.
The [DSON format notes](https://github.com/robojumper/DarkestDungeonSaveEditor/blob/master/docs/dson.md)
remain useful for canonical save inspection, not live native struct serialization.

## Validation and next native step

Run `runtime\test-dungeon-msvc.cmd` in the VS2022 x64 prompt. Set `DNG_ASAN=1` for AddressSanitizer.
Both native `/W4 /WX /MT` and ASan runs passed:1018 committed fixture actions across three copies,
including1000 mixed-owner exploration actions; all10 packet types;32 provisions/8 targets;
10000 packet mutations,100000 random packets and10000 malformed ownership inputs. Checks cover
local readiness/content mismatches, turn/owner/phase gates, exact post-state/RNG comparison, delayed
native completion, required all-peer ACK, replay/gaps, request/sequence overflow boundaries,
timeouts, disconnection/owner fallback, camping, loot and verified return. Four deliberate guard
removals (ownership, all-peer ACK, checkpoint metadata and written_by) each caused a test failure.

Unchanged Hamlet/runtime/launcher tests already passed for test13 before this source-only addition.
No native mission/attack/walk hooks, canonical real-game snapshot, RNG restore, map replication,
live protocol routing, automatic resync or dungeon F9 control has been implemented/tested here.

Next: a bounded READ-ONLY build28385 departure snapshot (quest/party/provisions/RNG/map lifecycle),
audited against the actual executable and isolated memory fixtures. Existing verified function leads:
Darkest::RaidStart RVA3d7e80, Party::OnRaidStart RVA602cd0, raw RNG28f9f0, named uniform28fcd0,
named seed28fc40. Do not reuse old NOTES offsets for RaidStartParameters/App/RNG globals until
independently verified on this build. Then connect one host-controlled departure and compare the
first loaded checkpoint before allowing any battle input. This requires a NEW kit and owner test.

Never use PvP/PlayFab/TamperingManager. Preserve original installs via owner-run kits; host saves,
guest verified borrowing/-nosave/recovery; only slot3/profile_2, never profile9 or other slots.

## Departure, step 1: one map from one seed (test16, not yet tested live)

Checked in the pinned EXE: `Darkest::RaidStart` 3d7e80 -> Raid start 60c480 (call at 3d82de) -> 5e36c0 -> map generator
5f1ff0, which seeds a local prng from one `rand_s` call at 5f2164 (`& 0x7fffffff`, zero -> 0x17fa1). So the map seed
is drawn while RaidStart runs, on the path of a fresh embark.

Built in `runtime/dd_departure_bridge.inc`:

- The host draws a seed when it embarks and sends `DD_ACT_DEPARTURE` {seed, quest key, party ids, supply digest
  prefix} through the ordered town FIFO (DDCP protocol 5). It fits the existing body limit: supplies are already shared,
  so the 32-item DNG1 OFFER is not needed for this. Guests can not originate it; a host refuses it.
- The guest stores the order. When that player embarks, the map generator gets exactly that seed, once.
- `rand_s` goes through its import slot (found from the call at 5f2164, otherwise sharing stays off) and is only
  replaced while 5f1ff0 runs. Everything else gets the native function.
- Both copies log quest, party and supply digest at embark; the guest logs the comparison with the host's. A mismatch
  is logged, not refused: this kit is a measurement.

Read from memory at embark: app+1720 = RaidStartParameters; params+0 = quest definition, key at +40; +208/+210 =
vector of Hero*, id at hero+137c. The meaning of the key at +40 is inferred from RaidStart's comparison at 3d8039;
only the live logs can show it identifies the mission.

Not in this step: an automatic guest embark, ownership in the order (Codex's answer in
`HERO-OWNERSHIP-RESPONSE.md` still applies for when input is linked), Rules/Battle/quest stream states, settling a
pending Back before leaving, walking, combat. Equal seeds do not prove equal maps: the generator also takes two other
arguments and may read other state. That is what the owner's test answers.

Solo measurement: building the runtime with `DD_SOLO_MAP_SEED` makes a host without a guest use a fixed seed (embarks 1
and 2 equal, later ones different, counter in `mods\DDCoop\ddcoop-solo-run.txt`). Test build only, never a player kit:
`C:\dev\ddcoop-handoff\0.5.0-solo-map1`. Normal builds compile this out.

Result of the solo measurement (owner, 5 Nov): same seed, same room and corridor layout; other seed, other layout. The
initial scouting differed between the two equal runs. See `HANDOFF-CURRENT.md`.

Rules stream, built in test16c, not yet tested live: `DD_ACT_DEPARTURE` also carries a Rules seed (protocol 6). Rules is
the global named prng at 2add250, recognised by the name at +10, seeded through the game's own 28fc40 as RaidStart begins.
The supply digest now comes from the last poll with the panel open, as RaidStart can not read it. `tools/compare_rules.py`
compares two runs' draws from `app.log`.

## Following the host's departure (test19)

The hero list of the start parameters is built by 5575c0 through 589fa0: the roster entries at location 1 (predicate
590260), best four by +15b8 (comparator 590220), which the slot panel's update 67a630 writes from the slots. So location 1
means in the party and 0 free. A freshly loaded copy has its party in the vector and all of them at location 0 (read from the running
game); the screen sets 1 when it fills its slots. Confirmed live on one PC: after correcting `hook_hamlet_location`, a party nobody
dragged gave `heroes=1,18,2,8`.

**6e3390, 6e3690's callers, 6debf0, 6dfb40 and 6dfbc0 are the Butcher's Circus: they carry "#Status_InCircus", "matchmaking_%s" and
"str_circus_offline_match_popup". Do not call or hook them.** test17, test18 and duo1 to duo3 did, wrongly taken for the expedition
screens. No host ever went through them, which is why the hook stayed silent. In duo3 the guest called 6e3390(town, 1) once on the
departure order and crashed about a second later in 6dfbc0 (6dfc43, a null pointer). 58d0c0, 647d00 and 3c4630 belong to the same
family as far as read. The game's flag at 2a6d4bb selects the Circus roster (campaign+728).

The normal way out: in 6e2150, with the supply panel open (town+37c8), confirming hides panel town+3770 with a callback that runs 6e7c70
(also 6e2f40). 6e7c70 takes no arguments and only sets the next app state to 0x14, which reaches 3de6f1 (5575c0 stored at app+1720)
and RaidStart. The selected mission is an index at campaign+1248.

The guest follows: on DD_ACT_DEPARTURE (seeds, mission, party; sent at the host's RaidStart) it takes the mission, sets location and +15b8
as the screen would (`depart_party`) and calls 6e7c70. It skips the hide of the panel; only a live run judges that. The screens do not
follow yet, so the guest opens the expedition and supply screens itself. Finding the normal functions that open them is the next step
here; anything with Circus strings stays out. A guest can still embark on its own, and nobody is stopped from embarking before the
connection: guarding that needs the normal embark click.

## Walking (since test18, not yet tested live)

The raid is the global at 118e2c8 and its display at 118e570; both exist from just before RaidStart until the raid is left.
raid+55d0 is the current area (+18 its id, +0 the tiles with stride c8), raid+294 the party's position (a tile is 720 wide,
tile = (pos + 360) / 720), raid+8 the party, raid+740/748 all areas, raid+5118 set during a fight. display+40b0 is the
display's state.

- Every frame the display calls 7657a0. It asks 76dff0 whether the player may act; if so, 75b530 reads the keys and calls the
  party move 604af0 with (+1 or -1, 0, 0). The party accelerates and the leader's position sets raid+294. The same block of 7657a0 then
  makes the other three heroes follow the first (7659cf), and handles use: 7747c0, 774410, 7745e0. Crossing a tile runs
  774950 etc. with the step and tile events.
- Every move to another room or corridor goes through 770080 (display, dest, from area, flip, extra). dest is 16 plain bytes:
  area id, tile, 8 more; from is an area pointer or null. It does nothing in display states 0, 1, 6 and 7. The fade ends in
  7648c0, which sets the area through 613370.

One copy decides, the other follows. The host's position goes to the guest in DD_MSG_WALK, at most every 50 ms and only when changed; that
passes the order queue and only the newest counts. On the guest, 75b530 still runs, so the game itself checks the corridor's ends and blocked
tiles, but its call of 604af0 is only captured (`hook_party_move`) and sent as DD_MSG_WALK_INTENT, and the party moves through 604af0 towards the
host's position. On the host, if its own keys asked nothing, a wish younger than 300 ms moves the party. So the host's key goes first. test18 and
test19 answered no at 76dff0 instead, which also stopped the followers (owner's run: one hero of four walked).

A travel is DD_ACT_TRAVEL in the FIFO, which waits while the guest is loading, fading or fighting, and does nothing when already in that area. The
guest's own door and map calls (7427ae, 77d84f, 77e3ea) ask the host, which refuses while its own display cannot; the others pass unshared.
In a shared game a travel to an area the map does not have is dropped: the game does not check and crashes at 77014c.

A door is a prop too (DoorProp, vtable eaa2d8). Its result (4bef80, 779620) first walks the party to the door; only its callback 77d7b0 travels, reading the
prop's +170. So doors run on the host only, whichever player clicks; test21, test22 and duo5 ran them on both, and the first to arrive moved the other, whose
callback then read a stale prop (owner's run, 6 Nov 03:13: `move to area ... tile 456`, both copies crashed). The guest's party walks there by following the host.

Props (area+118/120, Prop: +0c kind, +c8 curio data, +124 has an overlay): curios and chests are the same. 7747c0 is the click. Without an overlay it interacts at once: prop vt+28 gives a
result; result vt+10 runs it, vt+0 frees it. With one, 712b70 sets the overlay at display+20b8, and for kinds 0 and 1 its next update (70de39, 7107c0) can run the curio by itself (7111e0), with no second click;
the first version shared only the second click, so curios ran on one copy (owner's run, 03:32). So the click is DD_ACT_USE: the guest asks, the host does it, and the guest repeats the game's own click near the host's
spot with the same hero selected (606450 and display+3f88). If the curio asks for an item, the overlay's button 714360 (a functor with the overlay at +8: prop +6a0, hero +d78, item +76c and +770, mode +d88) is the same
order with `door` 3. 714770 is the button for the other two prop kinds (+d70).
774410 is RaidDisplay::InteractWithTrap, clicking a trap (prop kind 2): it sets up the overlay and the overlay draws the attempt by itself. test21 to test25 took it for a door or obstacle and left it unshared on the
host, so one disarm split the Rules numbers: owner's run, 6 Nov 14:51, 288 draws equal until a disarm on the host, then the guest walked into the trap, curios differed and the Battle seeds differed (723878912, 1219288636),
and in the fight the guest stood on another turn. Now order `door` 1, on both copies with the same hero, near the same spot. Walking onto a trap (HandleTrapOrObstacleTile, 774cc0) needs nothing: both walk.

Traps have no call of their own: a tile's content fires when a party enters it, on each copy from its own Rules stream. If the streams
stay equal the result is. Both logs get one line per tile with area and Rules state:
`Walk: role=... area=... tile=... rules=...`, to lay side by side; that is the check.

Not read or linked: the loot window that follows, fights, camping, moving items. Also unknown: what the game-driven travel callers do (75b0cc, 42c67a, 5a9542); whether anything time-driven drifts.
A fight on the guest holds later orders until it ends.

## Combat: choices are linked, seen in step (duo9)

Owner's run, 6 Nov 15:08 to 15:14: one whole fight in step, chosen from both windows: same Battle seed, Rules state equal at every turn, same rewards. Monsters played alike. Ownership was not yet in that kit.
Since test27 a choice for a hero is only made by its owner: the host reads `ddcoop-heroes.ini` when embarking (hero+137c is the roster id) and sends a mask in DD_ACT_DEPARTURE; the guest keeps that, and the host also refuses a guest's choice for a hero that is not the guest's. A broken file means all heroes are the host's, with a notice; agreed was to fail the embark, but the hook at RaidStart cannot stop it.

## Items, torch and loot (test27, `runtime/dd_items_bridge.inc`, not yet seen live)

Same run, 15:17:47: a torch used on the host drew a bark (Random uniform, int range 4) the guest did not, and the numbers parted. So:
- Item: 429750 DungeonController::HeroUseItem (controller, hero, inventory, slot, 1), reached from the panel (7384bc), the loot window (717605), the controller. Order DD_ACT_ITEM; inventory raid+218 or the loot at raid+5240. Sent whatever the game answers: it answers the same on both.
- Torch: 6155f0 (raid, amount, 1) from the shift-click (783d26) and the controller (430dbb), by return address. The game's own burning and effects pass.
- Loot window: OverlayLoot at display+2f10, +0 its state (0 closed, 3 and 4 closing). Button functors 717d80 and 717d40 only set +218 (take all) and +219 (close), read at 716e27; 7171d0 takes one item (+10 place among filled slots, +20 raid+5140, +28 widget, which only plays a sound and is skipped when its +30 is empty, so the order builds its own with an empty widget). The first click counts: a later one meets a closed window and is refused. If an order finds its flag ignored, the guest may close by hand for 60 s.
In a fight an item is only shared at the turn's choosing and for your own hero. Outside one the guest waits to be near the spot, as for curios.
A curio's item slot (test28): the owner's Alchemy Table, 6 Nov 15:43:23, a torch dragged in on the host; it ran there alone and drew four numbers. The drop at 710fc7 calls 712e40 (overlay, a copy of the item in the overlay at +7c0; +44 and +88 its kind) and the overlay's next update runs the curio by itself. So the drop is an order by kind; each copy takes the first filled stack of that kind from its own bag. 712e40 is also reached from inside 429750, which is an order already, so only that return address counts.

Numbers drifting: in that run the numbers had already parted at 15:42:32, a curio with one draw more on the host before its loot, with no action unshared that I can name. Earlier curios clicked from the guest window were equal. To be read from the next run: since test28 each Rules draw writes `Rules: state from a b c` (return addresses) in the log. And since one drifted number sets the fight's seed, a travel order now carries the host's Rules state, and the guest takes it over if it differs (`Walk: Rules differed at the door`). That repairs numbers, not what they already caused.

Owner's run, 6 Nov 16:02 (duo11), with the `Rules:` log: all 329 draws of the expedition equal apart from the host's door (two draws at 608186, taken over at the next door as designed). Still:
- Stress on one copy, 16:07:20: 75b970, a bark on starting or stopping to walk, draws its chance from another stream but picks its hero with Rules (6057f0). The host barked, the guest did not, so the next tile (605450, uniform below a chance gives stress to a random hero) read a number meant for something else; the total stayed equal, so the Rules states looked fine afterwards. Since test29 the bark gives its number back.
- Torch up twice on the guest: 430dbb is inside using a torch item, not the controller's button as I had it, so the item's own effect went out as a second order. Reducing was never shared: it goes through a stored function at 783dd3, which returns to 783dd6.
- The selected hero (raid+438): a curio dropped on with another hero selected ran with the old one. Since test29 a change is polled every frame outside fights and sent; the drop also carries its hero.
- Tutorial popups: 820ae0 shows it, 7a8c80 is a close button shared with other panels; the panel pointer tells them apart.
Hero bars in the owner's colours: not looked at; a strip at the top of the window (`dd_overlay_turn`) says whose choice it is for now.

Test30, none of it seen live yet:
- Doors run on both again, so the walk shows and their two Rules draws are made on both. What crashed in duo5 was a callback outliving its area, so the guest's callback (return 77d84f) now only ends there, and the host's travel order waits for it, at most 6 s. The door repair stays as a safety net.
- A travel order also carries the torch (set on the guest if it differs) and a hash of the bag (only reported: `Walk: bag differs`, one notice per session).
- Retreat: 7499a0 QuestInfo::AttemptBattleRetreat, panel at display+3590, reached by the flag's confirm (flags +145, +146 read in 74a3f0). It rolls and travels itself, so its travel (return 5a9542) is not sent. In a fight only on your own hero's turn.
- Camp: Overlay_Camp at display+2058, raid+5118 the part of the camp. Meal 709cf0 (its arguments as numbers), skill 5c7420 (three numbers), Leave 70b7c0. Each waits for the same part. Starting one is using firewood, already an order. Either player may choose a camp skill for now, whoever owns the hero.
- After a finished quest: 7506c0 and 7504e0 set the panel's +148 and +14c. Which is which is not read; both are shared.
Party in town: the screen opens empty while the party is remembered, and slot+28 holds what is shown. A drag to the rank a hero is remembered at changed nothing in memory, so nothing was sent; now it is DD_ACT_PARTY with `show`, one slot.

Test31, not seen live either:
- After the raid: RaidResultsDisplay (7a0170 sets up, entry 19 of its table says whether it may move on, 7a2440 moves on, the last step being 7a2a50, to town). A click is an order; before each step the host's Rules state goes out and the guest takes it over, so both enter the new week with the same numbers. If the host does not answer within 3 s the guest goes on alone, so nobody gets stuck. What the new week does on each copy has not been compared: the next run's logs and the screens must show whether roster, events and stagecoach are equal. `Darkest::RaidFinish` (3d8da0) itself is not touched; the game's own analytics events in it are the game's.
- The bag: each copy lets its own drag happen (7ed600, 7ed150, 7ecf00, 737c20, 737070 only raise a flag), then sends every slot that differs from last frame's picture, filled ones first, as DD_ITEM_OP_BAG_SLOT with count and kind, across bag (raid+218) and loot (raid+5240). The other copy swaps whole slots (0x5a8 bytes) to get that kind there, sets the count, and clears emptied slots with 5de910. Assumes an item holds no pointers into itself, which has not been seen to fail or hold live. Splitting a stack into an empty slot where the kind stays in the old slot is covered only if it finds it elsewhere; it may report `no item of kind`.
Test32, after duo14 stopped in the provisions screen (6 Nov 17:34; cause not proven, the numbers fit an unshared drag):
- Town, provisions panel open: a drop on a slot (7ed600) or on the shop (7ef6d0, which sells back through 7ebcf0) returns 0, the game's own refusal. Not in a raid, not during an order.
- A stale supplies order logs the order and the copy's whole state: `Supplies order op=`, `Supplies differ, here:`; a refused guest request logs `Supplies refused by the host, here:`.
- Team name: the roster screen at town+3478 (vtable ebe530) looks it up in 697fa0 from its update 699190 when byte +b0 is set. A real drop sets it through the character_add event; `hamlet_party_apply` sets the byte itself.
Test33, from the first run with two real PCs (6 Nov 17:53-18:11). The run showed:
- Camp, fights, party, provisions: in step; `Rules differ` only where a guest's request carried the guest's number (the host's own number was not sent then).
- Focus: a copy without focus stops its input and walking (118525d, 372067). The guest fell two tiles behind and took a chest's numbers in another order: other loot. `dd_focus.h`: in a session the lost-focus event does not reach the game. Keys and mouse stop by themselves; sound goes on.
- Numbers at a use: `walk_rules_send` sends the host's number with the place before each use outside a fight (click, trap, overlay, curio button, tool, item, curio item). The guest takes it over when it stands at that place (or after 3 s), so a chest holds the same even after drift.
- Rubble: 70ff50 (through functor 714640) takes a tool from the bag and interacts; nothing shared it, and each copy cleared it alone. Now use order door 4 with the item's kind and id; the guest builds the capture (overlay at +0, its own item copied at +8) and waits for its overlay (up to 8 s after reaching the spot).
- Item on a curio: the host's drag took one from its bag, the guest's order did not: 4 holy water against 3. The copy without the drag now takes one (`Item: one taken`). A drag of a guest's last one: the item is in the overlay's hand, +7c0. Where exactly the drag takes it is not read; the numbers and the log say so.
- Doors: a different bag hash makes the guest ask (DD_ITEM_OP_BAG_ASK); the host sends every slot, filled ones first. It repairs counts and places, not an item this copy does not have. Notice only when the next door still differs.
- Clicks: a guest's item use is dropped while its previous one is on the way, up to 4 s (5 used in 5 s in the run).
- The pump: the game's message dialog (23601b) pumps events, and so came back into `dd_runtime_poll` during an order: the next order failed its sequence and stopped the session (18:10:35; the outer order finished at 18:10:45). `dd_runtime_poll` no longer runs nested. Hook on 2357e0 logs `Game assert:` with its strings; which message it was is not known yet.
Test35, Claude's review of test34:
- A loot click outside the ready state is dropped on both copies (the game ignores it itself at 717014), and a guest's repeat of the same button within 3 s. Otherwise a second click on a closing window became an order that waited 30 s on the other copy and then stopped the session.
- The host refuses a guest's loot click at once when its window was dealt with in the last 5 s and is not open (`loot_settled`). It waits when the window is opening or not there yet, because then the guest is simply ahead.
- A trinket move with the loot as an endpoint waits in `item_gate` like the loot buttons.
- On the guest a loot order or raid trinket that cannot be done no longer stops the session: notice, order counted as done. `g_loot_escape` lets that window be closed locally, also after the host refused a click. The bag is compared at the next door; a hero's trinket is not, so such a difference stays until the next start.
- The crash of 6 Nov 18:46 (test33), from the dump: `bag_drop` (hook length 10) on the same address as `trinket_drop_hero` (18). The second hook's trampoline went back to byte 10, into the first hook's padding: 7ed600 ran without its five pushes, and a movaps at 481a5c faulted on the misaligned stack. Claude's bug of test31; Codex's one-hook-per-address fix is the right one.
Test37: in a session the game starts straight into the chosen slot; `runtime/dd_frontend_bridge.inc` says how. Seen live in duo20 (owner, 7 Nov): both windows load the slot, about 3-4 s after starting; once the second window was too late for the skip, which test38 addresses.
Test38: Host on an empty slot makes a new campaign, alone, through the same screen. Not seen live.
Test39: party changes reach a copy without its expedition screen, in the model (`hamlet_party_apply`); the slots follow when they exist. The shop opens on both copies in the model (`supply_bg_begin`, native 57b100 and 57b7c0); only the clicker's panel opens; Back closes it for both. Not seen live.
Test40: a recruit reaches a copy without the Stagecoach screen (`recruit_background`, what 6c3c10 does apart from its row); a dismissal on a copy that does not update the roster list runs that update once (`hamlet_dismiss_poll`, native 6b4e30). Not seen live.
Test41: a door also carries the party by rank (id, HP, stress, a hash of the two trinket slots; protocol 21). The guest takes HP and stress over like the torch and reports another hero at a rank or other trinkets once (`Walk: hero N ... differed/differ at the door`). HP is the float at Hero+10a8 (the actor's serializer 47dc20 writes it as `current_hp`; 76 float accesses in the EXE), stress the float at Hero+12fc (`m_Stress` in the hero's 5d1ad0). The old note of HP at +1038 is not what the serializer reads. Back in town after a raid, the host sends a hash of the currencies (vector at campaign+e70), of the heroes in roster order (id, stress, trinkets) and their count two seconds after its town display is there (`Town: back from the raid`); the guest compares once it is in town itself (`Town: same as the host` or `Town: differs`), waiting up to 60 s, and only reports. Not seen live.
Test41: placing a hero in a Tavern, Abbey or Sanitarium activity no longer needs that building's screen on the other copy: the activity is found in the town model (campaign+1138, buildings map at +c0, activities vector at +c0 of each building; `hero placement: activity N taken from the town model`). Before, a copy that had not opened that building this session refused, and the session needed a restart. Upgrades, the Nomad Wagon, names, skills, heirlooms and trinkets already went through the model. Not seen live.
Test41: in a raid each copy draws one small block per hero at the top left, in screen order (the front rank on the right): green for your own hero, orange for the other player's, with the hero's stress (0..200) as a white bar along its bottom. Cleared rectangles only, like the turn strip; nothing of the game's own drawing changes. HP is not shown: its maximum is not found yet. Not seen live.
Saving: the host saves when its launcher's "Voortgang opslaan als ik host" is ticked (test kits: off until ticked); guests always run -nosave.

Bag (test41): a changed slot also says where its item came from (`round` = 1 + source slot, `turn` = its inventory) and whether that slot keeps its own (`flag`: a split). The other copy then takes exactly that item, and for a split makes a copy with the game's own Item copy, swap and destroy (as the raid trinket moves do), so the source keeps its share. Before, it took the first item of that kind and moved it whole: a split emptied one of the two stacks, and with two stacks of one kind the wrong one could move. Not seen live.
Provisions screen (test42): a drag from the shop onto a bag slot is the same shared purchase of one as a click, and a drag from the bag onto the shop the same return of one; the native drop never runs there. The source is the slot widget it was picked up from (+28 inventory, +b8 slot), the bag slot dropped on the widget's +18. Moving inside the bag stays refused. Not seen live.

Crash, same run, 15:19:03 on the host at 6087e9 from 77d8fe: a key held on a door. The guest's click does not change its screen, so it asked every frame (23 requests in 100 ms); the host ran them all, and a door's callback reads its prop after the first had travelled. Now the guest asks once a second per prop, and the host takes one door per 3 s.

## Combat: first notes

Owner's run, 6 Nov 14:56: the host's skill and target buttons reached the hooks. The fight had split before it began, see the trap above.

`runtime/dd_battle_bridge.inc`. A choice is DD_ACT_BATTLE (round, turn, state, op, hero, value, side, place, Rules for comparison), and each copy runs it through the game's own function; the results are never sent.
Read from 765d40: state 0x1d (bs_turn_select_skill) moves on when the hero's +8e0 holds a skill, 0x1e (bs_turn_select_targets) when its +8f0 list holds a target. So these are the players' calls:
- Skill: button functor 71f1e0 (slot at +14) calls 5cdf70(hero, slot or -1, 1), then empties +8f0 (71f2da). 5cdf70 has 48 callers, so only a pick from inside the button is a choice. In a corridor it stays local.
- Target: functors 707850 and 707ae0 call 5abae0 (enemy) and 5ab6d0 (party) (battle, skill, actor, target); these can redirect by their own draws, and 479af0 adds to +8f0. The state code calls them too, so the return address decides.
- Move and Pass: 5b9540(battle, hero, rank, -, 0), its own rank being Pass, from 707b40, 71f3c0, 780058. A skill's own move at 5b9f11 passes.
The guest asks, the host decides, as for doors. Both wait at the order's exact round, turn and state, because presentation takes its own time on each copy; the host also waits for a choice the guest made ahead of it. After 30 s
the order is dropped: the guest logs it and goes on, the host refuses. A choice outside the current hero's turn, or outside those two states, is dropped on the copy that made it, so that no copy draws alone.
Not linked: Retreat, items in a fight, whose hero it is (either player may choose for now), the loot window. Open: whether 5cdf70 draws Rules numbers in a corridor (47a7b0 passes Rules to 4cb4d0, not read); `Battle: Rules differ` in the logs would show.
Stayed out of: 5ab100, 777e40 and the arena branches in these functions. 5b1e90 sets the selected hero (raid+438), it is not a target.
## Combat: what is known

Measured, owner's run with two windows, 6 Nov 03:32: both copies got the same Battle seed, the same turn order, and 291 equal Rules draws, until one window picked another skill by accident at the draw after. So a fight
is deterministic from equal choices, as the old notes predicted: linking combat means sending the choices, not the results. The game logs its states: `bs_turn_select_skill`, `bs_turn_select_targets`, `bs_turn_wait_to_do_skill`, `bs_turn_do_skill`.

Checked in the pinned EXE (PE timestamp 6ae4c882): raid+4898 is the battle (768d29), +20/+28 its turns at 0x18 each with the actor first, +a0 the current
turn (5aa4ad), actor vt+10 gives the hero or monster object; raid+4b20 is the battle's state, written all over the battle update 765d40, which also
calls the monster AI 5aa490 (768d33). Since test22 each copy logs `Battle: role= round= turn= actor= state= rules=` per turn; laid side by side these
and the Rules draws in app.log (`tools/compare_rules.py`) show whether fights run in step. test18 to test21 took raid+5118 for "in a fight", which it is not.

Sources. The Red Hook modding document is a list of data features (statuses, effect targets, buff triggers, stats) and says nothing about battle structure or
randomness; one thing to remember: Daze moves a turn to the end of the round at turn start, so turn order can change during a round in modded data.
[Blindest Dungeon](https://github.com/Vicorin/Blindest-Dungeon) (MIT) reads the same things for accessibility, for other builds (timestamps 6ab13309,
6ab13742, 6aa1be59). Its data offsets 4898, 20, a0, 4b20, 438 and actor 8e0 agree with ours where checked; its function addresses do NOT: here they land
inside other functions. Leads only: battle state 0x1c to 0x1e is choosing a turn, 0x20 doing the skill; a skill record is 0x508; skill +3a4 launch ranks, +3a8
target ranks, +3ac is a move; `actor+8f0` holds targets; a "set single target" function holds an RNG draw, so a preview can consume numbers. It has no single
"action finished" signal and polls state, as we will have to. DD Together's own text is what taught us about turn order and surprise splitting; its closed DLL stays closed.

Stay out: everything `arena_mp`; 5ab100 (the branch at 768d1c next to the AI call, to be read before anything else here); `CombatActionProcessor`,
`GameControlMessageProcessor`, `CosmeticGameControlMessageProcessor`; the Circus functions above. Blindest Dungeon lists its arena functions by name; check each
function's strings before calling it, and confirm in a normal run that a hook on it fires.

Plan, each step its own test:
1. Done: they match until a choice differs (6 Nov 03:32).
2. Done, see above.
3. Whose turn: the heroes' owners from `ddcoop-heroes.ini`, sent by the host at departure, per `runtime/HERO-OWNERSHIP-RESPONSE.md`. The other player's input is off for that turn.
4. Done, see above. Monsters: each copy's own AI from the same numbers, to be compared in the logs.
5. Barriers at the fight's start and end, moving heroes, passing, retreat, then the loot window.


## Completed-quest seal (test53, 8 Nov 2026, Codex)

After choosing to continue exploring, the top-left Quest Complete seal uses
DrawQuestSeal's stateless callback74fdd0, separate from quest_choice_one/two.
Protocol24 adds QUEST_RETURN20: GAST requests, HOST approves, both wait for the
normal QuestInfo+14c==2 state and no battle; host Rules precede results state9.
Only one return per raid; RaidStart and session reset clear that guard. Existing
shared results Next steps still lead to town. No game/global-state write is used
for replay: each copy calls its original callback.

The callback starts with a4-byte prologue followed by a RIP-relative instruction.
Hook only its specific normal std::_Func vtable Do_call slot (RVAecc4c8), checked
against the address-table entry and atomically replaced with protection restored.
Never copy this prologue into the inline trampoline. All117 table patterns and
tests/test_quest_return_layout.py verify the exact known EXE. The bridge fixture
uses a protected synthetic slot and actual dispatch/FIFO/wire in both directions,
including slow readiness, double/crossed choices, Rules order, Next and resets.
Real game confirmation remains the owner's next test.
