# Tale of Immortal (鬼谷八荒) notes

Labels follow AGENTS.md rule 4: observations, derived, guesses and unknowns are kept apart.
Private paths and owner state are in `local/`.

## Identity

- Steam app 1468810 (depot 1468811), build id 21758240, depot updated 2026-10-05, Windows 11.
- `guigubahuang.exe` SHA-256 `038789f47291a3a9dfd4c6f6897e24757630d95139d6b12af3987d649a25feac` (653,824 B).
- `GameAssembly.dll` SHA-256 `affd4e9446b933678fe74fbe0a7732476ed638b9d2fd94cf5296fc4fe14fdde9` (120,637,440 B).
- `global-metadata.dat` SHA-256 `d2351023ac89b11fa437c5a2c044216ae333c6eadecc2bd13d2bbaba9265ae3b` (40,489,264 B).
- `steam_api64.dll` SHA-256 `1add7f151fa644870a735ae86e68d1f019f296130d8e7c0a7ed3ecc7482dccbc` (300,392 B), Steamworks SDK 1.60.
- Unity 2020.3.9f1, IL2CPP, x64. Newest version string in the Steam news feed: 1.2.113 (2026-02-06).

## Search record, 2026-11-02

- Hub: no entry in `DIRECTORY.md`, `data/directory.yaml` or `games/`. Issue and PR search for "Tale of Immortals", 鬼谷八荒,
  guigubahuang and guigu returned 0 (public GitHub API).
- Finished sweeps, as logged by research agents: English web (106 lookups), Chinese web (118), GitHub and Gitee (156).
- The first sweep stopped when usage credits ran out. The second round below finished official modding, game systems,
  archives and video, engine siblings, independent verification and the completeness critic.
- Nexus web pages returned 403. Its public API gave mod 87's description and counts. Its bugs tab stays unread.

## Observations (read directly)

### Tale of Immortal Multiplayer (Akulia)

Source: Workshop item 3811631543 page, change notes and discussion index, fetched 2026-11-02.

- Posted 1 Oct, updated 28 Oct, 3.315 MB, about 1,030 subscribers, 11 comments, 6 discussions, 6 change notes.
- Newest change note: v1.1.10, shown as 27 Oct 5:44pm Pacific, which is 2026-10-28 00:44 UTC. The item's Updated stamp is later,
  28 Oct, with no matching note. The description text still says v1.1.8, so the page is behind the build.
- "Two or more players share one world and one save". "tested with 5 at once".
- World sync: NPC positions, deaths, grades, breakthroughs, relations. Sect tournaments and promotions.
- Co-op PvE groups in dungeons, caves, dens and NPC fights. Own loot per player. PvP spar and death duel.
- Trade, theft, marriage, master and student, chat on T.
- "No death penalty in multiplayer (no save deletion)". "A shared world only opens when every player is there".
- Requires a VPN LAN (Radmin VPN, ZeroTier) or a real LAN, port 47813 TCP and UDP. The page does not mention Steam networking,
  and the code uses none.
- Same mod version and game version for everyone. Install from one place only. Back up saves first.
- The month changes only when every player is ready. v1.1.10 adds a host option for a month time limit.
- Author, 9 Oct: the Steam-installed copy did not work, Nexus did. Change note 20 Oct: "WORKING ON STEAM !".
- Player comments:
  - 11 Oct: a three-person session worked, skipping a month needs everyone, group quest bosses did not work.
  - 21 Oct: must use a virtual LAN. Map and NPCs matched. Secret realms and encounters differed between players.
    Affinity toward the host capped. Group fights hard to use. Summary: mostly just being in one world.
  - 5 Oct: a host stuck on connecting. Threads titled 无法联机 (cannot connect) opened 5 and 7 Oct, last posts 8 and 12 Oct.
    The author said on 8 Oct that the Steam install did not work yet, which fits these reports. No reporter confirmed a cause.
  - 13 Oct: no Multiplayer button on Steam Machine. 19 Oct, Steam forum: "It works great !", one line with no session detail.
- Every player report found predates v1.1.10, which claims fully shared dungeons, events and chests.
- Author, 21 Oct: only the world map and NPCs sync. Secret realms, mining and qi points are per player on purpose.

Load test and static review, 2026-11-02, on Steam build 21758240:

- Subscribing through Steam placed `ModExportData.cache` and `ModCode\dll\TOI_MP_MenuMod.dll` in the Workshop content folder.
- `MelonInfo` reads Tale of Immortal Multiplayer 1.1.10 by Akulia. DLL SHA-256 `5f78b6285db1015574edf7f3d368c68d6401e44a0e4239a78c14b1ba997fbfcd`, not signed.
- Log: `[MENU] BUILD = Menu-V1110-S328-dev-cleanup-2026-10-27`, `[WS] CHANNEL = Workshop?`, 186 mod lines, all hooks installed, no exception.
- The Multiplayer button itself was not seen, because no screenshot was taken. The log says the title-screen pump started.
- No web address, no P/Invoke, no registry, no `Assembly.Load`. Sockets on 47813 only; the host binds 0.0.0.0.
- `Process.Start` appears only in `FirewallHelper`: `netsh` to read rules, and `cmd.exe` with `runas` to add them.
- The add command opens TCP and UDP 47813 with `profile=any`, and nothing found removes the rules later.
- `Directory.Delete` and `File.Copy` sites are in the save merge and fork code, not reviewed line by line.
- Strings mention a dedicated multiplayer save outside the 9 slots and `MpBackups`. Save slots were unchanged by the load test.
- Strings show same-PC support on 127.0.0.1 and a `TOI_MP_dev.txt` switch, so a two-copy test may be possible. Not tried.

### Immortal Co-op (JosephCarmichael)

Source: repo, site, `latest.json`, release v0.5.1 assets and `ImmortalCoop-README.md` in the release zip.

- Releases: 0.4.0 on 2026-10-04, 0.5.0 and 0.5.1 on 2026-10-18. `latest.json` `gameBuild` is 21758240, this build.
- Guide: "It has been played with up to four copies of the game on one computer; it has not yet been tested across
  separate computers." It says to play on a new campaign.
- Release note: with 0.5.0 a campaign hosted on any other computer stopped after Commit characters.
- Guide: "The group shares a common starting world; after that each player has their own world and calendar."
- Only player-caused NPC deaths and the resulting hatred are shared. NPC ages, shops, auctions and quests can differ.
- Other players are blue specters on the tile they occupy in their own world. Battles can be joined.
- Trials, tribulations, tournaments, sect contests and story battles are solo.
- Normal death penalties apply, including permanent death once Eye of Providence is used up.
- Stronger players are scaled down to at most 1.5 times the weakest participant.
- Steam invites, 2 to 4 players, each on their own Steam account. Optional LAN or VPN on TCP 24880.
- Roster locks at Start campaign. Nobody can join later.

Static review of the 0.5.1 binaries, with Mono.Cecil, nothing executed:

- All three release files match `SHA256SUMS.txt`. The DLL inside the installer and the zip equals the release DLL.
- Neither the DLL nor the installer is code-signed.
- DLL targets MelonLoader 0.5.4, Harmony 2.9 and Unhollower 0.4.18, the stack this game ships.
- DLL embeds a template starting world as gzipped JSON, the "shared starting world" of the release note.
- Network calls found in the DLL: Steam sockets through `steam_api64.dll` exports, TCP campaign connections, an update
  check of `latest.json`, the update download, and the problem-report upload.
- Problem reports are referenced only from the Multiplayer menu and the report hotkey handler.
- `ProblemReport` adds log files and session text, then removes the Windows user name and session codes.
- `GameCapture.Capture` has no caller in the DLL, so no screenshot reaches a report by any path found here.
- Updates are refused unless the file's SHA-256 matches `latest.json`, which comes from the author's own site.
- `Environment.UserName` is read in `ConnectCampaign` next to `GetPersonaName`, and in the report redactor.
- Installer reads the registry, never writes it. It can close the game or Steam, restart itself as administrator, run
  `mklink /J`, edit the Steam `installdir` for its folder-name fix, and move other code mods aside.
- This was a targeted trace of sensitive calls, not a full audit.

### Game

- No netcode in the game. No Mirror, Netcode, Photon, FishNet, Lidgren or LiteNetLib in `ScriptingAssemblies.json`.
  The only network-ish classes in the 15,036-type proxy are `SteamManager` and `ServerGameConfInfoManager`.
- No anti-cheat files. DRM on the exe was not probed.
- The game ships MelonLoader 0.5.4 (`version.dll` proxy, `Mods\GGBH_MOD.dll` bridge), written in the depot update batch.
  The official tutorial says "游戏中已内置了MelonLoader". Proxies come from Il2CppAssemblyUnhollower 0.4.18.
- Code mods are .NET Framework 4.7.2 libraries with `ModMain.Init()` and `Destroy()`. `Destroy` runs on return to the
  menu, then `Init` runs again, so patches must be idempotent.
- Pure MelonLoader mods also load from `<game dir>\Mods`, which is how Immortal Co-op installs.
- `steam_api64.dll` exports `SteamAPI_InitFlat`, `SteamNetworkingMessages_SteamAPI_v002`, `SteamNetworkingSockets_SteamAPI_v012`,
  `SteamMatchmaking_v009`, `SteamFriends_v017` and the old `ISteamNetworking` P2P calls.
- Saves live under `%USERPROFILE%\AppData\LocalLow\guigugame\guigubahuang\`. `boot.config` has a `single-instance=` line.

### 诸天万界（联机mod）

Source: Workshop item 2958503186 page. Updated 1 Jun 2023. A chat window that links all players, plus assist copies,
invasions and duels between separate games. No shared world. Duels can freeze when the two mod lists differ.

### Research trap found here

Two research agents reported that the Akulia item was "removed from the community" and "incompatible". Both strings are
hidden blocks with `display: none` in Steam's page template. They appear on an unrelated Project Zomboid item too.
The item is live. Recorded in `playbook/research.md`.

## Derived

- Path: use an existing mod. Neither revival nor build from zero is needed.
- Akulia's mod is the closer match to shared-world co-op, and the only one with reports from real sessions.
- Immortal Co-op is the fallback when a VPN is not an option, and suits parallel solo play with joined fights.
- Akulia's build target is not stated. Its October releases postdate build 21758240, so it should match.
- Any other code mod raises the risk for both, since friends must match and both mods hook game systems heavily.

## Guesses

- A copied install needs its company name patched in `globalgamemanagers` before saves are isolated, because `app.info` is
  ignored. Reported by a private test copy on this PC, not reproduced here.

## Unknown

- Whether hosting and joining work on this build. Only a load test ran, with no second player.
- Whether Akulia's mod works over Tailscale. Its page names Radmin VPN and ZeroTier only.
- The license of Akulia's mod, which neither the Workshop page nor the Nexus description states.
- Who else has played Immortal Co-op on separate PCs. No independent report was found.
- The version string of build 21758240: a third-party README says v1.2.113.259, and the news feed has no post for it.
- Bilibili prototype by 每周五更新的银行保安 (August 2026): whether it was ever released.

## QA lab, 2026-11-02

Observations:

- Two copies run at once on one PC with one Steam account. Gate run: both reached the Login scene with mod build V1110.
- A plain copy is not isolated: saves and PlayerPrefs follow the company name in `globalgamemanagers`, not `app.info`.
- Same-length patch at offset 4136 (`guigugame` to `toicoop00` and `toicoop01`) moved saves and PlayerPrefs. Both were created
  under the test company names, and the real folders did not change.
- Removing the `single-instance=` line from `boot.config` in the copies was enough for a second copy.
- `tools/gamepool` needs private files listed in `link.per_instance_files`, and leaves `per_instance_dirs` empty, so
  MelonLoader, Mods and UserData are copied in afterwards.
- `isolation_check.py` gate result: saves 782 unchanged, PlayerPrefs 49 unchanged, install 21,087 unchanged, Steam userdata unchanged.
  A tampered snapshot made it print FAIL, so the check can fail.

Rule 2 record: before this lab existed, one load test ran on the real install, a private mod package was moved out of its
`ModExportData`, and the Workshop item was subscribed on the owner's account. No test uses the real install after that.

## Research, second round, 2026-11-02

Four areas finished, 8 claims went to independent skeptics (10 votes), then a completeness critic and 3 follow-ups.
Agents worked from the live sources. Items below are their reports unless marked as read directly.

### Searched

- official-modding: 22 lookups.
- game-systems: 18 lookups.
- archives-video: 25 lookups.
- engine-siblings: 21 lookups.
- follow-up, Akulia DLL read for two-copy feasibility, player identity and transport (local, ...: 12 lookups.
- follow-up, Akulia failure reports and post-v1.1.10 reports on Steam: 12 lookups.
- follow-up, Nexus mod 87 through the documented public API, not the blocked web pages: 7 lookups.

### Independent verification

| Claim | Lens | Refuted | Needs correction | Finding |
|---|---|---|---|---|
| akulia-live | existence | no | no | Optional precision: "newest change note is v1.1.10, shown as 27 Oct 2026 5:44pm Pacific (2026-10-28 00:44:37 UTC); the item's time_updated is later, 2026-10-28 13:07:11 UTC, and its description still says v1.1.8. It requires a VPN LAN or real LAN on port 47813 TCP/UDP; that it does not use Steam networking is derived from this ... |
| akulia-live | substance | no | yes | Tale of Immortal Multiplayer by Akulia is live and public on the Steam Workshop as item 3811631543. Its newest change note is v1.1.10, shown by Steam as 27 Oct 2026 Pacific (2026-10-28 00:44 UTC), although the description body still says v1.1.8 and the item shows a later Updated stamp of 28 Oct 6:07am PT with no matching note. ... |
| akulia-reports | substance | no | yes | Players have reported real multiplayer sessions in Workshop comments: 11 Oct (with friends, three-player time sync noted) and 21 Oct (two players, map and NPCs synced). A one-line reply on the Steam forum on 19 Oct says "It works great !" but gives no session detail. The 21 Oct reporter said secret realm, mining and qi spot ... |
| immortalcoop | existence | no | yes | Facts hold, source attribution needs fixing. 0.5.1 and gameBuild 21758240 are correct. The "copies on one computer / not tested across separate computers" and "each player has their own world and calendar" wording is in ImmortalCoop-README.md ("Immortal Co-op — playtest guide") inside the v0.5.1 release zip, not on ... |
| immortalcoop | substance | no | no | All four parts of the claim hold against the sources, and all of them are about Tale of Immortal.  1. Newest release (observed): the GitHub releases API lists v0.5.1 first, published 2026-10-18T16:43:56Z, not a prerelease or draft. latest.json gives version 0.5.1, released 2026-10-18.  2. gameBuild (observed): latest.json has ... |
| zhutian | substance | no | no | I could not refute the claim. Title, author, item ID, game and date all match, and the feature list (chat, assist, invasion, duel) is what the author describes. One limit: "not a shared world" and "between separate games" are derived, because no source says either in those words. The description's save/reload advice and the ... |
| no-official-mp | substance | no | no | The claim survives on substance. The live appdetails API and the store page both list Single-player as the only play mode, with no multiplayer, co-op, LAN or Remote Play Together category. No developer promise of multiplayer for this game was found. The claim is incomplete rather than wrong, because 'no statement found' ... |
| version-news | existence | no | no | Not refuted, and all sources were reachable. Every Steam news channel I checked has 1.2.113 from 2026-02-06 as its newest version string and has no post of any kind after that date, so nothing could explain a 2026-10-05 update. One limit: the build id and date are confirmed only by the local install manifest, because SteamDB ... |
| melonloader | substance | no | yes | The owner's install (Steam build 21758240) runs MelonLoader 0.5.4 with Il2CppAssemblyUnhollower 0.4.18, and no net6 or Il2CppAssemblies layout is present. Most inspected community projects (Toybox, 4azuo ModLib, plus older TOI_Mods) target net472 and use that Managed layout; Toybox and ModLib reference the Unhollower DLLs ... |
| mobile | existence | no | no | Not refuted. The links work and the item is the official mobile port.  MODS (observed): the official launch notice FAQ says plainly that the mobile version does not support mods. That notice was last edited 2026-05-15 and the sentence is still there, so the "no PC mods" half has a primary source.  MULTIPLAYER (derived): I found ... |

### official-modding

- (observed) Official data mods can add or change luck traits (innate, acquired, breakthrough 逆天改命), events, drama and dialogue, items, map points, tasks, map events, custom NPCs, reward tables, sects and small sects, dungeons and mounts, all within the framework. Source: <game copy>/Mod/modFQA/鬼谷八荒MOD使用指南基础版 1.0/鬼谷八荒MOD使用指南基础版 1.0.docx
- (observed) The mod kit also covers Excel config tables (including DLC tables, per the DLC修改配置例子 sample with DLCBattleSkillBase.xlsx and DLCRoleBase.xlsx), asset replacement through Unity AssetBundles, and character art and UI replacement. Source: <game copy>/Mod/modFQA/MOD模板例子/
- (observed) Code mods are .NET Framework 4.7.2 class libraries. The main class must be named ModMain, and the namespace must match the mod editor and assembly settings or the DLL fails to load. Output is bin/Release/MOD_XXXXXX.dll. The mod editor's 进入游戏调试 button copies Release DLLs into the game and loads them. Source: <game copy>/Mod/modFQA/代码编写教程/代码编写教程.docx
- (observed) The tutorial says MelonLoader is built into the game, and that extra DLL references come from the game's MelonLoader folder with Copy Local set to false. The console is enabled with the launch option --melonloader.showconsole. Source: <game copy>/Mod/modFQA/代码编写教程/代码编写教程.docx
- (observed) The template csproj references MelonLoader, UnhollowerBaseLib, UnhollowerRuntimeLib, 0Harmony, Il2Cppmscorlib, Il2CppSystem and proxy Assembly-CSharp. Mods therefore compile against generated proxies of the IL2CPP game, and Harmony patches must be enabled by the mod itself with PatchAll. The ... Source: <game copy>/Mod/modFQA/代码编写教程/ModMain/ModMain.csproj
- (observed) ModMain lifecycle: Init runs on entering the game. Destroy runs on return to the main menu, and the mod is then reinitialised. The template unpatches and repatches Harmony in Init, and per-frame work uses g.timer.Frame. Source: <game copy>/Mod/modFQA/代码编写教程/ModMain/ModMain.cs
- (observed) ModExportData is the exported mod package class. Members include AddItem, Export, private LoadFileAndEncry (doc: 加载文件并加密, load file and encrypt), data, pathExport. Nested ModExportData.Data has items, modNamespace, projectData. Source: <game copy>/Mod/modFQA/代码编写教程/GGBH_API.chm
- (observed) ModMgr.LoadMod(string modExportDataFile) is private and returns DataStruct<ModExportData.Data,string>. ModImportTool.LoadModExportData(string) is public static and returns ModExportData.Data (doc: 读取一个MOD的配置). ModMgr.LoadAllMod loads all mods. ModMgr also has DeleteDLLFile, ExcelImportConf, ... Source: <game copy>/Mod/modFQA/代码编写教程/GGBH_API.chm
- (derived) ModExportData.cache on disk is not plain text or JSON. A 421-byte sample starts 1b8bg-/%ePe]P[-/921CG followed by high bytes. It sits in debug/Mod_XXXXXX beside ModExcel and ModAssets. It is treated as encrypted or obfuscated, consistent with LoadFileAndEncry; the scheme is unknown. Source: <game copy>/Mod/modFQA/MOD模板例子/DLC修改配置例子/debug/Mod_BpOnHa/ModExportData.cache
- (observed) ModProject.ProjectData fields: accountID (publishing Steam account), author, name, desc, curUpdateDesc, soleID, publishedFileID, tags, ver, exportVer, openCode (open source flag), excelEncrypt, isCreateNPC, createTicks, autoSave, addPreviewPaths, visibleState. Source: <game copy>/Mod/modFQA/代码编写教程/GGBH_API.chm
- (observed) Workshop publishing and browsing are built into the game: UIModMain.OnWorkshopClick, UIModWorkshopUpload.OnUploadClick(bool isOneUpload), ModWorkshopUploadData (authorName, openCode), and method signatures using UGCQueryHandle_t and SteamUGCDetails_t. So Steamworks UGC is used in-game. ... Source: <game copy>/Mod/modFQA/代码编写教程/GGBH_API.chm
- (derived) Load order is persisted through ModMgr.ModLoadData.mods with Item.modFolderName and Item.isLoad. The Steam workshopdepot is 1468810. Combined with the NOTES load test (subscribing put ModExportData.cache and ModCode/dll/*.dll in the Workshop content folder), Workshop items load through ... Source: <game copy>/Mod/modFQA/代码编写教程/GGBH_API.chm and <Steam>/appcache/appinfo.vdf
- (observed) Workshop items can be copied manually: Mod, Local Mods, Import Mod Data. The official guide also names file copying and the Fanbook MOD site as distribution routes. Source: https://www.nexusmods.com/taleofimmortal/mods/16
- (observed) The official API exposes no networking. The full CHM index has zero hits for socket, steamworks, netmsg, NetworkManager, p2p, tcpclient, udpclient, WebRequest, photon or rpc. Steam-related entries are only SteamManager, GameTool.SteamCheck, AchievementMgr Steam stats, GameBuildsTag.steamAppID and ... Source: <game copy>/Mod/modFQA/代码编写教程/GGBH_API.chm
- (observed) Coverage limit on the above: the name index is complete, but only 67,977 of 106,588 HTML pages were extracted because hh.exe hit the timeout. The zero-hit result rests on names in the index, not on full-text search of all pages. Source: scratchpad toi-chm/out
- (derived) The template csproj lists System.Net, System.Net.Http, UnityEngine.UNETModule, com.unity.multiplayer-hlapi.Runtime and UnityWebRequest modules. These look like blanket references to shipped assemblies, not a game feature. A code mod can reach them, so transport is the mod's own job, as with the two ... Source: <game copy>/Mod/modFQA/代码编写教程/ModMain/ModMain.csproj
- (observed) Steam store categories for 1468810 list Single-player, Steam Achievements, Full controller support, Steam Workshop, DualShock Controller Support, Steam Cloud, Remote Play on Tablet, Family Sharing. No multiplayer category. Windows only. Source: https://store.steampowered.com/api/appdetails?appids=1468810&l=english&filters=dlc,basic,release_date,categori ...
- (observed) Version posts in 2025: 1.2.105 (Jan 9), .106 (Jan 22), .107 (Feb 17), .108 (Mar 6), .109 (Apr 3), .110 (Apr 19), .111 (Jun 20). That is 7 updates. Source: https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=1468810&count=100&maxlength=200&feeds=steam_co ...
- (observed) Version posts in 2026: 1.2.112 (Jan 26, outfit and sale) and 1.2.113 (Feb 6, a new mount and outfit tweak). That is 2 updates, with no versioned post after Feb 6 as of 2026-11-02. Announcement counts by year: 2021 8+ (feed truncated), 2022 44, 2023 27, 2024 11, 2025 8, 2026 2. Source: https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=1468810&count=100&maxlength=200&feeds=steam_co ...
- (derived) CORRECTS NOTES: build 21758240 is the public branch build with timebuildupdated 2026-02-03T03:43:00Z and timeupdated 2026-02-06T08:02:12Z. That matches the 1.2.113 post of 2026-02-06, so build 21758240 is very likely v1.2.113. NOTES line 141 says the news feed has no post for it; the post is there ... Source: <Steam>/appcache/appinfo.vdf and ...
- (derived) Update cadence is now slow. Public build unchanged since 2026-02-06 per the local app cache (entry last refreshed 2026-09-29), roughly 9 months, and the updates in 2026 were cosmetic. IL2CPP proxy breakage risk from updates is low at present, but the January 2026 posts show the developer can still ... Source: <Steam>/appcache/appinfo.vdf
- (observed) DLC per the store: Ground of No Return (2408730, 26 May 2023), BaHuang Chronicle (3294480, 26 Dec 2024), Five Blooms (3326620, 26 Dec 2024), plus 鬼谷八荒 Soundtrack (1536810, type music, 11 Feb 2021). A January 2026 post also mentions a Tale of Immortal Art Book at 25% off, which may be BaHuang ... Source: https://store.steampowered.com/api/appdetails?appids=2408730&l=english&filters=basic,release_date
- (observed) No public older-build beta is offered. The local app cache lists only one branch, public (buildid 21758240). depots.privatebranches is 1, so private or password branches exist or are flagged, but their names and contents are not visible. Source: <Steam>/appcache/appinfo.vdf
- (observed) Steam Cloud path from the app config: root WinAppDataLocalLow, path guigugame/guigubahuang/Steam/CacheData, pattern *, recursive, quota 524,288,000 bytes, max 500 files. This is relevant to the Rule 2 save isolation hashing. Depots in the cache: 228988, 229006, 1468811, 1551860. Source: <Steam>/appcache/appinfo.vdf
- (observed) The game has a Steam ownership or environment check, GameTool.SteamCheck (doc: steam检测). Update 1.0.103 was titled 'Fixed network verification problem', so some online verification existed in 2023. This may matter for isolated copies. Its behaviour is unknown. Source: <game copy>/Mod/modFQA/代码编写教程/GGBH_API.chm and Steam news feed post of 2023-05-26
- (observed) No official multiplayer statement from the developer was found. Search surfaced a 2023 gcores interview on platform ports and a gcores article saying 鬼谷八荒2 is in early project setup, neither mentioning multiplayer per the search summaries. Source: https://www.gcores.com/articles/166136

Unknown after this area:

- Encryption or obfuscation scheme of ModExportData.cache, and whether DLLs in ModCode are also encrypted (LoadFileAndEncry body not read; the CHM has signatures only).
- The exact code path that takes a Workshop folder into ModMgr, and whether Workshop DLLs load through MelonLoader or Assembly.Load in the GGBH_MOD bridge. Would need decompiling the proxy or GGBH_MOD.dll, out of scope ...
- The managed runtime hosting mods on this build; the Mono answer is a guess. The melonwiki il2cppdifferences page was reachable but not read.
- Names and contents of private branches. SteamDB returned 403 and api.steamcmd.net returned no depots without a token.
- Whether old builds can be fetched by manifest through depot downloads for this app; manifest IDs not retrieved, and that would need an owner login.
- Whether the 61,000-odd remaining context of the CHM (38,611 pages not extracted) contains networking text; the name index suggests not.
- Any silent hotfix between the app cache refresh on 2026-09-29 and today. NOTES report build 21758240 installed on 2026-11-02, which suggests none.
- The exact version string for build 21758240 (1.2.113 is derived; the third-party value v1.2.113.259 stays unconfirmed).
- Whether a sequel or an official multiplayer plan exists beyond the early-stage 鬼谷八荒2 mention.
- Full Bilibili guide text: blocked by captcha, snippet only.

### game-systems

- (observed) Steam store categories list Single-player, Steam Achievements, Full controller support, Steam Workshop, DualShock Controller Support, Steam Cloud, Remote Play on Tablet, Family Sharing. No multiplayer, co-op or Remote Play Together category. Release date 26 May 2023. Developer 鬼谷工作室, publishers ... Source: https://store.steampowered.com/api/appdetails?appids=1468810&l=english
- (observed) Wikipedia lists the mode as single-player only. Source: https://en.wikipedia.org/wiki/Tale_of_Immortal
- (derived) No developer statement on multiplayer or co-op was found in 5 targeted searches. Absence of a result, not proof that none exists. Source: searches listed in searched; https://www.gcores.com/articles/166521 and ...
- (derived) A 2023 interview says later updates will be irregular and limited to what the existing framework allows, and mentions Switch and planned mobile ports, with no mention of multiplayer in the summarised text. The exact article among the gcores results was not identified. Source: https://www.gcores.com/articles/166521 (search summary, not opened)
- (derived) Tale of Immortal 2 was confirmed as early stage at the 1.0 launch. Whether it has multiplayer is unknown. Source: https://www.gcores.com/articles/165940 and https://www.gcores.com/articles/166136 (search summary, not opened)
- (observed) World map movement is a square tile grid with travel time counted in days. Source: https://en.wikipedia.org/wiki/Tale_of_Immortal
- (observed) Each grid move and each practice action consumes days. Time is settled monthly. The player is forced to pass the month at the last day, and that triggers an autosave. Player reports from Feb 2021. Source: https://steamcommunity.com/app/1468810/discussions/0/3111394481602259631/
- (derived) The world does not tick in real time while the player acts: the player triggers the month end, and the game then simulates other characters and saves. Co-op implication: the month change is the natural sync point, matching Akulia's all-players-ready month gate. Source: https://steamcommunity.com/app/1468810/discussions/0/3111394481602259631/ and ...
- (observed) Month-end events include new city quests and sect missions, Tao Points refill, NPC theft from the player's items, title awards, and tournaments at month end. Scheduled sect wars start the next month. Auctions restock every two years, Ascension Conferences every 3 years, sect pavilions clear every 6 ... Source: https://steamcommunity.com/sharedfiles/filedetails/?id=2597185200
- (observed) NPCs steal from the player at month end unless items are stored in the cave. Auction inventory comes from NPCs, so it cannot be save-scummed. This suggests NPCs hold real inventories in world state. Guide dated 2022. Source: https://steamcommunity.com/sharedfiles/filedetails/?id=2597185200
- (observed) World simulation can vary: the guide says it is sometimes terrible, so cultivators from the next region can appear early. Each region has 6 elemental zones. Source: https://steamcommunity.com/sharedfiles/filedetails/?id=2597185200
- (derived) Combat is a separate top-down action scene with bullet hell mechanics, unlike the turn-like tile map. It is real time, so co-op combat needs a different sync model from the map. Source: https://en.wikipedia.org/wiki/Tale_of_Immortal
- (observed) Mod API separates scenes: SceneType.battle is the battle manager and SceneType.map the big map, with world data through g.world. Battle can spawn a human NPC controller with SceneType.battle.unit.CreateUnitHuman<UnitCtrlHumanNPC>, which is a possible host for a remote player's avatar in battle. Source: <game copy>\Mod\modFQA\代码编写教程\ModMain\Example\Example.cs lines 127-144
- (observed) Dungeons consist of five rooms in a row ending with a boss fight. Sect wars have 6 outer and 6 inner segments. Source: https://steamcommunity.com/sharedfiles/filedetails/?id=2597185200
- (observed) Official save folder is %USERPROFILE%\AppData\LocalLow\guigugame\guigubahuang\Steam\CacheData. Source: <game copy>\Mod\modFQA\配置修改教程\配置（只读）Json格式\LocalText.json (patch note text, English and Chinese)
- (observed) The game keeps automatic backups under ...\guigubahuang\Steam\CacheData备份的存档. A player recommends reimporting the recent 30 files. Source: https://steamcommunity.com/app/1468810/discussions/0/3196991412357590263/
- (observed) Save data is per-slot .cache files decoded to DataUnit.json, DataUnitLog.json, DataUnitDie.json, DataWorld.json and DataBuildSchool.json, encoded with DES and GZip in the editor's DE.cs. The editor predates current builds by an unknown margin, so this may have changed. Folder example: ... Source: https://github.com/godcong/ggse (CacheData.cs, DE.cs, bin/Debug/config.ini)
- (observed) Saving is restricted, not any time: game text says "当前操作下不可保存存档" (cannot save during this operation) and "本月天数不足，结束本月后才能保存存档" (not enough days left this month, save only after ending the month). Source: <game copy>\Mod\modFQA\配置修改教程\配置（只读）Json格式\LocalText.json
- (observed) Export and import of saves is built in (added in an early patch) and can create duplicate saves. Source: https://steamcommunity.com/sharedfiles/filedetails/?id=2597185200 and LocalText.json patch note
- (observed) Save reloading to a month-end save is common play, and the wiki snippet says reloading works even in Chaos. A co-op design has to decide who may reload. Source: https://steamcommunity.com/sharedfiles/filedetails/?id=2597185200 and ...
- (observed) Save deletion on death exists: game text says "复活次数不足，存档已删除！" and "人物寿命不足，存档已删除". GameDifficultyValue.json has a removeSave field per difficulty. Wikipedia says the last difficulty, chaos, is permadeath. Co-op test characters could trigger deletion, and Akulia's mod advertises no save deletion. Source: LocalText.json and GameDifficultyValue.json in modFQA; https://en.wikipedia.org/wiki/Tale_of_Immortal
- (observed) Five difficulty levels from normal to chaos. DLC difficulty tables also exist (DLCGameDifficultyValue.json, GameDifficultyCrazy.json). Source: https://en.wikipedia.org/wiki/Tale_of_Immortal and modFQA config listing
- (derived) Cloud saves were disabled in patch 0.8.1012 on 28 Jan 2021 due to major issues. Steam Cloud is listed on the store today, so it was restored at some point (date unknown). Cloud sync is a risk to isolated test copies under the same Steam account. Source: LocalText.json patch note and https://store.steampowered.com/api/appdetails?appids=1468810&l=english
- (observed) The game has some network use: strings "网络验证失败，请打开网络重新进入游戏！" (key game_gengxinyouxi), "网络错误，请重试！", and one on upload errors from network or file locks. The first suggests an online verification on start-up, and the upload string may concern Workshop mods. What is verified and when are unknown. Source: LocalText.json in modFQA
- (observed) NPC-related config includes npcGoalMin and npcGoalMax and caps for persisted dead units (persistenDieUnitIntimMaxNum, persistenDieUnitHateMaxNum, persistenDieUnitNormalMaxNum, persistenDieUnitDuration). Values not read. ModNpcAddWorld.json is empty as shipped in docs. Source: GameParameter.json and ModNpcAddWorld.json in modFQA
- (observed) Dual cultivation and partners: dao partners (道侣) depend on favour and charm, and failure lowers favour. Reported heart thresholds differ across guides. One guide says one spouse but multiple dao partners. Sources from 2021. Source: https://www.3dmgame.com/gl/3824864_23.html and https://gl.ali213.net/html/2021-2/563507_19.html (search ...
- (observed) A dao partner cannot follow the player all the time in the base game, per a 2024 guide. Workaround is raising favour or going to their map marker. Source: https://gl.ali213.net/html/2024-12/1586041.html (search summary, not opened)
- (derived) There are no persistent allied cultivators in the party sense; only temporary summons from some combat styles, destinies and artifacts. So no second-character slot exists to reuse for a remote player. Source: https://steamcommunity.com/app/1468810/discussions/0/3038229380318602209/ (search snippet, which thread ...
- (observed) Sect disciples can be assigned by a sect leader, and True Disciples sent on missions. These are management orders, not playable characters. Source: https://steamcommunity.com/sharedfiles/filedetails/?id=2597185200
- (observed) Bagua Jade teleports the user to the holder of the other half, once a month. Could be a thematic hook for player-to-player teleport. Source: https://tale-of-immortal.fandom.com/wiki/Bagua_Jade (search snippet)
- (observed) A Tao Seed can be transferred to a family member, so a kin NPC can take over in a generational sense. Source: https://tale-of-immortal.fandom.com/wiki/Taoist_Mind (search snippet)
- (observed) Each character is presented as having its own friends, family and life trajectory, independent of the player's progress. Source: https://ec.nintendo.com/apps/0100FB701C48C000/US (search summary, marketing text)
- (observed) The game also ships on Switch (2024) and Android and iOS (2025), per Wikipedia. Source: https://en.wikipedia.org/wiki/Tale_of_Immortal

Unknown after this area:

- World generation: whether a world seed exists, is exposed, or is fixed at creation. Local text search found no seed or world-seed string; no source describes it. Example.cs only shows CommonTool.Random.
- Whether month simulation is deterministic given the same save. Not sourced; the Akulia notes report secret realms and encounters differing between players.
- Total simulated NPC count in a default world, and per-region counts. No official number found.
- Whether all NPCs are simulated in full each month or only by region.
- Save timing in detail: exactly when autosave fires, whether saving on exit exists, and how the 'not enough days' rule works. Only string text and 2021 player posts are available.
- Whether the DES and GZip save encoding in ggse still applies to build 21758240. Not tested; would need an isolated copy.
- When Steam Cloud was re-enabled, and which folder it syncs.
- What 网络验证 verifies, when it fires, whether it blocks offline play, and all network endpoints the base game calls. Needs a capture in an isolated copy.
- Whether any official developer statement on multiplayer exists. Official Steam announcements, Weibo, Bilibili and Zhihu were not reached; gcores and 3dm interviews were seen only through summaries.
- biligame wiki: the correct URL was not found (gg8h guess returned 404). Fandom pages returned 402 to WebFetch, so only snippets were used.
- Steam guide 2597185200 past character 100000 not read.
- Values of npcGoal and persistenDieUnit parameters in GameParameter.json not read.
- Identity and status of the 诸天万界 mod: URL not confirmed, not opened.

### archives-video

- (observed) Three-part question 1 is unresolved: no text found names the mod used in the 2023 联机对抗赛 series. Source: https://www.bilibili.com/video/BV11d4y1f7v7/ ; https://www.bilibili.com/video/BV1Hh4y147Si/
- (derived) The BV1LrpC6CEy2 claim of 全端支持联机 has no support on the page: it survives only in an earlier search snippet, while the fetched title, tags and page text carry no multiplayer statement. The title may have been edited, but that is unconfirmed. Source: https://www.bilibili.com/video/BV1LrpC6CEy2/ ; earlier sweep in wf-summary.txt
- (derived) The AI-made mod by 每周五更新的银行保安 was announced as a test release on 2026-08-30, but no public download was found. The only route shown is contacting the uploader. Treat it as not publicly released, with private sharing possible. Source: https://www.bilibili.com/video/BV1vv4X6VEYd/
- (derived) 诸天万界 appears to use a shared room system: the author says earlier rooms were full and a player was placed in a new one. This suggests a central relay or a Steam lobby, but the transport is not stated. Source: https://steamcommunity.com/sharedfiles/filedetails/comments/2958503186
- (derived) The removed and incompatible wording that tools report for Workshop 2958503186 must not be trusted as fact. NOTES.md records that both strings are hidden display:none template blocks that also appear on an unrelated item. Current status needs a check that ignores hidden blocks. Source: games/tale-of-immortal/NOTES.md lines 116-120 ; ...
- (observed) The date for BV11d4y1f7v7 conflicts: the page shows 2023-05-11 19:05 and the earlier sweep's API record shows pubdate 2023-05-14. The cause is unknown. Source: https://www.bilibili.com/video/BV11d4y1f7v7/ ; wf-summary.txt line 95
- (observed) Nexus mod 87 is still unread. Wayback availability reports no snapshot for that URL, CDX returned 429, WebFetch cannot reach web.archive.org, and a new search did not surface the page. Source: https://archive.org/wayback/available?url=nexusmods.com/taleofimmortal/mods/87
- (observed) archive.org full-text search for "Tale of Immortal" returned 5 items, none a co-op mod by title: a Steam manifest item gegamomanifest-1468810, a trailer youtube-xNGw2v7hhfk, a wuxia game collection, and two unrelated items. Source: https://archive.org/advancedsearch.php?q=%22Tale+of+Immortal%22
- (observed) No web page in this round tried to give instructions to agents. Source: all pages listed under searched

Unknown after this area:

- Which mod the 2023 联机对抗赛 used. The next useful evidence is the rules video's spoken content, or Tieba thread https://tieba.baidu.com/p/8511951874, which returned 403 earlier.
- Whether tournament matches were real-time play between two humans or imported characters.
- What BV1LrpC6CEy2 actually shows, which needs viewing the video; its current title and original title history are also unknown.
- Whether the AI-made mod was shared privately, how it loads, what it syncs, what '11我' means, and any link to Akulia's mod or Immortal Co-op. The bilibili comments were not readable.
- Nexus mod 87 description, bugs tab and download counts: still unread.
- ModDB and itch.io: both search pages returned 403, and WebSearch found nothing there. This is not proof that no listing exists.
- YouTube: checked only through one web search, with no direct YouTube search and no video reviewed.
- archive.org 鬼谷八荒 search: only the first 20 of 644 hits were seen, mostly newspapers.
- No video in this round was actually watched, so the 'what each video really shows' judgements rest on titles, tags and descriptions only.
- Bilibili search and API detail: blocked. The cookie retry was refused by the permission classifier and needs the owner's decision if this data is wanted.
- Current live status of Workshop 2958503186; the removed text may be the hidden template string.

### engine-siblings

- (derived) No published repo was found that is an exact stack match (MelonLoader 0.5.x, Unhollower, Unity 2020.3). SkyCoop is the closest: it references MelonLoader 0.5.3.0 with Unhollower. Its Unity version was not determined. All other co-op repos found target MelonLoader 0.7.3. Source: https://github.com/Filigrani/SkyCoop/blob/HEAD/SkyCoop.csproj
- (derived) All co-op repos where authority was stated are host-authoritative: SkyCoop by embedded server, apfep-coop, LYCOREIA, Ranching Together (host save), PVZ. None used a dedicated server. BONELAB Fusion and SR2MP authority models were not read. Source: https://github.com/Filigrani/SkyCoop/blob/HEAD/Server.cs ; https://github.com/Nreel/apfep-coop ; ...
- (derived) MelonLoader 0.5.4 updated to Il2CppAssemblyUnhollower v0.4.18.0 and fixed TLS Bridging Failure on Unity 2021.2.7 x64 Il2Cpp. Unity 2020.2.0+ start screen support was fixed earlier (changelog line 312). v0.5.3 temporarily disabled the Start Screen on Unity 2020.3.22 and higher. This game's ... Source: https://raw.githubusercontent.com/LavaGang/MelonLoader/v0.5.7/CHANGELOG.md
- (observed) MelonLoader 0.5.5 fixed a Start Screen issue with Signature Scanning GfxDevice on Unity 2020.3.15 x86 Mono. That is a Mono x86 fix and does not apply to this game. Source: https://raw.githubusercontent.com/LavaGang/MelonLoader/v0.5.7/CHANGELOG.md
- (observed) MelonLoader launch option --melonloader.agfvunhollower forces the assembly generator to a specified Unhollower version. Source: https://raw.githubusercontent.com/LavaGang/MelonLoader/v0.5.7/README.md
- (observed) On IL2CPP, MelonLoader patches with a native hook on the copied method info pointer (MelonUtils.NativeHookAttach) and a shim delegate. Line numbers are from tag v0.5.7, not the 0.5.4 this game ships. Source: https://github.com/LavaGang/MelonLoader/blob/v0.5.7/Dependencies/SupportModules/Il2Cpp/HarmonyMethodPatcher.cs ...
- (observed) Harmony inlining gotcha: MelonLoader warns when GetIl2CppMethodCallerCount is not above zero and the method is not a Unity magic method, with the text that it may have been inlined and your patch may not be called. Treat this warning as a likely silent no-op in sync patches. Source: https://github.com/LavaGang/MelonLoader/blob/v0.5.7/Dependencies/SupportModules/Il2Cpp/HarmonyMethodPatcher.cs ...
- (observed) Transpilers on IL2CPP only see Unhollowed IL, not the game's logic. MelonLoader warns that you should not use them on such methods unless modifying the Unhollowed IL is the goal. Prefix and postfix only in practice. Source: https://github.com/LavaGang/MelonLoader/blob/v0.5.7/Dependencies/SupportModules/Il2Cpp/HarmonyMethodPatcher.cs ...
- (observed) A community guide warns that Harmony patches on IL2CPP work poorly on class constructors and on generic methods and types. Secondary source, seen only through a search summary. Source: https://cementgb-4-0.github.io/docs/guides/runtime/introduction-to-il2cpp.html
- (observed) Unhollower known issues that matter for a sync mod: in/out/ref on generic parameter types such as out T in Dictionary.TryGetValue are broken; types implementing interfaces, especially IEnumerable, can be janky in foreach; non-blittable structs can't be used in delegates; nullables have issues from ... Source: https://github.com/knah/Il2CppAssemblyUnhollower/blob/master/README.md
- (observed) Unhollower class injection limits: virtual methods can't be overridden; only instance methods are exposed to IL2CPP, with no fields, properties, events or static methods visible to IL2CPP reflection; component fields are not deserialized from asset bundles; since 0.4.16.0 interfaces can be added ... Source: https://github.com/knah/Il2CppAssemblyUnhollower/blob/master/README.md
- (derived) Unity 2020.3: Standalone persistentDataPath is usually %userprofile%\AppData\LocalLow\<companyname>\<productname>, resolved through Windows known-folder APIs. No documented command-line argument changes it. Two copies of one build therefore share the save folder unless the mod redirects it or a ... Source: https://docs.unity3d.com/2020.3/Documentation/ScriptReference/Application-persistentDataPath.html
- (derived) Unity 2020.3 PlayerPrefs on Windows standalone live in HKCU\Software\<company>\<product>. Two copies with one company and product name therefore share keys under one Windows account. For this game the key name is expected to be HKCU\Software\guigugame\guigubahuang, which is inferred from the save ... Source: https://docs.unity3d.com/2020.3/Documentation/ScriptReference/PlayerPrefs.html
- (observed) The 2020.3 command line lists -single-instance (Linux and Windows only; launching again focuses the existing instance). It lists no argument that permits multiple instances. It also offers -screen-width, -screen-height, -popupwindow and -parentHWND, useful for tiling two clients. Source: https://docs.unity3d.com/2020.3/Documentation/Manual/PlayerCommandLineArguments.html
- (observed) PlayerSettings.forceSingleInstance makes a standalone player detect another running instance of the same player at startup and abort with an error (older docs wording). A Unity issue says the expected behaviour is focusing the running player, fixed on Linux in 2019.4.7f1 and 2020.1. Source: https://docs.unity3d.com/400/Documentation/ScriptReference/PlayerSettings-forceSingleInstance.html ; ...
- (observed) steam_appid.txt is needed only when launching the exe outside the Steam client, belongs in the working directory, and should not ship. It affects SteamAPI_RestartAppIfNecessary. The Steam client must be running for the interfaces to work. Source: https://partner.steamgames.com/doc/api/steam_api ; https://kb.heathen.group/steam/steam_appid.txt
- (observed) Some games use Steam to block a second launch or need distinct Steam IDs for matchmaking. A 2014 AI War forum reply claims direct launches bypass Steam's running check. That claim is old and is about another game. Source: https://universalsplitscreen.github.io/docs/goldberg ; ...
- (derived) A precedent exists for two same-PC instances of a Unity IL2CPP MelonLoader game in automated QA: separate install copies and a plain UDP QA transport that avoids Steam identity, with a bot driving menus from env vars. Its save and PlayerPrefs isolation method is not stated, and qa.ps1 has no ... Source: https://github.com/Nreel/apfep-coop/blob/HEAD/tools/qa.ps1
- (observed) Save-safety precedents: Ranching Together forces backups before host, before connect and after disconnect, and overwrites the joiner's save. apfep-coop suppresses puppet saves at the writer call. Both models are relevant because two clients share one save folder on one PC. Source: https://github.com/pyeight/SlimeRancher2Multiplayer ; https://github.com/Nreel/apfep-coop

Unknown after this area:

- Whether the boot.config single-instance= line is honoured by the Unity player, and what value disables it. Two searches found no documentation for boot.config keys. It must be tested on an isolated copy, and editing ...
- Whether Unity 2020.3.9f1 enforces single instance by default, and by what mechanism (mutex or window lookup). Whether a second copy in another folder counts as the same player.
- Whether two copies of this game launched directly can both initialise Steam on one account, and whether either client refuses to run. No current source; a 2014 forum reply is the only claim. This must be tested in the ...
- How to isolate persistentDataPath and PlayerPrefs without a second Windows user. No documented Unity argument exists. Options a mod could use (redirecting the path with a patch, patching PlayerPrefs) are ideas, marked ...
- No explicit MelonLoader statement of support for Unity 2020.3.9 IL2CPP on 0.5.4 was found. The game ships it, so it works in practice, but the changelog does not say so directly.
- Unity version of The Long Dark at the SkyCoop 0.5.3 commit; whether SkyCoop has been built against anything newer at HEAD.
- BONELAB Fusion: IL2CPP status, MelonLoader version, authority model, layer file paths. A guessed path returned 404 and a tree grep for Layer and Proxy found nothing. The wiki was not read.
- SR2MP (Egor935): loader and authority model not confirmed.
- SkyCoop puppet implementation details not read; packet list only.
- Megabonk multiplayer mods: GitHub search hit the rate limit; nothing recorded.
- apfep-coop's 11-check QA pass is the repo's own claim; not rerun or independently checked.
- Search-summary items (cementgb guide, SteamNetworkLib) were not read directly at source.

### Completeness critic

Research is about 85% complete for the decision the owner has to make. The path (use an existing mod, Akulia's first) is well supported, and more web or video searching is unlikely to change it. What is still open is mostly about making the bot test possible. Three gaps could change the owner's actions.  1. Whether two copies on one PC can really run a host and a client session. NOTES line 61 still says "Not tried". The strings suggest same-PC support, and local/lab/inst/1/UserData/toi_mp_lobby.cfg already holds 127.0.0.1. Nobody has read how the mod identifies a player, so two clients on one Steam account may collide. That decides whether the owner must be asked for a second Steam account. 2. Failure reports for Akulia's mod are thin. The 6 Workshop discussions and full comments returned HTTP 429 earlier. Only the titles of the 无法联机 threads are known, and every player report predates ...

### Follow-up: Akulia DLL read for two-copy feasibility, player identity and transport (local, no web)

- (observed) Q1. TOI_MP_dev.txt has no connection to same-PC play. CoopPve.DevPath = MelonUtils.UserDataDirectory\TOI_MP_dev.txt. Its only reader is CoopPve.InstallInvite (File.ReadAllLines in try/catch). It parses only coopDenyTypes, coopDenyDungeons and coopSpendTypes as comma-separated ...
- (observed) Q1. The 127.0.0.1 in the code is only a default IP, with no loopback mode: MenuMod._hostIp, LoopbackTcpTransport._hostIp, StartClient when hostIp is empty, TitleLobby.OnConnectClick when the box is empty, and TitleLobby.LoadLastIp. Lang also tells players 'Same PC: 127.0.0.1'. ...
- (observed) Q1. toi_mp_lobby.cfg is the last IP typed. TitleLobby.OnConnectClick calls SaveLastIp, which writes UserData\toi_mp_lobby.cfg; LoadLastIp restores it if it is 1-63 chars. local/lab/inst/1/UserData/toi_mp_lobby.cfg contains exactly 127.0.0.1 (9 bytes), per instance. It is not a ...
- (observed) Q2. The mod uses no Steam identity: no Steamworks, SteamUser or GetPersonaName reference in the DLL, and no MachineName. Three IDs exist. (a) Session id: PeerHandle. Host = 1. AddConn gives each client ++_nextPeerId (p2, p3...), sent by PeerAssign; until then a client uses ...
- (observed) Q2. Two clients on one Steam account do not collide, so a second Steam account is not needed for this mod. Nothing rejects a peer: AcceptLoop/AddConn accept every TCP connection with no check on IP, name or Steam, and a build mismatch is only an overlay warning ...
- (observed) Q2. Only same-CHARACTER (same unit id) is checked, not same person. The runtime check only warns: 'if (!_sameCharWarned && text2 == CoopMerge.LocalPlayerId()) { ... [L6c] YOU AND YOUR FRIEND ARE PLAYING THE SAME CHARACTER ... }'. PeerRouting logs '[NET][R] ... FLAPPING ... two ...
- (derived) Q2. Identical names have a soft risk. On one PC every client's name is the same Environment.UserName. Host and one client still resolve by role row (ArmBindFromMarker: item[0]==role && (text3==null || item[1]==name)). With TWO clients of the same name, the loop keeps the last ...
- (observed) Q3. Only the host binds 47813: TcpListener(IPAddress.Any, 47813) and UdpClient on 0.0.0.0:47813. The client makes an outbound TCP connect (ephemeral, seen as 127.0.0.1:49377 in r1) and UdpClient on port 0. The host learns client UDP endpoints from keepalives. One host plus N ...
- (observed) Q3. The port cannot be changed: const int Port = 47813 in LoopbackTcpTransport and TitleLobby, used literally at every bind and connect, with no config key or argument. Changing it means patching the binary.
- (derived) Q4. Joining is direct unicast IP typed into the lobby box. There is no LAN broadcast, multicast or discovery: the only 'Broadcast' hits are the mod's own app-level delegates and NoteMatchBroadcastTx. The IP list in the lobby is display only. So a VPN without broadcast is fine in ...
- (observed) Q4. Gotcha: the address must be a literal IP. ConnectWithRetry and StartUdp use IPAddress.TryParse and silently fall back to IPAddress.Loopback on failure, so a hostname such as a Tailscale MagicDNS name would connect to 127.0.0.1, not fail. Host-side, the [NET] log still prints ...
- (observed) Q5. Prompt condition: BeginHost calls FirewallHelper.EnsureHostRulesAsync; the first host start in a process (Interlocked _checked) runs EnsureHostRules on a thread; if netsh does not show BOTH 'TOI-MP 47813 TCP' and 'TOI-MP 47813 UDP', it starts cmd.exe with Verb=runas. ...
- (observed) Q5. The prompt does not block hosting: StartHost runs straight after on the main path, and declining only logs '[B4] firewall permission DECLINED - hosting still works locally; a LAN/VPN client may not reach you'. So loopback testing needs no rule. If accepted, it adds ...
- (derived) Q5. r1 printed no firewall line at all in any Latest.log, although it went through BeginHost ('[SESSION] Role = Host'), and CoopQA.Bot.dll contains the strings FirewallHelper and EnsureHostRules, with state.json patched=true. So the bot appears to suppress this code path. If so, ...
- Unknown: Whether host-side transport.IsConnected drops to false when the last client leaves; CleanupConn was only partly read, so the host-side '[NET] disconnected' and '[B69]' behaviour on a one-client drop ...
- Unknown: Exact method by which CoopQA.Bot suppresses the firewall path: bot source was not found, only strings in the DLL.
- Unknown: Whether Tailscale or another non-broadcast VPN actually works: no test, code reading only. Whether the host's lobby IP list shows the Tailscale adapter.
- Unknown: Whether the game itself blocks a second instance later than the title screen, or whether Steam behaves differently past world load on one account. r1 proves title and lobby only.
- Unknown: Whether shared-world sync works with two copies on one PC: not reached in r1, and the Workshop page's '5 players' claim is unverified here.

### Follow-up: Akulia failure reports and post-v1.1.10 reports on Steam

- (observed) The newest change note is still v1.1.10, 'Fully shared dungeons', dated 27 Oct 5:44pm in the page's cookieless time (UTC-7, so 28 Oct 00:44 UTC). There are 6 change notes: 27 Oct, 20 Oct, 17 Oct twice, 1 Oct twice.
- (observed) The item page shows Updated 28 Oct 6:07am, about 12 hours after the newest change note, with no matching note. Whether the file changed then or only the description is unknown. Description text still says v1.1.8.
- (observed) Item stats on fetch: 3.315 MB, posted 1 Oct, 2,823 unique visitors, 1,030 subscribers (NOTES says 1,029), 91 favorites, 11 comments, 6 discussions, 29 collections.
- (observed) NOTES correction: 8 and 12 Oct are the last-post dates of the 无法联机 threads. They were opened on 5 Oct (无法联机游玩) and 7 Oct (无法联机).
- (observed) 5 Oct stuck-on-connecting report: 蘭夢琴 wrote on 5 Oct that as host it kept showing connecting to host. Author's explanations: (a) 5 Oct, that text shows only on the joiner side, so the player clicked Join; (b) 7 Oct, Chinese labels were mistranslated; (c) 8 and 9 Oct, multiplayer ...
- (derived) Likely root cause of the early reports is the broken Steam build in (c), not the button mistake alone. No reporter posted a confirmation of any cause. 琼莹's cmd check found no listening port on Create Game, which fits a broken build better than a wrong button.
- (observed) There is no session browser. Host clicks Create Game, joiners click Join Game and type the host's LAN or VPN IP. On first host creation Windows Firewall prompts for access, or port 47813 TCP and UDP is opened by hand. Author names Radmin VPN, Hamachi and ZeroTier.
- (observed) After 27 Oct there is no player report anywhere on these pages. The only posts dated after the note are three Akulia replies on 28 Oct. The newest player post is 阿九, 24 Oct. The newest comment on the item is 21 Oct. So v1.1.10's fully shared dungeon claim is neither confirmed ...
- (observed) v1.1.10's stated scope: quest dungeons with an NPC, room events and chests work with friends. The fight starter handles NPCs, events and chests; others see them and pause while the starter talks to an NPC. Only the starter opens chests; every player in the room gets the contents.
- (observed) Akulia, 21 Oct: secret realms, mining and qi points are generated by each player's own world on purpose, so friends can be invited into fights in one's own world and quest timelines do not jam; spawn-area points match because they are fixed. The 28 Oct reply does not retract ...
- (derived) 'Fully shared dungeons' most likely means shared dungeon contents once inside a fight, not the same dungeon placement on the map. This limits the NOTES line 'share one world and one save': map points stay per player, at least as of the last statement. Not tested.
- (observed) Akulia, 28 Oct: in co-op your character exists in a friend's world as an NPC copy. He guesses the friend's world NPCs interact with that copy at the month change, which would explain unearned history entries such as dual cultivation. Reported by 阿九 on 23 Oct, still being checked.
- (observed) Save damage, 24 Oct: 阿九 reports 坏档 after the 噬魂塔 fight. The joiner's gourd would not open; after rolling back, the host's gourd would not open and the joiner could not move.
- (observed) Author's cause, 28 Oct, 'very likely the same issue', seen in his own 3-player test: a sect elder NPC died on one player's PC, the other client's built-in sect elder repair looks for it on every world entry and throws, world entry never finishes, so panels, movement and month ...
- Unknown: Whether v1.1.10's fully shared dungeons works in practice: no player report after 27 Oct on Steam. Nexus posts and the Discord were not read.
- Unknown: Whether v1.1.10 fixed the 24 Oct save damage, the group loss after a drop, or the purple squares: only the author's claim, no reporter confirmation.
- Unknown: Whether map points are still per player in v1.1.10: last author statement is 21 Oct, and the 28 Oct reply neither repeats nor retracts it.
- Unknown: What changed in the 28 Oct 6:07am item update, which has no change note.
- Unknown: Whether 琼莹's local fix for player position was merged. Whether the 1.1.5 visibility bug still exists: author asked on 21 Oct, no answer.

### Follow-up: Nexus mod 87 through the documented public API, not the blocked web pages

- (observed) Nexus mod 87 version is 1.1.10, which matches Workshop v1.1.10 at the version-string level. The mod-page version field does not prove the two files are the same build.
- (observed) The author's way to confirm build equality is the line '[MENU] BUILD = ...' near the top of <game folder>\MelonLoader\Latest.log, which must be identical on every machine. The author says a mismatch gives no clean error, only strange behaviour.
- (derived) NOTES line 139 stays open. The 13095-character description contains no license, redistribution or source-code statement, and the API's Mod type exposes no permissions or license field. The only related wording is thanks to third-party projects under BUILT WITH. Absence here does ...
- (observed) Author-stated install: the single file TOI_MP_MenuMod.dll goes in <game folder>\Mods. The author says a MULTIPLAYER button then appears on the title screen.
- (observed) Author-stated loader requirement: MelonLoader 0.5.4, installed by the game's own mod support. The mod reportedly will not load if it was manually replaced with 0.6.x or newer.
- (observed) Author-stated networking: direct connection to the host with no account, server or external service. Host accepts a Windows Firewall prompt that opens TCP and UDP 47813. Players join by typing the host's address, over VPN LAN (Radmin VPN, Hamachi, ZeroTier) or real LAN.
- (derived) The firewall step is relevant to QA: the host prompt may need the owner's approval, and a same-PC test would use the same port 47813 unless the host and join clients can share it. The second point is untested.
- (observed) Author-stated topology: the host's machine owns the world. If the host leaves, the session ends for everyone, and host migration is not planned.
- (observed) Author-stated sync model: players roam freely within each in-game month, then all machines re-converge at month end, with a panel reading 'Players ready: 2/3'. The author reports regular testing with five players at once. Each player needs their own copy of the game.
- (observed) Author-stated save behaviour is mixed: multiplayer uses its own save files and single-player saves are not touched, yet the roadmap says a character played in multiplayer keeps some of what it gained if later loaded alone. The author recommends backing up saves first. This ...
- (observed) Stated log paths: <game folder>\MelonLoader\Latest.log, older sessions in <game folder>\MelonLoader\Logs\, and crash log %USERPROFILE%\AppData\LocalLow\guigugame\guigubahuang\Player.log. The last path also indicates the Unity company/product folder name guigugame/guigubahuang.
- (observed) Author-stated limits: not multiplayer yet are sect master succession, True Elder and ranks above Elder, sect wars and spectating. Left-click attack visuals differ between screens in PvP. Trade window has no item icons. Not tested alongside other mods.
- (observed) Author credits MelonLoader, HarmonyX, Il2CppAssemblyUnhollower and Claude Code as build tools.
- (derived) The Nexus author name is spelled Akuliaa, while NOTES and the task use Akulia. Whether this is the same person as the Workshop author is unconfirmed, though the mod name and version match.
- Unknown: License and Permissions of mod 87: unknown. The owner must open https://www.nexusmods.com/taleofimmortal/mods/87 in their own browser and paste the Description, Permissions and Bugs text if they want ...
- Unknown: Bugs tab content for mod 87: unknown; not requested from the API in this pass.
- Unknown: Whether the Nexus and Workshop copies of 1.1.10 are byte-identical or share the same '[MENU] BUILD' value: unknown. No file hash or file list was retrieved, and nothing was downloaded.
- Unknown: Unit of fileSize 1010: not stated in the response.
- Unknown: Whether the file carries a published SHA-256: unknown from this query. No hash field was retrieved, so this does not settle the Rule 8 item.


## Bot test log, 2026-11-02

Each run below used two real clients on one PC, one Steam account, port 47813 on loopback, and `tools/gamepool` plus `tools/gameslot`.

| Run | Result | What it taught |
|---|---|---|
| r1 | 5 of 6 | Host and join worked. The bot pressed Close on the lost mod warning. Fixed the bot. |
| r2 | 7 of 8 | Merge retried for 7 minutes. The host save never finished. |
| c1, c2 | control | A plain save with no session also never finished on one test save, minimized or in a normal window. Mod and window mode ruled out. |
| r3 | 7 of 8 | Clean world as host: merge done in seconds. Client stalled entering it, because it had loaded the broken slot. |
| r4 | 8 of 9 | Mod logged SHARED WORLD ACTIVE with no merge, because both folder names matched. The character check caught it. |
| r5 | 14 of 14 | Through movement, with a real merge. |
| r6 | 18 of 19 | Client waited on a story dialog the bot did not answer. Fixed the bot. |
| r7 | 19 of 20 | Month passed, 3 NPCs differed after it. |
| r8 | 24 of 25 | All stages ran, 1 NPC differed after month 1, 0 after month 2. |
| n1 | 16 of 17 | Negative control: client dropped NPC death messages, `npc_death_reaches_client` failed as required. |
| r9 | 24 of 25 | Repeat: 6 NPCs differed after month 1, 0 after month 2. |

Observations:

- One test save throws NullReferenceException in `MapBuildSchoolData.InitRule` during `WorldBuildMgr.Init`, because its world needs a mod this copy lacks.
  `UIMapMainPlayerInfo.UpdatePlayerInfo` then throws every frame inside `TimerMgr`, so saves and loads scheduled on the timer never finish.
  A save that shows the lost mod warning can therefore break hosting and joining. Both clients need a world that loads cleanly.
- Saves copied here came from a hash-verified backup and stayed in the test company folders.
- The mod sets shared world active by comparing folder names, so two equal names report it with no merge.
- `tools/gamepool` window values are minimized, hidden and normal. An unknown value silently means hidden.
- The game reads `SOFTWARE\guigugame\guigubahuang` directly in `FixPatch139.RetrieveSavedPrefs`, even in a copy with another company name.
  The copies read the real PlayerPrefs (49 values). The hashes show nothing was written. Isolation here means no writes, not no contact.
- NPC deaths rolled in each machine's month simulation are not fully reconciled. The mod's own report lists the same ids and writes nothing.

Not covered: new character creation (runs use a prepared save), three or more players, separate PCs, a real VPN, the rejoin flow, lag and loss.
