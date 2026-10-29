# State of Decay (2013) co-op: progress and handoff

Single source of truth for the current state of the co-op work. Read this first, then the relevant parts of
[NOTES.md](NOTES.md) (full research log, by phase) and [README.md](README.md) (public summary).

**Rule for every agent:** after each discovery, fix or verified result, add a dated entry at the top of the
[Findings log](#findings-log) and update [Current state](#current-state) if it changed. Keep it free of private
data (no machine names, user folders, IP addresses, account names).

## Current state

Last updated: 2026-10-28.

Two copies of the game run side by side (host = Marcus Campbell, joining player = Ed Jones since 2026-10-28;
before that the joining player was "Maya"). Everything is applied at
runtime by Python scripts under the gitignored `local/` folder; no game file is modified except the loose
`Game/scripts/gamerules/singleplayer.lua` (extra RMIs) in both test copies.

| Area | State |
|---|---|
| Join, walking, camera, HUD, minimap, mission banner on the joining copy | Works |
| Attacks, dodges, jumps replicate both ways | Works |
| Zombies visible on both machines, clothed, animated on the client (no T-pose), host's hit reactions shown | Works (verified 2026-10-24) |
| Zombies target, chase and damage her, also with him 300-450 m away | Works (soaks 2026-10-24) |
| Her melee hits and kills zombies; her kills counted for her, not the host | Works (final build 2026-10-26: her +1, host +0) |
| Host player's own weapon kills zombies, counted for him | Works (final build 2026-10-26: his +4, hers +0) |
| Zombies attack the host player | Works (final build 2026-10-26: 90 -> 1 in 15 s) |
| Zombies attack her with the host's player near (20 m) | Works (final build 2026-10-26: 100 -> 64 in 9 s) |
| Zombies attack her with the host's player far (300 m) | Works, weaker (fresh boot 2026-10-26: 100 -> 86 / 72 in 15 s vs 100 -> 17 near; fewer zombies around her) |
| Weapons: her starter hammer, cycling from community storage (middle mouse), host's journal equip | Works |
| Host stability (hangs / silent deaths) | **Fixed**: minimap name-table race (2026-10-24) and a per-call buffer leak in our tooling (2026-10-26) |
| Looting / doors / searching for the joining player | Doors work on her machine: state, door and player animations, collision (`maya_record.py`, `door_sync.py`). Her search runs and completes on the host with her own search manager (`search_ctx.py`); her loot screen opens on her machine with her own backpack, and Take/Use apply to her backpack on the host (`loot_relay.py`) |
| Getting hurt, going down and getting up (her) | Works: health replicated, HUD follows, down at 1 HP, mash Space to get up (`downed.py`) |
| Starting a session from the game's main menu | Works: "Co-op Play" item with Host Game (Continue / New Story) and Join Game (hosts found on the local network); `coop_agent.py` per copy runs the co-op setup on each side (2026-10-28) |
| Characters | New story: host Marcus, player 2 Ed (Ed's own survivor record, his NPC body hidden everywhere), players 3-4 random generic survivors no record uses, never a story character (`maya_record.pick_def`) |
| Up to 4 players | Works (2026-10-28): host + 3 joining players through the menu; every co-op module handles every joining player; 20-minute 4-player soak 10/10 fight + search/loot cycles; a player leaving mid-game leaves the others playing |
| Leaving and coming back | Works (2026-10-29): a player who left frees its slots everywhere; one who comes back (or the next one) takes the free slots and the departed player's character (Ed stays Ed) |
| Separate PCs | Works (2026-10-29): a second PC on the LAN joined through Co-op Play > Join Game as Ed; its moves arrive on the host. Files for it: `local/make_coop_bundle.py` (scripts + README), `coop_firewall.ps1` on the host PC, `coop_hosts.txt` when broadcasts don't get through; a VPN on either PC must leave the LAN out |
| Session stability | Asserts and missing-entity updates no longer kill the host (`crash_guards.py`); 25-minute play soak 19/19 cycles without a crash |
| Vehicles | Untested in co-op: the only vehicle at the story start is locked for both players |
| Missions, community, journal data for the joining player | Not started (she now has a community record on the host; nothing mirrored to her machine) |

## How to run a session

1. Never start or modify the Steam install. Test copies live in `local/test-install` (host) and
   `local/client-install` (client); both gitignored.
2. Keep the PC awake and the display on: `python local/keep_awake.py 4`. With the display off a new game
   process finds no Direct3D device ("Could not find any compatible Direct3D devices"); waking from Modern
   Standby crashes both renderers. A locked screen also blocks booting.
3. From the game's menu: `local/play_menu.sh` starts both copies through `local/coop_agent.py` (one agent per
   copy; on two PCs run one `coop_agent.py` per PC). In one window Co-op Play > Host Game > Continue Story or
   New Story; in the other Co-op Play > Join Game > the host's entry. Steps 4-5 are then done by the agents.
   Scripted alternative: steps 4-5.
4. `local/restart_session.sh` (kills only the test copies, boots host + client, OKs the "Unsupported video card"
   dialog via `local/dismiss_gpu.ps1`).
5. `python local/coop_setup.py` installs everything on both machines and starts `local/hud_relay.py`
   (must keep running on the host).
6. Optional diagnostics: `Proc.trap_faults()` on both processes (in-process crash catcher), exit watchers
   (`local/exit_watch.py host|client`).

Use a real Python 3 (the Windows Store stub does not work); `capstone` and `pillow` are needed by the tools.

## Co-op layer (local/, installed by coop_setup.py)

| Module | What it does |
|---|---|
| `sodlib.py` | Process access, mailbox calls on the game's main thread (PeekMessage hook), hooks, Lua/console exec, hardware breakpoints (`count_exec`, `watch_writes`), `trap_exits`, `trap_faults` |
| `prep_client.py`, `client_tick.py` | Joining copy: UI gate, front-end/HUD update replacement |
| `player_sm.py`, `peer_input.py` | UL state machines for both players on both machines; input sent as chat messages (`coop:`/`cooh:`) |
| `peer_melee.py` | Her hits applied to zombies on the host (CombatDamage with her params) |
| `zombie_targets.py` | Zombies pick her as a target; contact/strike on her |
| `zm_focus.py` | Zombie manager: focus point for her, cull uses the nearer player, population pass around her (<=300 m) |
| `kill_counts.py` | Her kills counted for her (chat `kilh:`); HUD banner off (`SHOW_BANNERS`) |
| `weapon_relay.py` | Melee weapon hash relay both ways, starter hammer, cycling |
| `pause_sync.py`, `remote_fx.py`, `turn_clamp.py`, `sync_models.py` | Shared pause, no host screen FX on client, turning, NPC models |
| `hud_relay.py` + `zombie_sync.py` | Host -> client over RMIs: banner, minimap icons, zombie bodies + animations + visibility (matched by position), split into <=400-char parts |
| `coop_menu.py`, `coop_agent.py` | Main-menu "Co-op Play" item and submenus (native hooks on the menu filler, title lookup and back input); per-copy agent: LAN discovery (UDP 64091), control channel (TCP 64092), host/join flows, coop_setup steps split by machine |
| `crash_guards.py` | Null guards, NaN-position guard, **radar_cache** (stability fix) |

Main test tools: `zm_soak.py` (long stress cycles), `soak_ab.sh`, `zm_freeze_test.py`, `kill_verify.py`,
`goal_check.py`, `path_hit.py`, `zombie_spawn.py`, `dbg_watch.py`, `exit_watch.py`, `anim_twice.py`,
`zm_dupes.py`, `use_test.py`, `door_pose.py`, `use_conds.py`, `cond_snap.py`, `far_strikes.py`, `anim_rate.py`,
`zcomb_conds.py`, `reaction_ab.py`, `contact_ab.py`.

## Next steps

1. Optional: more zombies around her when the host's player is far (population pass density, `zm_focus`), so
   far fights are as dense as near ones. Zombies near her spend much time in `Reaction.Basic` (not from her
   attacks; cause unknown).
2. Her UL character data on her machine (journal, HUD item slot, using backpack items outside loot screens).
   The loot screen's backpack panel already shows how to mirror UI from the host (`loot_relay.py`).
3. Missions and community for the joining player; vehicles once a save has them unlocked.
4. Commit/PR only when the user asks.

## Findings log

Newest first. One entry per discovery; link the NOTES section for details.

### 2026-10-29 - first join from a second PC
- A second Windows PC on the same LAN joined a hosted game: its agent reached the host's control port, its game
  connected, it got Ed Jones (survivor record bound, NPC body hidden), the full setup ran (`client_first`,
  `client_main`, HUD relay, turn clamp), and the host saw Ed walk ~45 m in 20 s while the host's player stood still.
- What the second PC got: one folder with the joining game copy (co-op `singleplayer.lua` and `user.cfg`), the
  bundle scripts, a copy of the Python install with capstone + pillow (no install needed) and a `join.bat` that
  runs `coop_agent.py --install <game> --saves pc2`; Steam logged in there.
- Before that, a VPN client on the host PC hid the LAN from other machines (see the entry below); excluding the
  local subnets from it was needed.

### 2026-10-29 - rejoining, simultaneous loot, other PCs, Steam stats file
- Rejoining: every per-player table frees a departed player's entry (actor/entity id pair: a returning player can
  get a new actor at the old address) and reuses it (`peer_input`, `player_sm`, `search_ctx`, `downed`,
  `pause_sync` - its wish becomes "no objection", `weapon_relay`, `door_sync`); the per-player hook blocks of
  `zombie_targets`/`peer_melee`/`kill_counts` are forgotten and disabled, `remote_fx` wrappers keep the actor in a
  cell that is cleared, the turn clamp is reinstalled. The host agent tells the remaining agents
  (`player_left`); the next joiner takes the departed player's character (`host_pick(prefer=...)`).
- Crashes found while players left and came back (`crash_guards.py`): the players category list (capacity 1) got
  every network player on a joining machine and the game wrote past its arrays ("Buffer overflow in entity db"
  assert, garbage actor pointers): a full list now skips the add, as on the host (`entitydb_full_guard`, at boot);
  a category remove callback on a departing actor (`category_remove_guard`); the NPC-list entry the host keeps for
  a joining player is removed from inside the game's own EntityDbRemove, before the actor is freed.
- Simultaneous loot: the host keeps one copy of the backpack-panel capture per queued loot screen (8 slots);
  test `local/loot_two.py`: two players search the same container at once, each screen went to its own player
  with its own panel (2 of 3 runs; in the third only one search completed, nothing to compare).
- Other PCs (not testable on one PC): LAN search sends directed broadcasts for every interface's real subnet
  (from ipconfig, any mask) and probes the addresses in `coop_hosts.txt`; per-process state moved out of the
  scripts folder (%TEMP%\sod-coop, a folder per process id + creation time); `make_coop_bundle.py` packs the 40
  scripts the agent needs, `singleplayer.lua`, `user.cfg` and a README; `coop_firewall.ps1` opens UDP 64087,
  UDP 64091, TCP 64092 on private networks (not run here). A joining copy run from an extracted bundle joined,
  left and came back like the others.
- Tried a stand-in second machine: the WSL virtual machine on the same PC (its own IP on a separate virtual
  network). It cannot reach the host at all (no ping, no TCP connection to the agent's control port 64092,
  which listens on every interface): WSL's Hyper-V firewall blocks traffic coming in from the VM by default,
  and the VM has no UDP tool for the discovery probe. Testing across machines still needs a real second PC
  (or firewall changes on the host PC), with `coop_firewall.ps1` run on the host.
  Later the same day, after the owner excluded the local subnets from the VPN client running on this PC, the
  VM reached the host: TCP to the control port and UDP to the game port (64087) and the LAN-search port (64091)
  all arrived (UDP sent with BusyBox nslookup to the port). So the VPN client, not the Windows firewall, was
  blocking it; on a PC with a VPN, exclude the LAN from it. A real join still needs a second PC (the VM cannot
  run the game).
- The agent retries the connect once (one rejoin's connection was released by the game right after the server's
  settings arrived, no reason logged; the next attempt got in).
- Steam stats file: every test-copy boot wrote `UserStats.cfg` (game stats, not the save) into the owner's Steam
  save folder about 8 s after launch, before `save_redirect.install` ran (~18 s); probably since the redirect
  existed (2026-10-27). `save_redirect.install_early` now patches ISteamUser's GetUserDataFolder slot from the
  moment steam_api's interface pointer exists (no mailbox needed); the stats file then appears in
  `test-saves/<name>`, and the Steam folder's files kept their times through 6 more boots. (`Class3.0.ulsave`
  there changed on 2026-10-28 22:42: the owner's own Steam session, not a test copy.)
- Never start a test copy's exe directly (even to see whether it boots): a bare launch on 2026-10-29 had no
  redirect and wrote `UserStats.cfg` into the Steam folder again. Launch through the agent or call
  `save_redirect.install_early` right after the launch.
- Verified (one PC, four copies): fresh 4-player New Story; Ed leaves and comes back with his agent run from the
  bundle (Ed again); the 4th player leaves and comes back (same character); soak 6/6 fight + search/loot
  cycles after both rejoins, three knock-downs and get-ups, all four games running.

### 2026-10-28 - up to 4 players
- The network side already took more players: a third copy joined after one more join-crash guard
  (`patch_client.py` "join null local actor", 0x13BE162: a later joiner binds the other players before its own
  actor exists). Channels are not 2, 3, 4 in join order (a failed join used up channel 3), entity ids differ per
  machine, and player actors are named Dude, Dude(1)..Dude(3) everywhere: `local/peers.py` tells players apart by
  actor/name, own player = the client-actor getter.
- Every co-op module was single-peer ("channel 2", one baked actor/record). Now per player:
  - `peer_input.py`: a slot per remote player (queue + stand-in input), chat input routed by the sender's entity id
    (the game translates it per machine); the host passes each joining player's input on to all clients as
    "cooh:" with that player as source. Other chat handlers (pause, kills, weapon, loot) get the sender too.
  - `player_sm.py`: a joining machine ticks its own and every other player's state machine (record read from
    actor +0x1110 at run time).
  - `search_ctx.py` (a search manager + context + swap routine per player), `loot_relay.py` (screens tagged with
    the player, sent with `onClient` to that player's channel only; Take/Use by sender), `downed.py`,
    `maya_record.py` (record per player, `keep_active` table), `zm_focus.py` (focus point / cull / population
    pass per player), `pause_sync.py` (host is the authority: pause only when everybody has a menu open),
    `remote_fx.py`, `kill_counts.py` ("kilh:n:NNNN"), `weapon_relay.py`, `door_sync.py` (4 name slots; the
    client Lua `ClCoopPeerState` changed in every install's `singleplayer.lua`), `zombie_sync.py`.
  - Hooks with per-player code run as chains of blocks (`local/chain.py`): `peer_melee`, `zombie_targets`,
    kill counts.
  - `coop_setup.py`: once-per-session host part, a per-player host part, the newcomer's machine, and
    `client_player_joined` for the machines already in the game; `coop_agent.py` hosts up to 3 joiners one at a
    time (the LAN list shows "n/4 players").
- Crashes found and fixed on the way (all in `crash_guards.py` unless noted):
  - a record bound to a player whose actor UL could not resolve (record update, null body, 0x154837B /
    0x1548660); UL finds actors only through the query lists, so `zombie_targets` now registers the player first;
  - joining machines ticking other players' state machines: actions writing through a missing per-actor slot
    (0x170B19F, 0x170B3AE; joining machines give missing handles a spare slot, only for the action handlers'
    callers) and a condition on a freed zombie actor (0x17325DB; actor and entity vtables checked);
  - the Lua damage handler killed a joining player outright (health <= 0 -> ProcessDeath -> Kill) before the
    health floor applied: `singleplayer.lua` (all installs) keeps Dude(n) at 1 there;
  - a player leaving: its freed actor stayed in the NPC query list and the interaction list (both ours) and
    crashed the host within a second; the game's own EntityDbRemove now takes it out of the NPC list
    synchronously, the interaction loop and the NPC tag pass skip non-CPlayer entries, other native loops check
    the player vtable, and the host agent notices the leave.
- Verified on one PC with four copies (`local/play_menu.sh 4`, `menu_flow_test.py --new --direct`): New Story,
  Marcus / Ed / two random survivors, no NPC double of any player on any machine; 20-minute soak with the three
  joining players taking turns, 10/10 cycles (knock-downs and get-ups included), all four games running; with
  the 4th player's game killed mid-session the other three kept playing (soak 4/4 afterwards).
- Not done: a player rejoining into a slot used before; loot screens of two players opened in the same
  quarter second share one panel capture.

### 2026-10-28 - joining player is Ed Jones; no doubled characters
- Character definitions: 146 entries at `[0x1EE25E4]` (0x34 bytes, +8 model path). Story characters have models
  under `story_characters/`: 0 Marcus Campbell, 116 (swat01), 124-145 (142 Ed Jones, 145 Maya Torres); the rest
  are generic survivors (`/human/body/`).
- In a new story Ed has a community record from the start (flags 0x20029) with an NPC body (`ULNPC01`); the
  host player's own record body (`ULNPC00`, Marcus) is hidden by UL on the host only. Body names are the same on
  both machines but do not follow the record index (record 16 had `ULNPC15`), so the client gets names, not indices.
- `maya_record.py` now binds the joining player to a chosen character: `pick_def(slot)` gives Ed for player 2
  while he has a record, else (and for players 3+) a random generic survivor no record or player uses; story
  characters are never picked. Ed's existing record is bound like a saved Maya record was (record +0x58 = her
  entity, `keep_active`, bad trait stripped) and his NPC body hidden, deactivated and moved under the map.
- `coop_setup.host_first` picks the character and the host's actual model (Marcus in a new story; a continued
  save may have the host on another survivor - the old code always loaded Marcus); the client loads both, and
  `hide_player_doubles` hides every `ULNPC*` body wearing a player's model on the joining machine.
- Verified: fresh New Story started from the menu: host Marcus, joining player Ed; both NPC doubles hidden on both
  machines before and after a 5-minute play soak (3/3 search + Take, she went down at 1 HP and got up, both alive).
- Not checked yet: story missions that script Ed's NPC (his body is hidden now) further into the story.
- Only one joining player is supported by the co-op layer (channel 2 everywhere); the character rule for players
  3+ is ready in `pick_def` but has nothing to run on yet.

### 2026-10-28 - co-op from the main menu (Host / Join with LAN search)
- The main menu (front-end screen 2) keeps its items in a static array: 11 slots of 0x90 bytes at `0x1EE81A0`
  (+0 accept callback, +4 other-input callback, +0xC inline text shown as is, +0x78 1.0), count `0x1EE87D0`,
  selected `0x1EE87D4`. The filler `0x161A1C0` (menu enter and its refresh timer) fills them and tail-jumps to
  the shared builder `0x1618E10`, which sends them to Flash every frame. The shared input handler `0x16144E0`:
  6 accept calls the item's callback, 9 back pops the menu (`0x16144FE`), 11/12 down/up.
- `coop_menu.py`: the filler's tail inserts "Co-op Play" before "Exit Game"; in submenus the items come from
  per-mode tables. Every custom item has one native accept stub (navigation done natively, other choices
  posted to a request cell the agent polls); back in a submenu goes to the parent instead of popping.
- The menu title is a localization hash (`0x1EE88C4`, looked up by `0x14F1C50`); a plain string showed
  `ENGLISH_LOC_MISSING`. The builder's two lookups go through a stub that maps our own hashes to our titles.
- `coop_agent.py` (one per copy/PC): starts the game with test saves and boot guards, serves the menu. Host Game:
  force `map %s server`, accept Continue / Start a New Game through the menu's own input handler, answer LAN
  probes (UDP broadcast on every local subnet and loopback), wait for the player. Join Game: lists the hosts
  that answer, connects, then both agents run their half of `coop_setup.py` over a TCP control channel
  (`coop_setup` is now split into host_first / client_first / host_main / client_main / host_last;
  `sync_models.sync_to` and `weapon_relay.give_host/select_client` take the cross-machine data as values).
- Pitfall found on the way: `user.cfg`'s `sv_bind` held the test PC's old LAN address; after a DHCP change
  the server was not created ("Failed creating network nub") and every hosted load crashed (null current
  level at `0x143E5CB` for Continue, null `[0x19F65F4]` at `0x92D5A2` for New Game) - also with the old
  scripted boot. The agent and `session_join_leave.py` now set `sv_bind` to the current LAN address at runtime.
- The joining copy's game framework exists at the menu already, and a copy started while another holds
  `class3.log` writes `class3(N).log`: the agents detect the join from the actor list (channel 1 and 2 actors).
- Verified on one PC: both copies started by agents, host via Host Game > Continue Story, joining copy found it
  in Join Game and joined; setup finished on both sides in ~1 min; short play soak 2/2 search + Take cycles,
  both games alive.

### 2026-10-27 - final soak; vehicles and HUD consumables checked
- 25-minute play soak on a fresh boot (`local/play_soak.py 25`): 19/19 search + Take cycles, she went down
  5 times under real zombie damage and got up each time (game restored 51 HP), both games alive at the end.
  The assert guard caught two asserts ("TDBTagSet duplicate buffer overflow", tag set full at 0x13E5CA6) that
  would each have quit the host before.
- Vehicles: the only vehicle near the start (pickup ULVEH00) is locked at this point of the story for both
  players (`Vehicle.EntryLogic -> Vehicle.CannotEnter.Locked`); interaction kinds next to a vehicle are 7 (enter)
  and 9 (trunk inventory, opens the journal). Co-op driving needs a save where vehicles are unlocked.
- Keys seen while testing on the host: 1-4 emotes, C dodge, E use. Using a backpack consumable outside a loot
  screen (`ul_player_item`) needs her journal / item slot on her machine, i.e. her UL character data there (not
  done; see next steps).

### 2026-10-27 - playable sessions: she goes down instead of dying, assert/entity crashes guarded, play soak
- `local/play.sh` starts a full session (keep-awake, both copies, co-op layer, relay). `local/play_soak.py` runs
  play-like cycles: she fights a fresh zombie group (mashing jump when down), then searches and takes at a
  cabin spot. Result on a fresh boot: 5/5 cycles, real zombie damage, she went down at 1 HP once, got up and
  was given 51 HP back by the game, every search and Take worked, both games alive. Earlier 20- and 12-minute
  runs: both games alive throughout, no asserts.
- Her HUD health arc follows her health (health is replicated).
- Death: at 0 health UL's actor is dead for good; raising it again asserts "SetHealth reviving a dead actor"
  (ULActor.cpp), and the assert quit the host. `local/downed.py`:
  - her health never goes below 1 (hook on ULActor::SetHealth 0x1461E40, her actor only). Like the host's
    player, she then lies down and gets up by mashing jump (Space); her input reaches her host copy through
    peer_input, and the game restores her health on getting up;
  - a finisher's death animation (`ReactionGrab.Dying/Death_*`) becomes the down idle after 4 s.
  - Forcing her straight to idle instead (state index write) skipped the down states' exit actions and she then
    took no damage at all; the same happened to the host's player after test resets.
- Asserts: the game's assert handler 0x58DF40 calls exit(1) right away (no dump). `crash_guards.assert_guard`
  makes it record and return (count + ring of caller and message), installed at boot on both copies.
- Silent host deaths: UL's EntityDbUpdate (0x1525610, EntityDb.cpp) asserted on a registered entity id the engine
  no longer had. `crash_guards.entitydb_guard` skips that entry instead (the assert block is rewritten in place).
- Search fixes from the soak:
  - her search focus now survives 1 s without a refresh: the start animation moved her and aborted it;
  - the local player's "down, press Space to get up" byte (0x1EA6DED, read by the finder through 0x146DBD0)
    switched interactions off for everyone, so while the host's player lay down she could not search. It is
    now swapped in her search context too.
- Test-harness notes:
  - Escape on her machine with no loot screen open opens the Game Menu, which swallows her input (the first
    soak ran with it open);
  - the client crash at 0x1527E74 (null client actor) is a consequence of the host going away.

### 2026-10-27 - looting works for the joining player: her backpack on her screen, Take and Use
- Verified on her machine: search, then her loot screen shows the container on the right and HER portrait and
  backpack on the left (rifle, axe, snacks, painkillers, weight). Take moves the item into her backpack (stack
  1 -> 2, 11.9 -> 12.2 lbs, container "Nothing left"). Use consumes it (container emptied, backpack unchanged).
  Everything is applied on the host to her survivor record and the host's container.
- Her backpack panel: the left panel is drawn by 0x1653F60 from the current player's record with Flash calls
  PIShow, PIFocusSet, PICapacitySet, PIPortraitSet, PIWeightSet, PIItemSet/PIItemClear per slot, PIBackpackSet,
  PIRucksackSet. Her machine has no record, so:
  - the host runs 0x1653F60 for her record while Flash Invoke (0x6E37D0: name, variant array, flag) records the
    calls instead of making them;
  - `hud_relay.py` sends them as text (RMI `ClCoopLootPI`, in parts, before the container);
  - her machine's 0x1653F60 is replaced by a replay stub (cvar `coop_loot_pi`).
- Flash argument arrays: data with the element count at [data-4] (& 0x7FFFFFFF) and 8-byte elements
  (type info, value). Types: int 0x1970A88, bool 0x195F30C, float 0x195F32C, string 0x195F1EC (char* with a
  CryString header; refcount at -0xC, -1 = static, never freed), 0x195F2EC. The array object also holds
  converted values at +0xC and flags at +0x14: Invoke converts again only when bit 0x4 is set, so a reused
  array must set it before every call (the first replay showed every call with the first call's values). The
  object must be built with its constructor 0x5CD0A0 (a zeroed one crashed her machine inside Invoke).
- Use (flags 0x80) runs the record's UseItem (0x1548EE0), which only applies an item to a record with flag 0x4
  (+0x15C; his record has it, her survivor record does not). The host's action handler sets that bit on her
  record for the duration of a loot action and clears it again (her record stays 0x29, her state machine
  stays).
- Test notes: a host container menu closed with Escape can leave bit 0x8 of [0x1EA7FFC] set, after which
  ShowContainerMenu only updates data and opens nothing. `local/host_her_screen.py` opens the host's loot
  screen for her record (debugging), and `local/ui_capture.py` logs Flash calls.

### 2026-10-27 - Take from her loot screen works; boot crash on her saved record fixed
- Verified: on her machine Take (Space) moves the item from the host's container into HER backpack on the host
  (shown by the host's loot screen with her record as current player: her portrait, rifle, axe, snacks,
  painkillers, plus the taken snack), and her screen refreshes to "Nothing left. / Empty".
- How:
  - her machine's menu callback 0x145B520 is replaced by a stub that sends `lotc:SS:FFFF:DDDDDDDD`
    (slot, action flags, data) to the host as a chat message (new tag in `peer_input.py`);
  - the host runs UL's callback in her search context, with the client-actor getter returning her actor
    (search_ctx "force" cell) and the current-player record getter 0x14B0420 returning her record handle
    (0x14AF690 by entity);
  - the callback's own refresh (0x145CD60 with flag 1) goes back through the loot relay. Her machine passes
    flag 1 when the node is already the one shown, else an emptied container closed instead of redrawing.
- Her machine has no UL character records at all (the record table is empty, current-player handle 0xFFF8000),
  so the left panel of her loot screen shows a default survivor with placeholder slots. Next: show her
  backpack there.
- Use (Q) on her screen does not reach the menu callback (Take does). Her machine's UI probably checks the
  current player before calling back, and she has no record there. Same missing piece as the backpack panel.
- Intermittent host crash while the saved world loads (dumps 15:25, 16:07): the per-record update asks the body
  actor of her saved survivor record for its state machine (0x154832C -> 0x1461960) without a null check; the
  record names the previous session's player entity, which no longer exists. `crash_guards.record_body_guard`
  (null body skips that part) and `collision_guard` are now installed at boot by `session_join_leave.py`, right
  after the save redirect (both report True).

### 2026-10-27 - her loot screen opens on her machine
- Verified end to end: her full search on the host -> the loot screen ("Found some remaining items", Snack) opens
  on her machine, not on the host's. Still to do: the left panel shows the client's own current-player record (a
  default survivor with an empty placeholder backpack), not her backpack, and Take/Use/Close are not relayed back.
- How UL opens it: `0x145CD60(node, flag)` needs node == the last focused node [0x1970E04]. It reads the
  container entry from the table at 0x1EA64A8 (20 x 0x68 bytes: +0 node, +4 count, +8 item handles,
  +0x48 counts, +0x58 kinds), picks the status text (string ids 0x20B..0x213) and calls ShowContainerMenu
  0x148BAE0 with the menu callback 0x145B520 (Take/Use). Her manager reaches it through 0x1630C20 -> 0x145EE80
  (SetInput when the node is already searched, or right after a search completes).
- The client has the same node ids and UI. Writing the host's container entry into the client's table and
  calling the client's 0x145CD60 shows the same screen there.
- `local/loot_relay.py`:
  - host: 0x145CD60, called while her search context is in (search_ctx override cell set), copies the
    container entry into a ring and returns without the host UI;
  - `hud_relay.py` sends it as RMI `ClCoopLoot("seq:hex")`;
  - client: the Lua handler sets cvar `coop_loot`, and a stub chained on pass 1 writes the entry into the
    client's table, sets its last node and calls 0x145CD60.
  - New RMI in both `singleplayer.lua` copies (handler placed before the door marker).
- Her search-node perception: candidates come from her perception query (actor +0x1170 list, entries in the
  perception manager 0x1634090 at +0x6B1A8 + idx * 0xAC, results written to blackboard +0x204 + i * 0x6C by
  0x16493E0). The query's transform is set by a state-machine action (0x170B340). When it was stale she saw no
  node.
- Her host copy got stuck in `Combat.Idle.BluntLight` (weapon out) on fresh boots and after fights, while her
  client was back in Observer. The exit waits for combat tags (0x7E...) that the host only clears for its own
  player, and those tags (0xCD) block the search. `search_ctx.stand_down` (from `hud_relay.py`): after 3 s in a
  combat idle state with no button held on her machine, her copy returns to idle and the combat tags
  (0x2C, 0x09, 0x7E, 0xCD, 0x199) are removed. Verified: Observer about 4 s later.
- Her search could lose focus while her LowVert search animation moved her (node out of reach after ~1 s). Her focus
  now stays while her manager is searching (state 2/3).
- Crash found and fixed: `peer_melee` and `remote_fx` baked her state-machine record address into their stubs.
  After UL re-created her state machine (new address) the stub wrote through freed memory (host access
  violation in the stub). Both now read the record from her actor (+0x1110) each time. Host crash dumps are
  parsed with `local/dump_exc.py` (exception, registers, stack code addresses).
- Test notes: node state persists in the test save (a depleted node stays depleted, status 0 from 0x1451B30).
  For tests, node flags at [[0x1E80E80]+0x20]+node = 0 make it searchable again. Nearby fresh nodes: 1467
  (0x5BB) LowVert near 3281.5,3410 and 0x5BA. Zombies at the cabins knock the host player down during tests.

### 2026-10-27 - her search works on the host, completion opens the loot screen (on the host's screen)
- Verified: at a search spot she holds use on her machine, her host copy runs `Interact.Search.Logic.ContextTest
  -> Search.Normal.Ground.Start -> Loop`, progress accumulates while held, release gives `Stop -> Observer`, and a
  full search completes (manager state 4); further holds on that node stop at once. Her own screen shows her
  crouched searching (state mirroring from the door work). The loot screen then opens, but on the host's
  screen, with his portrait and backpack. Next phase: that screen on her machine.
- Why her player state machine dropped at random (the earlier "trait 492" theory was wrong): the per-record update
  0x1547920 also streams each survivor's NPC body. From distances it sets "hidden"/"inactive" and then
  releases the body's player state machine (0x1466AD0 -> 0x1636780) or creates one (0x1463C20). Her record's
  entity (+0x58) is her player actor, so depending on where she stood UL "despawned" her. `maya_record.
  keep_active` forces both flags off for her record at 0x15482C9; a lost state machine also comes back by itself.
  Caught with a logging detour on 0x1636780 (`local/sm_release_log.py`, installed by `coop_setup.py`).
- UL's search system is a singleton for the local player. The manager is the object at [0x1EED390]
  (0x7C bytes, ctor 0x162F880; +0x40 focused node, +0x6C state 0 none / 1 focused / 2,3 searching / 4 done /
  5 stopped, +0x60 time searched). Two more globals belong to it: the last node [0x1970E04] and the focus
  timeout [0x1E81F78], both written by the node candidate check 0x14569A0 that the finder runs for each player.
  - The finder runs for both players, so the player with no node in reach reset the focus (her kind flickered).
  - Only the local player's update feeds use input to the manager (0x1468A03: actor +0x10A4, null for her).
  - Tags and the search handle go to the client actor (getter 0x6683A0, used at 10 places in the manager code).
    One of them is tag 0xC6, which ContextTest needs.
- Fix `local/search_ctx.py`: she gets her own manager, focus and timeout, swapped into those globals:
  - around her finder call, then her use input (stand-in input bytes +0xFD/+0x117) is fed to her manager;
  - once per frame for her manager's tick, after the host's, including her focus timeout;
  - the HUD progress bar only for the host's manager;
  - the client-actor getter returns her actor when called from the manager code while her context is in.
- Actor tag sets: actor +0x10B0 (512 bits; tag = 64 * qword + 63 - bit, `local/tags.py`), copied to blackboard
  +0x12E0 each tick (0x1461E00); added/removed by CPlayer virtuals 0x1461DE0 / 0x1461E20 when states are entered
  and left. Condition type 19 tests them; type 40 checks a blackboard slot; type 92 is a "held for N s" timer;
  type 56 is an edge detector; type 17 combines other results (bit 0 OR, bit 1 negate).
- Test-harness traps found on the way, all from forcing her state index (`goal_check.stand_up`,
  `peer_input.reset_state`), which skips state exits:
  - stale tags (e.g. 0xCD, which blocks the search);
  - a stale search handle at actor +0x153C (the manager then saw "already searching"). `reset_state` now clears
    the handle.
  Also: writing a string over the blackboard hand-item pointer (+0x1458 is a `char*`) crashed the host. The
  "Fists" and trait differences were not needed.

### 2026-10-27 - looting for the joining player: findings so far (in progress)
- Goal chosen by the owner: the full loot screen on her machine (search, loot screen with take/use/close, her own
  backpack), not a simplified version.
- Search spots are not entities. The host's interaction finder `0x1464000(actor)` runs every frame for both
  players (actor update, gated by actor vt+0x130) and writes the interaction kind to blackboard +0x1524 (prompt
  icon 0x406000N): 1 = barricade, 3 = door, 5 = search, 8 = something else seen in cabins. Search spots found
  by sweeping the host player with the kind (`local/search_spots.txt`; a cabin at about 3246,3396 facing 3.14 and
  3276,3406 facing 0). A full hold (`sr_searchDurationNormal` 10 s; `sr_tapWindowDuration` 0.25) runs
  `Interact.Search.Normal.<Ground|LowFlat|...>.Start/Loop/Stop`, then opens the loot screen (journal UI,
  `Item.Journal` state): his backpack on the left, the container's items on the right, Take/Use/Close.
  `sr_debug 1` overlays the node id and the container model's states (base/full/searched/depleted).
- Her host copy gets kind 5 at a search spot, and her use input arrives, but she never enters
  `Interact.Search.Logic.ContextTest`. Forced into it, she drops back to idle at once. Condition type 96
  (0x16FEE50 -> 0x1716740 -> 0x14A5880 -> 0x15325B0) checks traits: record +0 holds 36 trait indices into
  [0x1EE2620] (0x34-byte entries, data id at +0). The host player's record has 0x06090001 (index 492; Ed has it
  too) and 0x060D000A (index 208); her record, created as a random survivor, has neither.
- Adding trait 492 to her record made UL release her player state machine (removed from the extra list by
  0x16366B0 via the release function 0x1636780(rec), callers 0x1461CC1 / 0x1466B6C / 0x14682D7, actor +0x1110 =
  its record). Redirecting her entity to the host player's record in 0x1716740 (`maya_record.share_traits`,
  currently off: `SHARE_TRAITS = False`) dropped it too in one run but not in another; not understood yet.
- Test harness: forcing a state every few ms (`local/search_conds.py`) crashed the host once (entity-query code);
  don't. Teleporting her into small cabins pushes her up to ~2 m; check her host position.
- Next: find why the trait makes UL release her state machine (probably a player/NPC role decision for records
  with that trait) and keep it, then the search result -> loot screen path for a non-local searcher, then the
  client side (loot screen and backpack).

### 2026-10-27 - her door animation on her screen; walking through doors
- No door animation on her screen: a state written into her client state machine neither switches the animation
  graph's "StateMachine" input on (0x1463510) nor starts the state's clips (entry actions skipped). The client stub
  now calls 0x1463510 for door states (0x1463550 for idle). The zombie_sync hook on 0x820BC0 also logs the players'
  clips ("Dude..." entities), and `PeerStates` sends each player's latest `Interact_door_handle_*` clip with the
  state ("...|clipHost|clipPeer"). The client handler starts it on "Dude" / "Dude(1)" 60 ms later. Her screenshot
  mid-`CloseFar` shows her stepping into the doorway, arm out; standing idle she is upright.
- Walking through doors: (1) the relay sent every `Door_N` with a position, but when the pool reuses a spot its
  old entity stays there (two doors on one spot), and a stale one could carry an open state over a closed door.
  `placed_doors` now uses the door system's placed list (entity pointers at 0x1E7E750, count 0x1E7E760). (2)
  Client collision built by a timer after the clip sometimes came from a mid-animation pose (7/9 or 0/9 rays). A
  closing/closed door is now reloaded (default pose = closed) and physicalized at once, then the clip plays.
  `local/door_audit.py` (every placed door: host flags, client state and physics, rays on both machines): 0
  mismatches, also after a pool shuffle. Walk: closed door stops her 0.43 m short; after she opens it she walks
  2.6 m through.
- Test harness: a door use pressed right after a teleport (before the door context and, for the host's player, the
  interaction kind +0x1524 = 3 were set) did nothing on slow frames; `door_use.py` now waits for them.

### 2026-10-27 - test saves separated from Steam; Maya clones; stuck after door use
- **The test copies were saving into the owner's Steam save folder.** The game saves `Class3.0.ulsave`,
  `Sandbox.0.ulsave` and `UserStats*.cfg` into the folder from `ISteamUser::GetUserDataFolder` (vtable slot 6):
  `Steam\userdata\<account>\241540\local`, the same folder the owner's Steam copy uses. `sys_user_folder` /
  `%USER%` only cover profiles. `local/save_redirect.py` replaces that vtable slot in each test-copy process
  (per process, nothing on disk) with a stub returning `local/test-saves/host|client`. `session_join_leave.py`
  installs it right after each process starts, before any game is loaded. The path is only asked for when a game
  is loaded or saved; a menu-only boot has no cached copy. Current save copied there once; the Steam save is no
  longer written (timestamp unchanged after reboots and sessions).
- "Other NPCs look like Maya": every boot `maya_record` created a new Maya record, and the save kept them, so their
  NPC bodies walked around as Mayas. It now reuses a saved Maya record. Extra ones become unused generic
  survivors with their bodies re-skinned. Her own old body (`ULNPC<record>`) is also hidden on the client, and
  setup runs it before the client model sync.
- "Stuck in place after a door": two causes on her machine, both from mirrored door states written into her
  client state machine (their state actions and exits don't run):
  1. The animation graph input "StateMachine" stayed on (actor +0x1114 = 1; on: 0x1463510, off: 0x1463550), so
     the graph kept waiting for the state machine and she could not move at all, even on open ground. The
     client stub now calls 0x1463550 whenever it writes the idle state (`peer_input.reset_state` too). 8 door uses
     in a row: she walks 3.6-3.9 m after each.
  2. ULActor stamina request stacks (+0x12EC cap 4, +0x1300 cap 8) filled up. The assert "SM stamina tank rate
     requests exceeded stack space" (ULActor::ProcessStateMachineBehavior) then quit the client (caught by the
     exit trap). The client stub resets a stack one short of full.
- Host side: her host copy could loop in `...Locked.Loop` (locked door rattle) forever after she released the use
  button (her stand-in input showed it up; her client left at once). `door_sync.end_locked_loop` moves it to
  the matching `...Locked.End` after 0.5 s with the button up.
- Test harness: teleporting her to a `door_pose.txt` pose can embed her in place even with no door involved; a
  stuck walk test there is not a game bug. Test movement on open ground after the door use.

### 2026-10-27 - door gaps closed: host collision, stuck pairing, Marcus turned into Maya
- Host collision for doors she closes out of his camera's view (the gap in the previous entry): the host never
  computes the closing pose, so the door's static physics stayed where the open leaf was. `settle_stalled` now,
  for a stalled clip that ends closed, reloads the door model (default pose = closed) and physicalizes it static
  before the jump-to-end replay; the game's finalizer then sets state and part flags as usual. Four out-of-view
  toggles: closed 9/9 rays on both machines, open 0/9 on both; the host player's in-view use of the same door still
  works. Not fixed by `ENTITY_UPDATE_ALWAYS` + `Activate` + `ForceCharacterUpdate` together either.
- Doors stuck in "clip playing" (0x4) from before the hook (seen on a fresh boot: 0xC) are settled too, using the
  clip of the state they are heading for.
- Stuck door pairing: a player whose state machine left a door state without DoorRelease (action 55: 0x1440960(actor,
  door) + blackboard +0x14E0 = 2) stays in the pair list 0x1E7E754 and never gets a door context again (her
  host copy stuck in Locked.Loop, then reset). `release_stuck` releases players paired but outside Interact
  states for 1.5 s.
- Bug: `maya_record.py` used the re-skin function 0x15472D0, which also re-skins the current player: the host's
  player had been showing Maya's model (both machines) since the record was introduced. It now writes the
  definition index (+0xF0) directly; models restored with `coop_setup.load_model`.
- The Radio Assistance menu that blocked her input yesterday could not be reproduced (held "use" 0.3-2 s, no
  target); unexplained. Test harness: `door_walk.py` now forces focus (ALT tap) and starts 0.15 m above the floor;
  `door_full.py [rounds] [door]` (pool names change between boots; Door_5 was locked in this save).
- Fresh-boot `door_full.py 2 Door_0`: her states mirrored, door state/seq equal, collision equal, walk blocked
  closed (0.49 m short) / through when open.

### 2026-10-26 - doors fully on the joining player's machine: animations, collision, state
- Initial state: door spots keep their state in a flags byte (`[0x1E7E770][spot]`: 0x8 open, 0x4 clip playing,
  0x2 other side, 0x1 locked); placed doors: count 0x1E7E760, spot ids `[0x1E7E758]`, positions `[0x1E7E74C]`;
  door types `[0x1E7E788]` (8 x 0x34: clip names, model at +0x28). Doors placed before the hook get the clip of
  their current state (`door_sync.state_clip`), so every door shows open/closed correctly from the start.
- Stalled doors: a door clip only advances while the host's camera renders the door. A door she used out of his
  view stayed at flag 0x4 with clip time 0 forever, and then no actor could use it again (the door search accepts
  flags & 0xC in {0, 8} only). `door_sync.settle_stalled` replays a clip that hasn't moved for 1.2 s with
  jump-to-end; the game's finalizer 0x143FE20 then sets the state bits and collision (0x143FB50). Not fixed by
  `ENTITY_UPDATE_ALWAYS`, `ForceCharacterUpdate` or `ca_ForceUpdateSkeletons` (cvar block 0x19627F8 +0xD8).
- Collision on her machine: the client handler physicalizes closed doors (`Physicalize(0, PE_STATIC)`, 2 s after a
  normal closing clip / 0.15 s after a jump, so the closed pose is used) and removes physics from open ones.
  Ray probe through the doorway (`local/door_ray.py`): closed 9/9 hits on both machines, open 0/9. Walking with
  real input (`local/door_walk.py --flip`): open -> she walks through (5.4 m past), closed -> stopped 0.47 m short.
- Players' door animations on her screen: the client has no door context, so the host mirrors each player's
  animated `MachineDriven.Interact.Door.*` states (not the logic states) through RMI `ClCoopPeerState` -> cvar
  `coop_peer_state` -> a stub chained in front of the client's pass-1 detour that writes the state index into her
  record or the client's copy of the host's player. Verified: her client went `OpenAway` / `CloseFar` /
  `CloseClose` with the host, and the client's copy of Marcus `OpenStart -> OpenAway -> idle`.
- Latency: the host's per-client send budget (`sv_bandwidth`, default 30000 bit/s) was full of co-op RMIs, so
  door changes reached the client ~3.4 s late; `coop_setup` sets 300000 -> 0.4-0.7 s.
- Test-harness notes: the client's "use" key with no client-side target opened the Radio Assistance menu
  (blocks input; Escape closes it). Teleports to exact floor height can leave her stuck (lift 0.15 m).
- Known gap: on the host, a door she operated out of his camera's view is settled by the jump-to-end replay, but
  its host-side collision does not match the closed pose (rays pass; the host never rendered the pose). Her
  machine is correct. Affects host zombies and the host player at doors he never looked at.

### 2026-10-26 - doors on the joining player's screen (door relay)
- The door system plays door clips through 0x143F0A0 (thiscall door entity; clip name such as
  `door_door_handle_left_open_away` / `..._right_close_close` / `..._close_far`; flag = jump to the last frame,
  used when a spot is set up). Clips are started by id (skeleton anim vt+0x28) with "repeat last key", so the last
  frame is the door's open/closed pose. Door models are animated `.cga` characters (slot 0: `animated_door`,
  `anmated_door_glass`, `animated_door_firedoor`, ...; path at character +0xB8). Lua: the script table's
  `__this` is the IEntity pointer.
- `local/door_sync.py` + new RMI `SinglePlayer.Client:ClCoopDoors` (loose `singleplayer.lua`, both installs):
  the host hooks 0x143F0A0 (ring of entity, flag, clip); `hud_relay.py` sends the placed doors every 0.5-3 s
  (`index,pos,angles,model,clip,flag,seq`, split into parts). The client handler loads the model on the same
  `Door_N`, places it and plays a new clip when the sequence changes (jump-to-end clips at speed 100). Doors the
  host no longer places go back under the map.
- Verified on a fresh boot: the client's door pool matches the host's (9 doors, same positions and angles).
  Marcus opened the restroom door, and it showed open on her screen. She pressed the interact key at it: her
  host copy ran `Interact.Door.Front.CloseClose`, the client handler applied the close clip (seq 1 -> 2), and
  her screenshot shows it closed. `local/door_check.py` prints the host clip/seq and the client's applied seq
  for the door at `door_pose.txt`.
- Not yet: her own door animation on her screen (her client state machine stays idle; the door interaction runs
  on her host copy), and collision for closed doors on the client (the client door has no physics). Doors set up
  before the hook was installed show their default pose until their next clip.
- `coop_setup.py` now runs `maya_record.install` (her survivor record + door-system registration).

### 2026-10-26 - doors are a host-side entity pool; the client has none
- `Door_N` entities (class `ULDoor`, native, empty script table) are a pool that UL's door system assigns to door
  spots near the player: the same name moves between doors (Door_2 was at 3100,3323, later at 3175,3366); unused
  ones sit at 0,0. On the host the assigned ones have one geometry slot and their open/closed angle; on the
  client every `Door_N` is at 0,0 with no slots (no UL world tick there). So her screen shows no door state at
  all, for either player's doors. Mirroring needs a door relay like the zombie one: for each assigned host door,
  position, angle and geometry (slot 0 static object), applied to the client's pool entities.
- Unlocked doors found for tests: Door_5 at (3173,3349) at the time of the test (the host player opened it:
  `Interact.Door.Back.OpenStart -> OpenAway`). Door_1 at (3102,3339) is locked. Pool names change, so look doors
  up by position.

### 2026-10-26 - a real survivor record for her; her host copy uses doors
- Record creation path found: console command `com_addMember` ("Add a new character to your community",
  handler 0x14B6BF0) takes the first free record of the character table ([0x1EA83B8], 100 x 0x178; free =
  flags +0x15C bit 0 clear, handle +0x48 = 0x0FFF8000), initialises it (0x15473C0 random character; with a name
  argument 0x1547580 with a data id), adds it to the community (flags 0x29) and spawns its NPC body `ULNPCnn`.
  The name argument is a `Table.Entry` data-id name, not a character name; character definitions are not in
  that registry, so re-skinning is used instead: 0x15472D0 (thiscall record, definition index).
- `local/maya_record.py`: com_addMember -> re-skin to definition 145 (Maya Torres) -> record +0x58 = her entity id
  -> hide/deactivate/move the spawned body (otherwise it floods the log in her place) -> register her with the
  door system (`interact.register`). Unlike the earlier borrowed "known" record, her player state machine stays
  in UL's extra list, and the "Entity <her id> is not part of the community" flood stops (all three log sites in
  0x1716740/0x17167F0/0x1716880 go to 0; they find records by iterating active records and comparing +0x58).
- Result (fresh boot): at the door pose the host player gets (`door_pose.py`), her host copy now gets the door
  context (blackboard +0x14D8 = 383) and runs `Interact.Door.Front.ContextTest -> OpenStart -> Locked.Start ->
  Locked.Loop` on the host (Door_1 is locked) when she presses the interact key on her machine
  (`local/door_use.py client`). Her own screen does not show it yet: the client runs no door system, so her
  states and the door state must be mirrored from the host.
- In a long session the same setup gave no context: the door routine (0x1442B60, per-actor loop over 0x1E7EBB0)
  visited her entry but her distance test to every door candidate failed. Not explained; it worked again after a
  reboot.
- Tooling: `Proc.count_exec(..., when=(esp_offset, value))` counts only hits where a stack slot (e.g. a loop
  index) matches. Fixed a crash in `count_exec`/`watch_writes`: when the exception handler resumes a thread,
  Windows restores the debug registers from the exception context, which could re-arm a breakpoint after it was
  cleared. The handler would then be removed and the next hit was an unhandled breakpoint exception that killed
  the host (dump 0x80000004 at the breakpoint address). The handler now clears Dr7 itself while disarming.

### 2026-10-26 - host out-of-memory after ~7 h idle: our own leak, fixed
- The host died after sitting idle for about 7 hours with "Memory allocation for 54884 bytes failed" (game log;
  dump save failed for the same reason). Cause: `sodlib.Proc.lua()` / `console()` / `run_code()` allocated a
  buffer in the game per call and never freed it. Each `VirtualAllocEx` reserves at least 64 KB of the 32-bit
  process's address space, and `hud_relay` calls `lua()` every 0.5 s (about 7.5 MB/min). This probably also
  explains the older "long sessions degrade" observations.
- Fix: `Proc.free()` and the per-call buffers are released after the call (kept only on a timeout, when the game
  may still use them). `local/vm_watch.py` (address space per process): after the fix, flat at ~636 allocations
  / 1.3 GB on the host over 5 minutes with the relay running.
- Side note: the host log grows by gigabytes per day from "Entity <her id> is not part of the community"
  (every frame); it goes away once she has a survivor record.

### 2026-10-26 - test-harness side effects in zombie fights (not co-op bugs)
- `goal_check.stand_up()` (forcing her host state machine to idle while she is knocked down) leaves the zombies
  believing she is still down: they cycle `Combat.*.Strike.TargetDown.*` and walk loops without moving, and her
  health does not change. Moving her a few metres frees them (they then wander off). Prefer a fresh group and
  `SetHealth` only (`local/reaction_ab.py`, `local/contact_ab.py`). `stand_up` stays for the fatal-landing case
  only.
- `SetHealth(100)` reads back 90 / 80 after heavy damage: max health drops with injuries (game rule), not a
  sync problem.
- Zombies teleported onto her show `Reaction.Basic` (light hit reactions) for a while; around the host's player
  they never do. In steady state (15 s later) there were 0 reaction samples and `peer_melee` fired 0 times, so it
  is the arrival push of her teleport, not phantom hits. zombie_targets' injected "Hero" contact makes no
  difference (A/B, radius 1.4 m vs 0).
- Fresh-boot baseline (`reaction_ab.py`): the host's player among a group goes 90 -> 1 in 15 s; she goes
  100 -> 74 with him 20 m away. The zombie spawner around the test sites runs dry after ~30 min of tests (reboot).

### 2026-10-26 - far combat works; the "never attack" result was a test artifact
- Fresh boot, `local/far_strikes.py` (her among a group, host's player moved, 4 s settle, 15 s): with him 300 m
  away her health went 100 -> 86 and 100 -> 72; with him 20 m away 100 -> 17 and 94 -> 2. Far fights are weaker
  because fewer zombies stay around her (about half the zombie-samples within 4 m), not because combat is
  blocked. The damage is the game's own: zombie_targets' fallback strike hook fired 0 times in all runs.
- Teleport hitch: for 2-3 s after the host's player jumps 300 m the host streams the new area and runs at about
  8 fps, and each frame advances at most about one normal frame of game time, so every zombie's animation clock
  (UL time-in-state, blackboard +0x1340) runs at about 0.12x. Measure only after a settle delay
  (`local/anim_rate.py`). Animation-state time: actor vt+0x158 -> vt+0x16C -> state +0x28, written each frame
  at 0x81FBD4 from the animation's normalized time.
- The earlier "0 strikes in 20 s" run was in a long session where her host state machine was stuck in
  `Stunned.Basic.Down` and her health read a constant 60/80 even after `SetHealth(100)`. Reboot before judging
  combat.
- Console variables `ca_ForceUpdateSkeletons`, `g_forceFastUpdate` and `es_VisCheckForUpdate` do not change at
  runtime (`System.SetCVar` and the console both leave them at the defaults 0/0/1), so the earlier "not
  caused by" checks with them proved nothing.
- Condition snapshots: the UL flag buffer (0x1F36D60) is shared by all records. Each evaluation writes only the
  first `[0x1F36D58]` flags, and which condition sits at which index depends on the active states. Compare by
  (condition type, data), not by index (`local/zcomb_conds.py`; `cond_snap` now keeps the flag count and
  arguments).

### 2026-10-26 - final-build zombie checks, animation NaN guard, far-combat gap
- `local/goal_check.py` on fresh boots: her kills counted for her (+1, host +0), the host player's weapon kills
  counted for him (+4, hers +0), zombies take him from 90 to 1 in 15 s, and her from 100 to 64 in 9 s with him
  20 m away.
- Test-harness trap: teleports can leave a player in a fatal-landing state (client) / lying downed (host); zombie
  hits then never apply and `SetHealth` reads oddly (she read 80). `goal_check.stand_up()` resets both players'
  state machines to idle and heals them. Earlier soak "80 (from 100)" readings were this, not damage.
- New client exit: assert `!_isnan(val)` in `CAnimationGraphState::SetInput` (0x7C9F60). `crash_guards.
  anim_input_guard` (both machines) replaces non-finite values with 0.
- (Resolved in the newer entry above: a test artifact.) With the host's player 300 m away, zombies targeting her walk up and stop 2-4 m away in Basic.Move/Taunt
  states and never enter Combat (0 strikes in 20 s; with him at 20 m they close to ~1.5 m and hit her). Not the
  physics range (es_MaxPhysDist 2000), skeleton updates (ca_ForceUpdateSkeletons 1), g_visibilityTimeout (already
  0) or g_forceFastUpdate 1. zombie_targets' strike reach (1.8 m) is not the limit either (2.6 m: no strikes).

### 2026-10-26 - repository housekeeping
- Moved the old handoff notes and chat backup from the parent folder into the repo root (excluded from git via
  `.git/info/exclude`), deleted all logs, created this file. `restart_session.sh` now starts `dismiss_gpu.ps1`.

### 2026-10-24 - host crash root cause: minimap renderer races the name table
- `Proc.trap_faults` caught the host crash on the render thread: access violation in the engine's global
  name-table hash map (0x112700E), reached from the minimap renderer 0x1380B00 asking the UI manager for
  "Class3HUDRadar" every frame (0x1380D7F). The table is not thread-safe; the main thread changes it constantly.
  Explains the hangs (self-linked bucket node), heap-corruption exits (0xC0000374) and dump-less crashes.
  A debugger hides it (timing).
- Fix `crash_guards.radar_cache`: element looked up once on the main thread, renderer uses the pointer. Two
  fresh-boot 40-minute soaks clean on both machines (before: crash within about a minute).
- Not the cause: relay rate/size, pooled zombie characters shared by two slots (engine returns early).

### 2026-10-23 - zombie animation on the client, position matching
- Client zombies were in T-pose: only the host runs their UL state machine. Host clips (0x820BC0 by name) are
  relayed and played on the client; entity ids/names differ between machines, so zombies are matched by
  position; client ghosts the host removed are hidden. A NaN-position assert exit on the client led to
  `crash_guards.nan_guard`.
- Joining player once showed a zombie body on her own screen: the old model relay matched by entity index.

### 2026-10-23 - zombies around her at any distance
- Cull at 0x13AD9C6 (175 m from camera) now uses the nearer player; population pass run a second time around
  her (<=300 m from camera); physics range raised (es_MaxPhysDist 2000). Spawn density is per map area.

### 2026-10-23 - test environment
- Laptop sleep/Modern Standby and display-off break or kill test sessions; run `keep_awake.py`.
- Never place 5-byte hooks inside functions (branch targets); count with `Proc.count_exec` instead.
