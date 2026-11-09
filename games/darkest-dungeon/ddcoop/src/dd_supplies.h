/* Canonical supply transaction validation. Engine pointers/calls and UI readiness belong to runtime. */
#ifndef DD_SUPPLIES_H
#define DD_SUPPLIES_H
#include "dd_provisions.h"
#include "dd_proto.h"
#define DD_SUPPLIES_GOLD 0x00eed472u
/* HMAC with a fixed public domain key is a deterministic digest, NOT peer authentication.
 * Excludes UI visibility/view pointers; includes physical slots, quantities, identities and funds.
 * Zero wallet entries are equivalent to absent entries. Caller must measure actual local data. */
int dd_supplies_digest(const dd_provisions_snapshot *, uint8_t out[32]);
int dd_supplies_equal(const dd_provisions_snapshot *, const dd_provisions_snapshot *);
/* Prices must be read using the game's native pricing code; first integration is gold-only.
 * Nonzero shard cost is deliberately refused; it must never be silently ignored. */
int dd_supplies_quote(const dd_provisions_snapshot *, unsigned op, unsigned physical,
                      uint32_t gold, uint32_t shards, dd_action *out);
int dd_supplies_before(const dd_provisions_snapshot *, const dd_action *);
/* Verifies conservation of ALL stock/bag items and all wallet currencies after native completion.
 * Cancel must clear stock AND bag, preserving all funds except the exact quoted native refund. */
int dd_supplies_after(const dd_provisions_snapshot *before, const dd_provisions_snapshot *after,
                      const dd_action *);
#endif
