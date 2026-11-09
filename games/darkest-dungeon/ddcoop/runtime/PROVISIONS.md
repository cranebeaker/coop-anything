# Native supply integration, build 28385

Current state: see "Test15: shared supplies wired" at the end. The sections before it are the measured
foundation and describe the state at the time they were written (read-only observer, no writer enabled).

## Verified from the legitimate pinned executable

EXE SHA256 `f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12`.
All addresses below are RVAs; no third-party executable/code was copied. MSVC dumpbin static analysis
only, no game launch. These observations still require the owner's live measurement, especially the
panel lifecycle and stock reset on Back. Do not interpret fixture success as native-game validation.

| Normal campaign data | Current layout |
|---|---|
| Campaign / TownDisplay globals | `118e138` / `118e360` |
| TownDisplay primary vtable | `ec37a8` |
| Embedded ProvisionSelect | TownDisplay + `3770` (`17a8` + `1fc8`), primary vtable `ebdde8` |
| Visible / stock-view / bag-view | Panel + `60` / `80` / `88`; view-manager vtable `ee1420` |
| Stock Inventory / expedition bag Inventory | Campaign + `1618` / `15b0` |
| Inventory vector / occupied count | begin/end/capacity + `8/10/18`, count + `24` |
| Item stride / quantity / type / ID | `5a8` / `0` / `44` / `88` |
| Wallet vector | Campaign + `e70/e78`, stride `48`, quantity `0`, type `44` |

**Test14a correction:** the first observer incorrectly used TownDisplay +1fc8. The actual TownDisplay
constructor at6da730 passes this+17a8 into helper6dad90; that helper preserves its adjusted this in RSI
and embeds ProvisionSelect at RSI+1fc8 (6daf04), then sets its vtable (6daf54). Total offset is3770.
Owner's two first-observer logs report only unrecognised-layout, no snapshots; the old log does not
identify which check failed, so additional problems cannot be excluded. The offset bug is independently
proven from the exact EXE. A corrected constructor-based fixture failed on old reader before the fix,
passes with3770, and rejects a decoy matching vtable at the OLD address. No type check is bypassed.
`tests/test_provisions_layout.py` verifies the actual constructor instruction chain against the pinned
EXE rather than treating a fixture as evidence of native layout. The new capture_ex API reports a
static stage (panel type, stock vector/count/item, bag, wallet, roots) without dumping game memory,
addresses or credentials. It still discards the entire snapshot on failure.

**Owner14a / correction14b:** the supplied183340 observer log has one successful CLOSED snapshot:
stock14 slots empty, bag16 slots empty, five wallet currencies. It then rejects stock-item at37sec
and bag-item at50sec, until returning ready at121sec and game exit at125sec. No purchases/refunds
were captured, so no claims of correct native prices/cancel behavior follow from that log.
The exact EXE panel fix is now live-supported; another invalid assumption was exposed by nonempty
supplies. Original `inventory/base.supply.inventory.items.darkest` defines food as type `provision`,
ID empty string (hash0). Native buy6961f3/6961f9 compares BOTH type+40 and ID+84, including zero.
14a's generic nonzero-ID check incorrectly refused this genuine occupied stack. The stage alone
does not show field values, but the concrete defect is independently reproduced with original food.

14b retains zero item ID in snapshots when the EXACT (type,id=0) pair exists in the loaded item library
(global118e0f8; bounded16384 entries; definition type+40/id+84). No replacement IDs or dropped food.
Type must remain nonzero, quantity positive/bounded; quantity0 alone defines an empty slot. Unknown
empty-ID pairs, corrupt/changed/unreadable library fail and clear the full snapshot. A per-capture
bounded cache avoids repeated searches for the same empty-ID type; library root/span is rechecked
on lookup and at capture end. Types/IDs with nonzero ID retain the earlier reader semantics. Mode,
slot3, genuine panel/view, header, quantity and currency checks remain. No native writer is enabled.
Food regression failed14a before the fix. Tests cover14/16 slots,73010 starting gold, food buy/refund/
clear observations, every failed read on this definition path, unknown/malformed/multiple empty-ID
types, oversized library and lookup/final root races. Native shape check now also verifies the real
pair-comparison instructions. Updated source-only dungeon coordinator preserves food's zero ID in
departure/codec, with a separate regression; its native adapter must still verify actual catalogue
membership. No dungeon module is linked by this change. Actual open-supply14b measurement pending.

Campaign + `1838` is used in quest-related supply pricing/initialization; the reader deliberately
calls it `context_1838`. Its full meaning/quest identity is not yet established. It is NOT a seed.
No UI tree names or screen-state enum guesses are used. Panel + `90` is a focus/event object,
NOT an Inventory. Retained empty-item identity bytes are excluded from canonical observations.

Native calls audited:

- `6960c0`: buy one. Capture payload contains genuine panel, stock and bag pointers. Checks capacity,
  resolves actual supply prices, deducts gold AND shards, moves one item, updates UI. Visible input
  index may need remapping; a physical slot alone is not an adequate identity/price quote.
  std::function Do_call `697b60` (vtable `ebe018`) forwards its capture payload, not an engine object.
- `57bbc0` / `57bc70`: current supply gold/shard prices. Depend on actual quest modifiers and item
  definitions. Nomad prices/sale values are not a substitute. Definition base offsets `9c/a0`.
- `6978b0`: normal return-click Do_call (vtable `ebdf38`), captures bag. Resolves item, copies to a
  temporary Item, calls `7ebcf0(temp,1)`, then swaps into the real bag with `5df8f0` on success.
  `7ebcf0` refunds gold/shards and returns stock, but receives a TEMPORARY, not the live bag item.
  Free/unreturnable components matter. Never call that helper directly on the live bag stack.
- `6e0250`: refunds the bag in aggregate and clears bag AND stock (`5dffa0`). Cancel/Back lifecycle
  must be understood and synchronized before enabling purchases. A buy-only bridge will diverge.
- Panel enter `694df0`, pre-show `695460`, update `695120`, pre-hide `6953e0`. Enter constructs real
  views; hide frees/nulls them. View presence/visible transitions need the live test.

The old provision DDCP actions only carry slot/count and have no live native handlers. Do not enable
them as if they carried item identity, resulting inventory, authoritative quoted prices or lifecycle.
Future implementation must serialize purchases/returns/cancel under host authority, validate the
current pre-state and item identity, wait for UI/model readiness and actual native completion, then
check both resulting inventories AND currencies. Never fabricate C++ widgets or copy process pointers.

## Measurement and tests

`src/dd_provisions.c/.h` reads bounded stock128/bag64/wallet32, refuses unsupported larger models,
verifies normal mode, real mounted profile_2, loaded roster, genuine town/panel/view types, quantities,
unique currency keys and repeated root/vector headers. Partial failures clear the complete output.
The caller must first pin the EXE hash. Two equal consecutive captures reduce transient observations;
they do not stop the game, prove atomicity, or authorize writes.

`testkit/provisions-watch/` builds a separate /MT EXE using only Windows system APIs. It requires the
known game and loaded test13b runtime hashes plus `[net] role=guest`. It opens the game with VM_READ,
QUERY_LIMITED_INFORMATION and SYNCHRONIZE only. No injection/hook/remote allocation, write API, network,
save access or elevated privileges. It writes its OWN log under Desktop/DDCoop-metingen, using hashes
and counts rather than strings, addresses, identities, paths or credentials. Regular log collection
does not include this new folder yet. Owner sends this log separately. Do not start a mission.

Tests: every individual read failure; non-slot3/profile9 and alternate-mode refusals before inventory
access; empty/holey inventories, purchase/refund/cancel observations; wrong genuine types; bad spans,
oversized models, negative counts, duplicate wallet keys; changing roots/headers; ASan. Windows observer
tests use only this test's own process and isolated files for SHA256, config paths and role selection.

## Dungeon bridge: freshly rechecked leads, still source-only

The existing portable coordinator/codec remains unlinked. This step also statically rechecked
`3d7e80` (Darkest::RaidStart) and `602cd0` (Party::OnRaidStart) in the NEW executable:

- App now uses start parameters at **+1720**, restoring byte **+188b**. The old research's +16a0/+180b
  must NOT be reused. RaidStart frees/zeros +1720 on completion, so an external polling tool may miss it.
- Party::OnRaidStart receives its parameters in **R8** (RDX is the restoring flag). The hero-pointer
  vector at params +208/+210 is copied then ranked. Another vector copy reads params +228/+230 and
  writes Party +218; an alias check uses params +10. Do NOT infer that this object has the same
  layout as the campaign Inventory above. Params +240/+244 and +248 also feed the party.
- These are static leads, not a completed quest/map/RNG reader or native departure adapter. No new
  departure hook is installed. Snapshot canonicalization, map/Rules/Battle/quest RNG handling, scene
  barriers, native loading and real two-PC departure validation remain required in DUNGEON.md.

## Owner14b measurement190129: open supplies now confirmed

Latest supplied ZIP has one14b log plus the older14a failure; analyze them separately. The14b
log has20 stable snapshots, zero rejected polls, matching game/runtime pins and normal user exit.
Snapshot1 is closed with stock14/bag16 empty and73010 gold. Snapshot2 is open with both genuine
views, nine stock types and one free firewood in bag slot0. Context1838 remains0 throughout;
it does not identify the selected quest here. Every later snapshot remains open.

Snapshots3..11 show one each: food75, shovel250, antivenom150, bandage150, herbs200, key200,
holy water150, torch75 and laudanum100. Snapshots12..20 return all nine individually at their
purchase prices (not the cheaper estate sale prices). Gold73010 ->71660 ->73010. Stock+bag
quantity is conserved for the exact(type,id) pair on ALL18 transactions; unrelated items and
all other currencies stay unchanged. Food retains ID0, returns leave physical holes; at the end
only free firewood remains at bag0. Stock is restored exactly. These deltas match actual base
supply configuration. This confirms the reader/lookup and single-item native behavior on this
tested quest/content, not synchronized purchases, other quests/modifiers or engine atomicity.

Back/automatic refund-and-clear/reopen were NOT recorded: the final snapshot still has visible1,
stock_view1 and bag_view1, followed by stop=user. Returning every item manually is a different
operation. Do not infer aggregate refund/reset or quest identity from this data. No new observer
binary/installer or live DLL is needed for the remaining measurement.

Next owner action: same14b, fresh guest session, host idles. Guest opens provisions, buys one food
and one torch, waits2sec, clicks BACK to expedition WITH THOSE PURCHASES STILL IN THE BAG, waits2sec,
reopens supplies, waits2sec, BACK once more, waits2sec, then Q. Never embark. Send that observer log;
close both games and allow normal guest restore. Do not repeat the completed nine-item return test.
Then add authoritative buy/return/aggregate-cancel native handlers and lifecycle/queue tests before
enabling co-op supplies. Only afterwards wire mission departure to source-only dungeon barriers.

## Test15: shared supplies wired (not yet tested in the real game)

Back/refund/reopen was measured by the owner (observer log 191241: Back refunded 150 gold, cleared stock and bag;
now fixtures in `tests/provisions_observed.inc`). Codex wrote `src/dd_supplies.c` and most of
`runtime/dd_supplies_bridge.inc`; Claude Code finished the wiring at the owner's request.

Rules as built:

- `DD_ACT_SUPPLIES` (protocol 4) carries op, physical slot, (type,id), native gold quote and the digest of the state
  it was made on. Nonzero shard prices are refused. After the native call all items and currencies are compared.
- One supply action at a time. Orders wait in the FIFO until this player's panel is in the needed state: open for
  buy/return, closed for Back.
- **A panel without shared purchases is local.** Stock is only filled on enter (closed: 14 empty slots), so a shared
  Back from an unshared open can never match the other copy. `g_supply_dirty` marks a shared buy/return since the
  last shared Back; without it, Back runs natively on that PC alone.
- A shared Back does not refund during the native hide; the order goes out once the panel is closed. Crossing Backs
  are one refund: the second is recognised (same quote and pre-state, state equals the first's result).
- New hook on the Back button callback 6e85e0 (`provision_back`, 13 bytes checked against the EXE): Back is refused
  there while a purchase is unanswered or the other player's buy/return waits, so the panel simply stays open.
  6e0250's other caller (6dedc6, forced close) cannot wait; that case stops shared actions, as Codex designed.
- Reopening during an unfinished shared Back stops shared actions, but the native enter still runs: an entered panel
  without views is not a state the game knows.

Checked against the pinned EXE by disassembly: argument shapes of 6960c0, 6978b0, 6e0250, 6e85e0, the compact index
mapping (118d514/118525a), callers of 6e0250, and every hook length on an instruction boundary without RIP-relative
bytes. `tools/gen_address_table.py` regenerates `src/addresses.inc` exactly (54 unique signatures).

Tests: `tests/runtime_supplies_fixture.inc` drives the real hooks, FIFO and authority with a fake game model. Removing
each of ten guards in a throwaway copy made it fail (one survived at first; the test was tightened).

Known limits, to watch in the live test:

- Both players must pick the same mission first; supply stock and free items must match. A guest with other stock
  gets a rejected request, but a host order on a different guest state stops shared actions.
- Stock equality between two real copies and the panel timing are unproven: everything above ran against fixtures.
- Forgetting Back and embarking is not handled. Departure and dungeons are the next milestone (`runtime/DUNGEON.md`).
