/* Hero ownership: who commands which hero in the dungeon (platform independent;
 * launcher/tests/test_lr_owners.c). Design and the runtime contract: launcher/OWNERSHIP.md.
 *
 * The host's launcher keeps the table (roster from its campaign, owners chosen by the players) and is the
 * authority: the guest asks, the host decides and sends the whole table back. Both write it to
 * ddcoop-heroes.ini, which the host runtime reads at embark. Players: 0 = host, 1 = guest. */
#ifndef LR_OWNERS_H
#define LR_OWNERS_H

#include "lr_roster.h"

#define LR_MAX_PLAYERS 2
#define LR_PLAYER_HOST 0
#define LR_PLAYER_GUEST 1

typedef struct {
    uint32_t revision;              /* host: +1 on every change; guest: the host's revision */
    int count;
    lr_hero heroes[LR_MAX_HEROES];  /* sorted by id */
    uint8_t owner[LR_MAX_HEROES];   /* player index per hero */
} lr_owners;

void lr_owners_init(lr_owners *o);

/* Index of a hero by roster id, -1 if not in the table. */
int lr_owners_find(const lr_owners *o, uint32_t id);

/* Legacy saved-status sort. Status0 does NOT prove live dungeon membership;
 * the launcher/F9 must use lr_owners_live_order. Retained for older callers/tests. */
int lr_owners_display_order(const lr_owners *o, int order[LR_MAX_HEROES]);
/* Live view only: supplied unique persistent IDs first, in formation order.
 * Unknown/stale count (-1), foreign/duplicate IDs remove all markers. Owners and
 * save-status fields are untouched. flags[] is indexed by the original roster. */
int lr_owners_live_order(const lr_owners *o, const uint32_t ids[4], int count,
                         int order[LR_MAX_HEROES], uint8_t flags[LR_MAX_HEROES]);

/* Host: a new hero list (from the campaign). Owners of heroes that are still there are kept; new heroes
 * belong to the host. Returns 1 if anything changed (revision raised). */
int lr_owners_set_roster(lr_owners *o, const lr_hero *heroes, int count);

/* Host: player ticks (claim = 1) or unticks (claim = 0) a hero. Ticking makes it theirs; unticking a hero of
 * theirs gives it to the other player. Returns 1 if the owner changed, 0 if nothing changed (unknown hero,
 * bad player, or unticking a hero that is not theirs). */
int lr_owners_claim(lr_owners *o, uint32_t id, int player, int claim);

/* Host: takes over the owners from an earlier ddcoop-heroes.ini for heroes in the table (so choices survive
 * a restart). Returns 1 if anything changed. */
int lr_owners_apply_ini(lr_owners *o, const char *text, size_t len);

/* The table as sent in an LR_ROSTER message (revision travels in the message's id). Returns the length,
 * 0 if it does not fit in cap. */
size_t lr_owners_encode(const lr_owners *o, uint8_t *out, size_t cap);

/* Guest: strict decode of an LR_ROSTER body. 1 = ok (out replaced), 0 = refused (out untouched). */
int lr_owners_decode(const uint8_t *buf, size_t len, uint32_t revision, lr_owners *out);

/* The text of ddcoop-heroes.ini, written by player `writer` (written_by=host/guest: a host only restores
 * choices from a file it wrote as host, never from one it got as a guest of someone else's campaign).
 * Returns the length, 0 if it does not fit. */
size_t lr_owners_ini(const lr_owners *o, int writer, char *out, size_t cap);

/* Was this ddcoop-heroes.ini text written by a host? */
int lr_owners_ini_from_host(const char *text, size_t len);

#endif
