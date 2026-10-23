# Pitfall: The Lost Expedition tools

For PS2 NTSC-U `SLUS-20408` / CRC `394F402E` on PCSX2 with PINE enabled. Run each with
`python -I <script>`; every script's docstring has its usage. Savestate tools need
`SavestateCompressionType = 0`.

| Script | What it does |
|---|---|
| `ptle_logger.py` | Read-only logger: level, position, facing, health for one or more PINE ports |
| `pine_check.py` | PINE client core; prints version, status, title, serial, CRC |
| `pine_state.py` | Save or load a savestate slot over PINE |
| `pine_poke.py` | Read or write one word over PINE (writes change the running game) |
| `pine_sample.py` | Sample EE RAM to prove reads return game memory |
| `pcsx2_keys.py` | Post key presses to one PCSX2 process's windows without taking focus |
| `pos_search.py` | Find a position vector from five savestates (base, idle, left, right, right) |
| `ram_diff.py`, `ram_pattern.py` | Earlier, noisier searches (GameCube layout test, movement pattern) |
| `teleport_test.py`, `move_test.py` | Find which copy of a position the game reads, by writing each |
| `actor_scan.py` | List objects sharing the player's base layout |
| `obj_*.py`, `pos_copies.py`, `ptr_search.py`, `hp_search.py`, `level_search.py`, `cave_check.py` | One-off savestate checks used in NOTES.md |
| `elf_xref.py`, `vtable_dump.py`, `mips.py` | Static helpers on `SLUS_204.08` (constant builders, class table, small R5900 decoder/assembler) |
| `DecompileTargets.java` | Ghidra headless script: function, callers, decompiled C; `refs:<addr>` lists references |
| `spawn_patch.py` | Second-player (puppet) spawn: `pnach <file>` writes the tested patch file; `pine <port> [steps]` writes it live |
| `ptle_peer.py` | Network peer, one per PC: UDP state exchange and puppet steering (see ../PLAY.md) |
| `ptle_bridge.py` | Same as two peers, for two PCSX2 copies on one PC; also the shared `Game` / `steer` code |
| `peer_trial.ps1`, `peer_check.py` | Automated two-client test and its pass/fail check |
| `spawn_trial.py`, `catch_load.py` | Spawn bisection on a mid-load savestate |
| `pad_search.py`, `input_flag_scan.py`, `puppet_stick.py`, `port2_stick.py`, `puppet_enable_scan.py` | Input-path experiments (several were dead ends; see ../NOTES.md) |
