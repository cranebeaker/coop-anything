# HOST/GAST input audit, test54 (Codex, 8 Nov 2026)

Owner: both players may initiate shared gameplay. HOST coordinates order/seeds and
save persistence. Hero ownership restricts combat equally on either copy.
Audit used delivered test53, owner duo36 logs21:54, real hook fixtures and exact EXE.
Automated evidence below does not replace live two-copy tests.

| Input | HOST/GAST route and evidence | Result |
|---|---|---|
| Hamlet placement, recruitment, dismissal, rename, party, skills, purchases, trinkets, heirloom exchange | Shared authority/FIFO, `runtime_hamlet_fixture.inc` and main bridge fixtures | Both can initiate; local screens can remain different. |
| Provisions open/buy/refund/Back | `runtime_prepare_fixture.inc`, `runtime_supplies_fixture.inc` | Both can initiate. Other screen may stay closed; shared background shop is intentional. |
| Embark | Previously only HOST's RaidStart sent the departure. GAST's callback ran privately. New `runtime_embark_fixture.inc` dispatches the actual protected callback, acknowledgement, then host-seeded DEPARTURE | Fixed GAST-first with HOST in Hamlet/background shop; HOST-first retained. Pending buys finish first, duplicate/crossed calls start once. |
| Corridor movement | Existing GAST intent message already allows GAST to move the party. HOST's opposing key had priority | Fixed: native inputs captured before movement; opposing directions stop, agreeing directions move once. Stale intent expires after300ms. |
| Doors/map travel, curios, traps/tools | `runtime_walk_fixture.inc`, request/apply paths in `dd_walk_bridge.inc` | Both can initiate; area/loading/door guards remain. Unknown/unreadable prop layouts still need investigation if encountered. |
| Torch, bag/loot items and trinkets | `runtime_battle_fixture.inc`, Hamlet fixture, item bridge | Both can initiate; same ordered native effects. In combat only own hero at its choice boundary. |
| Hunger eat/refuse | `runtime_hunger_fixture.inc` + exact-EXE proof | Both directions, zero/two food included; previous fix retained. |
| Camp food/skills/leave and retreat | `retreat_and_camp_checks` in battle fixture | Both input paths present; native replay and readiness tested. Retreat during combat follows the same own-hero rule for each player. |
| Quest completion choices and top-left completed seal | Battle fixture, `runtime_quest_return_fixture.inc` | Both initiate; existing test53 seal fix retained. |
| Results Next | `results_and_bag_checks` in battle fixture | GAST asks HOST; HOST applies and broadcasts. Existing3s fallback after a repeated unanswered GAST click is retained; this remains a potential drift/recovery edge case. |
| Hero ownership changes | Existing launcher claims protocol + live runtime owner fixtures | Both launchers can request their own claims; only authenticated HOST table becomes canonical. |

Still open: actual live confirmation of this release, explicit resync/restart UI,
Guild auto-select of a newly learned camping skill, and DLC-specific screens/actions.
No claim that every unknown native input or DLC transition has been audited.
