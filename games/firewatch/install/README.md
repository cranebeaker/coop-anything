# Installer

`install.ps1` sets up BepInEx 5.4.23.6 (pinned, SHA-256 checked) and the mod for a player:

```
powershell -ExecutionPolicy Bypass -File install.ps1
powershell -ExecutionPolicy Bypass -File install.ps1 -GameDir "D:\SteamLibrary\steamapps\common\Firewatch"
powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
powershell -ExecutionPolicy Bypass -File install.ps1 -OpenFirewall
```

It expects `StoryCoop.dll` next to the script. Build it from the upstream source with
`../patch/shoshone-0.5.0-fixes.patch` applied; no binary is committed here because the
upstream project has no license.

The script:

1. Finds Firewatch through Steam (`HKCU\Software\Valve\Steam`, `libraryfolders.vdf`,
   `appmanifest_383870.acf`), or takes `-GameDir`.
2. Refuses to run while the game is open.
3. Checks `Assembly-CSharp.dll` against the tested build hash
   (`145B56AE…49D0522A`); `-Force` overrides.
4. Downloads BepInEx from the official GitHub release and verifies
   `6c57a714…a0755` before extracting.
5. Writes a default config if none exists (`Ip`, `Port`, `PlayerName`).
6. Optionally adds the TCP 7778 firewall rule (needs elevation).
