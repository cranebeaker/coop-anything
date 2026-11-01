# Persona 5 Royal (2022)

| | |
|---|---|
| Engine | Atlus in-house (GFD), closed native x64 binary, Denuvo-protected |
| Versions tested | Steam 1.0.4.0, `P5R.exe` SHA-256 `5D9E18C383700302D029C947DDF8C811BA6AFA47E60EC1AFA0554D452BDC0CBF`, Windows 11 |
| Projects | Shared-screen turn-rotation prototype `p5rpc.coop` (Reloaded-II mod, not public yet); directory entry: gap |
| QA level | none (offline unit checks plus one tester playtest; no automated in-game test) |
| Last updated | 2026-10-30 |

## Where things stand

No co-op mod exists. A prototype makes one game copy shared between players: in battle, each party member's turn
only accepts the controller of the player who owns that character; outside battle Player 1 walks. It worked in
one tester playtest on one PC with two XInput pads. Two copies on two PCs (own screen each) are not started: they
need real netcode, and every player needs their own licence.

## Existing work

- No multiplayer or co-op mod found (web search, GitHub repository search, this repo; 2026-10-30). Nexus Mods,
  Reddit and the ShrineFox Discord not checked.
- Modding base: Reloaded-II with [p5rpc.modloader](https://github.com/Sewer56/p5rpc.modloader) ("Persona
  Essentials"). The executable is Denuvo-protected, and inlining makes functions hard to patch
  ([ShrineFox guide](https://shrinefox.com/blog/2022/10/22/modding-persona-5-royal-on-pc-switch/)).
- Useful libraries (GPL-3.0): [p5rpc.lib](https://github.com/AnimatedSwine37/p5rpc.lib) (call flowscript functions,
  sequence info) and [p5rpc.inputhook](https://github.com/AnimatedSwine37/p5rpc.inputhook) (edit the game's merged
  key list; too late for per-device routing, see below).
- No Remote Play Together on Steam: the store's feature list for app 1687950 is Single-player, Achievements, Full
  controller support, Trading Cards, Cloud and Family Sharing
  ([store API](https://store.steampowered.com/api/appdetails?appids=1687950&filters=categories)).

## How the game works (for co-op)

- **No netcode.** Single-player only.
- **Input:** `P5R.exe` imports `xinput1_4` by ordinal (#2 `XInputGetState`, #3 `XInputSetState`),
  `dinput8!DirectInput8Create`, and asks Steam for `SteamInput006`. In practice it polls XInput about 470 times a
  second. A hook on `XInputGetState` in the system DLL routes pads without touching the protected exe.
- **Flowscript function table:** 461 entries of `{fn, argc, name}` at VA `0x14192F9C0` (plaintext on disk),
  including `BTL_GET_CURRENT_CHARAID` (`0x140B295A0`) and `AI_GET_MY_ID` (`0x1407B5580`). Both read the global
  script context at `0x14293D008`, then `[ctx+0x260]`, so they only work inside a battle script. Calling them from a
  mod through p5rpc.lib would dereference null.
- **Acting party member in battle:** u16 chara id at `+0x592` of the battle object (Joker 1, Ryuji 2, Morgana 3,
  Ann 4, Yusuke 5, Makoto 6, Haru 7, Futaba 8, Akechi 9, Kasumi 10). The exe has no ASLR (DllCharacteristics
  0x8120, base `0x140000000`). Six pointer chains give the right id in all 12 probe snapshots across two launches,
  and the second launch included a One More:

  | Static (`P5R.exe+`) | Chain | Code refs to the static |
  |---|---|---|
  | `0x286C2A8` | `+0x88 -> +0x48 -> +0x592` | 142 |
  | `0x2900BF8` | `+0x98 -> +0x48 -> +0x592` | 18 |
  | `0x2530FB0` | `+0x90 -> +0x48 -> +0x592` | 11 |
  | `0x293DB58` | `+0x148 -> +0x48 -> +0x592` | 23 |
  | `0x2901F58` | `+0x380 -> +0x108 -> +0x592` | 26 |
  | `0x2536F78` | `+0x18 -> +0x18 -> +0x592` | 10 (noisy outside battle) |

  Outside battle the chains read 0 or junk, so "at least two chains agree on 1..10" is a usable in-battle test
  (seen in two tester sessions). A "One More" keeps the same actor id. Also seen in the battle object: `+0x2B0`,
  `+0x2BC`, `+0x2C4` u32 count 0, 1, 2, 3 over the first four turns (turn or slot index, unknown which).
- **Traps:** `P5R.exe+0x293EEF8` pointed at the battle object in the first probe but has no code references. Don't
  use it. `0x1423F3E70` is a generic intrusive list head (nodes linked at +8, +0x10, +0x18).
- **Launching a copy:** with Reloaded-II (`Reloaded-II.exe --launch`), a copy outside a Steam library needs
  `steam_appid.txt` with `1687950`; Steam must be running and the account must own the game.
- **Saves:** `%APPDATA%\SEGA\P5R\Steam\<steam id>\`, shared by every copy of the game. Back it up before any test.

## What we changed

1. **Turn ownership** (pure C#, `P5RCoop.Core`): players take party members round-robin, in the order the characters
   first act in each battle. Optional pins per character; Player 1 walks outside battle.
2. **XInput gate** (`XInputGetState` hook): the game sees only the tester's pad, as pad 0. Player N gets the Nth
   connected XInput pad. If the tester has no connected pad, every pad passes.
3. **Battle actor reader:** polls the six chains every 30 ms through `ReadProcessMemory` on the pseudo-handle
   (`-1`). It caches the result, so the input hook never touches game memory. It logs actor decisions, not raw
   reads.

Bugs on the way: see lessons 84 and 85 in [playbook/lessons.md](../../playbook/lessons.md). Details are in
[NOTES.md](NOTES.md).

## QA

- Offline: `dotnet run --project P5RCoop.Tests` covers ownership rules, pad mapping and the actor tracker.
  32 passed, 0 failed (2026-10-30). This includes the exact turn order from the in-game probe.
- Offline replay: the six chains plus the vote, re-read from both probe sessions' snapshots, give the reported actor
  order (4/4 and 8/8).
- In game: one tester playtest on one PC, two XInput pads. The log shows Joker on pad 0, Morgana on pad 1, Ann on
  pad 0, then field on pad 0. No automated in-game test yet, so the QA level is `none`.

## How to play

Not released. The prototype needs Reloaded-II, `reloaded.sharedlib.hooks`, a copy of the game and two XInput pads.

## Known issues

- Keyboard and DirectInput pads are not gated. A pad in DirectInput mode cannot be Player 2; switch it to XInput.
- Only Player 1 walks outside battle.
- Streaming to a remote friend is untested. Steam Remote Play Together is not available for this game.

## Next steps

- In-game bot for the turn-rotation test (playbook rule 11), e.g. send pad states through the XInput hook and
  assert the logged tester.
- Two-copy co-op (own screen each): check battle determinism first (same save, same commands), then command lockstep
  with host ordering and shared RNG, as with BattleTech in
  [how-coop-mods-work.md](../../playbook/how-coop-mods-work.md) 1.6. Needs the battle command function, the battle
  RNG and a way to start the same encounter on the guest.
- Field avatar for a second player (puppet), unknown.
