# Directory entry schema

`data/directory.yaml` is a list of entries. One entry = one co-op project for one game.
A game can have several entries (competing or successive mods). `tools/build_directory.py`
renders `DIRECTORY.md` from it, so edit the YAML, never the table.

```yaml
- game: Deus Ex                       # display name of the base game
  year: 2000                          # base game release year
  engine: Unreal Engine 1             # engine family, see playbook/engine-families.md
  project: HX                         # mod / project name
  url: https://example.org/hx         # main page (repo, Nexus, ModDB, site)
  other_urls: []                      # mirrors, Discord invite pages, forks
  kind: mod                           # mod | fork | decomp-port | official | emulator-netplay | framework
  status: alpha                       # playable | partial | alpha | prototype | abandoned | official
  coop_scope:                         # what is actually shared, only what a source confirms
    players_visible: yes              # can players see each other at all
    story: yes                        # yes | partial | no | unknown
    enemies: yes
    world_state: partial
    items: per-player
  players: "2-8"                      # as stated by the project
  networking: direct-ip               # direct-ip | vpn-needed | steam-relay | dedicated-server | hosted-relay | lan | unknown
  open_source: no                     # yes | no | partial (license if known in notes)
  last_activity: 2019-06              # YYYY-MM of last release/commit you could confirm
  qa: community-reports               # see QA levels below
  notes: Last official build 0.9.89.4. Revival fixes in games/deus-ex.
  sources:                            # every claim above must be backed by one of these
    - https://example.org/source
  checked: 2026-10-16                 # date the entry was last verified
  up_for_adoption: false              # optional; true = the author stopped and wants someone to take over
```

## QA levels (the flag that matters most)

| qa | Meaning |
|---|---|
| `none` | Nobody has reported testing it, or only the author's claims exist. |
| `community-reports` | Players report playing it (forum, Steam, Discord, Reddit), with no structured testing. |
| `automated` | A scripted test harness drives real game clients and checks shared state (see playbook/automated-qa.md). |
| `verified-playthrough` | A full campaign was finished in co-op and written up with known issues. |

Gaps (no project found) use `kind: gap`, `project: "(none found)"`, `status: none`, an empty
`url`, and a `searched:` list saying where you looked and when (for example "GitHub, Nexus,
ModDB, Steam forums via web search, 2026-10-16; Reddit not checked"). `sources` can then be empty.

Set `up_for_adoption: true` when the author has stopped working on the project and is happy for
someone else to continue it. Keep `status` honest about how far it got; the flag only says it
needs a new owner. These entries are also listed in their own section of `DIRECTORY.md`.

Rules: don't guess. Unknown is `unknown`. Every entry needs at least one source URL and a `checked` date.
