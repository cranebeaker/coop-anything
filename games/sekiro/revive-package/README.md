# Sekiro Online — revive package

Reviving **Sekiro Online v2.05** (Nexus mod 577, by LukeYui), the only released
co-op mod for Sekiro: Shadows Die Twice. The author marked it discontinued in March 2025
and its promised rewrite has never shipped, while the original matchmaking server has
stopped answering. This package makes the old build install cleanly, tells you what still
works, and captures evidence when something does not.

Status on 2026-10-20, tested against Steam build **5794815** (`sekiro.exe`
SHA-256 `637ACA52…98B856`):

| Check | Result |
|---|---|
| Mod DLL loads on today's game build | **works** — game reaches the title screen, `DINPUT8.dll` from the game folder is mapped, no crash |
| Version gate (`VERSION` on GitHub) | **works** — answers `v2.05`, so the mod does not think it is outdated |
| Online item table (`DATA` on GitHub) | **works** |
| Public summon-sign server `74.91.125.31:54000` | **down** — TCP connect times out; random summon signs are gone |
| Friend co-op through a Steam invite | **not yet proven** — needs a real second client (see below) |
| In-game host/join, desync, boss sync | **not tested** here |

So: the mod is still the best way to play Sekiro with friends, but treat it as
"friends only, arranged over Steam" until the friend test passes.

## Install

You need your own copy of the archive; this package redistributes nothing.

1. Download `Sekiro-Online-v2.05-577-2-05-1623599819.zip` from
   <https://www.nexusmods.com/sekiro/mods/577> (Files tab).
2. In PowerShell, from this folder:
   ```powershell
   .\install.ps1
   ```
   It finds Steam's Sekiro install, verifies the archive SHA-256
   (`BE6EF9F6…311CC`), moves any existing `dinput8.dll` aside
   (ReShade and other `dxgi.dll` mods are left alone), installs the mod, backs up
   `S0000.sl2`, and writes `sekiro-online-revive.install.json`.
3. Launch Sekiro through Steam.

Uninstall with `.\uninstall.ps1` (restores the `dinput8.dll` it moved aside).

## Play with a friend

Both players need the same version, and both must have been past the tutorial at least
once (the mod locks its online items until then).

1. **Host:** load a save, then use the **Cursed Emblem** item to open a session. You
   cannot stop hosting until you die or quit.
2. **Host:** invite your friend through the Steam friends list (right-click → Invite to
   game / the mod's lobby).
3. **Friend:** accept the Steam invite. You appear in the host's world as a phantom.
4. **Leave:** use the **Coward's Bell**, or die.

Other online items: **Demon Eyes** (invade), **Vengeful Spirit** (invite invaders),
**Serendipitous Spirit** (invite friendly phantoms), **Spirit Essence**, **Ethereal
Confetti**.

Because the central matchmaking server is down, ignore the summon-sign / random-player
flow; direct Steam invites are the path that should work.

## Before you play

- Back up your saves: `%APPDATA%\Sekiro\<steamid>\S0000.sl2` (the installer already makes
  a copy at `save-backup-before-sekiro-online-<timestamp>`).
- Sessions end when someone changes area; re-invite afterwards rather than treating it as
  a bug.
- Known upstream bugs that this package does not fix: crashes on start reported against
  1.06 + 2.05 (2023–24), a Cursed Emblem crash report, Lady Butterfly's door closed for
  phantoms (2024), Guardian Ape not dying / not dropping the Finger Whistle (2024), and
  Floating Passage showing as sold out (2024). See
  <https://www.nexusmods.com/sekiro/mods/577?tab=bugs>.

## Diagnostics

```powershell
.\verify.ps1          # game identity, installed DLL, original services, mod logs
.\watch.ps1           # run this, then play: records connections + mod log
```

`watch.ps1` writes `sekiro-online-watch-<timestamp>.log` next to `sekiro.exe`. If a host
or join fails, send that file — it shows whether the game tried the dead server or whether
the Steam lobby path was used.

## What is not fixed yet

- Public summon signs / invasions need a replacement matchmaker. The endpoint is a plain
  ASCII string (`74.91.125.31`, `54000`) inside `dinput8.dll`, so redirecting it is
  technically simple; speaking its protocol is not, and nobody has captured it yet.
- No source exists for v2.05 (author declined to publish it), so every fix would be a
  binary patch shipped as a separate loader, never a rebuilt DLL.

## Credits and licence

Sekiro Online is LukeYui's work (<https://ko-fi.com/lukeyui>). This package only
installs the files from your own download, verifies them, and documents how to test.
No game files, mod binaries or save data are redistributed here.
