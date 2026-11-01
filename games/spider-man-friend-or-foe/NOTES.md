# Research notes: Spider-Man: Friend or Foe

Checked 2026-10-21. Implementation and detailed evidence live in the [separate source repository](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online).

## Identity and entry relationship

The inspected Windows executable is version 1.0.0.111, SHA-256 `ebd30047d7e58c95bf7dc4bd1fb4a5780e16aa472b61c25a8c421632662083db`. It supports two coherent language packs with shared gameplay content and differing localized files. Other builds are unsupported.

The three directory entries describe official local co-op, this native source prototype, and a previously listed Discord prototype. The latter's relationship to this source and executable is unknown. Earlier labels mixed a developer attribution with an Alchemy engine attribution; neither establishes an engine family for this inspected build. All three now use unknown pending corroboration. The [publisher announcement](https://investor.activision.com/static-files/d8459280-fceb-49b0-9210-86bb7010c3d3) establishes co-op/platform context, not engine identity.

## Observed and derived

The current [status record](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/STATUS.md) separates successful bounded bot gates, failed precursors and unverified features. Current scenario 11 passed a 7,200-tick three-area fixture, a 3,600-tick pause/combat fixture, and a 361-tick quiet gameplay lifecycle. These used same-machine processes; they do not establish WAN or complete campaign co-op. A one-account lobby smoke carried no game traffic.

Useful lessons from retained failures: stale exit volumes survived streamed-area changes; a checksum included uninitialized padding and inactive mode history; a fixture changed input after the measured native binding stage. The corrections retain semantic fields and continuous checks rather than suppressing mismatches. A later startup crash exposed an unbounded native key-name lookup. The bounded guard passes current tests, but the original layout/target trigger remains unconfirmed.

## Unknown and next gate

Retail New Game/Continue, hub and mission selection, native save/resume, localized NIS progression, physical controls and two-PC Steam delivery remain incomplete or unverified. Next is a finite retail New Game to hub to first-mission gate with synchronized native choices and load readiness. No human playthrough, public binary or complete-world oracle is claimed.

## External sources

- [Activision launch announcement](https://investor.activision.com/static-files/d8459280-fceb-49b0-9210-86bb7010c3d3): official co-op and platform context.
- [Contemporary PC review](https://www.swiftworld.net/2007/10/13/pc-spider-man-friend-or-foe-review/) and [GameSpot review](https://www.gamespot.com/reviews/spider-man-friend-or-foe-review/1900-6180451/): local co-op and lack of online support.
- [Valve networking documentation](https://partner.steamgames.com/doc/features/multiplayer/networking): intended transport API, not evidence of this mod's delivery.
- [Existing Discord listing](https://discord.com/channels/1561411702082510879/1562185850777772152): retained project reference; no account access or build verification performed here.


## 2026-11-01 — beta campaign, in-place joining and launcher update

Sources: [released v1.1.0-beta](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/releases/tag/v1.1.0-beta), [source revision](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/commit/9ff4f6a), [status and failed attempts](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/9ff4f6a/docs/STATUS.md), [harness](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/9ff4f6a/launcher/play_lifecycle.py), [launcher documentation](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/9ff4f6a/docs/LAUNCHER.md).

### Observed

- Exact Windows 1.0.0.111 executable SHA-256 `ebd30047d7e58c95bf7dc4bd1fb4a5780e16aa472b61c25a8c421632662083db`; engine/storefront provenance remain unknown. Tests use coherent English/Russian packs and isolated redirected test state.
- Real-client bots covered all 20 mission endings and seven bosses in a shortened campaign, host-only tutorial dialogs and native host slot/overwrite choices. This does not prove full-mission playthroughs or shared campaign save recovery.
- Protocol 6 / scenario 13 is published in v1.1.0-beta. Verified input-journal catch-up admits a guest into the current mission without resetting the host. Cancellation/history-refusal preserve host play. Poor-network first arrival 532 and swapped reconnect 535 matched 1,904 / 1,898 post-admission frames; Doc Ock 548 matched 2,074 replay ticks plus 1,887 subsequent frames. All listed passes closed normally; earlier failures remain failed in project status.
- New source-only WPF/single-EXE launcher changes passed eleven-language/five-state layout, native fixed-size/no-maximize, accessible-mode/notification and digest/data-preserving upgrade gates. Cross-volume discovery retained Valve signature/export checks and found compatible non-system-drive components without loading them or using an account.
- Native joiner-menu 005 passed 601 matching frames in English/Russian, with guest row 0/1 hide markers, host no hide markers, automatic host-selected NewGame and game/launcher exits 0. Runs 001/002 failed pre-game under restricted process launching. Run 003 passed 601 frames but did not prove hiding because it intercepted a base renderer.004failed when a temporary cursor relocation entered Options; that change was removed before 005. Options/Quit keep unchanged indices/render paths, with no separate post-hide click test.
- Owner-reported evidence: first Tokyo mission played with a friend on two PCs over real Steam. Do not promote this to an independently verified WAN/full-campaign gate.

### Derived

- Presentational hiding should preserve native menu indices. The actual derived main menu calls a generic row renderer; filtering the base renderer can appear safe while never affecting the target. Calling the native row routine with a null draw object preserves its cleanup and calling convention. The filter is restricted to the exact main-menu list, row count 4, guest role and waiting/start mode.
- A cursor relocation can change the meaning of an already-latched accept and break automatic startup. Test the hidden-menu state and host-driven startup together, not only a direct together-mode fixture.
- Native cutscene/loading/control-release readiness belongs in replay validation. Matching world position/RNG alone can miss local controls becoming active early.
- Single-file distribution still needs managed runtime/player-data storage. Hash/allowlist checks and synthetic upgrade sentinels are separate from game/network QA.

### Unknown / still pending

Shared save/Continue recovery; real controllers; a new two-PC Steam admission gate for in-place mission joining; broader later-mission and long-history coverage; native-speaker review of all translations; Windows scaling/mixed-monitor review for the newest fixed launcher. Relationship to the separate Discord listing remains unknown. No game files, raw logs, personal paths or account identifiers are included here.
