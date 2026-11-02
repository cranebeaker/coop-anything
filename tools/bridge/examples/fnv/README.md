# Example: the bridge in a Fallout: New Vegas co-op project

This page lists the parts of the bridge that were specific to [fnv-coop](https://github.com/cranebeaker/fnv-coop) (commit e85460f), a shared-story co-op
mod for Fallout: New Vegas. The client and server in this folder's parent need none of it. It shows what a game puts into the adapter and into custom methods.

The original code is `tools/runner/nvbridge.py` (client) and `plugin/bridge/` (in-game server) in that repo. Its server was a plugin loaded by the game's script extender, and
`Pump()` was called from the plugin's per-frame hook.

## Endpoints

The original opened one pipe per game copy: `\\.\pipe\nvcoop-<instance>`, with instance numbers 0, 1, ... for the copies on one PC. With this bridge that is
`pipe://nvcoop-0`, `pipe://nvcoop-1` (set `Options::pipeName = "nvcoop-" + instance`).

## Method names

| fnv-coop | this bridge | Note |
|---|---|---|
| `bridge.version` | `version` | `{gitHash, dirty, buildTime, protocol, pid}` became `{protocol, game, build, server, pid}`. Put the git hash in `Options::build`. |
| `loadSave` | `load` | The original finished when the script extender's post-load message fired; that hook calls `NotifyLoaded()`. |
| `saveGame` | `save` | The engine's save menu needs a name that starts with `autosave`. The adapter refused the reserved names, odd characters and existing files unless `overwrite` was set, and used code -32012 for an existing save. The save message calls `NotifySaved(path)`. |
| `exec {cmd, ref}` | `exec {cmd, ref}` | Same shape. |
| `eval` (client side) | `eval {expr, ref}` | The original client sent the console line `printc "NVCOOP_EVAL=%g" (<expr>)` through `exec` and parsed the number. With the bridge the adapter's `Eval` does that. |
| `waitFor` | `waitFor` | Same params. The original polled `printc` the same way for `expr`. |
| `state` | `state` | The original returned a snapshot built by its own module, optionally to a file. |
| `lock.acquire`, `lock.renew`, `lock.release`, `lock.info` | not included | A lease that let several test lanes share one game copy. Add it as custom methods if you need it; keep codes -32010 and -32011 for it. |
| `plugins`, `quit` | not included | Register them as custom methods if you want them. |
| `coop.status`, `coop.owner`, `ui.status`, `bot.menu`, `bot.menu.press`, `bot.fire`, `ref.info`, `world.query`, ... | custom methods | Registered by the mod and its bot module, not by the bridge. [tools/inspect/examples/fnv](../../../inspect/examples/fnv/README.md) lists the ones the inspector reads. |

## Console quirks the adapter handles

- The game's console does not accept `<hexFormID>.Command`. A line for any reference other than the player names the reference in `ref`, as `"Plugin.esm:HEX"` or a runtime id, and the adapter
  runs the line with that reference as the console's calling reference. A `ref` that names no loaded reference is an error (the original used -32013). `player.X` lines need no `ref`.
- Quest and actor checks went through expressions such as `GetStage <hex FormID>`: editor ids did not resolve in the console parser, so the tests use hex FormIDs.
- A path given to `state` or `screenshot` is resolved by the game process, whose working directory is the game folder, so the Python helpers send absolute paths. `Bridge.state_to_file` and `Bridge.screenshot` do that.

## Shape of the adapter

```cpp
class FnvAdapter : public coopbridge::Adapter {
    Capabilities capabilities() const override { /* exec, eval, state, save, load, screenshot */ }
    ExecResult Exec(const std::string& line, const std::string& ref) override;   // console line, or with `ref` as the calling reference
    json::Value Eval(const std::string& expr, const std::string& ref) override;  // runs printc "NVCOOP_EVAL=%g" (<expr>) and parses the number
    // Load/Save issue the engine call; the script extender's PostLoadGame / SaveGame messages call NotifyLoaded / NotifySaved.
};
```

This is a sketch of where the original's code goes, not a second plugin.
