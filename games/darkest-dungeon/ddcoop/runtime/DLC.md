# DLC groundwork: CoM, Shieldbreaker and Musketeer (Codex, 8 Nov 2026)

Owner selected The Color of Madness first, plus Shieldbreaker and Musketeer now.
This authorizes
research/groundwork beyond the former base-game scope. Installed game/DLC/save files
remain untouched. Butcher's Circus/arena_mp stays excluded.

## What already exists

- Launcher parses campaign `base_root.dlc`, including separate Crimson Court
  features. GAST checks installed feature folders before backup or replacing its
  campaign. Existing launcher tests verify missing-DLC refusal before writes.
- Runtime prices/snapshots already carry shard fields and enumerate wallet types.
  This is not full shard support: `dd_supplies_before/quote` explicitly reject
  nonzero shard costs. The current fixture verifies that refusal.
- Base combat, hero-ID ownership, skills and item identity are data-driven. Their
  reuse for DLC is a candidate to test, not a live compatibility claim.

## Read-only first step delivered

`testkit/dlc-readiness/readiness.py GAME_ROOT --enabled color_of_madness shieldbreaker musketeer` produces
a deterministic fingerprint of the selected official DLC data directory, a file
count and required test groups. It prints JSON only: no activation, native calls,
save writes, copying game content or Steam changes. Synthetic tests cover missing
and feature-specific data, stable/change-sensitive fingerprints, subset selection,
arena exclusion and no mutation. It does not verify entitlements or Workshop mods.
No runtime/launcher wire contract is changed for this source-only tool.

Shieldbreaker and Musketeer have no hard-coded class rejection in the normal
recruit/skill/upgrade/ownership paths audited here. The data check deduplicates
their levelled combat-skill definitions and compares counts/levels against the
existing32-skill/level4 codec. Generic trinket validation uses the native class-ID
restriction vector. This supports testing both heroes through the existing native
paths; it does not prove their live layouts or extra transitions. In the next live
run test both heroes' recruitment, Guild/Blacksmith, active/camping skills, class
trinkets and ownership in both directions. For Shieldbreaker also test a camping
nightmare and the return to the camp/dungeon. Do not infer that an automatic event
needs a new command until both logs show where the copies diverge.

The local legitimate build28385 has CoM data under `dlc/735730_color_of_madness`.
Its actual inventory files define a `shard` currency and `spice` supply with
`purchase_shard_value3`; a gold-only bridge therefore cannot fully handle it.
The exact EXE RTTI also exposes distinct ShardTrinketSupply, ShardHeroRecruit and
WaveLogic/WaveDungeon types, plus RaidResultsDisplay::ShowWaveItemsState callbacks.
These are research leads, not approved native endpoints or verified ABI layouts.
No addresses from these leads are hooked or called in this release.

## Next implementation steps

1. Prove the native shard wallet key and purchase/refund semantics in this exact
   EXE. Extend canonical pre/post validation to shard balances and native pricing,
   including insufficient funds, mixed currency quotes, refunds, duplication and
   concurrent HOST/GAST purchases. Never silently drop a shard price.
2. Audit the separate shard trinket/recruit stores; the ordinary Nomad and Stagecoach
   hooks must not be assumed to cover those classes.
3. Verify first Farmstead departure through normal TownDisplay: one mission/party,
   one map/Rules seed on both copies. The new symmetric embark path is a reusable
   foundation, but Farmstead itself is not live tested.
4. Measure wave boundaries, continue/retreat/reward choices, camping and the special
   wave-results inventory. Add ordered actions for explicit player decisions after
   proving captures/readiness. Keep automatic wave logic native and compare RNG.
5. Test HOST then GAST initiation and cross-session persistence from a disposable
   CoM campaign; compare shards, rewards, roster and week after returning to Hamlet.

First target is both DLC heroes in a normal short raid, then an initial Farmstead
run. Endless mode and complete DLC support remain separate validation milestones.
Other DLC profiles are inventoried only.
