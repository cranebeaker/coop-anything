# Forager — agent notes

Working notes for adding co-op to Forager. Written incrementally per AGENTS.md rule 3.
Game slug: `forager`. Design proposal lives in `DESIGN.md`; one-page write-up in
`README.md`.

Owner-specific state (their install, saves, license, backups) lives in
`local/owner-state.md`, which git ignores. This file is PR-safe.

## Identity

| Field | Value |
|---|---|
| Game | Forager (2019), HopFrog / Humble Grove |
| Stores | Steam app **751780**, GOG, consoles |
| Engine | **GameMaker Studio 2** — confirmed by `data.win` in the install folder |
| Modding | built-in modding API; mods are `.gml` script folders published on Steam Workshop |
| OS tested | Windows (win32) |
| Steam build | `6160665` (Steam `StateFlags` 4; verified 2026-10-19) |
| `Forager.exe` SHA-256 | `AD2B90F164320F646C340B0BE60AF966851DFDC74DCE366580F3DCD531853AE6` |
| `data.win` SHA-256 | `0F5CCBFBBD80DD7EB5F1C4FDE676C4C1F8F1DC77E24BCCE9086D8EC2B38144E8` |
| `steam_api.dll` SHA-256 | `AF17DF745250D1814EAA274FFF7B0FAEB43381E6762E026267E5859778477ABD` |
| Caveat | **Modding is not Unity.** No BepInEx / Harmony / Il2Cpp. See "How modding works". |

## Observations (checked)

### The official multiplayer is dead
- Announced, beta-tested, then **cancelled in 2021** because it was too buggy; refunds
  offered to backers who bought for multiplayer.
  Sources: https://www.rockpapershotgun.com/forager-cancels-multiplayer ,
  https://techraptor.net/gaming/news/forager-developers-respond-to-multiplayer-cancellation

### The modding API and the cancelled netcode share an author
- **YellowAfterlife (yal.cc)** wrote Forager's modding API and helped with its networking.
  Sources: https://yal.cc/2019-in-review/ , https://yal.cc/2020-in-review/
- His retrospective on the cancelled multiplayer is the authoritative "why":
  https://yal.cc/forager-retrospect/ (mirror:
  https://web.archive.org/web/20250123110432/https://yal.cc/forager-retrospect/ ).
  Key quote: *"if the work started from local multiplayer, the game could still be played
  over internet using Parsec, Remote Play Together, or a tool I made. But it was not, so
  it cannot be, and at most someone might make a slightly more advanced version of
  topherlicious' PenPal mod."*

### Generic GameMaker co-op injectors do NOT apply
- **GMTogether** (also by YellowAfterlife, https://yellowafterlife.itch.io/gmtogether ,
  readme https://yal.cc/docs/gm/gmtogether/ ) injects online play into GameMaker games,
  but it is explicitly *"a tool for playing **local multiplayer** GameMaker games over the
  internet"*. Forager has **no local multiplayer**, so it is out — consistent with the
  retrospective quote above. (Forager is not in GMTogether's tested-games sheet as far as
  we could see; the sheet export returned only its README tab.)

### Prior art found by us, on Workshop
- **PencilPal (BETA)** — the co-op mod, Workshop item `2843162754`.
  - Its Workshop page states: *"This item has been removed from the community because it
    violates Steam Community & Content Guidelines"* and *"This item is incompatible with
    Forager"*. Subscription is no longer possible; page visible to existing subscribers.
  - Last updated **2023-04-08**; author "Two" (SteamID64 `76561198160202307`); target
    API version 1.0.1 (from `fmConfig.json`).
  - Comments into late 2025: frequent crashes, "unplayable", "full of bugs".
  - Needs the host's **TCP port 6511 forwarded**; no relay. Community workaround: Hamachi.
  - Predecessors: **PenPal** (topherlicious) which "only showed a friend's sprite".
  - Sources: https://steamcommunity.com/sharedfiles/filedetails/?id=2843162754 ,
    https://playco-opgame.com/multiplayer-mod/forager-multiplayer-mod/
- **Foragipelago** — a Forager mod that integrates with Archipelago. Proof that Forager
  mods are able to talk to **an external server over sockets**. (Found in the Workshop
  listing; item id 3760668082 — page did not render through our fetcher, verify later.)

### The hub has no prior work on Forager
- No Forager row in `data/directory.yaml`; no issues/PRs mentioning Forager in
  cranebeaker/coop-anything (checked 2026-10-19 via GitHub search UI).

## How modding works (from PencilPal source, observed)

A Forager mod is a **folder of `.gml` files** compiled by the game's own modding system.
PencilPal's folder (still in the owner's Steam Workshop cache) is a complete reference
implementation; the file list was:

```
fmConfig.json          manifest: {name, author, targetAPIVersion, dependencies}
Main.gml               #define Main — entry point, globals, setup
Networking.gml         #define Host / Join / Disconnect / packets (47 KB)
GUI.gml                in-game phone UI (18 KB)
Math.gml
Events/OnStep.gml      per-step hook (33 KB) — the sync loop
Events/OnEvent.gml     (15 KB)
Events/OnNetworkAsync.gml   async networking callbacks
Events/OnDrawEnd.gml, OnDrawGUIEnd.gml, OnChatMessage.gml, OnLevelUp.gml
Resources/Sprites/..., Resources/Sounds/...
```

### Modding API surface seen in the code
- Game data: `Spawn.Count`, `SpawnGet(l, SpawnData.ID)`, `Structure.Count`,
  `StructureGet(l, StructureData.Object)`, `StructureCreate(...)`, `ZoneCreate(...)`,
  enums `StructureType`, `ZoneType`, `BuildMenuCategory`, item enum `Item.*` (e.g.
  `Item.Steel`, `Item.Coin`), `spr*`/`tiles*` built-in asset names.
- Player/world: `objPlayer`, `objPlayer.room`, `objPlayer.sprite_index`, `image_index`,
  `ItemCount(Item.*)`, `RemoveItem(Item.*, n)`.
- Persistence: **`ModSaveDataSubmit` / `ModSaveDataFetch`** — used to sync treasure chests.
- Utils: `Trace(...)` (logging), `get_string` / `get_integer` (GM dialogs),
  `sprite_add`, `audio_create_stream`.
- Event hooks the API calls into: **`OnStep`, `OnEvent`, `OnDrawEnd`, `OnDrawGUIEnd`,
  `OnChatMessage`, `OnLevelUp`, `OnNetworkAsync`**.
- The author confirmed the game **allows mods to use networking functions** (his
  retrospective + the fact that PencilPal ships TCP code and was published).

### Transport used by PencilPal
- **GameMaker built-in TCP sockets** (not Steam):
  - Host: `network_create_server(network_socket_tcp, 6511, 1)`.
  - Client: `network_create_socket(network_socket_tcp)` +
    `network_connect(sock, ip, 6511)`; IP is typed by hand into a `get_string` dialog.
  - Async event `OnNetworkAsync`: `network_type_connect` / `network_type_disconnect` /
    `network_type_data` → `HandleNetworkingData(async_load)`.
  - Serialization: `buffer_create(1, buffer_grow, 1)` + `buffer_write(buf, buffer_u8/u64/...)`
    + `network_send_packet(socket, buf, buffer_tell(buf))`.
- Handshake: on connect the host pushes `SyncLand()`, `SyncResources()`, `updateVault()`.
- Ping/pong keepalive (first enum value is `PONG`).

### PencilPal's protocol (the Commands enum)
```
PONG, PLAYER_UPDATE, LAND, HP, DEL, VAULT, UPDATE, RESOURCE, DR, NPC, ENEMY, ITEM,
INDUST, DROID, LEVEL_UP, SEND_GOLD, CHAT, HOST_DISCONNECT, EMOJI, STARFALL, RANDOM_GEM,
CHEST, COOKIE, BATTLE, BATTLE_ACCEPT
```
Sync scope actually attempted: resources, islands/land tiles, breaking objects, dropped
items, object health, `image_index`, chests, industrial structures, NPC quests, slimes,
drones, vaults (a custom "Multiplayer Vault" structure), the partner puppet
(x/y/sprite/image_index/facing/angle/held item), gold transfers, chat, emojis, a paid
"store", and a PvP battle mode. Multi-zone handling via `objPlayer.room`/`hostRoom`.

### PencilPal's own open TODO list (lines 29–36 of Main.gml)
- Host is not synced with industrial structures.
- Land syncing is too intensive on big worlds; resource syncing needs work.
- Forge weapon upgrades not synced.
- "Changing menu crashing game" and "changing rooms deleting inventory" — both
  unreproducible, possibly side effects of `/recompile`.
- Emoji app not working; missing credits screen; missing a settings toggle for
  "sync on save".

## Modding API, from the official docs mirror (observed)

- **Live docs mirror:** https://topheranselmo.com/forager-modding-docs/ — a docsify SPA.
  Raw markdown is fetchable at e.g.
  `https://topheranselmo.com/forager-modding-docs/CoreFunctions/Modding/README.md`
  (served as `application/octet-stream`, so `webfetch` refuses it; plain HTTP fetch works).
  Sidebar (page list) at `.../_sidebar.md`. Original location was
  `http://138.197.196.157/docs/#/`. Written by **lazyeye**, built by **topherlicious**;
  current released API version **1.0.1**.
- **Mod structure:** every mod needs `fmConfig.json` + `Main.gml`. `Main` is required and
  runs at compile time; if it errors the mod is not activated. You **cannot create
  objects** — spawn blank instances (`ModObjectSpawn` / `ModObjectSpawnPersistent`) and
  attach methods with `InstanceAssignMethod`; wrap script references with `ScriptWrap`.
- **Lifecycle:** boot → intro → `rmInputSelect` → **`rmModCompile`** (first compile; also
  `/recompile` in the main menu) → `rmMainMenu` → `rmGame`. Hand-made locations
  (dungeons, space towers) live in their own rooms; the main world, Zones and the Void are
  in `rmGame`.
- **System step:** heavy work runs once per **20 frames**, spread across frames so one
  "slot" fires per frame; extend it via the `OnSystemStep` event. Includes pathfinding,
  cooldowns, structure crafting, dirt despawn, culling.
- **Culling / True Instance:** offscreen instances are **deactivated**; `ModObjectSpawn`
  instances are not culled, and `antiCull = true` opts an instance out. The game's
  **True Instance** system lets code reach deactivated instances — use `TrueInstanceFind`
  instead of `instance_find`. PencilPal's TODO ("Use TrueInstance") means it did not.
- **Controller objects:** `WorldControl` (main, persistent), `VoidControl`,
  `WeatherControl` (rmGame only), `TimeControl`, `LightControl`. Parents:
  `parSolid`, `parObject`, …
- **Functions seen (docs + source):** `ModObjectSpawn`, `ModObjectSpawnPersistent`,
  `ModNPCSpawn`, `ModEnemySpawn`, `ModStructureCreate`, `ModZoneCreate`,
  `InstanceAssignMethod` (events: draw/drawGUI/step/interact/clean up/destroy),
  `InstanceAddVariable` / `InstanceRemoveVariable` (serialization), `ScriptWrap`,
  `ScriptCall`, `ModFind`, `ModIsOnline`, `ModFunctionGet`, `ModGlobalGet`, `CommandCreate`
  (custom `/commands`), `ItemEdit`, `ItemCount`, `RemoveItem`, `HasSkill`,
  `WaterCollision`, `LighthouseCalculate`, `variable_global_get` / `_set` (extended GML),
  `ModSaveDataSubmit` / `ModSaveDataFetch`, `Trace`.
- **Events (`#define OnX(...)`)** include `OnLoad`, `OnSystemStep`, `OnStructureSpawn`,
  `OnChatMessage`, `OnLevelUp`, `OnStep`, `OnNetworkAsync`, `OnDrawEnd`, `OnDrawGUIEnd`.
  Events pass arguments from the game (e.g. `OnStructureSpawn(id, structure)`).
- **Tooling (official):** the *Forager Modding Tool* (create/upload mods, `.zip` on the
  docs site) and a *Forager API dialect pack* for **GMEdit Beta**
  (`assets/files/forager.zip`). Not downloaded yet; download only with owner approval,
  pinned by SHA-256 (rule 8).

### Forager's text functions are NOT GameMaker's (2026-10-21, live)

**The modding API's drawing functions have different signatures from the GameMaker ones**, and
the mod loader rejects the stock call. Live evidence, from the owner's console:

    Mod failed to unit! Forager Coop/Events/OnDrawEnd.gml[L93,c1]
    `draw_text_ext` takes 5 arguments, got 8

The stock `draw_text_ext` is `(x, y, string, color, alpha, alignment, xscale, yscale)` - eight
arguments. This build's takes **five**. The same happened with the sprite getters:
`sprite_get_yoffset` takes **1** argument here, not 2 - the subimage variant of the sprite
getters is not in this build, unlike in the GameMaker manual.

**The real signatures are in the one mod that runs in this build** (PencilPal, Workshop
2843162754, `GUI.gml`) - the same trick that settled the `variable_instance_exists` question:

```gml
draw_text_ext(x, y, string, size, width)                                    // 5
draw_text_ext_transformed_color(x, y, string, size, width,
                                xscale, yscale, rotation,
                                c1, c2, c3, c4, alpha)                     // 13
draw_text_transformed_color(x, y, string, xscale, yscale, rotation,
                             c1, c2, c3, c4, alpha)                          // 11
```

- `draw_text_ext(1265, 32, ping, 3, 300)` - the fourth argument is a **text size** and the
  fifth a **wrap width**. Stock `draw_text_ext` has alignment and two scales in those slots
  instead, so the stock call cannot be adapted by reordering the arguments: it is a different
  function.
- The transformed variants take **four colours**, not one (the reference mod passes the same
  colour four times for flat text).
- The scales and the rotation sit in slots 6, 7, 8 of the "ext_transformed" variant - the same
  slots they occupy in the non-"ext" variant, which confirms the reading.

**Method, worth more than the signatures:** a mod that runs in the build is a better oracle
than the manual, and the loader's own error message is exact and free - but each one costs a
round trip through the game. The fix for that is to record the arities in the local checker
(`tools/check_gml.py` now has a `BUILTIN_ARITIES` table) so the mistake is caught before the
game is launched, with the loader's own wording.

**A dead end worth recording:** the game's own scripts are **not** in `data.win` as text (they
are compiled), so the game cannot be used as a second oracle - `data.win` holds only the
string table, where the internal names live as `__draw_text_ext` and
`ref_draw_init_draw_text_ext`. The modding API docs mirror does not document the drawing
functions either; it covers only the *extended* GML and the Forager-specific functions.

### The player's body is a constant asset, not the instance's sprite (2026-10-21, live)

**`objPlayer.sprite_index` is 0 in this build, and it is not a sprite index.** The owner
reported that the peer's puppet "appears only when I walk", and the measurement says why: the
value read **0 on 19366 frames and 1 on 7354** - a flag, not an asset - while the game's
sprites are numbered from about 1000 (a furnace reports 1097). A peer that sent that value
could not be drawn at all, and the stand-in that used it blinked in and out.

**The body probe** (a read-only scan of the objects around the player, run once per session)
found the whole instance chain, all at the player's exact position:

```
#20 'parEntity' sprite 0   at 698,656 d 0
#22 'objPlayer' sprite 0  at 698,656 d 0
#46 'parAlly'   sprite 0  at 698,656 d 0
#80 'parObject' sprite 963 at 664,616 d 53
#80 'parObject' sprite 964 at 696,648  d 8
```

So Forager does not put the player's sprite on the instance - it draws the body from a
**constant asset**. The cosmetic database gives the naming scheme: `sprBeetIdle`,
`sprBeetWalk` - `spr<Name><State>` - and the player's own pair is:

- **`sprPlayerIdle`**, **`sprPlayerWalk`**

There are 1389 `spr*` names in `data.win`'s string table, so the name is a fact and not a
guess: `asset_get_index("sprPlayerIdle")` returns the index, and -1 for a name that is not in
the build, so a wrong name fails visibly instead of drawing the wrong thing.

**Two dead ends worth recording:** the game's own scripts are not in `data.win` as text (they
are compiled), so the game is not a second oracle for call signatures; and the modding API
docs mirror does not document the drawing functions either. The **mod that runs in the build**
(PencilPal) is the oracle that worked - the same trick that settled
`variable_instance_exists`.

### M3.1 done and verified live (2026-10-21) — the second player is visible

**The owner confirmed: "everything works well."** What it took, in the order it was found:

- **The name travels in the handshake** (`HELLO`/`WELCOME` grew a `string name`), so it is known
  before the first puppet is drawn, and **the protocol number moved 4 → 5** because the wire
  format moved. A peer still speaking 4 would read the name as the tail of the version string.
- **The body sprite had to be named, not read.** The player's instance reports
  `sprite_index` 0 (see above), so the mod resolves `sprPlayerIdle` by name and sends *that*
  over the wire. `asset_get_index` in this build returns **0 for "not found"**, not -1 - a
  detail that cost a run: the chain of candidate names stopped at the first 0 and the puppet
  was silently never drawn.
- **The text functions are Forager's, not GameMaker's** - see the section above. The size of a
  label is the **scale**, not the fourth argument; changing the fourth argument changed
  nothing at all.
- **The puppet's feet are put on the ground by computation**, not by a fixed offset:
  `drawn_y = peer_y - height + yoffset`, using `sprite_get_height` and `sprite_get_yoffset`.
  The old guess of -30 put it in the air or underground depending on the sprite.
- **The host decides where a peer may be drawn.** A peer with no physics (the mock) reports a
  position inside a tree or in the water; the host checks `WaterCollision` and
  `CoopSolidInCell` and puts the peer on the nearest cell a body could stand on.

**The mock peer's two bugs, which were the mock's and not the mod's:**

1. It walked a small square around the **host's live position**, so the puppet was glued to the
   host's camera - "the mock follows the player". It then stood still and re-anchored only when
   the host was more than 128 px away, which kept "stands still" and "always visible"
   compatible - but it still stood on a tile of its own, and the game spawns on such a tile. It
   now stands **exactly where the player stands**; see the section below.
2. It sent a position **before it knew where the host was**, using a hard-coded fallback
   `(620, 620)` - an arbitrary point in the world, so the puppet could appear far away and
   only then jump to the right place. It now reports no position until the anchor exists.

**Method, the third time it paid:** every one of these was found by **measuring**, not by
reasoning - the peer's sprite index distribution (0 vs 1, 19366 vs 7354), the instance chain
around the player, the `bodySpr` value, the distance to the host. Four of the five theories
built before the measurements were wrong.

### Steam relay for Forager: what the build does and does not have (2026-10-21)

**Forager ships its own `steam_api.dll`, and it is almost - but not quite - enough.** The file
next to `Forager.exe` is **32-bit x86**, 225 056 bytes, SHA-256
`af17df745250d1814eaa274fff7b0faeb43381e6762e026267e5859778477abd`, and it exports **842
names**. Among them the **whole lobby API**: `SteamAPI_ISteamMatchmaking_CreateLobby`,
`JoinLobby`, `LeaveLobby`, `GetLobbyOwner`, `GetLobbyData`, `SetLobbyData`,
`InviteUserToLobby`, and the friends list.

**What it does not have is `ISteamNetworkingMessages`** - the interface that actually carries
bytes. The dll is from 2019 and that interface came later. So the game's own copy can make a
lobby but cannot move a packet through it, and the modern standalone library is needed.

**The relay is not Forager work.** It is a game-agnostic TCP-over-Steam bridge, and it is
already written: [cranebeaker/deus-ex-coop](https://github.com/cranebeaker/deus-ex-coop)
(MIT) has `steamlink/hxcoop.c` - 750 lines of C, the flat API loaded by `GetProcAddress` with
no SDK headers, `--port <game port>` and `--no-launch`, so it does not need to know what game
it is in front of. The same repo has `steamlink/mock/mock_steam_api.c`, a mock library for
testing without Steam - the Deus Ex QA suite runs the relay through it.

Two things that make it cheaper here than it looks:

* **The mod needs no change at all.** The join prompt already defaults to `127.0.0.1`
  (`get_string("Co-op: enter the host's IP", "127.0.0.1")`) and the host already listens on
  6511. Host side: the relay dials `127.0.0.1:6511` when a peer arrives over Steam. Join
  side: the relay listens on 6511 and the game connects to it.
* **The relay is a separate process, so it may be 64-bit** - the game is 32-bit and never
  loads this library, so the two bitnesses do not clash. That matters because the machine has
  **no compiler at all** (no cl, gcc, clang, cargo, dotnet, go, cmake or Docker), so a C
  relay would need an install, while a Python one does not.

One difference from the reference, and it is the important one: **HXCoop sends game data
unreliably**, because Unreal Engine 1 does its own reliability on top. A TCP stream cannot
work that way - one lost byte and the game's framing is broken for good - so the data channel
has to be **reliable**, which in the Steamworks API means an unreliable hello first (an
unreliable send is what *creates* a session; a reliable send needs one).

Written and tested: `local/forager-coop/tools/steamlink/`. The relay's logic is proven offline
(two relays, a mock Steam, a fake game server, bytes in one end out the other, also with 30%
of the unreliable messages dropped), and the ctypes binding is proven against the **real**
library: the flat API initialises, a lobby is created, lobby data survives a round trip, and a
message sent to our own SteamID comes back - a loopback, so the whole messaging path runs over
Steam's real network without a second account.

Four things the binding got wrong, all of them ways to produce a **plausible-looking wrong
answer**, which is why the real library was needed to find them:

* **The lobby id is at offset 8** in `LobbyCreated { int32 result; u64 lobby; }` on 64-bit, not
  4 - a uint64 after an int32 is 8-byte aligned. The reference is 32-bit, where it is at 4, so
  copying its offset gives a number that looks like a lobby id and is not one, and every later
  call with it fails.
* **A call's result does not arrive in the callback parameter.** The parameter of
  CB_CALL_COMPLETED is an `APICallCompleted`; the result has to be fetched with
  `GetAPICallResult`, and the first attempt can answer "not yet" while Steam is still fetching
  the app's config. The reference asks once and drops the result; asking again is what makes it
  work.
* **The `result` field has to be checked** - a failed call still carries a buffer.
* The `CallbackMsg` struct was declared twice, so the argument type and the argument were
  different classes.

The library comes from the Steamworks.NET standalone release and both files match the SHA-256
the playbook pins. It is **not** committed - it is Valve's.

### The harness: the mock plays the host, and the client's half gets tested (2026-10-21)

The mock peer could only ever be a **client**, so it could assert the bytes that arrived from a
host but never the world's reaction to them. And the world's reaction is the thing that has to
be right. So the mock can now play the **host**: it answers a client's `INTENT_PLACE` with a
`MUTATION_PLACE`, and then sends a **second** mutation the client never asked for, so the
player sees a structure appear that they did not build.

The offline end-to-end test asserts the confirmation too, so the mock's host side is covered
without the game.

**What it found, in one run, on a path that had never been executed before** (the mod had
always been the host):

1. **The client never sent a HELLO.** The mock accepted the connection and completed
   GameMaker's own handshake, but the mod's async connect event did not arrive - and since
   `global.coopPeerAlive` is set by the WELCOME, **nothing the mod could send was sent**: not
   the placement intent, not the loot, not the position. One missing event froze the whole
   client. The fix is a design change, not a patch: the hello is a **retry** until the host
   answers, not an event.
2. **The mock ate the HELLO.** The client's half of the GameMaker handshake starts with 12
   bytes, but the mock consumed 16 whenever its buffer happened to be that long - and four of
   those were the start of the HELLO when it arrived in the same TCP segment. A frame that
   starts four bytes late does not parse, so the first packet after the handshake was simply
   lost, and the mock reported no error because it was waiting for bytes that would never
   come.
3. **The client answered the host's own structure with an intent for it.** The structure
   event for a structure *we* created arrives a frame later, so the `coopApplyingRemote` flag
   was already down by the time it fired. The host confirmed that, and confirmed the
   confirmation, and the game built a row of furnaces marching to the right forever - 1382
   intents in one session. The structures we create from a host's mutation are now remembered
   **by id**, and the event checks that list.
4. **A peer standing in the middle of a lake stayed there**, because the host's search for
   standable ground was two cells wide. Six cells, spiral, nearest free cell first.

**Two things the harness is not**, worth knowing before trusting it:

* the mock's **hosted** puppet is not a game character, and it picks a spot once and stands
  still - it does not have a body, an inventory or a save;
* the mock's scripted `RESOURCE_DESTROY` is **destructive** (it takes a tree out of the
  client's island), so it is off by default and only the offline test turns it on.

### The world grid: corners, centres, and a structure's 2x2 footprint (2026-10-21)

**The world's 16 px cell has two reference points, and mixing them up is a real bug.**

* a **structure** stands on the **corner** of a cell: `x mod 16 = 0`;
* a **natural object** (a tree, a rock, a bush) sits at the cell's **centre**: `x mod 16 = 8`;
* a structure covers a **2x2 block** of cells (32x32 px), whose **centres are at +/-8 from the
  corner** it stands on.

So the cell centre is *inside* the footprint of the structure that owns the corner next to it.
The host's placement check asks exactly that question - it snaps to `round(v/16)*16` and samples
the four cells at `+/-8` - and the client once asked a *different* one, starting its own search
from the cell centre. The result was both halves of one report: **a furnace half a cell off the
lattice, and a furnace standing in a tree** (live, 2026-10-21).

There are two questions, not one:

| | base position | what has to be free |
|---|---|---|
| a **peer** | its own coordinates - it stands where it stands, which is not on the lattice at all | the cell it stands in |
| a **structure** | the cell's **corner** | its whole **2x2** footprint |

### The game spawns nothing on the tile a player stands on (2026-10-21, live)

**The owner noticed ore and grass appearing on the mock's tile** and asked for the game to be
unable to spawn anything there. The rule that explains it is the game's own: **nothing spawns on
the tile a player stands on**. The mock stood on a tile of its own - 96 px to the side at first,
then an anchor 32 px off - so it was not under that rule, and the game was free to spawn on it.

The fix uses the rule instead of fighting it: **the mock stands exactly where the player
stands**. Exactly, not a few pixels to the side - a few pixels to the side is a **different
cell** on the world's 16 px grid, and the rule would not cover it.

**This split the one occupancy test into two questions**, which is the part worth keeping:

| | what has to be free |
|---|---|
| a **structure** | water, trees, rocks, ore nodes, buildings **and the player** - the owner's requirement is a free spot, not on a tree, a rock, an ore node, a building, water or a character |
| a **peer** | water and **solids** only - a peer is a *drawing*, it does not push the player and the player does not push it, and the peer's cell has to be allowed to be the player's own cell, because that is the cell the game protects |

Before the split, the peer test counted the player as an obstacle and pulled the peer off the
very cell it had to be on. The cell test now takes the question as an argument
(`CoopSolidInCell(x, y, living)`).

**And a bug the split exposed:** the client looked for a **free spot** for a structure before
asking whether it **already had** that structure. A structure's own 2x2 footprint contains the
structure, so the spot always read as occupied, the search moved it one cell to the left, and
the client created a **duplicate** furnace next to the one the owner had built (live,
2026-10-21). The check for "is it already here" comes first now, and the free-spot search only
runs for a structure this side does not have.

### Community mod repos (examples of real, buildable mods)
- `Zamiell/FastCrafting` — https://github.com/Zamiell/FastCrafting — **GPL-3.0**, minimal
  (`fmConfig.json` + `Main.gml`). Shows `ItemEdit(Item.X, ItemData.CraftingTime, speed)`.
- `CainDev/Forager-Mods` — https://github.com/CainDev/Forager-Mods — source of several mods.
- `AlejandroAkbal/Forager-Cheater-Mod` — https://github.com/AlejandroAkbal/Forager-Cheater-Mod
- `Forager Evolved` — the first official mod pack (Workshop item 2147146084).

## Verified in-game (2026-10-19, M0)

The scaffolding mod (`fmConfig.json` + `Main.gml`, a few globals and one `Trace` inside
`#define Main`) **compiled and loaded** in the live Steam build 6160665 through the game's
own loader — main menu → modding → enable → COMPILE. Console output (`t`), owner's SteamID
redacted:

```
Forager Co-op 0.0.1-m0 loaded.
Main method ran succesfully. Registering...
Generated ID for mod 'Forager Co-op' is '<steamid64>ForagerCoop'
Cleaning...
Finished loading and initializing mods!
Mods active: 1
Importing currently active mods to loader...
Scanning /mods/ to find inactive mods...
Found an inactive mod: PencilPal
```

What this pins down:

- **The toolchain works with no external tool.** No *Modding Tool*, no *GMEdit*, no
  `data.win` editing: a folder dropped into the mods directory and COMPILE in the game is
  enough to build and run GML. `Trace` prints to the `t` console.
- **Mod id rule:** the loader derives the id as `author` + `name` with non-alphanumeric
  characters removed — `<steamid64>` + `Forager Co-op` → `<steamid64>ForagerCoop`. So the
  `author` field in `fmConfig.json` is not cosmetic (it must be the SteamID64).
- **Mods directory** on Windows: `%LOCALAPPDATA%\Forager\mods\<Mod Name>\`. The enabled set
  and order live in `%LOCALAPPDATA%\Forager\mods\settings.json`, rewritten by the game to
  `{ "loadOrder": [ "mods/Forager Coop" ] }`.
- **The loader also scans subscribed Steam Workshop content** for the app, not only the mods
  folder — the log's "inactive mod PencilPal" is Workshop item `2843162754` sitting in the
  Workshop cache. So a co-op mod can be delivered either locally or through the Workshop.
- `Main method ran succesfully` precedes `Registering`, i.e. mod code really executes during
  the compile/registration pass.

### M1 network layer (2026-10-19/20)

The mod's M1 layer (HELLO/WELCOME handshake, PING/PONG, POS presence packets over DirectTCP
6511) was run against a **non-GML** peer (a Python script, now kept with the mod:
https://github.com/Egolz/forager-coop → `tools/coop_mockpeer.py`)
with the game hosting. The mod completed the GameMaker socket handshake, exchanged
HELLO→WELCOME (the peer saw the game's real mod id/version `0.0.2-m1`, protocol 1), and
ping/ponged both ways for ~25 s with no disconnects. The owner also confirmed the
disconnect path: the host logs `socket disconnected` + `still hosting` and keeps listening.

**Presence verified with real game data.** Standing inside a world, the mod sent 467 `POS`
packets in 26 s — `x=672 y=672 sprite=0 image=0..5 room=8` (the `image` field animating, i.e.
genuine per-frame state) — and, from the mock's `POS` packets, drew the remote marker; the
owner confirmed both the marker and the F8 status line. **Room 8 is the main world** (the same
constant PencilPal hard-codes as "host room"), which is useful for the M2 zone checks.

**Forager's mod sockets use GameMaker's own TCP protocol** — the part that bites anyone
writing a peer in another language:

- on connect the server sends the ASCII banner `GM:Studio-Connect\0` (18 bytes);
- the client answers with three little-endian u32s: `0xCAFEBABE`, `0xDEADB00B`, `16`;
- the server replies `0xDEAFBEAD`, `0xF00DBEEB`, `12`;
- every packet after that is framed `u32 0xDEADC0DE`, `u32 12`, `u32 payloadLen`, payload.

Constants confirmed against YoYoGames' own implementation
(`GameMaker-HTML5/scripts/functions/Function_Networking.js`) and verified live by dumping
the running game's bytes (the banner, then the step-3 signatures after our 16-byte reply).
Consequences:

- A relay/server written in another language must implement this handshake **and** the
  framing; a GML↔GML path (e.g. the Steam relay) gets it for free.
- GML also has `network_create_server_raw` / `network_connect_raw` for the unframed variant;
  whether Forager's API exposes them is `unknown` — the mod does not need them, since it
  talks to other GameMaker peers.

**Two instances on one PC: not possible** (owner-checked 2026-10-20 — launching Forager a
second time while it runs is refused). So automated two-client QA needs a second Windows
session/VM, or a second machine. Meanwhile the transport half can be tested with the
non-GML mock peer above.

**Terrain note for QA:** the mock peer also sends POS packets, so the game side of presence
can be inspected with the in-game status dump (F8 → Trace) without a second Forager copy.

### M2.1 host-authoritative structure placement (2026-10-20)

**Verified in the live build (2026-10-20):** the mod compiles and loads, F5 hosts on 6511,
and a mock peer completes GameMaker's socket handshake, `HELLO`/`WELCOME` (protocol 2),
`PING`/`PONG`, `POS` and the host's `STRUCTURE_TABLE` - **64 structures**, so
`Structure.Count` and `StructureGet(id, StructureData.Name/Object)` do work at runtime in
build 6160665 (see the caveat below about what the names look like).

**The round trip is verified live (2026-10-20, 01:58:39).** The mock sent
`INTENT_PLACE 'Лесопилка' at 809,651 (room 8)` and the host answered
`MUTATION_PLACE seq=1 intent=#15683 room=8 depth 0` on the console, with no crash and a live
socket. What is **not** verified - and is the open bug - is that the host *applied* it: the
player sees nothing appear in the world. The host's own apply path is therefore wrong
somewhere (`CoopCreateStructure` in `Net.gml`), and the next step is to probe it rather than
guess: F10 dumps the game's own structure instances next to ours (see the diagnostics note
below).

**Structure names look localized.** The 64 names the host sent were not ASCII on a Russian
install (the console mangles them), which would make name-keyed sync language-dependent -
the `STRUCTURE_TABLE` divergence check exists for exactly that. The peer now dumps the
table to UTF-8 JSON (`%TEMP%\coop_structure_table.json`) so this can be settled from the
file rather than the console. `unknown` until that file is read.

**Settled: the names are display strings, and the key has to change.** The dumped
`%TEMP%\coop_structure_table.json` holds 64 **Russian** strings, several with an embedded
newline (`"швейная\nмастерская"`, `"для\nпромышленности"`), i.e. wrapped UI labels, not
stable ids. The mock's English `PREFERRED_STRUCTURES` silently fell back to entry 0 for the
same reason. The right key is visible in the API docs' own event example:
`#define OnStructureSpawn(id, structure)` + `if (structure == Structure.Forge)` - the API
exposes a **`Structure` enum** with symbolic members, and that index is what PencilPal
actually syncs. `STRUCTURE_TABLE`/`INTENT_PLACE` should carry the index (name kept for logs).
Fix not yet made.

**F10 world probe (2026-10-20, 02:33): the structure was invisible because it was never a
structure.** The mock's request landed on structure **#0**, which this database exposes as
the build-menu **category header** `"для промышленности"` (`StructureGet(0,
StructureData.Object)` = `objEmpty`, the placement-preview placeholder). `objEmpty` is not a
`parStructure` descendant, so `instance_create_depth(..., objEmpty)` puts nothing real in the
world, and the preview ghost is gone by the next probe. Two more facts fell out of the same
dump:

- The database has **92 rows**, not 64: a hard cap in `CoopSendStructureTable()` cut the
  table at 64, so a peer only ever saw part of it. Rows #68..#80 have **empty names**
  (objects `objGate`/`objFence`/`objVoidPortal`/`objLure`) and cannot be addressed on a
  name-keyed wire at all.
- **Depth is `-y`.** The probe found `objPlayer` at `691,656 depth -656` and a hand-built
  `objFurnace` at `624,656 depth -656`, while the host had announced that same furnace as
  `depth 0`: `OnStructureSpawn()` fires *before* the game y-sorts the instance. We were
  creating remote structures at depth 0, i.e. on the wrong layer.

Fix (mod `0.0.5-m2c`, protocol unchanged at 3): a new `CoopStructDepth(y)` returns `-floor(y)`
and is used by both the applying host and the receiving client, so the mutation's `depth`
field is now informational. `CoopSendStructureTable()` lost its 64 cap and skips unnamed
rows. The mock's `choose_structure()` skips the `"для…"` headers and gained `--place-index`,
so its automatic choice cannot land on a placeholder again.

**The round trip is verified end to end (2026-10-20, 02:46).** The mock asked for `"печь"`
at 800,672, the host answered `MUTATION seq=1 … depth -672` and then re-announced the same
instance itself as `seq=2`, and the next F10 probe found it as a real instance:
`par objFurnace 800,672 depth -672 image 0 visible 1`, counted under both `#1 'печь'` and
`#81 'ядерная печь'` (same object, two database rows). `depth = -y` is confirmed against the
game's own instance, and the table now carries **79 addressable** entries (92 rows minus the
13 unnamed ones).

**One behavioural gap found by the same test, since closed:** the mock's guessed spot was on
**water**, and the furnace was created anyway. A client's intent was validated for *who* asked
(the name, the room, the 448 px range) but **not for whether the spot is legal**. `0.0.7-m2e`
adds the game's own check - see the `WaterCollision` finding below.

**`ScriptCall` takes exactly one argument (live COMPILE, 2026-10-20):**

```
Mod failed to unit! Forager Coop/Net.gml[L536,c16] `ScriptCall` takes 1 argument, got 3.
```

The docs read as if a wrapper is only *described* by `[mod, methodName]`, but the arity error
is the ground truth: `ScriptCall(wrapper)` passes the wrapper and nothing else. So
`ScriptWrap` + `ScriptCall` reaches a **zero-argument** first-party script only, and
`WaterCollision(x, y)` - which needs x and y - is not reachable through that path. `F11` is
therefore a read-only probe (`0.0.6-m2d`) that reports `typeof`/`array_length` of the wrapper,
what `ModFunctionGet("Forager", "WaterCollision")` returns, and a `GetLandsOwned` control call,
instead of guessing at an argument-passing spelling. How (or whether) arguments can be passed to
a first-party API script is `unknown`; the API symbol table also holds `ModCall` and `InsideMod`,
untried.

**What a `ScriptWrapper` actually is (F11, live 2026-10-20):** not the documented
`[mod, methodName]` pair but a **3-element array**

```
ScriptWrap type   = array, len 3
  wrap[0] = wrapper
  wrap[1] = EgolzForagerCoop
  wrap[2] = WaterCollision
```

The tag `"wrapper"` comes first and the mod id second. So `ScriptWrap("WaterCollision")` does
not reach the game's own script at all - it resolves the bare name **inside our own mod** and
hands back a lazy reference to it, which is why
`ScriptCall(ScriptWrap("WaterCollision"))` could never have asked the game about water.
`ModFunctionGet("Forager", "WaterCollision")` returns `undefined`, so the first-party id is
**not** `"Forager"`; whatever it really is has to be discovered (`ModFind` / `ModIsOnline`, now
part of the probe) rather than assumed. The one control that did run,
`ScriptCall(ScriptWrap("GetLandsOwned"))`, returned `-1` - consistent with "no lands owned", but
also with "the name resolved inside our mod and hit nothing", so it proves the *call path* works
and not that first-party scripts are reachable.

**Settled - and the answer was "just call it" (live 2026-10-20, `0.0.7-m2e`):** the game is
**not registered as a mod at all**. `ModFind` returned `-1` and `ModIsOnline` `0` for every id
tried (`Forager`, `ForagerModdingAPI`, `ModdingSample`, `forager`, `Forager Coop`), so the
registry only ever holds real mods and `ModFunctionGet("Forager", ...)` could never have worked.
But the functions the docs list under *General Forager Behavior* are **directly callable** - a
bare `WaterCollision(x, y)` compiles and runs:

```
WaterCollision(703,630) = 0     (the player stood on land)
left 0   right 1   up 1   down 0   far +512 right 1
HasSkill(0) = 0
```

So `0` = land, non-zero = water, and no wrapper is involved. The F11 probe, the `ScriptWrap` /
`ModFunctionGet` detour and both around it were unnecessary - the docs' plain list was the whole
story. **Lesson for the next agent:** try the plain call first; reach for `ScriptWrap` /
`ScriptCall` only for a *mod's own* scripts, and remember `ScriptCall` takes the wrapper alone.

The host now refuses an intent whose spot is water, and the round trip is verified:

```
-> INTENT_PLACE 'печь' at 859,641 (room 8, #2268); our puppet is at 635,649
<- INTENT_REJECTED #2268: 'that spot is on water'
```

The client rolls its optimistic placement back on `INTENT_REJECTED` (it had already drawn the
furnace optimistically), so both ends end up agreeing with no structure on the water.

**A refusal alone is not proof, so the mock now supplies the other half (2026-10-20).** The
host's spot check is only shown to work if the *same* run also places something the host
accepts. The mock has no `WaterCollision` of its own, so it learns where the land is by trying:
after a `"that spot is on water"` refusal it walks a ring of offsets around the game's player
(all within the host's 448 px range) and stops at the first confirmed `MUTATION_PLACE`. A refusal
for any other reason stops the walk instead of repeating it, so the reason is the finding. The
state machine is covered offline by the selftest (a socketpair, no game):

```
selftest: 11 packets encode/parse correctly, spot-retry ok
-> INTENT_PLACE 'forge' at 728,600 (spot 1/10)
<- INTENT_REJECTED #9839: 'that spot is on water'
   retrying at another spot (water was at (728, 600))
-> INTENT_PLACE 'forge' at 472,600 (spot 2/10)
<- MUTATION seq=7 intent=#7576 room=8 'forge' at 472,600 depth -600
   accepted: ... the spot we asked for ((472, 600)) was land
```

`tools/mock_e2e.ps1` still passes (`handshake, welcome, pong, structure table and INTENT_PLACE
all seen`).

**And the round trip, live 2026-10-20 (`0.0.7-m2e`), refusal and acceptance in one run:**

```
[03:20:25] -> INTENT_PLACE 'печь' at 853,660 (room 8, #29143, spot 1/10)
[03:20:25] <- INTENT_REJECTED #29143: 'that spot is on water'
[03:20:25]    retrying at another spot (water was at (853, 660))
[03:20:25] -> INTENT_PLACE 'печь' at 594,654 (room 8, #7980, spot 2/10)
[03:20:25] <- MUTATION seq=1 intent=#7980 room=8 'печь' at 594,654 depth -654
[03:20:25]    accepted: ... the spot we asked for ((594, 654)) was land
[03:20:25] <- MUTATION seq=2 intent=#0 room=8 'печь' at 594,654 depth -654
```

`seq=2` is the game's own `OnStructureSpawn` re-announcing the instance it just made, exactly as
in the earlier M2.1 run. So the host's spot check is verified in **both** directions.

**Two known gaps in the placement rules, raised by the owner after this run - one confirmed, one
open. Neither is part of M2.1's wire work:**

1. *A structure cannot stand on an occupied tile* (another heater, a tree, a rock). The host
   currently checks **water only**. Nothing stops an intent for a spot the game would refuse, so
   this is a real hole, not a hypothetical. Open question: is there a first-party predicate for
   it? `WaterCollision` was luck - it is documented under *General Forager Behavior* and callable
   directly. The tile/occupancy side shows `objTile`, `parSolid` (the parent of solid objects),
   `objWateredTile`, and internal scripts `TileCollision`, `GridCollision`, `CheckTilemap`,
   `CheckDirtRect` - **but the docs list no plain predicate** for "is this cell free". Trying
   internal script names blind is expensive (a wrong name fails the whole COMPILE). Status:
   `unknown`, needs a probe, not a guess.
2. *A structure costs resources.* Right now `INTENT_PLACE` **creates the structure without
   spending anything** - the host applies it directly, out of band from any inventory. So a
   joining player builds for free, which is not acceptable in a real session. This cannot be
   fixed before **the client's inventory is synchronised** (M2.2/M2.4): there is nothing yet to
   charge. Ordering is therefore M2.2 (loot/resources) -> then a costed placement that the host
   validates against the client's synced inventory.

**Occupancy, first step: F12 (read-only probe), `0.0.8-m2f`.** The owner asked whether a
structure can be placed on top of another structure, a tree or a rock, and the honest answer
today is **yes it can - the host checks water only**. The documented modding API does **not**
expose an occupancy predicate: `CoreFunctions/GeneralForagerBehavior` lists exactly three
callable functions (`HasSkill`, `LighthouseCalculate`, `WaterCollision`), and the
`CoreFunctions/Modding` reference has no "is this cell free" entry. So `WaterCollision` was
lucky rather than representative - it happens to be documented *and* directly callable.

F12 therefore **measures** the spot with calls already proven live in F10, and does not guess at
a name:

- it resolves `parStructure`, `parResource`, `parSolid`, `objTile`, `objTree`, `objRock`,
  `objBush`, `objCactus` by name and prints each one's `instance_number` (a missing parent is
  itself a finding - a check that measures nothing looks like "free");
- it counts instances of those parents within **24 px** of the player, which is the build-cell
  size, and prints each hit with its object name and distance.

The **collision builtins** (`instance_position`, `place_meeting`, `position_meeting`,
`place_free`) are the *right* answer and are all in `data.win` - but being in the build is
necessary, not sufficient (`ini_open` was in the build and the loader still refused it), so they
stay commented in F12 and get **one dedicated COMPILE** each, exactly like the bare
`WaterCollision` call did. `data.win` also has `tile_get_index`, `tilemap_get_at_pixel` and
`layer_tilemap_get_id` at count **0** - the tilemap accessors are *not* in this build, so a
tile-map-based reading is off the table and the parent-object route is the one worth taking.

**F12's second live run settled the rule's shape and the rule is now implemented (`0.0.10-m2h`).**
The parent chains, walked with `object_get_parent` (already proven live in F10), are the whole
story:

```
chain objFurnace   -> objFurnace  parStructure  parObject  parSolid
chain objTree      -> objTree     parNatural    parObject  parSolid
chain objRock      -> objRock     parNatural    parObject  parSolid
chain parStructure -> parStructure parObject     parSolid
```

**Every tree, rock and built structure descends from `parSolid`** - a furnace through
`parStructure`, a tree or rock through `parNatural`, all meeting at `parSolid`. So a single rule
suffices; a rule against `parStructure` alone would have missed trees and rocks entirely, and
would also have looked broken, because `parStructure` reports **0 instances of its own** (the
game keeps a furnace under `objFurnace`). The same run confirmed the reading on a real object:

```
parent 'parStructure' #108 instances 1        (the furnace that was just built)
parent 'parSolid'     #271 instances 20       (19 world solids + the furnace)
  near objFurnace at 720,624 d 13 (parent parSolid)
```

`CoopSolidNear(x, y)` in `Net.gml` counts `parSolid` instances within `global.coopSolidRadius`
(24 px - the live reading put a tree at 11 px and a rock at 17 px, so 24 px is the build-cell),
using only calls already proven live. The intent handler refuses `"that spot is already
occupied"` after the water check. The mock treats an occupied refusal like a water one and steps
to the next candidate, so a run shows both obstacles being walked around.

**Verified live 2026-10-20, all four cases in one afternoon.** The mock was given a second round
that aims at the exact cell it just filled, so the occupied rule is answered by the host's own
new structure and not by luck:

```
[03:40:11] accepted: 'MUTATION_PLACE' at 665,578 (seq 1) - the spot we asked for was free
[03:40:11]    round 2: will ask for the occupied cell (665,578)
[03:40:14] -> INTENT_PLACE 'печь' at 665,578 (round 2, spot 1/1)
[03:40:14] <- INTENT_REJECTED #33770: 'that spot is already occupied'
```

and the owner standing next to a tree, where the first five candidate cells were all occupied:

```
[03:41:14] -> INTENT_PLACE 'печь' at 664,656 (round 1, spot 1/25)
[03:41:14] <- INTENT_REJECTED #64191: 'that spot is already occupied'
[03:41:14] -> INTENT_PLACE 'печь' at 680,656 (round 1, spot 2/25)
[03:41:14] <- INTENT_REJECTED #57968: 'that spot is already occupied'
... spots 3, 4, 5 the same ...
```

So the rule holds in all four states: **free -> accepted, water -> refused, occupied -> refused,
and a cell filled one second earlier -> refused**. The second run ended with every candidate cell
refused ("the ring is all water or occupied") because the owner was on a small island and the
ring reaches 320 px - a correct outcome, not a failure: the host refused everything illegal and
the mock had nowhere legal to ask. Worth noting for future tests: the candidate ring should
follow the *owner's* island, or the mock should ask at a cell it knows is free.

**Then the owner caught the gap that made the whole thing look wrong: a player is not a build
spot (`0.0.11-m2i`).** The dense spiral's first candidate was offset `(0,0)` - the cell the player
stands on - and the host **accepted** it, so the mock's furnace appeared *under the host*:

```
[03:47:29] -> INTENT_PLACE 'печь' at 595,656 (round 1, spot 1/169)
[03:47:29] <- MUTATION seq=1 intent=#22357 room=8 'печь' at 595,656 depth -656
[03:47:29]    accepted: ... the spot we asked for ((595, 656)) was free
```

`parSolid` does not cover characters, and in the real game you cannot build on yourself or on
another character - so the check was incomplete, not the rule. `CoopSolidNear` now also counts
`objPlayer`, `parNPC`, `parEnemy` and `objPet` inside the 24 px cell, resolved lazily and **skipped
when this build does not have them**, so a renamed object costs a missing guard and never a
COMPILE. `parEntity` is deliberately **not** in the guard: if it is a broad root (a parent of
`parSolid` too) it would refuse legal spots, and a check that refuses too much is worse than one
that misses a rare case - so F12 now dumps its chain and the next run adds it only if it really
is the living-things parent. The mock also no longer offers the player's own cell as a candidate,
which is the model the real game uses.

**Verified live after the fix (`0.0.11-m2i`), the full sequence in one run:**

```
[03:51:21] -> INTENT_PLACE 'печь' at 707,666 (round 1, spot 13/168)
[03:51:21] <- INTENT_REJECTED #35835: 'that spot is already occupied'
[03:51:21] -> INTENT_PLACE 'печь' at 723,602 (round 1, spot 14/168)
[03:51:21] <- MUTATION seq=1 intent=#8625 room=8 'печь' at 723,602 depth -602
[03:51:21]    accepted: ... the spot we asked for ((723, 602)) was free
[03:51:21]    round 2: will ask for the occupied cell (723,602)
[03:51:24] -> INTENT_PLACE 'печь' at 723,602 (round 2, spot 1/1)
[03:51:24] <- INTENT_REJECTED #55420: 'that spot is already occupied'
```

The owner confirmed the visual half too - *"the mock put the furnace on a free spot"*, on the
ground and **not** under the host. The spiral's first candidate is now `(-16,-16)`, so the cell
the player stands on is never offered, matching the real game.

**Anomaly, not yet explained:** in rows #46..#80 the name column and the object column look**misaligned**

**Anomaly, not yet explained:** in rows #46..#80 the name column and the object column look**misaligned** - `"каменная стена"` (stone wall) reads back as `objGate`, `"железная стена"`
as `objEmpty`, cycling `objEmpty`/`objGate`/`objFence` with period 3 while the names
alternate gate/wall with period 2, and #68..#80 have no name at all. Rows #1..#45 and
#81..#91 are correct and coherent (#1 `"печь"` = `objFurnace`, the object the game really
used for a hand-built furnace), so the fence/gate family is a separate problem:
`StructureData.Object` may not be the object index for those rows. Flagged for M2.x; it does
not touch ordinary buildings.

**A mod here cannot write a file.** Live COMPILE error, 2026-10-20:

```
Mod failed to unit! Forager Coop/Net.gml[L453,c1] `ini_open` is not function or script
```

`ini_open`, `ini_write_string`, `ini_write_real` and `ini_close` are all *in* `data.win` -
the game writes its own `secretSettings.ini` with them - so "the name is in the build" is
necessary but **not sufficient**: the loader resolves a mod's calls against its own function
table. `file_text_*`, `buffer_save`, `show_debug_message` and `working_directory` are absent
from the build entirely. `tools/check_gml.py` now has a `LOADER_BLOCKED` list for this class
(it is exactly the false "0 problems" that shipped the broken build). Consequence: a long
dump cannot become a file - the mod forwards it to the peer over the protocol
(`DEBUG_LINE`, packet 12, protocol 3) and `tools/coop_mockpeer.py` writes
`%TEMP%\coop_debug.txt`. Offline `tools/mock_e2e.ps1` covers that path too.

**More from the official docs mirror** (API 1.0.1, fetched 2026-10-20):

- `parStructure [parObject]` is the parent of **every** craftable structure, "both from the
  build menu and from placing objects from the inventory (like decorative items)".
- Structure **crafting runs on the system step** (once per 20 frames, with pathfinding and
  culling) - so a structure is not necessarily a thing the game builds on the frame it is
  placed.
- Culling deactivates off-screen instances; `antiCull = true` in a Create event prevents it,
  and `TrueInstance*` is the documented way to *reach* deactivated instances. Our probe uses
  `TrueInstance*`, so an instance that exists but is culled/deactivated (invisible to the
  player, present to the mod) is exactly what it can distinguish - one of the live theories
  for the invisible structure.
- Mods are expected to create their own instances with `ModObjectSpawn(x, y, depth)`;
  "you cannot create objects in Forager modding". `instance_create_depth` with a *native*
  object is still used in the docs' own examples (and by PencilPal), so creating a native
  structure object is legitimate - the question is which object and with what initial state.
- The API docs **do not document** `StructureGet` / `StructureCreate` / `StructureEdit` /
  `StructureAddItem`: the `Structure Database` page only links a tutorial video
  (YouTube `DMn9r4wmJFo`). For structure internals we are back to PencilPal's source plus
  live probing.

The notes below are what was learned while writing it (from the modding API docs,
PencilPal's source and the live build's symbol table).

**The structure database is readable at runtime.** For an id in `0 .. Structure.Count-1`:

- `StructureGet(id, StructureData.Name)` → the structure's key, **lowercase** (`"forge"`,
  matching what PencilPal compares: `"multiplayer vault"`).
- `StructureGet(id, StructureData.Object)` → the object index it is built from (`objForge`).

A received structure is created with `instance_create_depth(x, y, depth, object)` and then
`structure = <structure id>` set on that instance - exactly what PencilPal's receive path
does (`Networking.gml:937`: `with(instance_create_depth(_x, _y, _depth, _item)) { structure = _struct; }`)
and what `CoopCreateStructure` does. PencilPal also sets a `global.deny` flag around the
creation so the mod's own `OnStructureSpawn` does not echo it back, which is the same guard
as this mod's `global.coopApplyingRemote`.

`Structure.Count` is the one call here not yet exercised live: PencilPal never uses it (it
syncs **object indices**, so it needs no id/name table) and it comes from the API docs. If
it does not exist at runtime, `error.log` names the line, and the fallback is to identify
structures by object name (`object_get_name(i.object_index)` / `asset_get_index(name)`)
instead of building the database at all - good enough for base-game structures, though not
for mod structures, which all share `objModStructure`.

`StructureData` / `StructureType` are **compile-time enums**, so they do not appear in
`data.win`'s symbol table even though the code above works — an absent name there is not
proof of an absent feature. The database is not guaranteed to be up when a mod's `Main`
runs, so the mod caches it lazily (rebuild if the cache is empty) instead of once at load.

**A name in `data.win` is NOT proof the loader will accept it — proven the hard way
(2026-10-20).** `string_split` appears in `data.win`, so `check_gml.py` (which tests a name by
looking for its bytes anywhere in the file) passed it, and the mod was deployed. The loader then
refused to load the file at all:

```
Mod failed to unit! Forager Coop/Net.gml[L411,c11] `string_split` is not a function or script
```

Digging into the bytes explains why the symbol check cannot work as written, and why no simple
tightening of it works either:

- Built-in GameMaker functions are stored with a **`__` prefix** — the file holds
  `__string_copy`, `__string_length`, `__string_pos`, `__array_length`, `__instance_number`,
  `__asset_get_index`. Calling them **without** the prefix is what works in mod code, but the
  bare name is only ever present as a *substring* of the prefixed one, so a naive "is this name in
  the file" test finds `string_copy` and says yes.
- Library/script symbols are stored as **length-prefixed plain names** — `string_split` really is
  there, as `\x0c\x00\x00\x00string_split`, next to `string_join`, `string_trim`,
  `ds_list_add_unique`, `UnixTimestamp`. So `string_split` *is* in the file by every textual test,
  and still does not load.
- The same length-prefixed form is used by functions that **do** work: `array_push`, `DropItem`,
  `ItemGet`, `Trace`, `WaterCollision`, `TrueInstanceNumber`. So "plain name, no `__`" does not
  separate working from broken either.

A scanner over our own sources confirmed it: partitioning every called name by (a) absent
entirely, (b) present only as a plain length-prefixed name, (c) present as a `__` builtin gives
`string_split` the **same class as `array_push` and `DropItem`**, which are fine. **The difference
is not in how the name is stored; it is in which of them the loader chooses to expose to a mod.**
That set is not derivable from `data.win`, so `check_gml.py` can only catch the *absent* case and
its "0 problems" must never be read as "this will load". **The compile in the modding menu is the
only real check**, and a live COMPILE is now the gate after any new function is introduced.

Practical rule taken from this: **prefer constructs that need no new builtins.** The bush probe
was rewritten to use four **numeric** parallel arrays instead of packing a record into a string and
splitting it, which removed the dependency altogether rather than trading it for another unproven
string function.

**`#define` macros here behave like functions, not like textual includes.** Evidence:
PencilPal's `EaseOutBack` is used mid-expression (`var _y = EaseOutBack(_t, a, b, d);`) and
its body ends in `return` — as a text include that would be a syntax error, and the early
`return` would abort the caller's script (their phone UI would never draw). This matters
because it means (a) `return` in a macro returns from the macro, (b) macros may be nested
freely, and (c) re-declaring the same `var` name in different macros is harmless. PencilPal
also shows the game's compiler tolerates the same `var` name twice in *different blocks* of
one script (`Battle`, `SyncResources`), which is why 20 `var _packet` declarations coexist in
`Networking.gml`.

**Structures travel by name, not object index.** An object index is build-specific and
unreadable in a log; the name is neither. A build that renumbers structures keeps working,
and a log line says `place 'forge'` instead of `place 214`.

**Validation is the host's job.** The host refuses an intent unless the structure exists, the
client's reported room equals the host's room, and the target is within 448 px of the
client's *last reported position* (a sanity check against a peer that teleports builds
across the map — not anti-cheat). A refusal is answered with the reason, which the client
`Trace`s, and the client rolls its optimistic placement back.

**Protocol 2** — the packet set changed, so `coopProtocol` and the mock peer went 1 → 2;
a v1 peer is now rejected at handshake instead of silently ignoring the new packet ids.

**How this loader fails, learned the hard way (verified in the live build, 2026-10-20).**
The first M2.1 deploy did nothing at all in game: no error on screen, no console line, F5
dead. The loader's own console did say why:

```
Loading file: mods/Forager Coop/Net.gml...
Mod failed to unit! Forager Coop/Net.gml[L137,c26]: Expected a statement, got number
Exiting compile...
```

Several facts fall out of that, all worth knowing before writing any Forager mod:

- **There are no value macros.** `#define COOP_PLACE_RANGE 448` is rejected: this loader
  reads a macro body as *statements*, so a bare number is "Expected a statement, got
  number" and the whole mod refuses to load. A constant has to be a global (M1 happened to
  use none, which is why M1 loaded). Note the typo in the loader's message ("failed to
  unit") if you ever grep for it.
- **Not every GML builtin exists in this build.** `array_delete()` and
  `variable_instance_exists()` are absent from the 107 MB `data.win`'s symbol table while
  `array_get`, `array_push`, `array_length` and `instance_number` are present. Which
  functions a build has is answerable offline by searching `data.win` for the name — and
  that is now the mod's pre-deploy check (`tools/check_gml.py`, which also flags brackets,
  duplicate `var`s in one block, wrong macro arity and the value-macro trap). A mod compile
  itself reports only the *first* error, so an offline pass saves a round trip each time.
  **Caveat, learned later the same day:** presence in `data.win` is necessary but not
  sufficient — the string search says yes to `ini_open`, and the loader still refuses it
  (see the file-writing note below and `LOADER_BLOCKED` in the checker).
- **`try`/`catch` does not compile.** The loader turns mod GML into its own AST and cannot
  walk a `try_catch` node, so a `try` anywhere takes the game down at COMPILE time - and
  again on every launch, because mods are recompiled. There is no on-screen message and the
  loader console just stops (`Exiting compile...`); the game's own
  `%LOCALAPPDATA%\Forager\error.log` is the only place that names the culprit:

  ```
  ERROR in Other Event: User Defined 0 for object ModControl:
  Can't seekAllOut over Forager Coop/Net.gml[L150,c1] try_catch
  ```

  That is how the *second* M2.1 deploy died, after the value-macro fix above, and why the
  mod was then rewritten without a single try/catch (guards are plain `if`s). It also
  explains PencilPal's standing TODO *"Fix recompile crashing game"*: that mod uses
  `try/catch` in `Networking.gml` and `GUI.gml`, so it cannot be recompiled at all in this
  build.
- **`error.log` is the oracle.** `%LOCALAPPDATA%\Forager\error.log` records both compile and
  runtime failures, with a file and line for mod code. A mod that "does nothing", or a game
  that dies on COMPILE, should be diagnosed from there before anything else is changed.
- **The two failure modes differ.** A parse error the compiler *understands* is reported
  nicely in the loader console (`Mod failed to unit! <file>[L,c]: <reason>`) and the game
  survives it. A construct it cannot handle at all (`try_catch`) crashes the game instead,
  with only `error.log` to say why. Both were hit in one session on this game.

### M2.2 item database (2026-10-20) - the F3 probe

Started M2.2 (resources and loot) by measuring the item half before writing any sync code, the
same order that found `WaterCollision` directly callable. Mod `0.0.12-m2j`, hotkey **F3**
(read-only).

**The event list came from PencilPal, not the docs.** The official docs document exactly one
mod event (`OnStructureSpawn`), and mod event names are **not in `data.win`** (`OnStructureSpawn`
appears 0 times there): the loader fires an event by looking for a file of that name in the
mod's `Events/` folder. So the authoritative list is a mod that uses them - PencilPal's
`Events/OnEvent.gml` holds the full set with real signatures:

| Event | Signature | What it gives us |
|---|---|---|
| `OnResourceSpawn(l)` | instance | a resource appeared (tree, rock, ...) |
| `OnResourceDestroy(i)` | instance | a resource was harvested |
| `OnItemDrop(d)` | array of `{x, y, item, stack, id}` | items fell into the world |
| `OnItemGet(items, size)` | item name, count | the player picked something up |
| `OnMobSpawn(inst)` / `OnMobDeath(inst)` | instance | enemies (PencilPal leaves these `return`ed - WIP) |
| `OnStructureInteract(type, structure, inst)` | ... | a structure was used |
| `OnPlayerDamage(dodge, damage)`, `OnLevelUp(level)`, `OnRoomLoad()`, `OnSave()`, `OnLoad` | ... | misc |

**The Item Database API** (documented, all five present in this build's symbol table - each
appears exactly once): `ItemGet(id, ItemData.Name)`, `ItemCount(id, vip=false)`, `GainItem(item,
qty)`, `RemoveItem(item, qty)`, `DropItem(x, y, item, qty)`. `Item.Count` is the row count.

**Live F3 result (owner's install, 2026-10-20):**

```
Item.Count = 319, cached 319
anchor Wood  = 0    name 'Древесина'   found-by-name -1   owned 0
anchor Stone = 1    name 'Камень'      found-by-name -1   owned 0
anchor Coin  = 111  name 'Монета'      found-by-name -1   owned 0
```

- **319 items**, and the anchors land exactly where the docs and PencilPal assume:
  `Item.Wood == 0`, `Item.Stone == 1`, `Item.Coin == 111`. So the ids are stable and the wire
  can key on them - which is what M2.2 needs.
- **The item names are localized, exactly like the structure names.**
  `ItemGet(Item.Wood, ItemData.Name)` returns `"Древесина"` on a Russian install, so
  `CoopFindItem("Wood")` returns `-1` and always will. This is the same lesson the structure
  table taught, one milestone later: **an id travels, a name is for the log.** The divergence
  check that the item table originally ran (count matching names) would therefore have
  screamed on two installs that merely run different languages while their ids are fine - it
  was rewritten to compare **row count + the anchor ids**, which are build constants and
  language-independent, and to use the names only to name the row that moved.

The item table travels as new packet **13 `ITEM_TABLE`** (`u16 count`, `{ u16 id, string name }`),
sent right after `WELCOME` alongside the structure table; the mock parses and logs it, its own
host sends one, the selftest round-trips it (12 packets) and the e2e run asserts it, so the
packet is covered offline before a game speaks it.

### M2.2 loot and resource sync (2026-10-20) - live, first harvest verified

Mod `0.0.13-m2k` (protocol **4**), then `0.0.14-m2l` for the duplicate investigation.
Three new packets and three new event handlers.

**First live harvest (2026-10-20, tree at 664,680, room 8).** Felling one tree delivered
the whole loop, every packet decoding correctly:

```
<- RESOURCE_DESTROY obj#85 at 664,680 room 8
<- DROP item #0 x1 at 664,680 room 8
<- DROP item #0 x1 at 664,680 room 8
<- PICKUP item #0 at 673,679 room 8
```

- `OnResourceDestroy` carries the **object index** (85), not a name - as designed.
- `OnItemDrop` carries **item #0** = Wood, the id F3 had pinned.
- `OnItemGet` carries **item #0** and *no tile*; the `PICKUP` position (673,679) is the
  **player's** position, and the drop was found next to it - which is the whole reason
  `CoopApplyPickup` searches by radius instead of trusting a coordinate the event never
  sent.

**The open question the live run created: why TWO identical DROPs.** Both were
byte-identical (`664,680`, item 0, stack 1). From the wire alone that is ambiguous:
one call carrying a two-entry array (two piles that really exist) reads exactly like the
event firing once per instance (one pile reported twice). The difference matters on the
receiver - one is a pile to mirror, the other a pile to dedupe. So `0.0.14-m2l` does two
things: `OnItemDrop` now traces the **array length** (which answers it on the next
harvest), and `CoopApplyDrop` gained a second dedup layer - a short-lived pending list for
the case where the duplicate arrives *before* `DropItem()` has materialised its instance,
which is the timing the live run actually showed. The list is drained each step by
`CoopTickPendingDrops()`, so it cannot grow without bound and a genuine later drop still
lands.

**The event signatures came from PencilPal, again.** The docs document **none** of these
three, and the names are absent from `data.win` (they are file/define names the loader
looks up), so the only source is a mod that runs them - `Events/OnEvent.gml`:

| Event | What it really carries |
|---|---|
| `OnItemDrop(d)` | `d` is an **array**, one entry per dropped item, each with `.x .y .item .stack .id` |
| `OnResourceDestroy(i)` | the instance `i`; used as `i.x`, `i.y`, `i.object_index` |
| `OnItemGet(items, size)` | `items` = an **item id**, `size` = the count. **No position at all** |

The `OnItemGet` shape is the one that changed a design. PencilPal's own handler
*reconstructs* the tile by walking `parResource` and matching a drop within 20-30 px of the
mouse, the player or a droid. So there is no tile in the event; our `CoopApplyPickup()`
does the same walk on the far side instead of pretending to know an x,y the wire never
carried. It searches `parResource` for a drop of that **item id** within ~48 px of the
position the picker reported (their own player).

**What a drop's tile is matched on** is `parResource`'s own `item` field - PencilPal reads
`parResource.item` directly (line 161 of its `OnEvent.gml`), so no `ItemGet` name
comparison is needed and no localized string is involved. That is also why `CoopDropAt()`
walks `TrueInstanceNumber(parResource)` / `TrueInstanceFind` rather than `instance_number`:
a culled (offscreen) drop still has to be found when the other player picks it up.

**One limit copied from PencilPal's own code, recorded rather than fixed:** its
`OnItemDrop` can `instance_destroy(d[0].id, false)` and `return` before forwarding (an
industrial building absorbing its output, or `global.land`). Our handler announces every
drop it is shown, so a drop the game swallows may briefly be announced. The next reconcile
(pickup/destroy) corrects it; this is the first thing to watch in the live run, not a claim
that it is right.

**Scope, on purpose:** this syncs the **world** only. A drop that leaves the ground leaves
it on both machines; **nothing moves between the two players' inventories** - `GainItem`/
`RemoveItem` are the inventory half and are their own step (M2.4). Doing both at once is how
the placement slice grew its "invisible structure" bug, so it is one claim at a time again.

Offline coverage: the mock encodes, parses, describes and round-trips packets 14-16; the
selftest is 15 packets; and the e2e session now has the mock host announce a `DROP`, a
`PICKUP` and a `RESOURCE_DESTROY` that the mock client decodes, so all three are exercised
with no game running.

**Two GML availability checks that came out of writing this** (offline, `data.win` string
counts): `is_struct` **0**, `variable_struct_exists` **0**, `variable_instance_exists` **0** -
so none of the "is this thing there" predicates exist to guard a struct field read, and the
field access is the only test. `is_array` **2**, `variable_instance_get` **2** and
`array_set` **1** do exist. This is the same trap as `ini_open`: a build's symbol table
decides what a mod may even *name*.

**A test-tool lesson that cost one run (2026-10-20).** The first live attempt of this build
failed with `REJECT reason='protocol mismatch: host 4, peer 3'` on every reconnect: the
freshly compiled game spoke protocol 4 while the **background mock was still the previous
process**, running the old constant. Bumping `PROTOCOL` in the source does nothing to a mock
that is already running. **Restart the mock after every protocol bump**, or the test lies -
the game is fine, the harness is stale.

**A citrus drop corrected the dedup design (2026-10-20).** The second live harvest showed
three DROPs at one tile: two wood (`#0`) and one citrus (`#55`). The owner explained the
citrus is a random bonus a tree can give alongside its wood, so a multi-item fire is normal -
and, crucially, **the game drops several wood stacks as separate piles**. That inverted the
earlier guess: "two equal items at one tile" is the *common* case here, not a duplicate, and
the receiver-side dedup (`CoopDropAt` by position+item) would have **silently swallowed a
player's second wood**. `CoopApplyDrop` no longer dedupes at all; it creates exactly what it is
told. The only duplicate distinguishable from the wire is the same instance `id` listed twice
in one fire, so that - and only that - is filtered, on the **sender**, where the drop's own
`id` is visible.

**The measurement, and M2.2 closed (2026-10-20).** The `DEBUG_LINE` diagnostic answered it on the
next harvest:

```
OnItemDrop fired, 2 entries: 0:[item0 id=102045 x1 @696,664] 1:[item0 id=102046 x1 @696,664]
```

**Different `id`s in a single two-entry array** - so the two wood DROPs were two real piles, not
a duplicate, and the receiver dedup would indeed have eaten one. One call, one array, two
entries; the game drops each stack as its own instance. `PICKUP` followed once, because the
player picked up one of the two. Loot sync (M2.2) is therefore closed and verified live: harvest,
drop (single and multiple, including the citrus bonus) and pickup all travel and decode.

One fact for later: a drop's `id` is an **instance id, unique per machine**, so it is only good
as a key *within* one fire - it will never match across the two installs. Cross-fire duplicate
detection needs a different key (a host-assigned drop sequence) and is not needed yet.

**The bush is picked, not destroyed - and it is not a resource at all: its parent is
`parHerb`.** The evidence, after two probes (a frame-watch that found nothing, then a
disappearance-watch that did):

```
probe bush census: standard 7 vs true 7      <- active, so "gone" means picked, not "left the room"
probe bush GONE: obj#86 'objBush' parent 'parHerb' id 101967 at 696,648
OnItemDrop fired: item54 x3 @600,648         <- berries fell on the same tile
```

and the mock confirmed the removal traveled:

```
<- RESOURCE_DESTROY obj#86 at 696,648 room 8
<- DROP item #54 x1 at 600,648   (x3)
<- RESOURCE_DESTROY obj#86 at 600,648 room 8
<- PICKUP item #54 at 628,656 room 8
```

Two findings, both surprising:

* **This build's berry bush does NOT regrow and does NOT change frame.** PencilPal syncs
  bushes by watching `image_index` change under `variable_instance_exists(id, "grows")`, and
  the first probe copied that reading - watching for a frame change - and reported **nothing**
  while two bushes were picked. A picked bush here **vanishes**. So the PencilPal
  `grows`-and-regrow model does not describe this build's `objBush`, and copying a working
  mod's *mechanism* without measuring this build would have produced a sync that silently did
  nothing.
* **A bush is not a resource: `obj#86 objBush` has parent `parHerb`.** That is the actual
  reason `OnResourceDestroy` never fired for it - in the game's own taxonomy a bush is an
  herb, a separate family from `parNatural` (trees, rocks, ore). It also explains PencilPal's
  separate `["objBush", "objCotton", "objFlower"]` list, which looked like a convenience and
  is really a different object class.

Because there is no event, a bush must be **observed**: the mod remembers which plants were
here (`objBush`/`objCotton`/`objFlower`, resolved by name with `asset_get_index`) and, when
one vanishes, sends the **same `RESOURCE_DESTROY` the tree uses** - object index + tile. The
far end needed no new code: `CoopApplyResourceDestroy` already matches on index+tile and walks
with `TrueInstanceNumber`.

**Known limitation, accepted 2026-10-20:** the walk uses `instance_number`/`instance_find`,
which see only instances **active in the current room**, so a plant picked out of view is never
seen to vanish and survives on the peer. Widening it to `TrueInstanceNumber` would catch those
but would also make a room change look like every plant in the old room being picked at once;
the real fix is to remember each plant's room, which belongs with the rest of the room-scoped
world work.

### M2.2 resource coverage probe (2026-10-20) - three lifecycles, not one

The owner pointed out that a berry bush and built structures (which also drop items) were not
covered. A read-only probe (`0.0.17-m2o`: `OnResourceDestroy` enriched, and **new**
`OnStructureDestroy` and `OnResourceSpawn` handlers, both log-only) was run over stone, iron,
a bush and a demolished furnace. The result is that **resources do not have one lifecycle but
three**, and the mod only handled the first:

| Lifecycle | Example | Event seen | Sync |
|---|---|---|---|
| **Removed from the world** | tree, rock, ore | `OnResourceDestroy` fires | works (M2.2) |
| **Removed, but no event** (`parHerb`) | berry bush, cotton, flower | **no destroy event at all** | **observed, then sent as RESOURCE_DESTROY** |
| **Structure demolished** | furnace, veranda | `OnStructureDestroy(i, structure)` | **missing** (no handler existed) |

(The bush row was first written here as "picked, then regrows" from PencilPal's `grows` code.
The live probe disproved that for this build - see the section above - and the row is corrected
to what was actually measured: the bush is removed and simply fires no event.)

**The bush is picked, not destroyed.** The evidence: picking one produced only
`OnItemDrop fired, 1 entries: 0:[item54 (berries)]` - and **no `OnResourceDestroy`**. (What
happens instead is now settled: the bush is removed with no event, and since its parent is
`parHerb` it was never a resource to begin with - see the section above.)

**Stone and iron are the SAME object.** Both are `obj#84 objRock`, parent `parNatural`. They are
told apart only by **`image_index`** (stone `image 0`, iron `image 2`) and by what they drop
(stone `item1`, iron/ore `item2`):

```
probe OnResourceDestroy: obj#84 'objRock' parent 'parNatural' at 664,664 image 0   -> item1 x3
probe OnResourceDestroy: obj#84 'objRock' parent 'parNatural' at 696,680 image 2   -> item2 x3
```

So `object_index` + position does **not** identify a rock's kind. For *removing* the rock that is
harmless - `CoopApplyResourceDestroy` matches `object_index` + tile and destroys what is there, and
a rock of any kind is the right thing to remove. What breaks is **drawing**: M3 must draw the
peer's rocks, and without `image_index` every iron and gold rock is drawn as plain stone. So the
`image_index` change belongs to the **drawing/world-image** path (M3), **not** to
`RESOURCE_DESTROY`, which needs no change for this. Corrected here because the earlier note said
"wire change for M3" without saying which wire.

**Structures are a separate family and the event carries the structure id.** Demolishing a furnace
gave:

```
probe OnStructureDestroy: obj#109 'objFurnace' parent 'parStructure' structure=1 at 672,672
OnItemDrop fired, 1 entries: 0:[item1 id=102231 x10 @672,672]   <- the furnace returned 10 stone
```

`structure=1` is the **structure database index** (row 1), i.e. the wire key we already decided
structures should use - so `OnStructureDestroy` can send the id, not a localized name. The parent
is `parStructure`, distinct from `parNatural`, which is a clean way to tell the two families
apart. Note also that a structure's refund arrives as **one drop with `stack=10`**, not ten
entries - so `stack` genuinely carries counts and must not be assumed to be 1.

**Stone drops three stacks.** `OnItemDrop fired, 3 entries` with three `id`s of `item1` - the
same "several piles per harvest" shape as wood, which is the third confirmation that the
receiver-side dedup that was removed would have silently eaten loot.

### M2.2 resource coverage, part 2 (2026-10-20) - gold ore, and why a fairy is not a resource

The owner asked to cover "other resources and structures" next and named **gold ore** and a
**fairy** as the two to try first. Checking both against the game before writing anything showed
they are **not the same kind of thing at all**, which is the useful result:

**Gold ore is a rock, not an object.** `objGold`, `objGoldOre` and `objOre` appear **0 times** in
`data.win` - there is no separate gold-ore object. Gold is one more appearance of `objRock`, the
same object stone and iron already use (`obj#84`, parent `parNatural`), differing only in
**`image_index`** and in what it drops. So gold ore needs no new event, no new handler and no new
packet: it needs the **same one change stone and iron need**, `image_index` on the wire. Until
that change is made, every rock a peer sees is drawn as whatever the host's rock was - stone for
stone, but still stone for iron and gold.

**A fairy is a living creature, not a resource.** `objFairy` is the game's sprite/fairy critter,
and the evidence that it is the *other* family is direct:

- It sits in PencilPal's own **blacklist** of things it will not sync as world objects
  (`Main.gml:67`): `["ModEnemy","Slime","SlimeKing","Boar",...,"parCritter","Fairy","TorchBug",
  "Butterfly","DeathMoth","Beetle","Cow","Chicken","Sheep","MagicDeer"]` - i.e. it is filed with
  **critters and animals**, not with resources.
- PencilPal goes out of its way to **skip a fairy twice**, in two separate sync loops:
  `if(string_replace(object_get_name(l), "obj", "") == "Fairy") continue;`
  (`Networking.gml:310` and `:495`).

That skip is the important part. A resource is static: it stands at an (x, y) and is removed or
changes stage. A fairy **flies, moves on its own, and is not owned by either player** - mirroring
it the way a rock is mirrored would not make it fly, it would make it jitter between two
machines. So a fairy belongs with **mobs/moving things**, a family this mod does not sync at all
yet, and it needs a **movement-style stream (like POS)**, not the resource path. Deferring it is
the correct call, not a shortcut.

This gives the coverage plan for the rest of the world, by family rather than one object at a
time:

| Family | Examples | Event / mechanism | State |
|---|---|---|---|
| Static resource, removed | tree, rock/stone, iron, **gold ore** | `OnResourceDestroy` (+ `image_index` on the wire) | sync written, needs `image_index` |
| Static resource, picked and regrows | bush, cotton, flower | no destroy event; observed by change (`OnStep` bush probe) | **being measured now** |
| Built structure | furnace, veranda, walls | `OnStructureDestroy(i, structure)` | **handler missing** |
| Moving creature | **fairy**, cow, chicken, slime, butterfly | needs a position/tick stream, like the player's `POS` | not started |

### A native crash with a peer connected: what is known, and what was ruled out (2026-10-20)

The owner reported that "save and quit" (and, later, just playing for a few minutes) closed
the whole game instead of returning to the menu. **It is not fixed yet.** What follows is the
honest state: the measurements, the two explanations that were proposed and disproved, and
the one real bug the investigation did turn up.

Symptoms, as measured rather than assumed:

* Windows Application log: `APPCRASH`, `Forager.exe` (32-bit, version 3.0.0.0), exception
  **`0xc0000005`** (access violation), faulting offset **`0x012c265f`**.
* **The offset is identical in all sixteen crashes** of that evening (16:27 … 18:52). That
  is the single most useful fact: a leak or a race produces a spread of faulting addresses,
  while one fixed address is one instruction failing under one condition. It also maps into
  the exe's `.text` section (RVA `0x012c325f`), i.e. it is the runner's own compiled code,
  not our GML.
* `%LOCALAPPDATA%/Forager/error.log` was **empty** for these crashes. A native fault never
  reaches the GML error handler, so "no error.log entry" is itself evidence about *what kind*
  of failure this is.
* **No crash before 16:24 that day.** The loot and resource sync of M2.2 landed at 16:24;
  every crash is after it. That narrows the search to the M2.2 code paths.
* The peer stops replying to PING several seconds before the process dies, while the game is
  still sending PING - i.e. **the receive path stops before the send path does**.

**Ruled out, with the evidence that killed each:**

1. **"The incoming async buffer leaks."** Tempting: nothing freed it, and the symptom profile
   (fine for a minute, then dead) fits a leak. It is wrong - the GameMaker manual is explicit
   that the buffer "will be automatically freed when the event is over". The `buffer_delete`
   added on the strength of this guess was therefore a **double free**, and the crash arrived
   *sooner* with it (~6 s after the last reply) than without (~8 s), which is what a double
   free does to a heap. Removed, with a comment so it is not "fixed" again.
2. **"The peer socket leaks across a reconnect."** The mock log shows the mock connected
   **once**; the "session 2" line is a reconnect *attempt after the crash*. No reconnect, no
   accumulated sockets. (The socket is still now destroyed in `CoopResetPeer`, which is
   correct on its own terms, but it is not the cause.)
3. **"A garbage item id from a mis-parsed DROP goes out of range in `DropItem`."** This was
   the strongest theory, and it is **disproved by arithmetic**. The field order *was* wrong
   (see below), but unpacking a real `DROP` payload the wrong way shifts every field by
   exactly one byte, and the item id happens to survive that shift intact; the dropped id was
   therefore always valid. Worse, the room byte read back as `0` in every case, so the room
   test failed first and `break` ran **before** `CoopApplyDrop` was reached at all. No packet
   of 2026-10-20 ever reached `DropItem`.
4. **"`instance_destroy` on a deactivated instance."** A real hazard in GameMaker and a real
   defect in this mod (see below), but it cannot be the crash either: the three loot packets
   were being discarded before any apply ran, so nothing was destroyed.

**The one real bug found.** The field order of `DROP`, `PICKUP` and `RESOURCE_DESTROY` was
read differently from the way it was written:

```
sender :  u8 room, s32 x, s32 y, u16 item, ...
reader :  s32 x,   s32 y, u8 room,  u16 item, ...     <- wrong
```

GML does not complain - the buffers are untyped and a wrong read returns a wrong number - and
`docs/PROTOCOL.md` had documented the *correct* order all along, so the code contradicted its
own spec. The effect was that **every incoming loot and resource packet was silently
discarded**, because the room byte read back as `0` and the room test failed first. That is a
complete explanation of a *different* symptom (the mock never saw its drops or destructions
mirrored) and it is now fixed, with a checker that fails the build on any future mismatch:
`tools/check_gml.py` compares, per packet, the `buffer_write` order in the sender against the
`buffer_read` order in its `case`, and also fails a packet that is written and never read
(which is how a write-only `DEBUG_LINE` handler was caught). The check was verified by
reintroducing the exact bug.

**The other real defect.** `CoopApplyResourceDestroy` and `CoopDropAt` walked `TrueInstance*`,
which returns **deactivated** instances as well as active ones, and the first of them called
`instance_destroy` on whatever it found. A deactivated instance is not safe to destroy. Both
now walk `instance_number`/`instance_find` (active only), matching the set the plant snapshot
observes. `CoopStructureAt` keeps the true-instance walk on purpose: it only **reads**, and
reading is safe.

**Why the plant snapshot announced one bush four times** (live: `RESOURCE_DESTROY obj#86 at
616,728` at 18:50:42, :43, :49, :55): the snapshot is built from active instances, so a bush
the room merely **culled** (the player walked away) looked picked. It came back into view, was
culled again, and was announced again. `CoopPlantStillExists` now asks the true-instance walk
whether the plant is really gone before announcing, which separates "culled" from "picked".

**Still open.** With those three fixed the native crash may be gone - the discarded packets
mean the apply paths were never exercised before, and it is possible the crash came from
somewhere in the send path that is now different. But that is a **hope, not a finding**: the
cause of `0x012c265f` has not been identified. The next step is the phase marker now in the
build (`global.coopPhase`, announced every ~5 s): a native fault leaves no log, so the last
phase number the peer received names the operation the game died in.

> **Superseded 2026-10-21:** the cause *was* found, and it is not in this list. See
> "The crash is FIXED" below - it is the socket being live during world teardown, fixed by
> ending the session at the save-and-exit transition in `OnStep`.

#### Second run (2026-10-20 19:16-19:20): the phase marker, and a corpus to replace it

The phase marker did its job and returned a **negative**: the last line the mock received was
`probe tick 8700 rx 2822 phase 10 player 1`, and phase 10 is "walking the plant snapshot" - a
phase that is *set* and never reset, so a frozen value says only "somewhere at or after this
point", not where. 8700 ticks also settles one thing the owner had guessed at: the session ran
**about 2.5 minutes and died on its own**, not at world exit.

Two further facts from the same log:

* `rx` grew at ~0.33 packets per tick (one packet every three frames, i.e. the mock's POS
  stream) with no discontinuity up to the last tick - the receive path was healthy right up to
  the fault, so this is not a "packet handler half-way through a read" signature.
* The mock's own last lines are `-> PING nonce=126` at 19:20:43 and `connection closed by peer`
  at 19:20:44 - **the send path was still alive one second before the process died**, while the
  game's replies to `<- PING nonce=98/...` had already stopped. Same asymmetry as before:
  receive dies first, send lasts longer.

Conclusion: the phase marker cannot localise a fault that lands *between* marker changes, so it
was replaced (leaving it in place as a coarse hint) by a **packet corpus**: `global.coopTrace`
is a capped ring of short operation tokens - `pkt26`, `gotpos`, `pos`, `sndpos`, `spos`,
`plants`, `place`, `placed`, `watrej`, `occ`, `mplace`, `mplacesdone` - which is sent whole with
every addition over the existing `DEBUG_LINE` channel. The last corpus the peer holds reads like
a flight recorder: one that stops after `watrej` places the fault in the code *after* the
refusal walk, and one that stops after `place` puts it inside `CoopCreateStructure` on the host.

It is deliberately **sender-side only** - no new packet, nothing asked of the peer - which is
the one property that makes it safe: `DebugLine` is fire-and-forget, so a diagnostic built on it
cannot fail because the peer (a test tool) did not answer. One earlier design in this
investigation was an outstanding-request scheme that would have been an assertion no run could
ever satisfy.

#### Placement refusals: the spot check was sampling tile corners

Separately from the crash, the mock still never placed its furnace, and the reason turned out
**not** to be placement. The run's refusals were 252 out of 252 asks, and the killer line is:

```
-> INTENT_PLACE 'печь' at 672,672          <- the known-free cell, on land
<- INTENT_REJECTED #28032: 'that spot is already occupied (1 of 4 tiles)'
```

`672,672` is the cell a structure was observed standing on in the live probes. The host refused
it. The water and occupancy checks were sampling `(_ix +/- 12, _iy +/- 12)`, which on a 24 px
grid is the **shared corner of four tiles** - the one point where "which tile is this?" has no
answer, and the game's tile test resolves it differently from its own builder. A 2x2 structure
asked at six neighbours of the player was refused six times out of six, all false. That is the
whole of "the mock never placed a furnace".

The four points that actually cover a 2x2 block are its **cell centres**, 24 px out along each
axis (`global.coopFootprint`). The same bug also produced `(4 of 4 tiles)` for a *single* tree,
because one corner sample was counted in all four cells at once.

The player was then removed from the occupancy test: the mock asks for a spot next to itself, so
its own body was the nearest living thing to every request and would have refused all of them.
The host's own cell is still checked, so an intent that would put a structure on top of the host
is refused.

#### Third run (19:16-19:31): the corpus localises the crash to the socket teardown

The corpus paid off. The last ring the mock received before the 19:31 death was:

```
... pkt5 pkt6 gotpos pos sndpos spos plants plants plants
```

Every packet in flight was read to completion (`pkt5` -> `pkt6` -> `gotpos` -> `pos`) and the
position send finished (`sndpos`, `spos`) **before** the last `plants`. So the fault is not in a
packet handler and not in the plant walk's own body - both complete. `place` and `placed` appear
**16 times each** in the whole log and never once dangle, so the structure path is clean.

Two measurements then killed the "accumulating leak" idea outright:

* the receive rate is **flat at 0.30 packets/frame for the entire 110-second session** - 0.27 to
  0.31, no drift, no kink, right up to the death. A leak that builds would show a slope.
* the crash does **not** track elapsed time: it landed at tick **8700** on one run and tick
  **6900** on the next. Different durations, same instruction.

**What actually causes it: a double destroy of a socket the mod does not own.** `CoopResetPeer`
destroyed `global.coopSocket` unconditionally, but the two sides acquire that socket by
different routes:

| side | how it gets the peer socket | who must release it |
|---|---|---|
| client | `network_create_socket` - it makes its own | the mod |
| host | the runtime hands it over in the async connect event | **the runtime**, on disconnect |

So on the host, a peer disconnect means the runtime has already retired the socket, and the mod's
`network_destroy` is a **second** destroy of a dead handle - a socket whose owner is gone. That
is a native access violation, it needs a disconnect to happen first (hence minutes, not seconds),
it can only occur while a peer is connected, and it faults at **one fixed instruction** - which is
exactly the signature of the sixteen crashes. The mock's own log is the clincher: every death is
preceded by `connection closed by peer`, i.e. the peer's close, then the runtime's teardown, then
the mod's redundant destroy.

`global.coopSocketOwned` now records which kind of socket is in hand - set true where the client
creates its own, false where the host is handed one - so the single teardown path closes the right
one and the runtime keeps its own. **Not yet verified live**; it is deployed as `0.0.30-m2ab` and
the next run is the test.

#### The occupancy check was still wrong after the first fix

The tile-corner fix above replaced `(_ix +/- 12, _iy +/- 12)` samples with `global.coopFootprint`
(24), and that was **also wrong**, in a way the owner caught at once by building a furnace that
landed *on a rock*. Two separate defects:

* `CoopSolidNear` asked `instance_number(parSolid)`, which is **0** in this build, so the walk
  found nothing to count and every cell read free - which is why the furnace went down on top of a
  rock. The concrete families (`parNatural`, `parHerb`, `parStructure`, `objRock`, `objTree`) are
  listed instead.
* the occupancy loop asked "is a solid within `coopSolidRadius` (24) of this point?" at four points
  **24 px apart**. The samples overlap, so one rock counted in all four and every refusal read
  `(4 of 4 tiles)` while a single thing was there. **A radius cannot express "this cell is taken";
  it can only express "something is near", and near is not the question.** `CoopSolidInCell` now
  reduces a solid to its own 24 px cell and compares cell indices, so each solid counts at most
  once and only when it truly shares ground with the build. The water check asks the same four
  cells the same way, so nothing samples a corner anywhere.

The lesson repeated from `parSolid`: **a name present in `data.win` is not proof of the behaviour
the name suggests.**

#### The build grid is 16 px, and the "24" was measured off the mod's own output

The owner's second complaint after the occupancy fix landed was that the furnace was *still*
wrong: "on half a cell up". It was. The game-placed and mod-placed coordinates, side by side:

```
probe spawn: obj#109 'objFurnace' at 656,608  mod24 8,8  mod12 8,8   <- the OWNER built this
probe spawn: obj#109 'objFurnace' at 624,600  mod24 0,0  mod12 0,0   <- the MOD built this
```

The game's own structure is `656 = 16*41`, `608 = 16*38` - exactly on a **16 px** lattice. The
mod's is a multiple of 24, which is 8 px off that lattice on Y. That is the "half a cell".

The reason the mod believed 24 is the part worth remembering: **every coordinate ever sampled
had been created by the mod itself.** The mod writes multiples of 24 because it was told to, so
each sample "confirmed" the grid that produced it. The checks agreed with one another and not
with the game - a circular measurement, and no amount of live re-testing could expose it while
the only samples came from the thing under test.

The circle broke when objects **no mod touches** were collected instead: the game's own natural
spawns (rocks, trees, bushes - the map places them). Nineteen of them, from the live logs:

```
smallest non-zero gap between neighbours: 16 px on X and on Y  (gaps were 16 and 32)
every x remainder mod 16: one constant      every y remainder mod 16: one constant
mod 12 and mod 24: noisy, no constant
```

**16 px is the world's tiling.** The two families report different points on it - a natural
object reports its cell (remainder 8, the cell centre), a structure reports its block corner
(remainder 0) - but both are the same lattice, and 24 is not a step the world takes.

What changed: `global.coopFootprint` 24 -> 16, the snap `round(v/24)*24` -> `round(v/16)*16`,
`CoopSolidInCell`'s cell index `floor(v/24)` -> `floor(v/16)`, and the footprint offsets
`+/-12` -> `+/-8` (half a cell on *this* grid). The mock's spot spiral steps 16 px and now bases
itself on the **host's reported player**, not its own puppet - an on-grid furnace placed next to
a stand-in standing somewhere else is not a placement test, which was the other half of the
complaint. A new F4 probe dumps every structure with its remainder mod 8/16/24, so the next run
reports the lattice instead of leaving it to be inferred again.

The occupancy **rule** was unaffected: still cell membership, never a radius. Only the cell size
changed, and the old corner-sample bug did not return with the new offsets because every sample
is still a cell centre, strictly inside its cell.

**The general lesson, worth more than the fix: a grid inferred from data the program itself
produced is not evidence.** Sample something the program cannot have touched - here, the map's
own spawns - or the measurement is a mirror.


### The crash is FIXED (2026-10-20/21) - cause identified and verified live

**Closed.** The game now returns to the menu on "save and quit". Root cause and fix:

**Cause: a socket that is still live while the world tears down.** Not the world teardown
itself, and not anything in our drawing, socket ownership, buffers, or packet lifetimes - all of
those had been disproved earlier (above). The controlled evidence is a pair of live runs:

    press F7 first (our own CoopDisconnect)  -> the game returns to the menu, no crash
    leave the socket open                    -> the game dies ~1 s after OnRoomLoad

The second run also showed the crash is **independent of placement**: with `--no-place` (the mock
built nothing, the owner built nothing) the crash was identical, which removed the last
structure-related theory (`instance_create_depth` + writing `.structure`).

**Why closing the socket in `OnRoomLoad` was not enough.** That event fires on the way *out* -
the world is already gone - so a close there lands inside the window that faults. The engine's own
netcode closes its sockets at the transition instead (the reference mod's `OnStep`:
`instance_exists(objTransition) && objTransition.target == rmInputSelect`, then
`network_destroy`), which is the one moment early enough.

**The fix** is in our `OnStep`: as soon as the transition to the menu begins, end the session.
The guard is the same pair `CoopLeaveWorld` uses and it is naturally one-shot (the first call
clears both flags). `OnRoomLoad`'s close stays as a safety net for a crash or forced quit.

**The instruction at the fault, read with a disassembler** (capstone + pefile; the owner approved
the install). Note this corrected an earlier pass: `0x012c265f` is an **RVA**, not a file offset.
Treating it as a file offset shifts the reading by `0xc00` bytes (`.text`: RVA `0x1000`, raw
`0x400`) and lands on `xorps xmm0, xmm0` - a register-only instruction that *cannot* raise an
access violation, which is what made the first analysis nonsense. Read correctly (RVA ->
runtime VA `0x016c265f`):

    0x016c2650  push ebx
    0x016c2658  mov esi, ecx          ; this
    0x016c265a  call 0x16c2ca0        ; hash of the key
    0x016c265f  mov ecx, [esi]        ; <<<< FAULT: first field read of `this`

That is the engine's own hash-map lookup (chained buckets: mask hash by capacity-1, compare the
key at `+8`, walk the chain at `+4`), called from **six** places, none of them this mod. It faults
because `this` is null or freed. It is GML struct/`ds_map` access after free - consistent with a
live socket delivering an async event into a teardown that has already dropped its data.

**The general lesson:** an address that is already an RVA must not be re-translated as a file
offset, and the way to tell the two apart is to decode it - an instruction that cannot fault means
the reading is wrong, not that the fault is impossible.

### M3 blueprint (2026-10-20) - the visible second player, from working code

The owner asked to see the other player: model, name over the head, held item, cosmetics. This
is M3, and PencilPal - the one mod that runs in this build - **already draws exactly that**, so
the recipe is known rather than guessed. Its `Events/OnDrawEnd.gml` is the reference:

```gml
// the partner's held item, drawn beside them
var _sprite = ItemGet(global.partnerHold, ItemData.Sprite)
draw_sprite_ext(_sprite, _sprite, partnerX±5, partnerY-2, 1, 1, angle, c_white, 0.5)
// the partner's body
draw_sprite_ext(global.partnerSprite, global.partnerImageIndex,
                global.partnerX, global.partnerY,
                1 * global.partnerFacing, 1, global.partnerAngle, c_white, 0.5)
```

Facts this settles:

- **The puppet is drawn, not spawned.** PencilPal uses `draw_sprite_ext` in `OnDrawEnd`, not a
  game object. Simpler than the "puppet instance" the M1 placeholder comment assumed, and it
  sidesteps depth sorting for the first cut (no y-based depth to get wrong yet).
- **The ghost look is alpha `0.5`** on the partner sprite - one constant, not an art problem.
- **A held item travels as an item id** (`partnerHold`), rendered through
  `ItemGet(id, ItemData.Sprite)` - the same id-based rule as loot. Cosmetics ride along the same
  way: an id the far side turns into its own sprite. This is the "send facts, not pixels" idea
  made concrete: both installs run the same Forager, so only the ids, facing and frame travel.
- **The visual state of a puppet is four values**: `sprite`, `image_index` (animation frame),
  `facing` (±1, flips the sprite), `angle` (0 or 90 for tools). The current `POS` packet already
  carries sprite, facing and angle; **`image_index` is the missing piece** that makes a puppet
  walk instead of glide, and PencilPal's own notes flag that realtime `image_index` sync was
  "too laggy" for it and got cut - worth knowing before promising smooth animation.

Planned split: **M3.1** puppet (sprite/position/facing/angle), **M3.2** animation
(`image_index`), **M3.3** name over the head, **M3.4** held item and cosmetics. The project's
own `Events/OnDrawEnd.gml` is where the peer marker lives today (an aqua circle), so the puppet
replaces that placeholder in place.

## Derived (our reading, not facts)

- A Forager co-op mod is a **pure GML** problem. `UndertaleModTool` / `GMEdit` operate on
  `data.win` and are only needed if the modding API is insufficient; start with the API.
- PencilPal is best described as **peer-simulated diff broadcast**: both clients run the
  full sim and ship diffs, so any missed/duplicated mutation desyncs or crashes. This is
  the likely root cause of the reported instability, not just bugs in individual packets.
- A from-zero design should therefore be **host-authoritative with sequenced mutations**
  (host resolves world changes, client sends *intents*, not results) and should tolerate
  lost/duplicate packets. That is exactly what the cancelled official netcode and
  PencilPal both lacked.
- Transport upgrade path that matches the hub's standard: keep the GML sync layer, replace
  raw TCP + typed IP with **Steam relay via Spacewar (appid 480)** as described in
  `playbook/networking-steam-spacewar.md`, so players never forward port 6511.
- Because Forager has no local multiplayer, the honest cheap answer *today* is still
  streaming (Steam Remote Play Together / Parsec). The build-from-zero path is a real
  project, sized in weeks, with the sync design above as the differentiator.

## Unknowns

- **Resolved 2026-10-19:** live Steam build is **6160665**, verified by Steam "Verify
  integrity" (21 files, all valid) on the owner's machine. `Forager.exe` and `data.win`
  SHA-256 are in the Identity table. Whether this equals the 2022/23 build PencilPal
  targeted is still `unknown` (PencilPal's manifest only records API version 1.0.1).
- `unknown` — whether PencilPal's source is available outside the compiled Workshop copy,
  and under what license. **We must not copy its code** without a license; study only.
- **Resolved 2026-10-19:** modding API docs mirror found —
  https://topheranselmo.com/forager-modding-docs/ (API version 1.0.1).
- **Resolved 2026-10-20:** Forager **cannot** run two instances on one PC — a second launch
  while the game is running is refused. Two-client QA therefore needs a second Windows
  session/VM or a second machine.
- `unknown` — whether the modding API can load a native/GML extension
  (needed for Steamworks.gml inside a mod).
- `unknown` — whether `data.win` decompiles cleanly with UndertaleModTool for the live build.
- `unknown` — whether Foragipelago's source is public (would be a second, independent
  example of Forager mods doing real socket I/O).
- `unknown` — whether PencilPal's `Networking.gml` byte layout could be reused as-is, or
  whether the sequenced-mutation design needs a different packet schema (probably the
  latter).
- **Resolved 2026-10-19:** Forager had no directory entry; one is added in this PR, and
  Python 3.12 + PyYAML are installed so `tools/build_directory.py` runs.

## Sources index

- yal.cc (YellowAfterlife, modding API + networking author): 2019/2020 reviews,
  `forager-retrospect`, `introducing-nuclear-throne-together`,
  `introducing-caveblazers-together`, `docs/gm/gmtogether`.
- Official modding API docs: https://topheranselmo.com/forager-modding-docs/
  (mirror; original `http://138.197.196.157/docs/`).
- PencilPal Workshop page (id 2843162754) and its source in the Workshop cache.
- PencilPal/PenPal guide: https://playco-opgame.com/multiplayer-mod/forager-multiplayer-mod/
- Press on the cancellation: RPS, TechRaptor.
- Community mods: Zamiell/FastCrafting, CainDev/Forager-Mods, AlejandroAkbal/Forager-Cheater-Mod,
  Forager Evolved (Workshop 2147146084).

## Decisions

- 2026-10-19: Path 2 (build from zero) chosen by owner.
- 2026-10-19: Treated the owner's non-Steam repack as unusable for identity/tests; asked
  the owner to restore the legit Steam copy first (details in `local/owner-state.md`).
- 2026-10-19: Dismissed PencilPal-as-base and GMTogether; kept PencilPal's *source* as a
  reference to study (no code copied).
- 2026-10-19: **M0 passed** — the scaffold mod compiles and loads in the live build using the
  game's own loader, so no external modding tool is needed to build co-op. Next is M1
  (network layer) per `DESIGN.md`.
