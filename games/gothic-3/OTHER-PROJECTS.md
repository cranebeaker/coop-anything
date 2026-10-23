# Earlier Gothic 3 co-op projects (checked 2026-10-16 and 2026-10-17)

None of them gave the owner usable story co-op, so G3 Together was started from zero, to
avoid repeating their limits. Nothing of their code or files is used in G3 Together.

| Project | What we could check | Result |
|---|---|---|
| [Myrtana Accord](https://github.com/yevh/Gothic3) v0.35.0-beta.1 (Yevh, 2026-09) | Official ZIP (SHA-256 `c2c5b2752755aad7d27009cc04bc47c515f93b32f0e2409b039ab0fdf8d0ae4e` and its 31 internal checksums matched; its five pinned stack hashes matched the install). Licence file: MIT "except where noted", nothing noted. C++ core source not in the archive or the public tree (state `327c0b0`). Tested with two real clients and shipped settings. | Quest state is shared both ways; nothing else is. Players do not see each other, NPC and combat sync are off, rejoining with a new game is refused. |
| Gothic 3 Heroes (Pigeonade) | A [hands-on report dated 2026-09-19](https://gothic.org.pl/gothic/modyfikacje/gothic-3-heroes-w-trybie-kooperacji/) names version 0.3, Steam invitations and item trading. Release only through Discord. | Not tested: no archive, licence or source available. |
| [Sative's Gothic 3 Multiplayer](https://www.nexusmods.com/gothic3/mods/1) | Update3 (2016-03-25), "Public Alpha", needs CP 1.75.14. The author lists inventory loss and unsynchronised enemy combat. A 2024 author reply in the forum index makes "abandoned" unproven. | Not tested: download needs a Nexus login. |

## Myrtana Accord test (QA level `automated`, scope: shared quest state)

Two full copies of the install with redirected profiles, staged by hand from the verified
ZIP (installer not run), one local `g3coop_server.exe` on 127.0.0.1, shipped settings. An
SDK plugin in each game took numbered commands from a Python runner; the owner's real
saves were hashed before and after every run (unchanged).

- `coop`, 3 of 3 PASS (2026-10-17, runs `000901`, `001012`, `001120`): both players
  admitted and listed by the other; three shared quests Open on both; the host runs
  `Montera_Revolution` and the client reads Running (4 s); the client completes
  `Faring_Revolution` and both read Succeeded (0.5 to 4 s); `Silden_Revolution` stays Open.
- Negative control (an impossible status demanded on the client): FAIL as required.
- `rejoin`: FAIL. After a new game the client logged "Campaign character binding
  deferred: reason=3" every two seconds and never got the campaign's quests.
- Closing the game during a campaign was refused by the mod in 3 of 7 runs.
