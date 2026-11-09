/* Fake Hamlet with the game's rules (costs, one hero per slot, a recruit only once, ...), shared by
 * tests/test_town.c and tests/test_net.c. */
#ifndef FAKE_TOWN_H
#define FAKE_TOWN_H

#include <string.h>

#include "dd_proto.h"

#define HEROES 64
#define CANDIDATES 8 /* recruitable hero ids 40..47 */
enum { NONE = 0, ROSTER = 1, BUILDING = 2 };

typedef struct {
    int gold;
    int heirloom[4];               /* indexed by currency type 1..3 */
    uint8_t hero[HEROES];          /* NONE / ROSTER / BUILDING */
    uint32_t slot[DD_ACTIVITY_COUNT_][3];
    uint8_t candidate_taken[CANDIDATES];
    uint32_t upgrades;             /* running hash of bought upgrades */
    int provisions;
    uint8_t wagon_sold[8];
    int min_gold;                  /* invariant tracking */
} town;

static void town_init(town *t) {
    memset(t, 0, sizeof *t);
    t->gold = 6000;
    t->min_gold = t->gold;
    t->heirloom[1] = 40; t->heirloom[2] = 20; t->heirloom[3] = 10;
    for (int h = 1; h <= 10; h++) t->hero[h] = ROSTER;
}

static int pay(town *t, int amount) { /* the host-side balance check the real deduct lacks */
    if (amount <= 0 || t->gold < amount) return 0;
    t->gold -= amount;
    if (t->gold < t->min_gold) t->min_gold = t->gold;
    return 1;
}

static int town_apply(void *game, const dd_action *a) {
    town *t = game;
    switch (a->kind) {
    case DD_ACT_ASSIGN_HERO: {
        uint32_t h = a->u.assign.hero_id;
        if (h >= HEROES || t->hero[h] != ROSTER || t->slot[a->u.assign.activity][a->u.assign.slot]) return 0;
        if (!pay(t, 300)) return 0;
        t->slot[a->u.assign.activity][a->u.assign.slot] = h;
        t->hero[h] = BUILDING;
        return 1;
    }
    case DD_ACT_BUY_UPGRADE: {
        uint32_t h = a->u.upgrade.hero_id;
        if (h != 0 && (h >= HEROES || t->hero[h] == NONE)) return 0;
        if (!pay(t, 750)) return 0;
        t->upgrades = t->upgrades * 31 + a->u.upgrade.tree + a->u.upgrade.code + h;
        return 1;
    }
    case DD_ACT_RECRUIT: {
        uint32_t h = a->u.hero.hero_id;
        if (h < 40 || h >= 40 + CANDIDATES || t->candidate_taken[h - 40]) return 0;
        t->candidate_taken[h - 40] = 1;
        t->hero[h] = ROSTER;
        return 1;
    }
    case DD_ACT_DISMISS: {
        uint32_t h = a->u.hero.hero_id;
        if (h >= HEROES || t->hero[h] != ROSTER) return 0;
        t->hero[h] = NONE;
        return 1;
    }
    case DD_ACT_PROVISION_BUY:
        if (!pay(t, 75 * a->u.items.count)) return 0;
        t->provisions += a->u.items.count;
        return 1;
    case DD_ACT_PROVISION_RETURN:
        if (t->provisions < a->u.items.count) return 0;
        t->provisions -= a->u.items.count;
        t->gold += 75 * a->u.items.count;
        return 1;
    case DD_ACT_WAGON_BUY:
        if (a->u.items.slot >= 8 || a->u.items.type!=1 || a->u.items.id!=a->u.items.slot+1u ||
            a->u.items.cost!=500 || a->u.items.count!=1 || t->wagon_sold[a->u.items.slot] || !pay(t, 500)) return 0;
        t->wagon_sold[a->u.items.slot] = 1;
        return 1;
    case DD_ACT_WAGON_SELL:
        t->gold += 50 * a->u.items.count;
        return 1;
    case DD_ACT_HEIRLOOM_EXCHANGE: {
        int s = a->u.exchange.src_type, d = a->u.exchange.dst_type;
        if (s < 1 || s > 3 || d < 1 || d > 3 || t->heirloom[s] < a->u.exchange.src_amount) return 0;
        t->heirloom[s] -= a->u.exchange.src_amount;
        t->heirloom[d] += a->u.exchange.dst_amount;
        return 1;
    }
    }
    return 0;
}

static uint32_t town_hash(const town *t) { /* everything except the invariant tracker */
    town c = *t;
    c.min_gold = 0;
    uint32_t h = 2166136261u;
    const uint8_t *p = (const uint8_t *)&c;
    for (size_t i = 0; i < sizeof c; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static int hero_in_two_slots(const town *t) {
    for (int a = 0; a < DD_ACTIVITY_COUNT_; a++)
        for (int s = 0; s < 3; s++)
            for (int b = 0; b < DD_ACTIVITY_COUNT_; b++)
                for (int u = 0; u < 3; u++)
                    if ((a != b || s != u) && t->slot[a][s] && t->slot[a][s] == t->slot[b][u]) return 1;
    return 0;
}

#endif
