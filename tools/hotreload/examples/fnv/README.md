# How the Fallout: New Vegas co-op mod wired it in

The rest of this tool is game agnostic. This note is the one place that names a game: how the
[fnv-coop](https://github.com/cranebeaker/fnv-coop) mod (commit e85460f) used the same pattern, so you can see a real
mapping from "your game's plugin entry point" to the loader and core.

The game is 32-bit and the mod is an xNVSE plugin, so the split build is compiled for 32-bit (this tool builds
and passes its tests as 32-bit too: `python -I tools/hotreload/run_tests.py --arch Win32`).

## The mapping

| In this tool | In fnv-coop |
|---|---|
| loader DLL (`hotreload_loader.dll`) | `nvcoop.dll`, the xNVSE plugin. Its `NVSEPlugin_Load` entry point builds the `CoreHost` |
| core DLL | `nvcoop_mod.dll`, everything else in the mod |
| `HotReload_OnFrame()` | called from the xNVSE `kMessage_MainGameLoop` message, once per frame |
| `HotReload_OnMessage()` | every other xNVSE message is forwarded to the core |
| `HrLoaderApi::user_context` | the `NVSEInterface*` that xNVSE hands to the plugin |
| `HrLoaderApi::game_dir` | the game folder, with a trailing backslash |
| control pipe | `\\.\pipe\nvcoop-<instance>`, the mod's existing bridge pipe, which also serves console and state calls. The swap tool is its client |
| `hotreload.py swap --pipe` | `tools/runner/run.py swap --instances 0,1` (resolves the instance numbers to pipes and instance folders) |
| `HotCore_Init` | `NvCore_Init` |
| `COOP_RELOAD_STATE` | `NVCOOP_RELOAD_STATE`, blob key `nvreload.state` |
| `reload.state` method | the same name, registered by the core |

## What is specific to that game

- **xNVSE loads every `*.dll` in its plugins folder.** The swap tool therefore copies the new core to a
  temporary name that does not end in `.dll` (here `.swap`) and renames it over the core. A temporary file
  named `*.dll` would have been picked up as a plugin. The cache copies are kept in a subfolder
  (`nvcoop_cache`) instead of among the plugins.
- **Config files sit beside the module.** The mod's modules read `nvcoop.ini` and their data files from the folder
  of their own module file, which is the cache copy. The original loader therefore mirrored the ini and data files
  into the cache folder before every load. This generic version does not: it gives the core `api->core_dir` instead,
  and the core reads its files from there.
- **A file watcher** in the original loader could trigger a reload when the core file changed (`[loader] watch=1`).
  It was left out here: the swap is always requested by the client.
- **Hooks.** The original core patches game code, so its `BeforeUnload` took every hook off, drained with the
  trampolines as extra code ranges, and put the hooks back when the drain refused. A write guard also froze the other
  threads while a patch was written. Here the same steps are the `DrainHooks` callbacks (`suspend`, `resume`,
  `extraRanges`); the freeze is not part of this tool.
- **A co-op session across a reload.** The original swap tool also waits for a running co-op session to run again
  after the reload (the transport is closed and reopened by a reload). That check needs a method of the mod, so it
  is not in `hotreload.py`; add a call to your own status method after the reload if your mod has a long-lived
  connection.
- **The library worker thread.** In that mod the co-op session library starts a worker thread that sits parked in the
  mod's code. It is the reason `adoptThread` exists: until the worker was registered, every reload with a session
  running was refused by the drain.

## Where to read the original

At commit e85460f of fnv-coop:

- `plugin/loader/` (the loader: `loader_host.cpp`, `loader_abi.h`)
- `plugin/reload/` (the state registry and the drain: `reload_state.cpp`, `reload_drain.cpp`, `reload_glue.cpp`)
- `tools/runner/hotswap.py` (the client)
- `docs/FAST-QA.md` (the whole fix round around it)
