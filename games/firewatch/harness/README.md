# Firewatch co-op harness

Two real game clients, driven from inside the game (no simulated input). Used for the QA
claim in `../README.md`.

Expected layout (any folder outside Steam):

```
workspace/
  build/StoryCoop-patched.dll     # built from upstream + ../patch/
  build/CoopTestDriver.dll        # built from CoopTestDriver.cs
  game/                           # copy or hardlink mirror of the Firewatch install
  game2/                          # second copy
```

Each copy needs BepInEx 5.4.23.6 (x64) plus:

- `BepInEx/plugins/StoryCoop.dll`
- `BepInEx/plugins/CoopTestDriver.dll`
- `BepInEx/cooptest.ini` — see below

`game/BepInEx/cooptest.ini`:

```
role=host
name=Host
port=7778
autostart=0
shots=1
shotinterval=15
```

`game2/BepInEx/cooptest.ini`:

```
role=client
name=Client
host=127.0.0.1
port=7778
autoconnect=1
shots=1
shotinterval=10
```

Run: `powershell -ExecutionPolicy Bypass -File run_smoke.ps1` → `RESULT: PASS|FAIL`.

The suite uses `OnClickDebugStart` to enter `TeenLoop` without the click-gated intro and
hides the new-game save-slot canvas (`hideui SaveLoadCanvas`).

## Save isolation

Both copies share `%USERPROFILE%\AppData\LocalLow\CampoSanto\Firewatch`. Never test against
a player's real saves. Rename that folder aside and create a directory junction to a
scratch folder for the session, then restore and compare hashes. See
`playbook/multi-instance-testing.md`.

## Driver commands (write to `BepInEx/cooptest_cmd.txt`)

`host [port]`, `join [ip] [port]`, `leave`, `newgame`, `bring`, `goto`, `interact <name> <event>`,
`interacts [radius]`, `fsms [filter]`, `fsmevents [filter]`, `fsmevent <fsm> <event>`,
`fact <name> <value>`, `factget <name>`, `factlist`, `tp x y z`, `ui [filter]`,
`hideui <filter>`, `menu <method>`, `continue`, `debugstart`, `freeroam`, `pathto <name>`,
`scenes <name>`, `shot`, `dump`.

State: `BepInEx/cooptest_<role>.json` every 2 s (scene, status, partner, synced, position,
day, fact count, avatar, rolling log). Screenshots: `BepInEx/cooptest_shots/`.
