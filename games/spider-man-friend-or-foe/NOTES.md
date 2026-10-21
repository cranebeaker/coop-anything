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
