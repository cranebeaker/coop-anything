# Oracle of Seasons: two-player co-op design

Status: approved on 2026-10-16, including the edge-of-screen partner marker.

## Intent and decisions

Build a separate Linux co-op edition of the tested native Oracle of Seasons port.
Two friends should control two interactive Links, fight and solve puzzles together,
and also wander into different rooms while their partner is busy. Reuniting must
bring them back into the same enemies, objects and puzzle state.

Confirmed owner requirements:

- Two players.
- Both use the same Linux OS as the owner; first package targets Linux x86-64.
- Each player controls their own Link.
- Running off into another room while the partner is busy is desired. A forced
  party-follow rule is rejected.

The owner approved this design and authorized following the repository workflow,
including the previously described project-local source/dependency builds and the
prepared upstream claim issue. Unrelated account changes, shared-branch merges and
other destructive operations remain outside this authorization.

## Player-facing behavior

### Exploration and cameras

Each player has their own camera. Either may use a door, staircase, screen edge or
warp independently, subject to the original game's gates. No leash, automatic
teleport, or partner-confirmation prompt is imposed during ordinary exploration.

Show an offscreen partner marker clamped inside the gameplay viewport edge. It
contains the partner's blue Link portrait, the label `P2` (or `P1` when viewed by
player 2), and an outward chevron. Its position follows the partner's direction;
it disappears when the actual partner is visible. Keep it clear of the top HUD.
Show the partner's area name when different rooms, floors or interiors make a
straight-line bearing ambiguous; do not display an invented compass direction for
unrelated coordinate spaces. Hide it when the partner is disconnected or their
location is unknown. The approved image mockup is kept in ignored local artifacts
because it contains a gameplay screenshot.

Provide a consensual
"join partner" action as a convenience: the other player accepts and the engine
chooses a safe landing spot. It cannot cross an unearned story/ability gate, escape
a locked encounter, or be required to progress. Its implementation can follow the
core travel work; independent travel is part of the initial architecture.

When both players enter the same room, they share the same enemies, puzzle objects,
switches and pickups. They do not become two isolated copies with visible ghosts.
Distinct tunic colors and a small P1/P2 marker identify the two players. Players do
not body-block or directly damage one another in the initial edition.

### Puzzles and interactions

Keep the original puzzles and allow cooperative shortcuts. Both players can use
their earned tools on the same blocks, switches and other interactive objects.
Do not redesign every dungeon to require two people: either can continue alone
while the partner explores elsewhere.

An object moves, takes damage, opens or pays a reward once. Simultaneous requests
for a chest, movable block, NPC conversation or key door are resolved by the host
in a deterministic order. The losing request gets the resulting shared state.
Weapons/projectiles remember their player owner. An enemy can target and damage
either present Link; targeting cannot remain hard-wired to player 1.

Occupied rooms continue simulating while the other player is elsewhere. Preserve
an occupied room when the partner enters or leaves it. When nobody occupies a room,
persist permanent changes; transient enemies and puzzle resets follow the original
room-reentry rules unless a documented co-op exception is necessary. Never unload
one player's active room just because the other leaves.

### Items, abilities, health and death

Permanent tools, their levels, essences, heart-container capacity, dungeon maps,
compasses, keys and quest progress are shared. Finding a major ability immediately
unlocks it for both players, with a notification for the absent partner.

Equipment buttons, selected ammunition, action timers, damage, health and temporary
status effects are per player. Both may equip the same unlocked tool. A world item
is collected once. Rupees, ammo and small keys use a shared host-owned stock; spending
one unit or opening a key door is one transaction. Ordinary healing affects the
collector; the shared maximum-health upgrade grants its normal benefit to both.

A defeated Link respawns at their latest safe room entrance using that player's
normal death penalty; the other player continues. If both are defeated, return to
the host's co-op campaign checkpoint. Do not roll back a living partner's permanent
progress. Companion, transformation, mount and special-item behavior must be tested
explicitly rather than assumed to work for both players.

### Seasons, dialogue and story

The Rod of Seasons changes the authoritative state of the area affected by the
original game. Both players in that affected area see the same result; a player
elsewhere keeps playing. Do not silently create different seasons for two players
occupying the same physical room. Determine the original affected-area boundaries
from source during implementation. Test a partner standing on a tile whose
collision/terrain changes; resolve unsafe placement with the original hazard or
safe-respawn behavior, without trapping the partner inside new solid terrain.

Ordinary NPC dialogue and inventory menus pause only their player. Room-local
scripted scenes temporarily control the players present in that room. A scripted
global event may synchronize both players when required to avoid an inconsistent
world; enumerate such exceptions and show a clear notice. The initiating player
advances dialogue, while both receive the resulting world changes. A partner's
position must not be interpreted as player 1's position by a script.

Bosses are shared room encounters: either can enter, participate or join an active
fight if the original encounter permits entry. A boss cannot be killed or award
an essence twice. Rules that prevent leaving a locked encounter apply per original
gameplay, rather than automatically dragging a distant partner into the fight.

## Implementation constraints and architecture

Pin the initial base to native-port commit
`533860de269e0e9b338e12f3484ce8497367e6ed` and the verified USA/Australia ROM.
This is a C source fork with a GPLv3-compatible distribution. The existing Lua API
is observation-only and cannot implement the requested gameplay.

There is one canonical host campaign and one authoritative owner of every active
room. The intended host design has up to two occupied-room runtimes, with explicit
player contexts independent of the original singleton Link. If both Links are in
one room, that room's world is simulated once with two players. If they split,
both occupied rooms advance. A room transition migrates the player context and
owned entities under explicit rules; arriving at the partner's room joins its
existing simulation instead of resetting it.

Campaign transactions are explicitly typed: pickup claims, inventory unlocks,
resource spends, room flags, story stages and encounter completion. Do not OR whole
RAM images or copy a player's complete save over another room runtime. Numeric
quantities and temporary puzzle state are not monotonic flags.

The native source retains original register/RAM execution conventions. Independent
room runtimes, callbacks, scheduler state and global scratch must be audited for
isolation. A bare duplicate of the 64-byte Link object is insufficient. Same-room
collision, item ownership, enemy targeting, scripts and rendering all need explicit
support. The builder may select the exact process/runtime boundary and rendering
transport after proving these constraints. If the proposed room architecture fails
its isolation gate, report the evidence and revise the architecture without silently
replacing the requested behavior with single-player item sharing.

The host accepts each player's input and owns shared simulation outcomes. The
builder chooses snapshot/event encoding, compression, prediction and correction
after the native gameplay gates. The default first network path is direct IP on
loopback/LAN, retaining a transport boundary so an approved VPN or relay can be
added for friends on different networks. Internet connection setup is part of the
playable deliverable, with the exact service chosen only after account/install
permissions are resolved. No host firewall changes, public listeners, account use
or Steam integration are authorized by this design document itself.

## Saves and packaging

Use a new co-op profile with a versioned save format for campaign state, both
players, persistent room changes and required checkpoints. The host owns it; guest
progress is session state and must not overwrite any guest single-player save.
Joining/rejoining gets an authoritative snapshot and stable player identity.
An unexpected disconnect leaves the host able to continue and the guest able to
rejoin without duplicating items. Host loss returns the guest cleanly to the menu;
automatic host migration is deferred.

Ship a separately named Linux application with Host/Join controls and build/protocol
compatibility checks. Each player supplies their own valid ROM. No ROM, extracted
assets or existing saves are bundled. Original port installation and single-player
saves stay untouched. Online sync from the single-player application is not enabled
for the co-op profile.

## Evidence required before calling it playable

The first gate is native same-room gameplay: two independent in-engine input
drivers, a second player interacting with a real block and damaging a real enemy,
independent health, collision with actual room geometry, and one world update per
tick. A second sprite alone does not pass.

The next gate is independent rooms: one player moves to another room while the
other keeps solving/fighting; both rooms advance; a shared ability is acquired;
the absent player receives it; then players reunite without enemy/puzzle resets or
duplicate rewards. Check occupied-room joins and last-player departures separately.

Automated two-client tests then cover combat by both players, simultaneous claims
and spends, puzzle interactions, ordinary dialogue, seasons and changing terrain,
boss rewards, different-room travel/warps, death, late join, reconnect, save/reload,
and graceful shutdown. Compare canonical host/client state and assert the actual
two-player mode and room occupancy. Add latency, jitter, packet loss and interrupted
connections, plus negative controls proving failures are detected. Tests run bots
inside game code and use game-generated screenshots/logs, never the owner's desktop.

Use separate ROM copies and profiles in ignored `local/`. Record exact test commands
and results. A feasibility prototype, a tested early-game slice, and a verified
campaign are distinct evidence levels. Do not advertise full-campaign compatibility
until a repeatable campaign test has been completed.

## Bounds and remaining decisions

Confirmed scope is two Linux players in original Seasons. Ages, linked-game campaigns,
randomizer compatibility, more players, mobile, split-screen, host migration and
redesigned mandatory two-person puzzles are deferred.

Builder-owned choices include player-context layout, safe scheduling boundary,
room runtime isolation, authoritative event schema, packet format, renderer strategy,
bot implementation, dependency pinning and artifact layout. None may weaken the
independent exploration or same-room interactive gameplay requirements.

Implementation is authorized. Use the repository's required parallel subsystem
workers and automated game-client tests. Keep original game data and private state
under ignored `local/`; show contribution diffs before creating draft pull requests.
Approval does not change the evidence standard: co-op code must pass real gameplay
tests before it is described as playable.

## Source basis

- [Pinned native source](https://github.com/kirby-letsgo/oracles-decomp/tree/533860de269e0e9b338e12f3484ce8497367e6ed)
- [Pinned RAM definitions](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/game/ram.h)
- [Mod API limits](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/MODDING.md)
- [Co-op playbook agent rules](https://github.com/cranebeaker/coop-anything/blob/74d8f769ac6d1171d920ab307cf043558bef6384/AGENTS.md)
- [Existing multiworld behavior](https://jangle.dev/oracles-randomizer/manual.html#multiworld)
