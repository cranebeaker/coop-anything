# NieR:Automata: working notes

Running notes, newest last. Observations, derived facts, guesses and unknowns are kept apart.

## Game identity

- Store: Steam, app 524220, build id 7020666
- `NieRAutomata.exe` SHA-256: `5171bed09e6fec7b21bf0ea479dbd2e1b228695c67d1f0b478549a9be2f5726a`, PE timestamp 2021-06-23
- Engine: PlatinumGames in-house engine (name: unknown). Native code only; game logic partly in the `hap` script layer
  (class names like `hap::BunkerAction`, `script_export`), story scripts compiled for mruby (source: AutomataMP's `shared/sdk/mruby.*`).
- OS tested: Windows 11

## Existing work

- AutomataMP by praydog: https://github.com/praydog/AutomataMP. Players, animations, Pod and enemies over ENet with a Go server.
- nier-coop, fork of AutomataMP aimed at two-player story co-op: https://github.com/egoriyNovikov/nier-coop.
  Design: `docs/COOP_CONCEPT.md` there. Story sync not done yet.

## 2026-10-18: isolated test copies

Observed:
- The game finds Documents through `SHGetSpecialFolderPathA(CSIDL_PERSONAL)` (only shell import) and
  `%APPDATA%\PlatinumGames\NierAutomata\config.ini` through `getenv("APPDATA")` (string `APPDATA` in .rdata, `getenv` import).
- Redirecting only `SHGetSpecialFolderPathA` was not enough: the first run of a test copy still rewrote `SystemData.dat` in
  the owner's Documents (restored from the copy made before the run). Which call wrote it: unknown.
- With an `NtCreateFile` write guard (refuses writes to the owner's Documents, `%APPDATA%\PlatinumGames` and the store
  install) plus both redirects, 3 runs (title screen, save loaded) ended with 0 blocked writes and the owner's files unchanged.
- Remaining writes outside the test copy: NVIDIA driver caches, D3D shader cache, Steam overlay log.
- Two copies can run at once (the mod renames the `NieR:AutomataEvent` single-instance event).
- `data\*.cpk` can be hardlinked into the test copy; the game only reads them.

## 2026-10-18: test bot

- A virtual XInput pad 0 (hook of `XInputGetState` in `xinput1_4.dll`) is enough to get from the title screen to the loaded
  world: any button, Continue, choose the save, Yes, Start Game. The save menu offers Chapter Select on a finished save.
- In-game screenshots by copying the D3D11 back buffer in the present hook (no desktop capture).

## 2026-10-18: story phase

Observed (build 7020666):
- `.text` on disk is encrypted (SteamStub, `.bind` section). Search references in memory of the running game.
- The current phase is a string like `16_AB_bunker_Order` (Bunker, early route A save) or `60_05_AB_Forest_Castle`
  (finished save at the Resistance Camp). Same name in four globals: exe base + `0x13ecc34`, `0x13f8548`, `0x145b6a8`, `0x1495548`.
- `+0x1495544` is an int32 right before the name (256 on the first save, 512 on the second). The game compares it with `0x100`.
- Signature: `8b 3d ? ? ? ? 81 ff 00 01 00 00 75 ? 48 8d 15 ? ? ? ? 48 8d 0d` (mov edi,[id]; cmp edi,100h; jnz; lea rdx; lea rcx,[name]).

Derived:
- Phase names follow the LiveSplit autosplitter list for v1.01 (`00_85_A_RobotM_Pro_2ndArea`, ...):
  https://github.com/Henny022/NierAutomataSplitter. That autosplitter's addresses do not apply to this build.

Unknown:
- Meaning of the int32 before the name. Which of the four copies the game treats as the source.
- How to switch the phase (what Chapter Select calls).
