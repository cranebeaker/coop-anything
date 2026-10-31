# UE4SS v3.0.1 settings used on Crash 4 (Steam)

Changes from the shipped `UE4SS-settings.ini` of `UE4SS_v3.0.1.zip`
(SHA-256 `4b47d4bceddd2f561a4e395bfa00924ccfc945af576a2d0c613e6537846c57ec`):

```ini
[General]
EnableHotReloadSystem = 1      ; Ctrl+R reloads Lua mods in game

[Debug]
ConsoleEnabled = 1             ; text console window

[Hooks]
HookInitGameState = 0          ; off while testing (issue #720 suggestion; not the crash cause)
HookCallFunctionByNameWithArguments = 0
HookBeginPlay  = 0
HookLocalPlayerExec = 0
HookLoadMap = 0                ; REQUIRED: the LoadMap hook crashes on Crash 4's FURL layout (key missing from the shipped ini)
```

Install: copy `UE4SS.dll`, `UE4SS-settings.ini` and `Mods/` next to `Lava/Binaries/Win64/CrashBandicoot4.exe`
in a copy of the game, delete `dwmapi.dll` (the proxy loads too early for SteamStub), save `Mods/mods.txt` without a
BOM, start the game, run `inject-ue4ss.bat` and press Enter at the title screen.

`mods.txt` used:

```
CoopAutoDump : 0
CoopTools : 1
ConsoleEnablerMod : 1          ; F10 opens the UE console
Keybinds : 1
(all other stock mods : 0)
```
