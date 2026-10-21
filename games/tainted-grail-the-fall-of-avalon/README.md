# Tainted Grail: The Fall of Avalon (2025)

| | |
|---|---|
| Engine | Unity 6000.0.41f1, Mono scripting on the official Steam beta branch `mono` |
| Versions tested | Steam, `mono` branch, buildid 25966682, game version 1.30.102 (`[1.30a]`) |
| Projects | TGCoop, "2 Players Steam Coop" ([Nexus](https://www.nexusmods.com/taintedgrailthefallofavalon/mods/211), directory entry) |
| QA level | TGCoop: community-reports. Our work below: automated (two game copies on one PC; joining, saves and seeing each other only) |
| Last updated | 2026-10-19 |

## Where things stand

Nobody can play the story together well yet. TGCoop is playable but players report
softlocks, combat desync and story getting out of step. Our mod, built from zero, joins a
second player into the host's world with their own character, keeps both characters in the
host's save, and shows each player to the other as a moving (not yet animated) body;
enemies, story and world are not shared yet. It has no release.

## Existing work

- **TGCoop** (hektormenta, Nexus mod 211, newest build seen 0.5.36 from 2026-07-28, no
  license, no source). BepInEx 5 + Harmony, Steam lobbies on the game's own app id. Both
  players load their own save; the host pushes the true story flags at join and syncs three
  story calls live, places NPCs, syncs doors and objects; drops are first grab wins.
  Players' reports: gate closed for the teammate after the Odrán dialogue, teammate frozen by
  dialogue in boss fights, combat desync, A-pose in first person, quest rewards for one player
  only. Fixed in 0.5.36 per the author: enemies far from the host (the game stops simulating
  them) and host saves silently refused by a stuck combat state. The author: only Act 1 was
  really tested; last reply on Nexus 2026-07-28. Details and the cause where its code shows one: [NOTES.md](NOTES.md).
- No other co-op project found; unnamed mentions on the Steam forum and where we looked are
  in [NOTES.md](NOTES.md) ("Existing work", "Where we looked"). The game's Discord was not
  checked.
- The developer's official modding toolkit,
  [merlin-workshop](https://github.com/AR-Questline/merlin-workshop), makes asset and data
  mods only (Addressables replacements), so it cannot carry a co-op mod's code.

## How the game works (for co-op)

- **Modding route.** The `mono` beta branch runs Mono, so BepInEx 5 loads plugins and Harmony
  patches the game's C# (`TG.Main.dll`, `Awaken.*.dll`), which decompiles with original names.
  The default branch is presumably IL2CPP (not checked).
- **No netcode.** Single player only; Steamworks.NET is there for Steam features and cloud
  saves. Co-op needs its own transport and host-authority sync.
- **Models and IDs.** The game's own MVC framework: every piece of world state is a model
  with a string ID `<Type>:<counter>`, counters per type saved with the world. Two games
  from one save start with the same IDs and drift as soon as either creates something.
- **Story.** One flag store: `GameplayMemory` → `ContextualFacts.Set/Change/Remove`, behind
  quests, objectives and story flags. Stories run through `Story.StartStory` and
  `StoryStep.Execute`; interactions through `AbstractHeroAction.StartInteraction`.
- **One hero.** `Hero.Current` is a static read about 1,190 times. The hero is built as the
  local player (views, camera, runtime parts); a second real hero in one world needs
  exceptions everywhere (tried, see NOTES.md). Remote players are stand-ins.
- **Saves.** A slot is a zip with one `.data` per domain (Gameplay, each visited area,
  metadata), written through a cloud service (Steam Cloud, Steam without cloud, GOG). Extra
  zip entries travel through every save. New Game+ already exports a character and restores
  it into another world, which is what joining with one's own character builds on.
- **Areas** are separate save domains, loaded one at a time.
- More, with the code locations: [NOTES.md](NOTES.md), "How the game works".

## What we changed

Our mod is a BepInEx plugin with a LiteNetLib (UDP) transport for now; Steam relay is planned.

- **Joining with one's own character.** Problem: a guest needs the host's world but its own
  character. The guest loads the host's save, and inside the game's own load the host's
  hero and items are replaced by the guest's (fresh item IDs from the world's counters, one
  restore pass), so the world sees a normal save. Test: join, the guest plays its own
  character with all its items next to the host; twin: joining without a character must
  fail exactly the character checks.
- **Host-only saves.** The host's co-op save is the only permanent copy of the world and of
  every guest's character (stored as extra entries in the save zip). The guest receives the
  live world on join, keeps it in memory, and its game cannot save (the game's own save
  blocker). Tests: the guest's save folder is unchanged after a session; a character
  changed and stored comes back on the next join after both games quit; each with a twin.
- **Seeing each other** (playbook stage 2). A roster, player state at 20 Hz (unreliable),
  forwarded by the host; each remote player is the game's own third-person hero body with
  the hero-only parts removed, interpolated 100 ms behind. Test: one bot walks, the other
  game's body of it follows and ends within 0.5 m, both directions; twin: no state sent.
- **Test setup.** Two isolated game copies (own save and temp folders, no Steam Cloud, no
  automatic bug reports), windowed, a bot inside each game, a bad-network relay, and a
  launcher that stops a copy whose Harmony patches failed to apply.

## QA

- The suite (in the mod's repo, not public yet) drives two real game copies on one PC with
  in-game bots: `run-suite.ps1 -Quick` (one session, about 2 min) or `-Full` (also restarts,
  a bad-network run and the mutation twins, about 10 min). It checks joining, the guest's
  character, host-only saves, persistence across restarts, seeing each other, clean exit and
  no crash reports. Every check has a mutation twin that must fail, and the scorer is
  self-tested first.
- Last result (2026-10-19): `-Full` `SUITE: PASS` in 614 s: scorer self-test 9/9, main run
  40/40, bad network (100 ms each way, 20 ms jitter, 5% loss) 31/31, twin runs failed on
  exactly their 17 and 1 listed checks.

## How to play

Not playable yet: there is no release.

## Known issues

- The other player is a bald default male body in a T-pose, without animation or gear.
- Enemies, story and world are not shared; each game runs its own.
- The guest cannot save; its character is saved by the host.
- Tested only with two copies on one PC over local UDP; no Steam transport yet. GOG saves
  are not supported.

## Next steps

Playbook stage 3: enemies run on the host only and are shown on the guest; damage from
both players; drops appear once. Then animation and looks of the other player, then story
and world (stage 4). The design (host runs every script, guests send requests, a table of
shared and per-player state) is summarised in [NOTES.md](NOTES.md), "World design".
