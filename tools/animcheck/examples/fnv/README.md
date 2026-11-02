# Fallout: New Vegas profile

`profile.json` is the animcheck profile used with the New Vegas co-op probe
(`plugin/animprobe` in https://github.com/cranebeaker/fnv-coop).

- **Animation groups:** 245 names, in the order of the public xNVSE `TESAnimGroup` enum, so a group id in the
  probe stream is an index into the list. Categories map them onto what the detectors need: `Forward`, `Left`
  and the like are `walk`; `FastForward` and the like are `run`; `Dodge*` is `dodge`; `Death` is `death`;
  `Jump*` is `jump`; `Attack*` and `PlaceMine*` are `attack`; `Reload*` is `reload`; `Jam*` is `jam`.
- **Bones:** the standard biped names (`Bip01 Pelvis` as root, `Bip01 L UpperArm`, `Bip01 L Hand` and the right
  side for the T-pose check, plus spine, neck, head, forearms, calves, feet and `Weapon`).
- **Moves:** `move_flag_bits` says bit 9 of the probe's `move_flags` is sneaking and bit 11 is swimming.
- **Units:** about 70 units per meter, angles in radians, Z up.

The file holds names only. It has no game files and no engine addresses. Use it as a model for another game's
profile: copy it, replace the bone names, the group list and the category entries, then run
`python -I tools/animcheck/cli.py profile your.json`.
