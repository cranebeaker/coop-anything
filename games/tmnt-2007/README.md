# TMNT (2007, PC) Local Co-Op Mod

> **Experimental Prototype Notice:** This is an in-progress reverse-engineering project implementing local 2-player co-op for the 2007 PC release of TMNT. The campaign, scripted boss battles, and level transitions have not been universally tested. Expect visual quirks, edge-case desynchronizations, and limitations in untested levels.

| Attribute | Specification |
|---|---|
| Engine | Ubisoft Jade ("Engine v428-42" in engine telemetry; gameplay AI compiled into exe) |
| Versions tested | Retail PC, `TMNTGame.exe` 1.0.0.188 (DirectX 9, 32-bit x86, SHA-256 `701d9f00f8c1edf790466f2ecfeb72a64fe7ccdc491f7c370ae11c1942740589`) |
| Architecture | Dual-DLL injection (`coophost.dll` runtime injector + `coopmod.dll` hot-reloadable gameplay logic) |
| Tested Environments | Level 1 (*Mystical Jungle*), Map 5 (*Ninja Tag Time* - Multi-Turtle Rooftop) |
| Status | In active development (Mod 127). Not a final commercial-grade release. |
| Last Updated | 2026-10-29 |

---

## 1. Overview and Current State

The mod enables two players to play concurrently on a single PC screen within the same game process using two separate controllers (Xbox / XInput, PlayStation DualSense via direct HID, or Nintendo Switch Pro / Switch 2 Pro Controllers via WinUSB) or keyboard/controller combinations.

### Key Implemented Capabilities
- **Independent Player 2 Control & Input Pipeline:** The game's 584-byte active pad structure (`[0x01502198]`) is isolated and swapped across per-frame input updates and Player 2 AI ticks, eliminating the default engine behavior of merging all connected gamepads into Player 1.
- **Physical 3D Body Rendering:**
  - In multi-turtle levels (such as Map 5: *Ninja Tag Time*), Player 2 can be spawned as Raphael with complete 3D green mesh, shell, red head bandana, red belt, and twin Sai daggers, or as Leonardo with twin katanas, preserving all 31 body skin and shell materials.
  - In single-turtle solo levels (such as Level 1: *Mystical Jungle*), the Jade Engine loads only the designated story turtle into RAM. Player 2 is created via an unshared skeleton duplicate (`0x00491C40` + `0x006AC230`), retaining full 3D body visibility as a visual duplicate of the active character.
- **Two-Way Mutual Revive System:** When either turtle drops in combat (`state 18`), the standing teammate can revive them by remaining within proximity (2.8m) for 1.2 seconds, restoring 100 HP and returning them to playable ground state (`state 2`).
- **Dynamic Combat Aggro Distribution:** Enemy AI target evaluations dynamically re-route threat. If one player is downed, 100% of enemies attack the active player; when both are alive, aggro splits dynamically based on Euclidean proximity.
- **Direct3D 9 HUD and Visual Features:**
  - Player 2 slanted health HUD rendered via D3D9 EndScene hooks, tracking memory health (`AI + 0x174`).
  - Ground-projected tactical foot rings (emerald green for P1, crimson red for P2) conforming to uneven terrain.
  - Off-screen radar arrow and distance indicator pointing toward the partner when separated.
- **Team Moves Execution:** Air leapfrog (`state 11`) and ground charged team attacks (`state 25` -> `state 14`) trigger with Player 2 as the partner, with exit-leap animation clipping preventing unwanted partner ejection.
- **Mutual Body Collision & Boundary Clamping:** Physical cylindrical barrier (1.05m radius) prevents players from walking through each other during navigation.
- **Abyss / Pit-Fall Auto-Recovery:** Automatic safe repositioning when a player falls into out-of-bounds voids (`dz < -12.0m`).

---

## 2. Controls and Keybindings

The mod exposes a range of gameplay toggles, spawning triggers, and diagnostics via keyboard and controller shortcuts.

### Keyboard Shortcuts
- `INSERT`: Spawn Player 2 into the level or manually teleport Player 2 beside Player 1.
- `DELETE`: Unspawn / remove Player 2 from the active world.
- `F4`: Toggle off-screen radar pointer and distance beacon (ON / OFF).
- `F5`: Toggle 3D tactical ground foot rings (ON / OFF).
- `F6`: Toggle retro arcade synthesized audio cues (ON / OFF).
- `F7`: Toggle co-op combo meter (ON / OFF).
- `F8`: Toggle Player 2 slanted health HUD (ON / OFF).
- `F9`: Master Preset Toggle (Instant switch between **Arcade Enhanced** and **Purist Vanilla**).
- `F10`: Toggle controller vibration and force-feedback dispatch (ON / OFF).

### Controller Shortcuts (Player 2 Gamepad)
- `START` or `A`: Drop-in / join the game when Player 2 is not currently spawned.
- `LB + RB` (simultaneous press): Cycle Player 2 character identity (Leonardo, Raphael, Donatello, Michelangelo).
- `BACK + START` (simultaneous press): Toggle in-game co-op diagnostics menu.
- Hold `BACK` (1.2 seconds): Trigger instant tactical regroup / teleport Player 2 to Player 1.

---

## 3. Known Limitations and Objective Failure Modes

Because TMNT (2007) was designed from the ground up strictly as a single-player experience with hardcoded single-actor assumptions, users should be aware of the following technical boundaries:

### A. Level Asset Boundaries
- **Solo Campaign Levels:** Story missions like Level 1 only stream a single turtle's assets (models, textures, weapon skeletons) into memory. In these levels, switching Player 2 to an unstreamed turtle (e.g. attempting to force Raphael in Level 1) cannot load missing models from unmounted packages without causing asset stalls or fallbacks. In solo levels, Player 2 remains a visual duplicate of the active story turtle.
- **Multi-Turtle Levels:** Levels designed with tag mechanics (like *Ninja Tag Time*) contain all four turtles in memory, allowing full model and identity switching between Leonardo, Raphael, Donatello, and Michelangelo.

### B. Untested Campaign Scope
- **Broad Campaign Coverage:** Automated testing and verification have been executed primarily in Level 1 (*Mystical Jungle*) and Map 5 (*Ninja Tag Time*). The remainder of the retail campaign—including later city chapters, vehicular chase sequences, underground sectors, and multi-phase boss encounters—has **not** been exhaustively tested.
- **Scripted Checkpoints & Cutscenes:** Scripted in-game cinematic triggers, pre-rendered video handoffs, and checkpoint reloads may occasionally fail to account for the second player actor, resulting in temporary camera misalignments, partner despawns, or state freezes until manual repositioning (`INSERT` or hold `BACK`) is used.

### C. Controls and State Triggers Not Guaranteed in All States
- Shortcut keys (such as `INSERT`, identity switches, or revive channels) interact directly with Jade Engine internal finite state machines (`AI state`).
- Activating spawn, teleport, or character swap shortcuts while a character is in non-ground states (such as wall-running, pole-swinging, mid-air ledge grabbing, or inside cutscenes) may fail to transition smoothly or cause brief physical snapping.
- Controller hot-plugging requires controllers to be recognized by the OS prior to game launch; disconnecting and reconnecting non-XInput controllers during heavy combat may require a controller reset.

---

## 4. Architecture and Engine Hooks

The modification does not alter any original game files on disk (`TMNTGame.exe` and `turtles.bf` remain untouched). All functionality is dynamically injected at runtime:

- **Input Pipeline Detour (`0x00418E0B` -> `0x00639C80`):** Swaps pad buffers twice per engine frame to feed independent input history buffers.
- **Movement Input Hook (`0x004E07D9` -> `0x004F9910`):** Injects Player 2 movement vectors while enforcing camera leash boundaries and mutual body collision.
- **Team Move Interceptors:** Detours on `0x005759D0` (`GetCoopPartner`), `0x005C1DF0` (`CheckCoopLock`), and `0x005C17C0` (`Turtle_LockCoop`) to route cooperative attacks to Player 2.
- **Actor State Wrapper (`0x00585C50`):** Traps engine eviction signals (`state 0`) during team move exits to keep Player 2 active in the world sector.
- **Direct3D 9 EndScene (`vtable + 0xA8`):** Renders UI primitives, tactical foot rings, off-screen radar pointers, and health bars directly onto the backbuffer before presentation.

---

## 5. Verification Status

| Test Suite | Scope | Result | Evidence |
|---|---|---|---|
| Level 1 Solo Isolation & Input Test | Verification of zero input bleed between P1 and P2 | PASS | Backbuffer video capture |
| Level 1 Fixed Team Moves | Verification of air leapfrog and ground charged attacks | PASS | Backbuffer video capture |
| Map 5 Ninja Tag Time Multi-Turtle | Verification of full 3D Raphael model, red bandana, and twin Sais | PASS | Direct3D 9 full-res capture |
| Two-Way Mutual Revive Machine | Bidirectional revive of P1 by P2 and P2 by P1 | PASS | Automated testlane verification |
| Multi-Level Cycling & Abyss Recovery | Pit-fall auto recovery and Z-axis depth clamping | PASS | Automated testlane verification |
| Rest of Campaign (Levels 3..16, Bosses) | Campaign progression, boss scripts, vehicular levels | **UNTESTED** | Ongoing development |
