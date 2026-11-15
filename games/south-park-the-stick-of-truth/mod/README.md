# mod — the in-game half of co-op

The launcher will do host/join; this DLL is where the co-op actually happens. It is loaded as a
`dinput8.dll` proxy: the game imports `DirectInput8Create` statically, so a DLL with that name next
to the exe is loaded before any game code, and the one export forwards to the real system DLL.

## Build

32-bit toolchain only (the game is PE32 i386). MinGW-w64 i686 at `D:\mingw32` works; Visual Studio
Build Tools does not fit on the system drive.

```
cmake -G "MinGW Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
mingw32-make -C build
```

Output: `build/dinput8.dll` (~130 KB, statically linked runtime, no extra DLLs beside the game).

## What it does today

- Loads into the game, writes `coop-mod.log` next to the exe, ticks on a thread. The game stays
  alive and the harness's validity flags stay clean.
- Scans committed writable memory for live objects whose first dword is `module_base + RVA` for
  `OEI::Game_Component::DS_Component_Player` (`0x7d5684`) and `bfx::MoverImpl` (`0x773748`), and logs
  each address with the plausible position triples it contains (three consecutive finite floats in a
  sane range, at least two non-zero).
- Installs vtable hooks on every present path it can reach from a device it creates itself (D3D9,
  D3D9Ex, `PresentEx`, swap chain, DXGI `Present`, `Present1`). None of them fires: the engine
  presents through a chain of its own.
- Reads `SP_COOP_INLINE` to enable inline hooks one at a time (`d3d11`, `d3d9`, `factory`, `all`);
  unset means none. Enabling several at once killed the game during startup, so this switch exists.
- Embeds the hub's `tools/bridge` server on `tcp://127.0.0.1:27990` (endpoint also written to
  `coop-bridge.json` next to the exe). It answers `ping`, `version` and `state`; `state` returns the
  pid, the tick count and the player and mover addresses the scan found. That is the channel the
  later bots and tests use, and it does not depend on the frame hook.

## What it is for

Stage 1 of `playbook/how-coop-mods-work.md`: our code inside the game, a per-frame tick, and the
local player's position. Stage 2 is the puppet: a second body that takes its state from the network
instead of from input.

## Rules that came out of testing this

- **Nothing but bookkeeping in `DllMain`.** Creating a thread or opening a file under the loader lock
  hung the process and Frida could not even attach afterwards. The worker starts from the first
  `DirectInput8Create` call, i.e. after the loader is done.
- **The engine resolves d3d/dxgi through `GetProcAddress`**, not through its import table, and it
  caches the pointers before an IAT patch can matter. That is why the inline hooks exist.
- **The Steam overlay owns the noise**: it creates the DirectInput devices, it polls them, and its
  message pump is the one that calls `PeekMessageA` hundreds of thousands of times per second. The
  game itself reads no DirectInput device. Do not attribute those calls to the engine.
- **Hooks that kill the game**: enable one at a time and keep the build that survives.
- `mod/build/` and `mod/third_party/*.zip` are not committed; MinHook 1.3.4 sources are vendored,
  pinned by the SHA-256 in `NOTES.md`.
