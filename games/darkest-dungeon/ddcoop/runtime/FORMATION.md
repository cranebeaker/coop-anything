# Test57: restore default dungeon party order

The test56 report identified a missing player action. After an enemy shuffled the
party, one player restored the default order with the button beside the map.
The other copy did not. The next travel message already showed the front two
heroes reversed between copies. Later the same round/turn selected different
actors. Protocol26's peer stop worked and prevented further shared choices.

Protocol27 adds FORMATION_RESET: area identity, before IDs and after IDs, four
persistent hero IDs per array. Both arrays must be unique (zero is an empty rank),
contain the same nonempty membership and represent a change. Network host orders
the request, but either player may initiate it. Ownership assignments do not change.

Normal DDTabPanel::RenderPartyReorderButton's stateless callback at7812d0 and the
controller keyboard path at42bc65 both call RaidDisplay's native method76f060.
The runtime hooks that method with an instruction-aligned11-byte trampoline.
The pinned48-byte address table adds it and Raid::GetDefaultPartyOrder at614cb0.
The read-only ABI proof checks the button's actual RTTI/vtable, the keyboard call,
register arguments, rank masks, immediate vector changes and busy flag.

The guest sends intent before any mutation. The host validates its current party
and native default against both arrays, runs the native reset once, then verifies
the actual result before broadcasting. The guest performs the same validation and
native call. No sender pointers, raw party/rank writes, battle AI or PvP endpoints
are used. Native code also schedules its own animation and sets display+4119.
Walking and further shared inputs wait for that animation to finish.

A request waits up to15 seconds for matching area and safe phase on the receiving
copy. Loading, battle, a blocking overlay, door transition and an existing reorder
prevent mutation. Repeated guest clicks stay one request. Crossed clicks cannot
run the reset twice. Stale requests are rejected before mutation. A partial native
result or guest apply failure stops both players through existing SYNC_STOP.

At travel, formation identity is checked before HP/stress correction and native
entry to the next area. A different party order now stops both peers instead of
offering to continue into another fight with divergent initiative. Already-arrived
travel acknowledgements are also checked. This is a guard, not an automatic repair.

Scope: default-order button and shortcut. Combat move/effect paths retain their
existing handling. Arbitrary manual dungeon party drags are not newly enabled by
this change; any unshared difference must stop before the next travel. That separate
input path still needs native mapping and live validation.

Validation uses the real bridge/FIFO and fake native effects: host/guest initiation,
crossed/double clicks, delayed combat end/presentation, stale area/members, exact
post-state, native partial failure, campaign/session/loading/door guards and passive
game behavior. Static ABI proof uses only the exact owned executable. A real two-PC
playtest remains required. No game or save is launched/changed by these tests.

Written by Codex,9 Nov2026. Prior test56 fixes, live party display and DLC groundwork retained.
