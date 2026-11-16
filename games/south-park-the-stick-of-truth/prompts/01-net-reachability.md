# Prompt 01 — Gate 0: can Onyx netcode be started in this build?

You are one agent on **South Park: The Stick of Truth** (Steam app 213670, build 228353, x86).
Read `../../AGENTS.md`, `../README.md`, `../PLAN.md` and `../PARALLEL.md` first, then
`playbook/how-coop-mods-work.md` §1.1-1.3 and `playbook/reverse-engineering.md`.

## Coordination (read this before you start)

You are **step 1 of 7** (order: `../prompts/README.md`); this prompt is `prompts/01-net-reachability.md`.
The contract is `../PARALLEL.md`.

**Start from the right base.** The paths this prompt names (`../notes/`, `../tools/launch_lock.py`,
`../PARALLEL.md`) exist only on branch `game/south-park-the-stick-of-truth`. If you cannot see them, stop and ask the owner for
that branch — do not recreate them.

**Your own worktree and branch**, so you never share an index or a private `local/` folder:

```
git worktree add ../sp-step01 -b game/south-park-the-stick-of-truth-step01-net game/south-park-the-stick-of-truth
cd ../sp-step01
```

Work only inside that directory from then on.

**Your files.** Write only `../notes/01-net-reachability.md`, `tools/net_probe.js`, `tools/run_net_probe.py`. Never edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`,
`data/directory.yaml`, `DIRECTORY.md`, or another agent's notes/tools: a separate integrator merges
your notes file afterwards, when no other agent is running.

**One launch at a time, machine-wide** (there is one Steam account, one desktop, one game
install). Take the lease before your first launch and hold it through the whole experiment,
two-instance tests included:

```
py -3.14 -B tools/launch_lock.py acquire --owner step01 --minutes 45 --wait 1800
py -3.14 -B tools/launch_lock.py release --owner step01
```

`run_net_probe.py` takes and releases it by itself. If another agent holds it, wait — never start a
second game "just for a minute", and never kill another agent's game process.

**Handoff.** Fill all five sections of your notes file, including "Handoff notes for the
integrator" with the exact `README.md`/`NOTES.md` lines you want published. End your chat with:
verdict, evidence (run folders and commands), what your result unblocks, and what you could not do.

## Why this is the first job

The retail exe still contains Obsidian's Dungeon Siege III co-op engine: Steam session manager,
Steam P2P communicator, LAN session manager, chat/voice managers, host/ghost replication classes
for `Player_Object`, `Secondary_Player_Object`, `Remote_Camera_Object`, `Creature`, `Item`,
`Door`, `Level_State`, plus `LOCAL_WITH_NETWORK`, `multiplayer_local_enabled`,
`multiplayer_remote_enabled` and console commands `connect IP<:port>`, `disconnect`,
`restart_server`, `server`, `status`, `kick`, `warp_to_player`. In a menu-only run nothing binds,
nothing listens, nothing dials out, and no Steam lobby or game server is created — but
`SteamNetworking()` is polled ~18 times per second from inside the game module.

If that stack can be started, co-op is "make the shipped netcode run the campaign". If it cannot,
the project falls back to a puppet/authority mod built from zero. That single answer decides
months of work, so answer it with tests, not with names.

## Answer these three questions, in order

1. **Host path exists?** Does any reachable code start a session/server in this build? Look for
   the session manager (RTTI/strings `steam_online_session_manager.cpp`,
   `lan_online_session_manager.cpp`, `.?AVSession_Func@Net@OEI@@`, `.?AVClient_Node@Net@OEI@@`,
   `NetConnection`, `Game::Network`, `network system`). The `.text` is Steam-CEG encrypted, so
   static disassembly of game code is unreliable: prefer runtime work — scan `Process.enumerateRanges`
   for vtable pointers whose RTTI name contains `Session`, then trace what constructs them, or hook
   `SteamMatchmaking`/`SteamGameServer_Init` in `steam_api.dll` (`Interceptor.replace` on the getter,
   then vtable-hook the returned interface) and see whether a lobby is ever created when the game
   starts a level. Also try the console command `restart_server` and `status` if Gate 1 gives you
   console execution.
2. **Client path exists?** Launch the isolated copy with `-connect 127.0.0.1:2005` (and a few other
   ports) under the network probe and check `ws2_32!connect` in `result.json`. A `connect` that
   fires and then fails is a **yes** for the client path; no call at all is a **no**.
3. **Two clients?** The exe refuses a second instance unless started with `-multiple_instance`.
   With the owner's approval, run copy A normally and copy B with `-multiple_instance -connect
   127.0.0.1:<port A>`. Record: does B reach a join attempt, a version error, a "server is not
   accepting connections" message, or a second `Player_Object`?

## Method

- Use `../tools/` (`run_net_probe.py`, `guard_session.py`, `save_guard.js`, `net_probe.js`).
  Every launch must be guarded and must end with `original_profile_unchanged`,
  `guard_integrity`, `original_exe_hash_still_matches` all `true`.
- Ask the owner before every launch session (Steam account). Keep runs to minutes, one variable
  per run. Do not install anything new without asking.
- Extend `net_probe.js` rather than writing a parallel harness: add `closesocket`/`WSAConnect`,
  Steam interface vtable hooks, and (for host detection) `CreateThread`-free logging only.
- Never call an engine function whose signature you have not established from a live trace.

## Deliverable

`games/south-park-the-stick-of-truth/NOTES.md` gets a new dated section with a verdict table:

| Question | Verdict | Evidence (run folder, counter, screenshot) |
|---|---|---|
| Host path | yes / no / unknown | |
| Client path | yes / no / unknown | |
| Two instances | yes / no / unknown | |

Plus, if any answer is "yes": which class/function is the entry point, its module+offset, the
arguments you established, and the exact reproduction command. If all three are "no": say so
plainly and hand off to the puppet plan in `PLAN.md` Gate 3-4.

## Do not

- Do not remove or bypass Steam CEG, DRM or anti-cheat, and do not distribute a patched exe.
- Do not "fix" a crash by editing the retail install.
- Do not report a string, an RTTI name or an address in `.text` as evidence of behaviour.
