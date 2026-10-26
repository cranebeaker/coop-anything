# The Legend of Zelda: Skyward Sword (2011)

| | |
|---|---|
| Engine | Nintendo EAD in-house engine (EGG / NW4R g3d), Wii. Decompilation: [zeldaret/ss](https://github.com/zeldaret/ss) |
| Versions tested | Wii disc, USA, game ID `SOUE01`, revision 0, disc SHA-1 `9cf9a4a7ed2a6a4abb4582e3304af1327c160640`; emulator Dolphin 2609a on Windows 11 x64 |
| Projects | [ss-coop](https://github.com/ItsPainY/ss-coop) (CC0, built with Co-op Anything). No other co-op project found |
| QA level | automated (two real game clients on one PC, Skyloft only; see QA) |
| Last updated | 2026-10-26 |

## Where things stand

Two players can play the Wii version together over the internet, each in their own Dolphin, and see each other as a
second Link: with head, face and hands, the right outfit, the other player's real animations, their sword, sheath,
shield and held items. Story, item and area flags, dungeon keys and chests and defeated enemies are shared, so progress
made by one player is in both games and in both save files. Living enemies stand in the same place in both games
and take damage from both players; for Keese, ChuChus, Remlits and Skytails they also play the same animations,
attacks included. Two players finished a real two-PC session over the internet with an early version (0.1);
the later features are covered by an automated two-client test, not yet by a long playthrough.

## Existing work

No co-op or multiplayer project for Skyward Sword was found: not in this directory or its issues and pull requests,
not on [Archipelago](https://archipelago.gg/games) (randomizers for Wii and HD exist but are single-player).
The [zeldaret/ss](https://github.com/zeldaret/ss) decompilation (CC0) made this possible: it rebuilds the game's
code byte for byte from this disc, so addresses, class layouts and functions could be read instead of guessed.

## How the game works (for co-op)

Observed on `SOUE01` rev 0 in Dolphin, addresses from the decomp's `config/SOUE01/symbols.txt` (full list and sources in
[NOTES.md](NOTES.md)):

- **No netcode.** Everything is driven from outside the game through Dolphin's guest memory, which Windows exposes as
  a mapped view starting with the game ID.
- **The player** is `dAcPy_c::LINK` (`0x8057578C`); position at actor `+0xC0`, facing at `+0xBA`. The current area is
  `dScGame_c::currentSpawnInfo` (`0x805B3860`, stage name first).
- **Progress** is in flag managers: story, item, scene, dungeon, chest (tbox), enemy defeat. Each has a live array
  and a copy in the in-RAM save file (`FileManager` `+0x8`); room loading reads the save copy, so both are written.
  Scene, dungeon and chest flags are per area: only the current area's words are live.
- **Defeated enemies** are bits indexed `(room << 10) | placement index`, the same in every copy of the game, set when
  an enemy dies and wiped on every area change (that is why enemies come back).
- **Actors** can be added without touching the game's own code: the unused test actor slot `J_TEST` (profile `0x14E`,
  module `d_a_morita_testNP`) is replaced with our own REL built from the decomp, and a Gecko code at
  `dScGame_c::execute` creates it once per stage. Its shared data block is how the outside bridge talks to it.
- **Player models**: the body, head, face and hands come from the resident `Alink` archive, animations from
  `PLCommonAnime`, sword, shield and held items from `PLCompItem` (`dat/<name>.brresC`). The player keeps them as
  `m3d::smdl_c` members; a model is drawn this frame exactly when its scene object has a parent (it was entered into
  the scene root, which is cleared every frame). That one bit tells which equipment is out.
- **Enemies** derive from `dAcEnBase_c` (health at `+0x342`). Keese, ChuChu, Remlit and Skytail keep model and
  animation in a `d3d::AnmMdlWrapper`; most other enemy types are not decompiled yet.

## What we changed

Nothing in the player's own game files or saves: the installer extracts their disc into a separate folder, swaps one
module in `rels.arc` (the original is kept) and uses its own Dolphin settings folder.

- **Second Link (puppet actor).** A REL in the `J_TEST` slot draws the other player from the same models the player
  uses. Fixes on the way, each found by a test: the local Link puts the shared body model into a mode that left a
  second copy in the bind pose, so the puppet draws from a private copy of that model block; `anmChr_c::setAnm`
  detached the animation, so it is re-attached the way the player's own blend does; a whole-file copy left too little
  room in a heap and hung the game, so the copy is now one model block with its name offsets rebased.
- **Bridge.** A small program next to each Dolphin reads the own Link and writes the other player's into the puppet's
  mailbox; host and join talk over UDP (one forwarded port on the host).
- **Flags.** Changes are sent reliably and in order; a new connection merges both players' flags; reloads and resets
  are detected (a game that suddenly cleared many flags is treated as a load), so loading an older save never
  erases the other player's progress. Dungeon flag clears (a small key being used) travel only within the same area.
- **Equipment and held items.** Each model's matrix is sent relative to the body, plus the "drawn this frame" bit;
  the puppet loads the same models and places them on its own body. Held items get their own frame heap, swapped
  without respawning. A fresh allocator points at a placeholder heap rather than at nothing; treating it as empty
  crashed the game (found by the test, fixed).
- **Enemies.** Defeats: the kill bits of the current area are resent and OR-merged, only within the same area, and
  the puppet removes enemies the other player defeated that are alive right now. Live enemies: the host gives each
  enemy to the player whose Link is nearer (with hold time and margin, epochs on every change, following the
  [actor ownership playbook](../../playbook/actor-ownership.md)); the other game pulls its copy to the owner's position,
  facing and, for the four types above, animation and frame. Health is the lower of both games.

## QA

- `python -I mod/qa/coop_qa.py` (in the [ss-coop](https://github.com/ItsPainY/ss-coop) repo) boots two Dolphins from savestates, connects host and join over loopback and drives
  both games through memory, no human input. It prints one `RESULT: PASS|FAIL` line and a JSON report; it checks the
  owner's real save files are unchanged (hash) and that both test Dolphins exited.
- 21 checks: game start and build; connect; position both ways (teleport, the puppet must arrive within 10 units);
  facing and animation in the mailbox; equipment; held items; a story flag set and cleared (live and save copy);
  small key pickup and use, and a chest; the mass-clear brake (a wiped game must not wipe the other); enemy kill
  (gone from the other game, bit set in both copies); enemy ownership and following with a real Remlit, including a
  handoff; enemy animation (the owner's Remlit switches, the other copy follows within a few frames); outfit change
  (two respawns, heap back to its level, game alive); disconnect and reconnect; a 5-second invariant monitor.
- Mutation twins: bridges deliberately broken by an environment switch (`no_position`, `no_flag_apply`,
  `no_mass_clear_brake`, `no_enemy_apply`, `no_held`, `no_enemy_anim`) each fail exactly the check meant to catch them.
- Unit tests without the game: `test_net_bridge.py` 16, `test_flag_sync.py` 22, `test_enemy_sync.py` 12,
  `test_enemy_motion.py` 19, including chaos-relay runs and a stale-assignment twin for the ownership rules.
- Last full run 2026-10-25 (puppet build 19): `RESULT: PASS`, 21/21; twin `no_enemy_anim`: `RESULT: FAIL` at
  "enemy animation" only.
- Scope: one area (Skyloft by day) in an early save, loopback network. The bot cannot press buttons, so real attacks,
  doors, cutscenes and a real sword or item in hand are not driven by it; those were checked with made-up data and,
  for the early features, by the two players by hand.

## How to play

Both players need their own dump of their own disc (USA, `SOUE01`, revision 0) and Dolphin 2609a. The package has
no game files: a setup checks the dump, extracts it into its own folder and installs the co-op module; `2 Host.cmd`
and `3 Join.cmd` start Dolphin and the bridge. The host forwards one UDP port. Start from the same save. Both players
need the same version (versions do not connect to each other). Source and build steps:
[ss-coop](https://github.com/ItsPainY/ss-coop).

No Wii hardware is needed: the package ships a Dolphin profile for any Xbox controller (left stick = Nunchuk, right
stick = sword swings, click = stab, mouse = pointer). Setup uses it when a player has no controls yet; otherwise it can
be picked in Dolphin's Controllers window.

## Known issues

- Only the Wii USA revision 0 works (other regions and revisions have other addresses).
- The other Link is only visible in the same area.
- Enemies only damage the Link in their own game. Enemies other than Keese, ChuChus, Remlits and Skytails (for
  example Bokoblins) stand in the right place and share damage, but animate separately in each game. Bosses are not
  shared.
- An enemy the other player defeats vanishes without its death animation or item drop.
- Moving parts of held items (bow string, whip, clawshot chain) stay at the hand.
- Cutscenes and dialogue play only for the player who triggers them; the other gets the progress quietly.
- Changing sword, shield or outfit makes the other Link disappear for about a second.

## Next steps

- Enemies hitting the other player: the owner's enemy should damage the puppet's player (detect the hit on the puppet,
  send it across).
- Animation sync for more enemy types as the decomp covers them (Bokoblins first).
- A savestate further into the game (sword, items, enemies nearby) so the bot can check real equipment and a real fight.
- The HD version on Switch has no decomp or memory map yet; it would be a separate project.
