# Console games through emulators

How to make a console game co-op when there is no PC version, or when the PC version is the wrong target. Written for an agent that has been asked "make game X co-op" where X ships on a console only. Status as of October 2026. Every concrete claim has a link. Items marked **UNVERIFIED** came from a single secondary source or could not be confirmed against a primary page; re-check before relying on them.

Related playbooks: [methods.md](methods.md), [split-screen.md](split-screen.md), [networking.md](networking.md), [reverse-engineering.md](reverse-engineering.md), [automated-qa.md](automated-qa.md).

## 0. Ground rules

- The user owns the game. Dump your own discs, cartridges and console BIOS/firmware. This playbook links to no ROMs, ISOs, BIOS files, keys or "where to download" pages, and neither should anything you write.
- Prefer a PC port or decompilation if one exists. It is almost always easier to add co-op to source or a native port than to a running emulator (see section 6, step 1).
- Never plan human playtests. Drive two emulator instances with scripts and read RAM to assert state (see [automated-qa.md](automated-qa.md) and section 7 below).
- Emulator availability changes fast, especially Switch. Treat all version numbers below as snapshots.

## 1. Choosing a route

```
Does a PC port / decomp / recomp of the game exist?
  yes -> add co-op there (engine playbooks). Stop.
  no  -> Does the game already have local co-op / versus / link mode?
           yes -> Route A: emulator netplay (inputs synced). Cheapest.
           no  -> Does the game have an online mode whose servers are dead?
                    yes -> Route C: LAN / private-server revival.
                    no  -> Route B: memory-sync co-op (two emulators, RAM bridge).
Fallback for local co-op games on any emulator: Route D, remote play.
```

| Route | Needs game-side co-op? | Effort | Fidelity | Typical failure |
|---|---|---|---|---|
| A. Emulator netplay | Yes (local multiplayer) | Low | Perfect, same game | Desync, latency, emulator lacks netplay |
| B. Memory-sync | No | High (per-game RE) | Partial, only what you sync | Determinism, loading, camera, scripts |
| C. LAN / revival server | Yes (network mode) | Medium | Original online mode | Server trust, DNS, adapter emulation |
| D. Remote play | Yes (local multiplayer) | Very low | Perfect, but video latency | Input lag, host owns the game |

## 2. The generic methods

### 2a. Emulator netplay (input sync)

Both machines run the same game and exchange controller inputs; the game's own local co-op does the rest. Requirements are always identical: same emulator build, same game dump (region and revision), same settings.

| Emulator | Netplay model and notes | Source |
|---|---|---|
| RetroArch | Instances running the same core and content stay in sync. Needs same RetroArch version, same core version, same exact content. Host needs TCP 55435 (UPnP or manual forward) or a relay. Any core with save states should work in principle, but PSX, N64, Dreamcast, GameCube, Wii and 3DS netplay are listed as unsupported for performance. GBA and PSP are listed as not possible. Link cable is not emulated, only same-system local multiplayer. More than two players is supported. | [libretro netplay FAQ](https://docs.libretro.com/guides/netplay-faq/) |
| Dolphin | Built-in netplay. All players need their own copy of the game with the same region and revision, and the same Dolphin version. Traversal server only helps connectivity, it adds no latency. | [Dolphin netplay guide](https://dolphin-emu.org/docs/guides/netplay-guide/) (page returned 403 to the fetcher; content read via the search snippet and a [mirror](https://central-new.dolphin-emu.org/docs/guides/netplay-guide); the Slippi rollback fork is a separate project, see [Slippi mention](https://lemmy.brendan.ie/comment/1236250), **UNVERIFIED**) |
| gopher64 (N64) | P2P netplay listed as a feature; server handles sessions and logs IPs. Rollback vs lockstep not stated in the README. | [gopher64](https://github.com/gopher64/gopher64) |
| simple64 (N64) | Had cloud-server netplay; project is archived, developer moved to gopher64. | [GameTechWiki simple64](https://emulation.gametechwiki.com:443/index.php/Simple64), [GamingOnLinux comment](https://www.gamingonlinux.com/2025/08/nintendo-64-emulator-gopher64-recently-added-save-state-slots-and-netplay-cloud-servers/comment_id=282408) |
| Project64 netplay | Only a low-authority claim that Project64 KSE had it. **UNVERIFIED**. | [comment](https://lemmy.brendan.ie/comment/1235709) |
| PCSX2 | No general netplay in mainline. The long-standing request is open as [issue 772](https://github.com/PCSX2/pcsx2/issues/772); duplicates (Kaillera, delay-based client) were closed, see [issue 8425](https://github.com/PCSX2/pcsx2/issues/8425) and [issue 14104](https://github.com/PCSX2/pcsx2/issues/14104). Forks exist: [pcsx2-online-Update](https://github.com/fraserofsmeg/pcsx2-online-Update) (P2P, delay-based, built for fighting games, last changelog 2012, settings reset to defaults during netplay), [pcsx2-netplay-reborn](https://github.com/YungMoniCA1/pcsx2-netplay-reborn) (5 commits, no releases, effectively a stub), and a Sept 2026 delay-lockstep prototype port ([PR](https://github.com/q13007868130/git-demo/pull/1), **UNVERIFIED**, no relay, no rollback). Conclusion: treat PCSX2 netplay as unavailable for modern builds. | links inline |
| DuckStation (PS1) | No mainline netplay found. A community GGPO rollback build by HeatXD existed as of 2023. | [Exputer, Mar 2023](https://exputer.com/news/playstation/ps1-emulator-rollback-netcode/), [build](https://github.com/HeatXD/duckstation-netplay/releases/tag/0.1) (current status **UNVERIFIED**) |
| PPSSPP | Ad hoc (P2P, "same room" design) and infrastructure modes, both served by volunteer servers; most listed servers support the Relay protocol so port forwarding is unneeded; players must share the same port offset (default 10000). | [PPSSPP multiplayer docs](https://www.ppsspp.org/docs/multiplayer/how-to-play), [infrastructure mode](https://www.ppsspp.org/docs/multiplayer/infrastructure-servers/) |
| melonDS (DS) | Standalone: local wireless and online Wi-Fi are listed as work in progress in 2026 packaging. libretro melonDS DS core emulates Nintendo WFC replacement servers (Kaeru WFC default) and says LAN multiplayer does not work through the Internet, VPNs or tunnels. A 2026 report says the RetroArch core added LAN multiplayer. | [libretro melonDS DS](https://docs.libretro.com/library/melonds_ds/), [Generation Amiga, Aug 2026](https://www.generationamiga.com/2026/08/22/retroarchs-new-melonds-ds-core-adds-wi-fi-and-better-dual-screen-controls/) (**UNVERIFIED** beyond the docs) |
| Azahar (3DS, Citra successor) | Room system that forwards local-wireless traffic over the internet, no Nintendo server or account. Android got the room browser in 2126.0 preview per one report. | [Azahar networking announcement](https://citra.azahar-emu.org/entry/announcing-networking-support/), [Android Authority](https://androidauthority.com/azahar-3ds-emulator-multiplayer-android-3686723) (Android details **UNVERIFIED**) |
| RPCS3 (PS3) | RPCN is a free open-source PSN replacement, so games' own online modes (and some fighting and racing titles) work between RPCS3 users. Some games need community servers. | [DSOGaming](https://www.dsogaming.com/news/rpcs3-now-supports-online-play-via-psn-emulation/), [GamingOnLinux RPCN 1.4](https://www.gamingonlinux.com/2025/02/playstation-3-emulator-rpcs3-expands-online-play-for-fighting-games-with-rpcn-v1-4/comment_id=276279). Check the RPCS3 netplay wiki for the current game list (not fetched). |
| Xenia / Xemu | System-link over XLink Kai, see 2c. | links in 2c |
| mGBA | Has a "multiplayer window" that runs several linked instances locally (useful for link-cable and test rigs). | [mGBA forum](https://forums.mgba.io/printthread.php?tid=5781) |

Latency and determinism rules of thumb (general, not sourced to one page):

- Input-sync netplay needs a deterministic emulator: same seed, same timing, no host-specific speed hacks. Emulators usually force safe settings; do not let a mod change them.
- Memory cards and saves must start identical. PCSX2-Online-style forks make the client memory card read-only and copy the host's.
- Games with a heavy in-game random source and 60 fps lockstep are the most desync-prone; fighting games on delay-based netplay work, big open worlds often do not.

### 2b. Memory-sync co-op frameworks (single-player to co-op)

Two (or more) emulators run the same single-player game. A bridge reads each player's state from emulated RAM, sends it over TCP/UDP, and writes the remote players into free actor slots or a custom overlay. The same idea as every PC "shared-keeper" mod, except the "game process" is an emulator's guest RAM.

| Framework | Emulator | Hooks | Notes and source |
|---|---|---|---|
| ModLoader64 | Modified Mupen64Plus bound to Node.js (N-API) | Memory access, ROM patching, networking | Game-agnostic N64 platform; requires Windows 10 or modern Linux, OpenGL 3.3, 32-bit Node 10 or 12 on Windows. [repo](https://github.com/hylian-modding/ModLoader64). Community says it is quiet; a 2024 thread asked if it is dead ([Reddit mirror](https://redlib.belloworld.it/r/ZeldaOoT/comments/1f21sk7/is_modloader64_dead)). |
| BizHawk Lua + socket | BizHawk (EmuHawk), many 8/16/32-bit systems, N64, PS1, GBA, DS, 3DS entries | Lua console; Archipelago ships a generic connector with READ/WRITE over a memory domain | [BizHawk README](https://github.com/TASEmulators/BizHawk), [Archipelago BizHawk README](https://git.hollymcfarland.com/monorail/Archipelago/raw/branch/main/worlds/_bizhawk/README.md), [bizhook](https://pypi.org/project/bizhook/1.1.0/) |
| mGBA scripting | mGBA 0.10+ | Lua only; `emu` object, memory domains (GBA: wram, iwram, io, palette, vram, oam, cart0-2), `frame` and `keysRead` callbacks, `socket` API (`tcp`, `bind`, `connect`, `accept`, `send`, `receive`, `poll`) | [mGBA scripting docs](https://mgba.io/docs/scripting.html). Headless operation is not stated in the docs. |
| Dolphin Memory Engine | Dolphin (GameCube/Wii) | External process hooks Dolphin's emulated memory; converts endianness (reads little endian, writes big endian); unhooks on failed access; also a [Python binding](https://pypi.org/project/dolphin-memory-engine/1.0.1) | [repo](https://github.com/aldelaro5/dolphin-memory-engine). Pushed Apr 2026 per a repo aggregator. |
| Dolphin Python fork | Felk's Dolphin fork | In-emulator Python scripts with `frameadvance` and `memorybreakpoint` events, on-screen text, memory module, `--script` flag; Windows x86-64 only; minimal API, no releases listed | [Felk/dolphin](https://github.com/Felk/dolphin). Not in mainline Dolphin; mainline Python PR stalled since 2022 per [this note](https://tasvideos.org/Forum/Posts/498712), **UNVERIFIED**. |
| PCSX2 PINE | PCSX2 (and others via the protocol) | Local socket IPC: 8/16/32/64-bit read/write, savestate and metadata commands | See section 5. |
| Archipelago | BizHawk and per-game clients | Items and checks exchanged, not shared presence | This is a randomizer multiworld. It is not co-op in one world, but its client code is a ready-made RAM bridge pattern. [Archipelago](https://docsearch.algolia.com/mcp/docs/repo/archipelagomw/archipelago) (mirror), [Zelda 64 Recompiled randomizer](https://thunderstore.io/c/zelda-64-recompiled/p/RecompRando/MMRecompRando/) |

Design pattern that works across all of them:

1. Find the player actor struct (position, rotation, animation id, health, held item).
2. Find a spare actor/object slot or an effect list to spawn a remote avatar.
3. Each tick: read local player, send delta, write remote player. 10 to 20 Hz plus interpolation is usually enough.
4. Sync discrete state (flags, items, doors, bosses) as events, not as continuous memory copies.
5. Pick a single authority for enemies and RNG, or accept that each player fights their own copies. Anchor does the latter by default and a fork adds host-authority enemies (section 4).

### 2c. LAN and online-mode emulation

For games with a network or system-link mode, emulate the network adapter and bring back the server.

| Technique | Where it applies | Notes and source |
|---|---|---|
| PCSX2 DEV9 network adapter | PS2 online games | SOCOM revival guides use the CLR-DEV9 plugin (0.8.9) and a community DNS; PCSX2 1.7 needs npcap. The official Network Access Disc servers were shut down. [PS2 online functionality (Wikipedia)](https://en.wikipedia.org/wiki/PlayStation_2_online_functionality), [SOCOM guide mirror](https://red.127.pp.ua/r/SOCOM/wiki/how-to-play-socom-ii-online), [PCSX2 wiki, Network Access Disc](https://wiki.pcsx2.net/Network_Access_Disc). Guides cited are partly older than current PCSX2; **UNVERIFIED** for 2026 builds. |
| Horizon / Clank (Medius servers) | PS2 Ratchet: Deadlocked and Up Your Arsenal | Open-source Medius server emulator and patches. [Horizon-Private-Server org](https://github.com/Horizon-Private-Server), [Clank](https://github.com/xtvaser/clank). Whether it works on PCSX2 is not stated on the repo pages. **UNVERIFIED**. |
| Dolphin BBA | GameCube online (e.g. Phantasy Star Online) | Broadband Adapter with tap server; old reports of DHCP errors and Mac failures. [bug 8222](https://bugs.dolphin-emu.org/issues/8222?tab=notes), [forum thread](https://forums.dolphin-emu.org/Thread-help-with-bba-and-pso-online-play). Current state **UNVERIFIED**. |
| Wiimmfi / RiiConnect24 / WiiLink WFC | Wii and DS Nintendo WFC games | Community replacement for WFC; Dolphin has had error codes 20100 and 20101 after a security update. [Wii hacks guide](https://wii.hacks.guide/wiimmfi), [bug 11455](https://bugs.dolphin-emu.org/issues/11455). |
| XLink Kai | Xenia (Xbox 360), Xemu (original Xbox), real consoles | Xenia support is experimental via the netplay canary fork, Windows only; Xenia and real 360 users cannot play together; Npcap needed. Xemu supports bridged networking. [Xenia setup](https://www.teamxlink.co.uk/wiki/Xenia_XLink_Kai_Setup), [Xemu setup](https://www.teamxlink.co.uk/wiki/Xemu_and_XLink_Kai_Setup). PS2 emulator support not confirmed. |
| PPSSPP infrastructure | PSP | Volunteer servers mimic game servers. [docs](https://www.ppsspp.org/docs/multiplayer/infrastructure-servers/) |

### 2d. Remote play fallbacks

For games with local co-op where netplay is unavailable (PS2 on PCSX2 is the main case).

- Steam Remote Play Together: host runs the game and streams; guests send controller input; the host alone needs the game; guests see only the game window. Add the emulator as a non-Steam game. Up to four players officially. Needs UDP 27031 and 27036, TCP 27036 and 27037. [Tom's Guide](https://www.tomsguide.com/reference/steam-remote-play-together-how-to-use), [GHacks](https://www.ghacks.net/?p=155430) (2019 launch articles; menu paths may differ).
- Parsec: same idea, not verified here (no source retrieved). **UNVERIFIED**.
- Cost: added input latency from video encode and network. Fine for casual co-op, poor for action games over distance.

## 3. Per-console table

"Co-op route" = the best approach for an agent. "Hooks" = what you can use to read and write guest RAM. Emulator names are those that were verified or are widely documented; everything about statuses links to a source where I found one.

| Console | Main emulators | Netplay status | Hooks for memory-sync co-op | Notable co-op work |
|---|---|---|---|---|
| NES, SNES, Genesis (and similar 8/16-bit) | BizHawk, RetroArch cores, Mesen, bsnes | RetroArch netplay works for same-system local multiplayer ([FAQ](https://docs.libretro.com/guides/netplay-faq/)) | BizHawk Lua with socket connector ([Archipelago BizHawk README](https://git.hollymcfarland.com/monorail/Archipelago/raw/branch/main/worlds/_bizhawk/README.md)) | Archipelago clients (randomizers, not shared worlds) |
| N64 | Mupen64Plus-based (simple64 archived, gopher64 active), Project64, ParaLLEl core | gopher64 P2P netplay ([repo](https://github.com/gopher64/gopher64)); RetroArch lists N64 as unsupported ([FAQ](https://docs.libretro.com/guides/netplay-faq/)) | ModLoader64 ([repo](https://github.com/hylian-modding/ModLoader64)); BizHawk Lua | Zelda64 Online (OoT, MM), see section 4; N64 recomps are the modern alternative ([Zelda 64: Recompiled](https://thunderstore.io/c/zelda-64-recompiled/p/RecompRando/MMRecompRando/)) |
| GameCube, Wii | Dolphin (plus forks: Slippi, PrimeHack) | Dolphin built-in netplay ([guide](https://dolphin-emu.org/docs/guides/netplay-guide/)); Wii WFC games via Wiimmfi ([guide](https://wii.hacks.guide/wiimmfi)); RetroArch unsupported | Dolphin Memory Engine ([repo](https://github.com/aldelaro5/dolphin-memory-engine)), Felk Python fork ([repo](https://github.com/Felk/dolphin)), pnach-like Gecko/AR codes | Wind Waker has a built-in GBA Tingle Tuner second-player role, emulated by Dolphin's GBA controller port ([ZeldaWiki](https://zeldawiki.wiki/wiki/Multiplayer) plus guide snippet). Metroid Prime 2: Echoes has a split-screen co-op campaign mod built on its versus mode with Gecko codes ([games/metroid-prime-2-echoes](../games/metroid-prime-2-echoes/README.md)); PrimeHack is a controls fork only and drives player 1 only ([Flathub](https://www.flathub.org/apps/io.github.shiiion.primehack)). |
| PS1 | DuckStation, Mednafen/Beetle, ePSXe | RetroArch lists PSX unsupported ([FAQ](https://docs.libretro.com/guides/netplay-faq/)); DuckStation GGPO build existed 2023 ([Exputer](https://exputer.com/news/playstation/ps1-emulator-rollback-netcode/)) | PINE is a PCSX2-origin protocol; DuckStation shipping a PINE server is only indirectly evidenced by a [DuckStation mirror source file](https://repo.retrodeck.net/Xargon/Duckstation/src/commit/1fd8d2701de0c9887509703a027b8d8bb439cd6a/src/core/pine_server.cpp), current status **UNVERIFIED** | Spyro 3 + Crash fan recreation in UE4 with 4 player co-op demo, not an emulator mod ([Retro Dodo](https://retrododo.com/spyro-crash-finally-work-together-in-new-nostalgic-mod/)) |
| PS2 | PCSX2 | No mainline netplay; forks are stale or prototypes (section 2a) | PINE IPC; pnach patches ([docs](https://pcsx2.net/docs/advanced/writing-patches)); DEV9 for online modes | Ratchet Horizon servers; SOCOM revival; see section 5 |
| PS3 | RPCS3 | RPCN for online-capable games ([GamingOnLinux](https://www.gamingonlinux.com/2025/02/playstation-3-emulator-rpcs3-expands-online-play-for-fighting-games-with-rpcn-v1-4/comment_id=276279)) | No PINE support found. External memory poking is possible via process memory tools but no verified project. **UNVERIFIED** | Few if any single-player-to-co-op mods found |
| PSP | PPSSPP | Ad hoc and infrastructure with relay servers ([docs](https://www.ppsspp.org/docs/multiplayer/how-to-play)); RetroArch PSP netplay not possible ([FAQ](https://docs.libretro.com/guides/netplay-faq/)) | PPSSPP has a debugger and memory view (not verified here), so the section 2b pattern is plausible. **UNVERIFIED** | Kingdom Hearts: Birth by Sleep has built-in co-op on PSP ([RPG Site](https://www.rpgsite.net/news/430-kingdom-hearts-gets-co-op-play-on-psp)) |
| GBA, DS, 3DS | mGBA, melonDS, Azahar, BizHawk | mGBA has local multi-window link; RetroArch lists GBA netplay as not possible ([FAQ](https://docs.libretro.com/guides/netplay-faq/)); melonDS Wi-Fi in progress in standalone ([libretro docs](https://docs.libretro.com/library/melonds_ds/)); Azahar rooms ([post](https://citra.azahar-emu.org/entry/announcing-networking-support/)) | mGBA Lua with sockets ([docs](https://mgba.io/docs/scripting.html)); BizHawk Lua | PK-GBA Multiplayer for Gen 3 Pokemon, alpha, possibly stalled ([PokeCommunity](https://www.pokecommunity.com/threads/pk-gba-multiplayer.484949/), **UNVERIFIED** status) |
| Xbox, Xbox 360 | Xemu, Xenia (canary) | System-link via XLink Kai, experimental ([Xenia](https://www.teamxlink.co.uk/wiki/Xenia_XLink_Kai_Setup), [Xemu](https://www.teamxlink.co.uk/wiki/Xemu_and_XLink_Kai_Setup)) | No verified RAM bridge | None found |
| Switch | Eden, Ryubing (Ryujinx fork), Citron, others | LDN local wireless existed in Ryujinx and its fork ([tech-insider](https://tech-insider.org/eden-vs-ryubing-best-switch-emulator-2026/), **UNVERIFIED**); current state in Eden/Citron not confirmed | None verified | Do not plan work here without checking legal status first. Original Yuzu and Ryujinx were shut down after Nintendo action, and 2026 reports of DMCA notices against forks are **UNVERIFIED** ([tech-insider](https://tech-insider.org/?p=19175)). Use only open emulators the user already has. |

## 4. Existing emulator-based co-op projects for single-player games

Status column is "as found", not a promise. "Kind" uses: `netplay`, `memory-sync`, `port-mod`, `server-revival`, `randomizer`.

| Game | Project | Kind | Status | Link |
|---|---|---|---|---|
| Ocarina of Time, Majora's Mask (N64) | Zelda64 Online (on ModLoader64) | memory-sync | Public, 849 commits, README lists shared items and quest progress, players visible, custom models; MM has Basic and Time Sync modes. Maintenance unclear (Reddit 2024 "is it dead" thread). | [Z64Online](https://github.com/hylian-modding/Z64Online), [ModLoader64](https://github.com/hylian-modding/ModLoader64) |
| Ocarina of Time (PC port, decomp-based) | Anchor for Ship of Harkinian | port-mod | Client/server; loads host save state, shares flags and items; rolling nightlies; reported merged into SoH. | [anchor](https://github.com/garrettjoecox/anchor), [XDA](https://www.xda-developers.com/a-zelda-classic-just-got-full-co-op-multiplayer-thanks-to-this-pc-port/) |
| Ocarina of Time (PC port) | OOT True Co-op | port-mod | Fork adding host-authority enemy and boss sync on top of SoH + Anchor. Early-stage; **UNVERIFIED** quality. | [OOT-True-Co-op](https://github.com/bghill95/OOT-True-Co-op) |
| Majora's Mask (PC port, Anchor fork) | anchor-2s2h | port-mod | Fork of Anchor for 2 Ship 2 Harkinian; not multi-world, same seed required. | [anchor-2s2h](https://github.com/tjoaks/anchor-2s2h) |
| Super Mario 64 | sm64coopdx (continues sm64ex-coop) | port-mod | Active org; lobby play up to 16; works on PC port built from user's ROM via a patcher. Not an emulator mod. | [sm64coopdx](https://github.com/coop-deluxe/sm64coopdx), [PC Gamer](https://pcgamer.com/super-mario-64-emulator-co-op-mod) |
| Super Mario 64 (N64 plus PS2) | PS2 recompilation linked to an N64 via a Pico | memory-sync (hardware) | One-off demo by Carl Does Tech Things, no public project page or download in the article. | [Time Extension, Feb 2026](https://www.timeextension.com/news/2026/02/modder-somehow-conjures-super-mario-64-cross-play-between-n64-and-ps2), [video](https://www.youtube.com/watch?v=gCza2KrstxE) |
| Banjo-Kazooie | Banjo-Kazooie Online (on ModLoader64) | memory-sync | Listed by Kotaku; no current repo checked. **UNVERIFIED**. | [Kotaku](https://kotaku.com/games/banjo-kazooie-online), [Highlander News](https://www.highlandernews.org/?p=74563) |
| Jak and Daxter (PS2, via OpenGOAL PC port) | Teamruns mod | port-mod | Online and local multiplayer in the OpenGOAL launcher Mods section; built for speedrunners; Jak 1 only per coverage. | [DSOGaming](https://www.dsogaming.com/news/the-unofficial-pc-port-of-jak-daxter-now-supports-online-and-local-multiplayer/), [Retro Dodo](https://retrododo.com/jak-daxter-return-with-online-local-multiplayer/) |
| Skyward Sword (Wii, Dolphin) | ss-coop | memory-sync | Alpha, built with this playbook: second Link drawn by an actor built from the zeldaret/ss decomp, shared flags, kills and enemy positions; automated two-client QA. | [write-up](../games/skyward-sword/README.md) |
| Pokemon Gen 3 (GBA) | PK-GBA Multiplayer | memory-sync | Alpha 4; developer silent for months per one commenter; **UNVERIFIED** | [PokeCommunity](https://www.pokecommunity.com/threads/pk-gba-multiplayer.484949/) |
| Ratchet: Deadlocked, Up Your Arsenal (PS2) | Horizon + Clank Medius servers | server-revival | Enhanced online servers with custom maps and modes; PCSX2 compatibility not confirmed on repo pages. | [Horizon-Private-Server](https://github.com/Horizon-Private-Server), [Clank](https://github.com/xtvaser/clank) |
| SOCOM (PS2) | Community servers with DEV9 | server-revival | Community DNS; trust the operators. | [guide mirror](https://red.127.pp.ua/r/SOCOM/wiki/how-to-play-socom-ii-online) |
| Zelda 64: Recompiled (MM) | RecompRando via Archipelago | randomizer | Multiworld, not shared-world co-op. | [Thunderstore](https://thunderstore.io/c/zelda-64-recompiled/p/RecompRando/MMRecompRando/) |
| Spyro 3 + Crash | UE4 fan recreation by qtderg | port-mod | Fan demo, 3 levels, 4 player local or online; not running the original game. | [Retro Dodo](https://retrododo.com/spyro-crash-finally-work-together-in-new-nostalgic-mod/) |

Checked and found nothing (as of this research; absence of search hits is not proof):

- Kingdom Hearts 1/2: no co-op mod on PCSX2 or the PC OpenKH ecosystem found. Related mods are QoL (Re:Fixed, Re:Fined) and an Archipelago randomizer client. [Archipelago KH2 guide](https://archipelago.gg/tutorial/Kingdom%20Hearts%202/setup/en), [RPG Site on Re:Fixed](https://rpgsite.net/feature/12932-kingdom-hearts-1-2-and-birth-by-sleep-on-pc-get-a-wealth-of-new-nonintrusive-features-and-enhancements-with-the-refixed-mod). This is a real gap and a good candidate for a PINE-based project.
- Metroid Prime: no co-op mod for Prime 1 or 3. Echoes (in the Trilogy) has one: its versus mode already runs 2-4 players in one world, so the mod loads the campaign in that mode and fixes what assumes one player (rooms, saves, scans, enemy targets, story item changes) with Gecko codes; hook bodies go to unused MEM2 through a Dolphin `[OnFrame]` patch to stay under the Gecko code list limit ([games/metroid-prime-2-echoes](../games/metroid-prime-2-echoes/README.md)). [PrimeHack](https://www.flathub.org/apps/io.github.shiiion.primehack) is controls only.
- Paper Mario (N64): no co-op mod found; only the single-player total conversion Star Nova.
- Wind Waker: no mod found; the built-in Tingle Tuner is the official 2-player mode ([ZeldaWiki](https://zeldawiki.wiki/wiki/Multiplayer)).
- Ratchet & Clank 1-3 and Sly Cooper: no memory-sync co-op project found.
- GTA San Andreas: PS2 shipped a local 2 player mode (shared screen, tethered distance, Free Roam and Rampage) per [Co-Optimus](https://www.co-optimus.com/game/1167/playstation-2/grand-theft-auto-san-andreas.html), while PC multiplayer is the SA-MP / open.mp mods ([Lutris open.mp](https://lutris.net/games/grand-theft-auto-san-andreas-open-multiplayer/)). For online co-op use the PC route, not PS2 emulation.

## 5. PS2: the full agent workflow

PCSX2 has the best RAM-access story of the "hard" consoles but no netplay, so PS2 co-op is mostly Route B (memory-sync) or Route D (remote play).

### Step 1: check PC alternatives first

1. Does a native PC version exist (GTA SA, KH2 Final Mix via OpenKH, many others)? If yes, use the PC modding playbooks.
2. Is there a decomp or reimplementation (OpenGOAL for Jak is the model; see [Jak Teamruns](https://www.dsogaming.com/news/the-unofficial-pc-port-of-jak-daxter-now-supports-online-and-local-multiplayer/))? Co-op in a port is cheaper than in an emulator.
3. Search existing mods per [coop-research](../.claude/skills/coop-research/SKILL.md) before writing code.

### Step 2: does the game already have local co-op?

If yes, there are two options with no RAM work:

- Remote Play Together or a streaming tool (section 2d). Zero development, works with any emulator.
- A PCSX2 netplay fork. Only for fighting-game style deterministic titles; the best known fork was built for a handful of games and desyncs on others ([pcsx2-online-Update](https://github.com/fraserofsmeg/pcsx2-online-Update)). Not recommended in 2026.

Split-screen or shared-screen modes in PS2 games often reuse the same code path as the second pad, so a second PINE-driven "virtual controller" (see below) may be enough to give player 2 control without any new gameplay code.

### Step 3: memory-sync via PINE

PCSX2 implements an IPC protocol, PINE, so an external program can talk to the running emulator. Facts:

- Wiki page: [PCSX2 IPC Protocol](https://wiki.pcsx2.net/IPC_Protocol) (403 to automated fetch; content from search snippets). Protocol spec and C++ reference in [GovanifY/pine](https://github.com/GovanifY/pine) (standard folder is marked as a draft; bindings folder exists; the page does not list opcodes).
- Enable it in PCSX2 Advanced settings, default slot/port 28011, PCSX2 1.7 Qt or newer ([mcp-pine setup notes](https://glama.ai/mcp/servers/dmang-dev/mcp-pine), **UNVERIFIED** third party).
- Opcodes: 8/16/32/64-bit read and write, plus version, savestate, and game metadata commands, in a reference server source ([pine_server.cpp mirror](https://repo.retrodeck.net/Xargon/Duckstation/src/commit/1fd8d2701de0c9887509703a027b8d8bb439cd6a/src/core/pine_server.cpp)). A later variant has bulk read/write opcodes 0x20 and 0x21; another third party notes there is no native bulk read so clients pipeline small reads ([glama mcp-pine](https://glama.ai/mcp/servers/dmang-dev/mcp-pine)). Check the version you target.
- Python wrapper: [pcsx2-memory-accessor](https://pypi.org/project/pcsx2-memory-accessor/).
- EE main RAM is 0x00100000 to 0x01FFFFFF (32 MiB) and scratchpad is 0x70000000 per the same MCP listing (**UNVERIFIED**, widely known but confirm in your tool).
- Multiple-instance behaviour is undocumented in the pages I found. Plan to test whether a second instance can use a different slot or a separate portable install. **UNVERIFIED**.

What PINE can and cannot do:

| Can | Cannot (or not documented) |
|---|---|
| Read and write guest RAM, query game metadata, savestates | Inject a new function call into the game thread; run Lua inside PCSX2 (no scripting found in PCSX2) |
| Poll at ~60 Hz from a bridge process | Hook the game's frame boundary precisely (you can only poll) |
| Write a value every tick to freeze it | Add a second controller without a patch; use a pnach or write the pad state each frame |

Code injection: PCSX2 loads `.pnach` files named `SERIAL_CRC.pnach` from `patches` and `cheats` folders, with lines of the form `patch=<place>,<cpu>,<address>,<type>,<data>` ([writing patches](https://pcsx2.net/docs/advanced/writing-patches)). For anything larger than a value write (a hook that reads a mailbox in RAM that your bridge fills), use a pnach to install a small MIPS routine into unused code or data space and have it consult the mailbox. This is the standard way to turn "poll memory" into "game reacts to it".

Recommended architecture:

```
PCSX2 A  <--PINE-->  bridge A  <--TCP/UDP-->  bridge B  <--PINE-->  PCSX2 B
   |                                                                     |
 pnach: reads mailbox for remote player's pad/avatar / spawns actor       same
```

1. Identify the player struct and pad input struct (see [reverse-engineering.md](reverse-engineering.md): Ghidra with a PS2 MIPS loader plus PCSX2's debugger for breakpoints).
2. Write a read-only logger first. Log position/state and prove stable addresses across loads (addresses often change per level).
3. Add a mailbox written by the bridge and a pnach routine to read it.
4. Add replication of remote player. Start with a "ghost" actor; then true shared interaction.
5. Add flags and inventory sync as events.

### Step 4: what to sync

| Class | How | Rate |
|---|---|---|
| Local player pose | Position, yaw, anim id, weapon id, health | 10 to 20 Hz, interpolated |
| World flags, progress, pickups, doors | Diff a flag bitfield | Event |
| Enemies, bosses | Single authority (host) or accept per-player copies | 5 to 10 Hz for hosts, or none |
| Camera | Do not sync; each client has its own | none |
| Cutscenes, loading | Gate: wait for both clients, or disable cutscene pauses (Jak Teamruns runs cutscenes in real time to avoid blocking the other player; see [DSOGaming](https://www.dsogaming.com/news/the-unofficial-pc-port-of-jak-daxter-now-supports-online-and-local-multiplayer/)) | Event |

### Step 5: latency and determinism limits

- Memory-sync co-op is not lockstep, so it tolerates 50 to 150 ms RTT for exploration games. Twitch combat and physics-coupled puzzles need host authority or will feel wrong.
- Both emulators run free; do not try to lock frames. Use interpolation.
- Cross-save compatibility: both players must start from compatible saves. Anchor's approach, loading the host save state on join, is a good model ([anchor](https://github.com/garrettjoecox/anchor)).
- Emulator speed hacks and overclock (some Deadlocked fixes use 130% EE cycle rate per [PCSX2 wiki](https://wiki.pcsx2.net/SCES-53285)) change timing but not RAM layout; keep all clients identical.
- Pointers move between versions and regions. Use the game serial and executable CRC (same naming as pnach files) to select an address table.

### Step 6: testing two instances on one PC

- Run two PCSX2 installs, each portable, with different PINE slots if supported (undocumented; test it). **UNVERIFIED**.
- Run two bridge processes on loopback ports.
- Script the test (see [automated-qa.md](automated-qa.md)): boot save state, write a pad value into player A, assert via PINE that player B sees the remote avatar's position within tolerance after N seconds, then assert world flags. No human needs to look at the screen.
- Both instances compete for GPU; use a low resolution and software or Vulkan renderer for the second one if frame times suffer.
- Check the user is not running a game before launching (shared PC rule).

## 6. Per-console quick recipes

Short decision notes; details in the sources above.

- **N64:** If the game has a recomp or decomp, use that (SM64 coop-dx, SoH + Anchor). Otherwise ModLoader64 for supported titles, or BizHawk Lua plus a Node/Python relay. Netplay for 4-player games via gopher64.
- **GameCube/Wii:** Dolphin netplay for local-multiplayer titles; Dolphin Memory Engine or Felk's Python fork for single-player-to-co-op bridges. Wiimmfi for dead Wii online services.
  If the game has a decompilation that rebuilds byte for byte, you can add your own actor instead of only writing memory
  (Skyward Sword, [write-up](../games/skyward-sword/README.md)):
  - Build a REL with the decomp's toolchain that replaces an unused actor slot (a debug or test actor whose profile
    the game never spawns), and pack it into the extracted game's `rels.arc` in a fixed-size slot so the archive layout
    never changes between builds.
  - Spawn it with a small Gecko C2 hook at the game scene's execute function. Keep a data block inside the hook: its
    address is the actor's parameter, and an outside bridge finds it from the hook's branch and uses it as a mailbox.
  - Guest RAM is a `MEM_MAPPED` view whose first bytes are the game ID; no Dolphin fork is needed.
  - A savestate contains the loaded REL and the hook's block, so it only works with the build it was made with:
    write a build number into the block and check it before trusting a savestate.
  - Dolphin's savestate hotkey only reaches the game when its render window has focus; check the state file's
    timestamp after asking a person to save.
- **PS1:** No mainline netplay; BizHawk Lua works on PS1 per its supported systems list ([BizHawk](https://github.com/TASEmulators/BizHawk)). Check for ports first (OpenGOAL-style efforts).
- **GBA/GB:** mGBA Lua scripting with sockets is the cleanest; one script per instance, TCP relay in the middle. Pokemon-style shared worlds are done by writing "other trainer" data into NPC slots.
- **DS/3DS:** Use the emulator's own local-wireless emulation (Azahar rooms, melonDS Wi-Fi path), not RAM sync, when the game already supports local wireless.
- **PSP:** PPSSPP ad hoc with relay servers for games that have ad hoc mode.
- **PS3/Xbox/360:** Use RPCN or XLink Kai for games with a network mode. RAM sync is untested territory.
- **Switch:** Out of scope unless the user explicitly supplies a legal context; see the table.

## 7. Legal and attribution

- Do not host or link ROMs, ISOs, keys or BIOS in this repo.
- Patches distributed by a mod must be patch-only (IPS/xdelta/pnach deltas) and apply to the user's dump. Do not ship copyrighted data.
- Server revivals are third-party infrastructure: say so in the README, and offer a self-host option (Anchor does both public and self-hosted, [README](https://github.com/garrettjoecox/anchor)).
- Switch emulation is legally sensitive: the original Yuzu and Ryujinx were shut down in 2024 and multiple successors are under legal pressure per secondary reports ([tech-insider](https://tech-insider.org/?p=19175)). Keep Switch work to existing open emulators, and do not contribute circumvention code.
- Credit and license: respect GPL licenses (ModLoader64 Z64Online is GPL-3.0 per its repo page), and check that any fork you build on has a license file (Anchor's repo page did not show one).

## 8. Open questions and unverified items

Re-check these before telling a user they work.

1. Whether PCSX2 supports multiple PINE slots for two simultaneous instances.
2. Whether any PCSX2 netplay fork builds against 2026 master and runs a real game.
3. Whether DuckStation has an official PINE server and whether HeatXD's GGPO build is maintained.
4. ModLoader64 and Z64Online maintenance status in 2026 (public repos exist, activity unclear).
5. Whether Dolphin BBA still reaches PSO private servers on current builds.
6. Whether Horizon or Clank servers work with PCSX2 DEV9.
7. Parsec as a remote play option (no source retrieved).
8. RPCS3 netplay game list and whether any RAM-bridge exists for RPCS3.
9. All Switch emulator network features.
10. Project64 netplay in current builds.
11. Mainline Dolphin scripting (reported stalled; fork only).

Nintendo Switch has its own page: [switch.md](switch.md) (local wireless, hooking on your own console, decompilations).
