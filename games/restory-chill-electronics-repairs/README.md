# ReStory: Chill Electronics Repairs (2026)

| | |
|---|---|
| Engine | Unity 6000.3.10f1, Mono (scripts in `Restory_Data/Managed/Restory.Assembly.dll`), Zenject, FullSerializer saves, Steamworks.NET |
| Versions tested | Steam 1.0.033r (Fuzzby update, 2026-10-27): `Restory.Assembly.dll` SHA-256 `98bcbc08ec102865787fafee0ee8f53a0f82b81ce2b6b7d9d9e33335421ed66a`, `Restory.exe` SHA-256 `ad7e0d4f72226c5741250658762a4b2f2688a9bea4288ba218ec80d7726630c6`, build-guid `eb5bb3732fca4140921c1f890d4ba957`. Earlier work was on 1.0.015r. Windows 11. |
| Projects | [ReStory Multiplayer](https://github.com/teecil/restory-multiplayer) (BepInEx 5 plugin: co-op and versus), directory entry "ReStory Multiplayer" |
| QA level | automated (two real game copies driven by bots, see QA), plus human playtests on two PCs |
| Last updated | 2026-10-30 |

## Where things stand

Two players can play the career together in the host's save: one shop, one wallet, one parts stock, shared mail
and quests, each with their own bench, and the day only ends when both agree. Two players played it over the internet
on two PCs (through Porthole) for several in-game days (build 1.0.015r), and their sessions found most of the hard bugs; the automated
two-copy suite passes on the current Steam build 1.0.033r. A versus mode (both race to assemble the
same device) works too. The Steam lobby route exists; there is no two-account session yet.

## Existing work

None found: web search for the title with "multiplayer", "co-op", "mod", "Nexus Mods" and "GitHub" returned only
news and store pages, and this hub had no entry, issue or PR for the game (2026-10-30; Steam Workshop and Discord
not checked). The game is single-player ([Tom's Hardware Italia](https://www.tomshw.it/videogioco/restory-chill-electronics-repairs-i-consigli-per-iniziare))
and has no netcode.

## How the game works (for co-op)

- **Scripts**: all gameplay in one Mono assembly; decompiles cleanly with ILSpy. Services are Zenject-bound
  MonoBehaviours or plain classes; most state lives in services with `CaptureState`/`RestoreState` (the save
  system), which made "send the service's saved state" the main sync tool.
- **Saves**: `%USERPROFILE%\AppData\LocalLow\Mandragora\Restory\SaveData`, `Story.slot<n>.release-No-<i>.restory`
  (FullSerializer JSON, game assets written by ID). 1.0.015r replaced profiles with save slots; "Continue" loads the
  last used slot's highest iteration. Steam Cloud syncs the folder (since the Steam install creates
  `steam_autocloud.vdf`).
- **Objects**: room objects in `InteractiveObjectRegistry` (object -> info), devices in `DeviceRegistry`, each
  with a unique id. Drops go through `DragResolver.ResolveDragResult`, but many moves don't: the bench is filled
  by `DeviceService.PlaceDeviceContainer` (a button, unpacking), services create objects on their own (customer
  devices, bills, cash, courier boxes).
- **Boxes**: the courier's box (`DeliveryService`), the customers' box (`DevicesFromNpcsService`) and the personal
  box are held by their services at a spot and keep content; a box carried away folds (`FoldableObject`) and is
  empty.
- **Cash**: the sum is not in the object's data but in `CashMoneyService` (`MoneyAmountHeld`).
- **Quests**: `ToDoListService` items, added by dialogue scripts (`ToDoList_AddItem`), each closed by a handler
  listening to local events; completing one can schedule a story visit.
- **Time and day end**: `TimeSystem` / `MainDayTimeSwitchingService`, the bicycle (`DayEnderFromPlayerInteraction`),
  the end-of-day screen (`GUI_DayEndWindow`, the stamp). Pause = the pause menu shown (`GamePauseState`, time scale 0).
- **Steam**: the game initializes Steamworks.NET itself and runs callbacks every frame, so a mod can use lobbies
  and `SteamNetworkingMessages` directly. A copy started outside the Steam folder needs `steam_appid.txt` or it
  restarts itself through Steam into the real install.
- **Analytics**: the game runs Unity Services Analytics (`RestoryAnalyticsService`) and the engine writes session
  files to the game's LocalLow folder; test runs should switch them off.

## What we changed

The mod is host-authoritative: the guest's actions go to the host, which applies them and sends the outcome back.
Highlights, each with the bug that led to it:

- **Transport**: UDP with its own reliability, later split from the link so Steam can carry it too. The first
  two-PC game filled the host's upload: every unacked datagram was resent every 150 ms with no window, and the parts
  stock was sent whole (keys are item JSON) on every change. Fix: at most 32 unacked datagrams in flight, resend
  after the measured round trip with backoff, stock sent as changes. Same headless test at a fake 200 ms round trip:
  1.47x -> 1.03x bytes per byte of data. A Steam peer is an account id, so each game run puts a run number on its
  datagrams; otherwise a friend who restarted the game received the host's late datagrams for the old run.
- **Room list (census)**: every 2 s the host sends every object and device id with a short signature; the guest
  asks for what it lacks, hides what is out of the room, removes what the host no longer has. Catches everything
  the game creates without a drop. Plus a twice-a-second watcher that reports moves made without a drop (a guest's
  device went onto the bench by unpacking, stayed on the host's desk, and the host could carry it off mid-repair).
- **Bench**: "on this player's bench" is `DeviceService.PlacedDeviceContainer`, not the box's state (after
  unpacking, the box kept its old "stored" state and the device was reported as lying on the desk).
- **Service boxes, cash, mail, quests, furniture**: each synced from the host's saved state of that service;
  takes from a box are numbered so a stale host state cannot bring back what the guest took. Cash carries its sum;
  the guest's deposit is done by the host. A guest taking an e-mail order asks the host, who takes it only if it is
  still free. A quest completed on either side is completed on the other (an item can complete at once when it is
  added, so "added" has to go out first).
- **Thrown away = removed**: a hidden object stays in the registry and so in the host's save; thrown-away courier
  boxes came back the next day until "gone" meant removed.
- **Shared rules**: one pause for both (the other side opens its own pause menu); the bicycle and the end-of-day
  stamp mean "ready" and need both.
- **Versus**: both players get the same device; the start is a shared countdown (the host repeats the time left
  while counting down, so a lost and resent start message does not start the other player late).

## QA

- Headless: `dotnet run --project mod/RestoryMultiplayer.Tests -c Release` (network, sync rules, fuzzers, fake Steam link,
  traffic at a fake 200 ms round trip). 242/242 on 2026-10-30 (mod 0.13.2).
- Two real game copies with bots (`scripts/run_coop_test.py`, `run_coop_ingame_test.py`, `run_race_test.py` in the
  mod repo): each copy is a copy of the Steam install with its own save folder; the bots move objects, take parts
  from the courier box, buy, deposit cash, take an e-mail order, complete a quest, pause, end the day by the
  bicycle and stamp, rejoin after a crash; the scripts compare both copies' rooms, stocks, wallets, boxes, mail and
  furniture, and check the real saves are untouched. On 1.0.033r, 2026-10-30, mod 0.13.2: co-op 78/78, co-op from inside
  a save 11/11, versus 70/70.
- `scripts/check_game_members.py <decompiled game>`: the 61 game members the mod finds by name; after the
  1.0.033r update only one was gone (the contest neon shadow, removed by the developers).
- Human playtests: two players on two PCs over the internet, connected through
  Porthole (IP route), game
  build 1.0.015r (before the 1.0.033r update). First a versus match (2026-10-25), then from 2026-10-27 several
  co-op days on mod 0.10.0 to 0.12.x in the friend's save with the friend as host: taking mail orders,
  repairing, handing devices back, buying parts, cash from customers, ending the day. Both players' logs (`BepInEx/LogOutput.log`) came back after
  each session. What only the two-PC games found, each fixed and then added to the bot suite:
  - the guest saw no devices at all (the device registry was only captured on a register call the guest's shop
    never made; the test save always had devices, so the bots never hit it);
  - the host's game took the whole upload and the players' voice chat lagged (resend with no window, the full
    stock on every change; see Transport);
  - the host could carry off the device the guest was repairing (unpacking onto the bench is not a drop);
  - the customer's money showed as 0 on the guest and vanished when deposited; two courier boxes on one spot
    acted as one;
  - three empty boxes after a day, the "ready 2/2" label carried into the next day, the one-order-per-contest
    rule hidden from the player who hit it.
  The players also asked for what the bots could not judge: one pause for both, the day ending only when both
  are ready, the host's rules (may the guest take orders) and avatars on a page reachable while playing.
  The Steam build 1.0.033r was covered by the automated suite only.

## How to play

See the [mod's README](https://github.com/teecil/restory-multiplayer): unzip the release into the game folder; the
main menu and the pause menu get an "Online play" sign. Host creates a game (by IP, UDP 27515, or through Steam),
the friend joins; the host presses Start.

## Known issues

- Work the guest leaves on its own bench when the day ends is not kept (the next day starts from the host's save,
  where that device is still in the room).
- Moving the customers' box or the personal box away from its spot is not shared.
- When the host's mail changes while the guest has the mail app open, the guest's list is rebuilt.
- Steam lobby route: no two-account session yet; IP works.
- Two players only.

## Next steps

- Run a Steam lobby session with two accounts that own the game.
- Keep the guest's bench work over the end of day (for example: return the bench device into the room, with its
  data, before the host's end-of-day save).
- Check the 1.0.033r devices (Fuzzby, PlayMachine controller) in co-op.
