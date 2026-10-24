# CP2077 工作笔记（中文，2026-10-23）

纯调研 write-up，未购买/安装/运行游戏，符合 AGENTS.md 红线。

## 数据来源与验证方式

- GitHub API `repos/<owner>/<repo>`：pushed_at / stars / license / archived（2026-10-23 实测）
- README 经 `api.github.com/.../readme`（raw.githubusercontent 直连返回空，API 端点可用）
- 官网直抓：nightcoop.net（俄语原文"Открытая бета идёт"=开放beta进行中，今天15:00 UTC 开）、open2077.net、cyber.mp（200 存活）
- 新闻面：Polygon / PCGamesN / GamingBible / player.one / kocpc（NightCoop 与 CyberMP 的区分报道）

## 生态结论（10 个项目）

1. NightCoop — 今天(10-23)开放beta，prologue+Act1，8人，剧情投票制，闭源免费。最有戏的剧情线。
2. CyberpunkMP (Tilted) — 2024-12-21 最后push（API实锤），patch 2.2 时代停摆。64★。
3. NightCityMP — CyberpunkMP 续命fork，2.31a，2026-07 活。沙盒向，NPC/任务/战斗伤害不同步。
4. oxylol/cp2077-coop — story-host 层，host 每2秒推时间/天气/追踪任务，guest 时钟只前进。2026-10-10 push。
5. KyleBuildsAI/cp2077-coop — v0.0.37/alpha.5 可下载（CET面板+Python relay 11778），已并入 Bukczyk session 基础。2026-10-20 push。
6. Bukczyk/CP2077-Coop — typed session 基础，一机双客户端诊断已达 JOINER→HOST 开火验伤验证；重连死亡恢复仍失败。2026-10-20 push。
7. RedSync — AGPL-3.0，RED4ext+redscript+.NET9 服务器，quest-fact 白名单中继（PL q301-q307）；README 下载链接还是占位域名（your-domain.com），未正式发布。2026-08。
8. OPEN//77 — 专用服务器平台（RP/freeroam/Lua脚本），2026-10 仍活跃（org 多仓库 10-22 push）。非剧情联机。
9. CyberMP — 媒体型条目维持，2026-06 报道，无公开下载，官网存活。
10. Chaoskjell/Coop-Cyberpunk-Mod — MIT，v0 脚手架（CET Lua+Python文件桥），2026-08 一天burst，无玩法同步。

## 技术面共识（写进 write-up 的 "How the game works"）

- 全社区栈：RED4ext(C++/RTTI) + redscript(@wrapMethod) + CET(Lua/UI) + ArchiveXL/TweakXL + Codeware
- 版本敏感是最大维护成本：2.31 RTTI 改名导致游戏拒启（上游PR#56-60 修复，oxylol fork 已含）
- 远程玩家=傀儡/替身NPC（Codeware/AMM）；NightCoop 例外：每个 guest 自带自己的 V
- 世界权威模型：NightCoop/oxylol=host 存档即世界；NightCityMP 规划专用多人存档
- 网络全自建：UDP / GameNetworkingSockets / Python relay；NAT 用 relay hub 或 Tailscale
- 公开项目里共享 NPC/敌人模拟无解；最深公开任务同步=RedSync quest-fact relay

## directory.yaml 改动清单

- 4条既有：checked→2026-10-23；CyberpunkMP last_activity unknown→2024-12+stale标注；oxylol/KyleBuildsAI last_activity unknown→2026-10；notes 增补
- 6条新增：NightCoop / NightCityMP / RedSync / OPEN//77 / Bukczyk / Chaoskjell，插在 KyleBuildsAI 条目后保持同游戏连续
- 跑 build_directory.py 重生成 DIRECTORY.md（CONTRIBUTING 要求）

## 后续待办（给下一位）

- NightCoop beta 社区口碑落地后 qa: none→community-reports
- 有游戏副本的 agent 可实测 KyleBuildsAI v0.0.37 并回报
- Nexus 深页/俄语中文社区/Discord 存档未搜
