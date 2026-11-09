# Test56: inventory timing and battle disagreement

Observed in the two-PC test55 run: the Rules random values matched through
departure and the tested fights. A guest inventory use waited behind a positional
Rules barrier and ran several seconds after the host, after combat had begun.
Later bag repairs were sent without verifying their result. A battle action was
rejected at equal round/turn/state, but the guest acknowledged it anyway; the
host continued while the guest stayed on the previous choice.

The delayed use is a plausible inventory-drift cause. The original reason for
the different actor/party pointer is not proven by those logs. This change does
not claim to repair every source of divergent combat state.

Protocol26 carries the persistent hero ID for the battle actor and friendly
target. The receiver uses its actual turn actor and resolves friendly targets by
ID. A differing party-vector ordinal alone cannot pick the wrong hero. Round,
turn, state, actor membership and guest ownership still have to match. A guest
that cannot apply an accepted battle choice now fails instead of acknowledging
it. Logs include actor/target IDs.

Outside-combat inventory use is refused while a door is pending or loading. Its
Rules prefix and action wait for the same area and safe phase, without requiring
the exact walking pixel. An expired use is never replayed inside combat. Other
prop interactions keep their existing positional barrier.

Host bag repair now has an ordered begin/hash, slot updates and end/hash. The
guest blocks shared input during repair and verifies the final bag hash. A
missing item kind or partial repair cannot silently report success. This remains
an ordered repair using the existing native Item lifetime, not a new raw memory
snapshot or an automatic combat-state repair.

SYNC_STOP reasons1..3 mean battle/apply, bag repair and late inventory use. It
uses a four-byte bounded body (the existing u.load.slot union storage), is sent
once on failure, and the network thread reports it to the game thread outside
the action FIFO. A peer blocked behind an animation still receives the stop.
The SDL bootstrap filters gameplay input after the stop; F9/overlay and
window close/quit remain available. Both players must close and restart for a fresh host snapshot. There is no
automatic reconnect/resync in this kit. Network role continues to order actions;
both players retain the same available gameplay actions and assigned heroes.

Validation: the native runtime/protocol/fuzzer suite and new real-bridge fixtures
cover rank reorder, friendly target identity, wrong actor, FIFO-independent stop,
both door-use directions, different walking pixels, late combat use, successful
bag repair and missing-kind failure. The fixtures fake native game effects;
the new kit still requires a real two-PC gameplay test.

Written by Codex,9 Nov2026. Existing DLC groundwork and live-party mapping retained.
