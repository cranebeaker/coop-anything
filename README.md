<h1 align="center">Co-op Anything</h1>

<p align="center"><b>Any single-player game. Your friends. Your agent does the work.</b></p>

<p align="center">
<a href="https://ko-fi.com/cranebeaker"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support me on Ko-fi"></a>
<a href="https://discord.gg/EXGB49R82t"><img src="https://img.shields.io/badge/Discord-Join%20the%20server-5865F2?logo=discord&logoColor=white&style=for-the-badge" alt="Join the Discord" height="36"></a>
<a href="https://coopanything.app"><img src="https://img.shields.io/badge/CoOp%3AAnything-Browse%20every%20co--op%20mod-1abc9c?style=for-the-badge" alt="Browse every co-op mod on CoOp:Anything" height="36"></a>
</p>

<p align="center">
<a href="#quick-start">Quick start</a> ·
<a href="#how-it-works">How it works</a> ·
<a href="#what-you-can-co-op">What you can co-op</a> ·
<a href="#games-so-far">Games so far</a> ·
<a href="#everyone-is-a-contributor">Contribute</a> ·
<a href="#faq">FAQ</a> ·
<a href="#documentation">Docs</a>
</p>

Want to play a single-player game with your friends? Point your AI coding agent at this
repo. It researches what exists, reverse engineers the game and its engine if it has to,
builds or fixes the co-op, **tests it with bots inside the game**, and packages it so your
friends install it in one click and join over Steam without port forwarding.

The sky is the limit. Every game that runs on your PC is a candidate.

**Playing on one PC?** [Couchlink](https://github.com/cranebeaker/couchlink) runs a copy of the game for each player,
each with their own part of the screen, their own controller or keyboard and mouse, and their own Steam account if
they want one. It works with co-op mods and with games that only play online. Mods made here can plug into it: see the
`couchlink` skill.

Games being made co-op right now: **[Deus Ex](https://github.com/cranebeaker/deus-ex-coop)** (released) ·
**[Fallout: New Vegas](https://github.com/cranebeaker/fnv-coop)** · **[Zelda: Ocarina of Time](https://github.com/cranebeaker/ocarina-coop)** (fully synced) ·
**[Dungeon Keeper](https://github.com/cranebeaker/dungeon-keeper-coop)** · **[S.T.A.L.K.E.R. Shadow of Chernobyl](https://github.com/cranebeaker/stalker-soc-coop-fixes)**

From the community: **[Mega Man X](https://github.com/mstan/MegaManXSNESRecomp)** and
**[Mega Man X4](https://github.com/mstan/MegaManX4Recomp)** (native PC with co-op netplay) ·
**[Pilotwings 64](https://github.com/NoahVl/Birdman64)** (split-screen) ·
**[Crab Champions](https://github.com/Dudiebug/CrabMods/blob/main/crabsync/README.md)** · **[Stacklands](https://github.com/Dudiebug/stacklands-coop)** ·
**[Half Sword](https://github.com/Dudiebug/halfsword-multiplayer-mod)** · **[Enter the Gungeon](https://thunderstore.io/c/enter-the-gungeon/p/GoTeamAlex/Gungeon_Online/)** ·
**[Clive Barker's Jericho](https://github.com/dolbaras/jericho-coop)** · **[Poppy Playtime](https://github.com/romannazarov057-spec/PoppyCoop)** ·
**[Divinity II](games/divinity-2-developers-cut)** (split-screen) · **[Command & Conquer 3](games/command-and-conquer-3)** (campaign co-op) ·
**[Far Cry 2](https://github.com/MortyRei/fc2-coop-probe)** · **[Gothic 3](https://github.com/GGM1k3y23/g3-together)** · **[Fear & Hunger](https://github.com/glynn568-glitch/fear-and-hunger-together)** ·
**[Modern Warfare 2](https://github.com/stxlvn/mw2_coop)** (campaign co-op) · **[Little Nightmares](https://discord.com/channels/1561411702082510879/1562210529504927836)** (on our Discord) · yours next?

## Quick start

Tell your agent (Claude Code, Codex, Gemini CLI, Cursor, OpenCode, ...):

```
Read https://github.com/cranebeaker/coop-anything and follow AGENTS.md.
I want to play <game> in co-op with my friends.
```

The agent follows [AGENTS.md](AGENTS.md) and the skills in [`.agents/skills`](.agents/skills)
(also in `.claude/skills` for Claude Code).

### Add the MCP server

The **Co-op Anything MCP** lets any agent search the co-op directory, detect a game's engine
from its install folder, read the playbook and skills, and validate contributions.
Claude Code picks it up from this repo's `.mcp.json`; for other agents:

```
claude mcp add coop-anything -- python <clone>/tools/mcp/coop_mcp.py
```

Codex, Gemini CLI and Cursor setup: [playbook/mcp.md](playbook/mcp.md). For the binary work,
pair it with a reverse-engineering MCP such as [REA](https://github.com/morluto/rea)
(`npx rea-agents setup`): Co-op Anything knows what to do, REA looks inside the game.

## How it works

Your agent takes one of two paths.

| | Path 1: finish what exists | Path 2: build it from zero |
|---|---|---|
| When | A co-op mod exists but is buggy, stalled or abandoned. Most loved games are here. | Nothing usable exists. |
| How | Find the last build, mine its bug reports (issues, forums, Discord), audit the code, fix it as a separate package, test, ship. | Reverse engineer the game and engine, hook in, show the second player, sync enemies, world and story, handle level changes and saves, test, ship. |
| Skills | `coop-research`, `coop-revive`, `coop-qa`, `couchlink` | `coop-reverse-engineer`, `coop-build-from-zero`, `coop-networking`, `coop-sync-testing`, `coop-qa`, `couchlink` |
| Guide | [Reviving abandoned mods](playbook/reviving-abandoned-mods.md) | [How co-op mods work](playbook/how-coop-mods-work.md) |

Both paths end the same way: automated tests with real game clients
([QA](playbook/automated-qa.md)), easy install, Steam relay networking
([networking](playbook/networking.md)), and a pull request back here.

### Sync test tools

Ready-made, game-agnostic tools for building and testing co-op sync, in [`tools/`](tools/README.md).
Each has tests, mutation twins and one command that prints `ALL OK`
(`python -I tools/test_all.py` runs them all).

| Tool | What it does |
|---|---|
| [chaos](tools/chaos) | UDP relay that adds lag, loss, reorder, duplicates and one-way cuts between real clients, with live control |
| [flight](tools/flight) | Flight recorder for your mod plus an analyzer: lines up 2 to 4 machines, finds the first desync, shrinks it and writes a regression test |
| [fuzz](tools/fuzz) | Seeded N-player co-op fuzzer with invariant oracles (converge, no double apply, one owner, no dupes, nothing stale) |
| [ownership](tools/ownership) | Small C++ library: one owner per NPC with epochs, handoff, and stale echo rejection |
| [fakeworld](tools/fakeworld) | Example in-memory game world for testing sync without launching the game |
| [gameslot](tools/gameslot), [gamepool](tools/gamepool) | Lock and warm pool for running several game copies on one PC safely |
| [scenarios](tools/scenarios), [bots](tools/bots) | JSON start states plus multi-bot co-op playbooks with a state diff after each step |
| [animcheck](tools/animcheck) | Compares animation and pose across machines and flags sliding, T-poses, floating and rubber-banding |
| [bridge](tools/bridge), [inspect](tools/inspect) | An RPC server template for inside your game (console line, state dump, save, load, screenshot) with a Python client, plus a live side-by-side view of every copy |
| [hotreload](tools/hotreload) | Split your mod into a loader and a core, and swap the core inside the running games instead of restarting them |
| [mutants](tools/mutants), [buildlock](tools/buildlock) | Runtime mutation twins for every fix, and a shared build lock with a build cache |

[Fast iteration](playbook/fast-iteration.md) shows where the time in an in-game fix round goes and which tool removes each wait.

They come from the [fnv-coop](https://github.com/cranebeaker/fnv-coop) project. The
`coop-sync-testing` skill tells your agent when to use which.

## What you can co-op

| Your game is... | Typical approach | Tools your agent uses |
|---|---|---|
| On an engine with multiplayer already (Unreal 1/2, Source, id Tech, X-Ray) | Make the single-player logic run on the existing netcode with script mods | Script source, SDK compilers, mod packages |
| Bethesda (Gamebryo, Creation) | Script extender plugin, host-authority sync, reuse Tilted Online style designs | SKSE / F4SE / xNVSE, Ghidra, REA |
| Unity | Harmony patches through a mod loader, a networking library in C# | BepInEx, MelonLoader, dnSpyEx, Il2CppDumper |
| Unreal Engine 4/5 | Lua or C++ mods with an SDK dump of the game's classes | UE4SS, Dumper-7 |
| RE Engine, FromSoftware, other proprietary | DLL hooks; extend the game's own online features where they exist | REFramework, MinHook, Ghidra, REA, x64dbg |
| Decompiled or open source (N64 decomps, KeeperFX, OpenMW, source ports) | Add networking directly in the code | A compiler, the decomp, Docker cross builds |
| Console only (PS1, PS2, N64, Dreamcast, GameCube, Wii, GBA, ...) | Decomp or recompiled PC port first; else emulator netplay, or a memory-sync mod between two emulators | Decomps and recompilers (N64Recomp, XenonRecomp), PCSX2 PINE, Dolphin Memory Engine, BizHawk / mGBA Lua |
| Closed binary, nothing else | Full reverse engineering of state, saves and levels; the hardest path | REA, GhidraMCP, ida-pro-mcp, Frida, Cheat Engine |

Console games: [emulator co-op](playbook/console-emulators.md) and
[decompiled or recompiled PC ports](playbook/decomp-and-recomp.md), the route Ocarina of Time co-op took.

Couch co-op on one PC? [Split screen](playbook/split-screen.md) builds on Nucleus Co-op
(Windows) and PartyDeck (Linux and Steam Deck), both open source.

Full tool list with install notes: [playbook/tools.md](playbook/tools.md).
Which engine has co-op work you can reuse: [playbook/engine-families.md](playbook/engine-families.md).

## Games so far

| Game | What was done | Release | Write-up |
|---|---|---|---|
| Deus Ex (2000) | Revived the HX co-op mod: fix package, Steam relay, automated test suite | [deus-ex-coop](https://github.com/cranebeaker/deus-ex-coop) | [games/deus-ex](games/deus-ex/README.md) |
| Dungeon Keeper (1997) | Shared-keeper co-op on KeeperFX, Steam relay networking | [dungeon-keeper-coop](https://github.com/cranebeaker/dungeon-keeper-coop) | [games/dungeon-keeper](games/dungeon-keeper/README.md) |
| S.T.A.L.K.E.R. Shadow of Chernobyl | Fix patch for Shadow of COOP (true ending unlocked) | [stalker-soc-coop-fixes](https://github.com/cranebeaker/stalker-soc-coop-fixes) | [games/stalker](games/stalker/README.md) |
| Fallout: New Vegas | Story co-op: shared quests, dialogue results, karma and reputation | [fnv-coop](https://github.com/cranebeaker/fnv-coop) | [games/fallout-new-vegas](games/fallout-new-vegas/README.md) |
| Zelda: Ocarina of Time | Fixes for a fully synced Ship of Harkinian co-op fork | [ocarina-coop](https://github.com/cranebeaker/ocarina-coop) | [games/ocarina-of-time](games/ocarina-of-time/README.md) |

Plus a [directory of 230+ co-op mods and gaps](DIRECTORY.md) for other games (browse and search it at
[coopanything.app](https://coopanything.app)), each with
what it really syncs and how well it was tested.

## Everyone is a contributor

Every agent that works on a game sends what it learned back as a pull request: a fix, a
bug list, a test, a failed attempt, a corrected directory entry. One person's agent gets a
game halfway; the next one starts from there. Pooled together, that's enough compute to get
even the biggest games over the line.

- Your agent opens the PR as a draft; you review the diff and publish it.
- Each game gets its own release repo, linked from [Games so far](#games-so-far).
- The **QA level** on every entry says how it was proven: untested, player reports,
  automated tests (bots drove real game clients) or verified playthrough.

[CONTRIBUTING.md](CONTRIBUTING.md) has the details. Chat with other contributors on [Discord](https://discord.gg/EXGB49R82t).

## FAQ

<details><summary>Do I need to know how to code?</summary>
No. You need the game, a coding agent and some patience. The agent asks before installing
anything or using your accounts.
</details>

<details><summary>Does my friend need to buy anything?</summary>
Everyone needs the game installed. The Steam relay uses Valve's free Spacewar test
app, so nobody buys anything else.
</details>

<details><summary>Is this legal?</summary>
Modding games for co-op with friends is what the modding community has always done.
This repo doesn't host game files or assets (agents can build on decompiled code in each
game's own repo), and it doesn't touch anti-cheat or online services. See the rules below.
</details>

<details><summary>Will it work for my game?</summary>
Yes. Some games need more agent hours than others, but it's definitely possible. Check the
<a href="DIRECTORY.md">directory</a> first: if a co-op mod already exists (path 1), most
projects take days at most. If nothing exists (path 2), it depends on the engine; the table
above gives a feel for it.
</details>

## Documentation

- [AGENTS.md](AGENTS.md): rules and phase map for agents · [MCP server](playbook/mcp.md) · [skills](.agents/skills) · [sync test tools](tools/README.md)
- Playbook: [triage](playbook/triage.md) · [research](playbook/research.md) ·
  [Discord bug mining](playbook/discord-export.md) · [reverse engineering](playbook/reverse-engineering.md) ·
  [tools](playbook/tools.md) · [how co-op mods work](playbook/how-coop-mods-work.md) ·
  [engine families](playbook/engine-families.md) · [reviving mods](playbook/reviving-abandoned-mods.md) ·
  [networking](playbook/networking.md) · [Steam Spacewar relay](playbook/networking-steam-spacewar.md) ·
  [split screen](playbook/split-screen.md) · [console emulators](playbook/console-emulators.md) · [Switch](playbook/switch.md) ·
  [decomp and recomp](playbook/decomp-and-recomp.md) ·
  [automated QA](playbook/automated-qa.md) · [shipping to players](playbook/shipping-to-players.md) · [multi-agent workflows](playbook/multi-agent-workflows.md) ·
  [network chaos](playbook/network-chaos-testing.md) · [desync hunting](playbook/desync-hunting.md) ·
  [fuzzing sync](playbook/fuzzing-coop-sync.md) · [actor ownership](playbook/actor-ownership.md) ·
  [fake world testing](playbook/fake-world-testing.md) · [several game copies](playbook/multi-instance-testing.md) ·
  [bots and playbooks](playbook/bots-and-playbooks.md) · [animation sync](playbook/animation-sync-checks.md) ·
  [fast iteration](playbook/fast-iteration.md) · [in-game bridge](playbook/in-game-bridge.md) · [hot reload](playbook/hot-reloading-your-mod.md) ·
  [story sync by effects](playbook/effect-capture-story-sync.md) ·
  [methods](playbook/methods.md) · [lessons](playbook/lessons.md)
- [DIRECTORY.md](DIRECTORY.md) and its [schema](data/SCHEMA.md)

## Rules

Don't commit game files or assets here. Building on decomps is fine; a game's code lives in its own repo. No
personal info. Credit the mods you build on and give fixes back to them. Co-op only:
nothing that cheats in other people's multiplayer games or touches anti-cheat.

License: MIT.
