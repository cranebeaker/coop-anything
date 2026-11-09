/* Fake-world test for host-authority town sync (src/dd_town.c over src/dd_proto.c).
 * A host and three guests share a small fake Hamlet with the game's rules (costs, one hero per slot, a
 * recruit only once, ...). All four fire random actions at the same time; every message is encoded,
 * queued per link in order (like TCP) and decoded on delivery, links interleaved at random.
 * Checks: all copies end identical, gold never negative, no hero in two slots, nothing needs a resync;
 * and a lost message makes exactly that guest ask for a resync.
 *   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc \
 *       -o build/test_town tests/test_town.c src/dd_town.c src/dd_proto.c && setarch x86_64 -R build/test_town */
#include "dd_town.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static uint32_t rng = 2024;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

#include "fake_town.h"

/* ---------------- fake network: one in-order queue per directed link ---------------- */
#define NODES 4 /* 0 = host, 1..3 guests */
#define QCAP 4096
typedef struct { uint8_t data[DD_PROTO_HEADER + DD_PROTO_MAX_BODY]; size_t len; } packet;
typedef struct { packet q[QCAP]; int head, tail; } link;
static link links[NODES][NODES];
static int drop_next_apply_to = -1;

typedef struct { int self; } endpoint;
static endpoint endpoints[NODES];

static void push(int from, int to, const dd_msg *m) {
    link *l = &links[from][to];
    packet *p = &l->q[l->tail % QCAP];
    p->len = dd_encode(m, p->data, sizeof p->data);
    if (!p->len) { failures++; printf("FAIL: message did not encode\n"); return; }
    if (to == drop_next_apply_to && m->type == DD_MSG_TOWN_APPLY) { drop_next_apply_to = -1; return; } /* lost */
    l->tail++;
}

static void net_send(void *net, int to, const dd_msg *m) {
    int from = ((endpoint *)net)->self;
    if (to == DD_TOWN_ALL) {
        for (int g = 1; g < NODES; g++) push(from, g, m);
    } else {
        push(from, to, m);
    }
}

static town towns[NODES];
static dd_town sync_[NODES];

static int deliver_one(int from, int to) {
    link *l = &links[from][to];
    if (l->head == l->tail) return 0;
    packet *p = &l->q[l->head++ % QCAP];
    dd_msg m;
    size_t used;
    if (dd_decode(p->data, p->len, &m, &used) != DD_DECODE_OK || used != p->len) {
        failures++;
        printf("FAIL: delivered packet did not decode\n");
        return 1;
    }
    if (to == 0 && m.type == DD_MSG_TOWN_REQUEST) dd_town_host_request(&sync_[0], from, &m);
    else if (m.type == DD_MSG_TOWN_APPLY) dd_town_guest_apply(&sync_[to], &m);
    /* REJECT: the guest's UI would say "not possible"; nothing to apply. */
    return 1;
}

static int deliver_random(void) {
    int pairs[NODES * NODES][2], n = 0;
    for (int f = 0; f < NODES; f++)
        for (int t = 0; t < NODES; t++)
            if (links[f][t].head != links[f][t].tail) { pairs[n][0] = f; pairs[n][1] = t; n++; }
    if (!n) return 0;
    int k = (int)(rnd() % (uint32_t)n);
    return deliver_one(pairs[k][0], pairs[k][1]);
}

static dd_action random_action(void) {
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = (uint8_t)(1 + rnd() % (DD_ACT_COUNT_ - 1));
    switch (a.kind) {
    case DD_ACT_ASSIGN_HERO:
        a.u.assign.activity = (uint8_t)(rnd() % DD_ACTIVITY_COUNT_);
        a.u.assign.slot = (uint8_t)(rnd() % 3);
        a.u.assign.hero_id = 1 + rnd() % 47;
        break;
    case DD_ACT_BUY_UPGRADE:
        a.u.upgrade.tree = 1 + rnd() % 5;
        a.u.upgrade.code = (uint8_t)('0' + rnd() % 4);
        a.u.upgrade.hero_id = rnd() % 12;
        break;
    case DD_ACT_RECRUIT:
    case DD_ACT_DISMISS: a.u.hero.hero_id = 1 + rnd() % 47; break;
    case DD_ACT_RENAME: a.u.rename.hero_id=1;strcpy(a.u.rename.before,"Hero");strcpy(a.u.rename.after,"Hero B");break;
    case DD_ACT_SKILL: a.u.skill.hero_id=1;a.u.skill.after=1;break;
    case DD_ACT_TRINKET_MOVE: a.u.move.src_type=1;a.u.move.src_id=1;a.u.move.dst_slot=1;break;
    case DD_ACT_PARTY: a.u.party.before[0]=1;a.u.party.after[1]=1;break;
    case DD_ACT_SUPPLIES: a.u.supplies.op=DD_SUPPLY_BUY;a.u.supplies.type=1;a.u.supplies.gold=75;
        memset(a.u.supplies.before,0x5a,32);break;
    case DD_ACT_DEPARTURE: a.u.departure.seed=12345;a.u.departure.rules=54321;a.u.departure.heroes[0]=8;break;
    case DD_ACT_TRAVEL: a.u.travel.dest[0]=77;a.u.travel.dest[1]=3;break;
    case DD_ACT_USE: a.u.use.area=77;a.u.use.prop=1;a.u.use.hero=2;break;
    case DD_ACT_EMBARK:
    case DD_ACT_PREPARE: a.u.prepare.index=1;a.u.prepare.key=8;break;
    case DD_ACT_OWNERS: a.u.owners.heroes[0]=1;a.u.owners.mask=1;a.u.owners.revision=5;break;
    case DD_ACT_ITEM: a.u.item.slot=2;a.u.item.hero=255;break;
    case DD_ACT_BATTLE: a.u.battle.actor_id=13;a.u.battle.round=1;a.u.battle.state=0x1d;a.u.battle.hero=2;break;
    case DD_ACT_WAGON_BUY:
    case DD_ACT_WAGON_SELL:
        a.u.items.slot=(uint16_t)(rnd()%8); a.u.items.count=1;
        a.u.items.type=1; a.u.items.id=a.u.items.slot+1; a.u.items.cost=500;
        break;
    case DD_ACT_HEIRLOOM_EXCHANGE:
        a.u.exchange.src_type = 1 + (int)(rnd() % 3);
        a.u.exchange.dst_type = 1 + ((a.u.exchange.src_type) % 3);
        a.u.exchange.src_amount = 1 + (int)(rnd() % 6);
        a.u.exchange.dst_amount = 1 + (int)(rnd() % 3);
        break;
    default:
        a.u.items.slot = (uint16_t)(rnd() % 8);
        a.u.items.count = (uint16_t)(1 + rnd() % 4);
    }
    return a;
}

static void setup(void) {
    memset(links, 0, sizeof links);
    for (int n = 0; n < NODES; n++) {
        town_init(&towns[n]);
        endpoints[n].self = n;
        dd_town_init(&sync_[n], &towns[n], town_apply, &endpoints[n], net_send);
    }
}

static void run(int steps) {
    for (int i = 0; i < steps; i++) {
        if (rnd() % 3) {
            int who = (int)(rnd() % NODES);
            dd_action a = random_action();
            if (who == 0) dd_town_host_local(&sync_[0], &a);
            else dd_town_guest_local(&sync_[who], &a);
        } else {
            deliver_random();
        }
    }
    while (deliver_random()) {}
}

static void test_concurrent(void) {
    long total_accepted = 0, total_rejected = 0;
    for (int round = 0; round < 50; round++) {
        setup();
        run(3000);
        uint32_t h0 = town_hash(&towns[0]);
        for (int n = 1; n < NODES; n++) {
            CHECK(town_hash(&towns[n]) == h0, "round %d: guest %d differs from the host", round, n);
            CHECK(!sync_[n].needs_resync, "round %d: guest %d asked for a resync", round, n);
            CHECK(sync_[n].applied_seq == sync_[0].applied_seq, "round %d: guest %d is behind", round, n);
        }
        for (int n = 0; n < NODES; n++) {
            CHECK(towns[n].min_gold >= 0, "round %d: copy %d went below zero gold", round, n);
            CHECK(!hero_in_two_slots(&towns[n]), "round %d: copy %d has a hero in two slots", round, n);
        }
        total_accepted += sync_[0].accepted;
        total_rejected += sync_[0].rejected;
    }
    CHECK(total_accepted > 1000 && total_rejected > 1000, "both paths exercised (%ld/%ld)", total_accepted, total_rejected);
    printf("concurrent: 50 rounds x 3000 steps, host accepted %ld actions and rejected %ld; all copies identical\n",
           total_accepted, total_rejected);
}

static void test_lost_message(void) {
    setup();
    run(200);
    drop_next_apply_to = 2;          /* the next APPLY to guest 2 is lost */
    dd_action a; /* selling always succeeds, so neither action can be refused for lack of gold */
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_WAGON_SELL;
    a.u.items.count = 1;
    a.u.items.type=1;a.u.items.id=1;a.u.items.cost=50;
    CHECK(dd_town_host_local(&sync_[0], &a) == 1, "first host action accepted (gold %d)", towns[0].gold);
    CHECK(dd_town_host_local(&sync_[0], &a) == 1, "second host action accepted"); /* reveals the gap on guest 2 */
    while (deliver_random()) {}
    CHECK(sync_[2].needs_resync, "guest 2 must notice the gap and ask for a resync");
    CHECK(!sync_[1].needs_resync && !sync_[3].needs_resync, "the other guests continue");
    CHECK(town_hash(&towns[1]) == town_hash(&towns[0]) && town_hash(&towns[3]) == town_hash(&towns[0]),
          "the other guests stay identical");
    printf("lost message: guest 2 asks for a resync, guests 1 and 3 stay identical\n");
}

/* The host's game did an action on its own (observed afterwards): guests must end up the same. */
static void test_observed(void) {
    setup();
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ASSIGN_HERO;
    a.u.assign.activity = DD_ACTIVITY_BAR;
    a.u.assign.slot = 0;
    a.u.assign.hero_id = 3;
    CHECK(town_apply(&towns[0], &a), "the host's game places hero 3 itself");
    dd_town_host_observed(&sync_[0], &a);
    while (deliver_random()) {}
    for (int n = 1; n < NODES; n++) {
        CHECK(town_hash(&towns[n]) == town_hash(&towns[0]), "guest %d follows the observed placement", n);
        CHECK(towns[n].slot[DD_ACTIVITY_BAR][0] == 3 && towns[n].gold == towns[0].gold, "guest %d slot and gold", n);
    }
    printf("observed: a placement made by the host's own game reaches every guest\n");
}

static void test_shop_orders(void) {
    setup();
    dd_action a={0};a.kind=DD_ACT_WAGON_BUY;
    a.u.items.slot=2;a.u.items.count=1;a.u.items.type=1;a.u.items.id=3;a.u.items.cost=500;
    dd_action bad=a;bad.u.items.id=4;
    CHECK(!dd_town_host_local(&sync_[0],&bad),"stale identity refused");
    bad=a;bad.u.items.cost=1;
    CHECK(!dd_town_host_local(&sync_[0],&bad),"changed quote refused");
    CHECK(towns[0].gold==6000,"refusal leaves wallet intact");
    dd_town_guest_local(&sync_[1],&a);dd_town_guest_local(&sync_[2],&a);
    while(deliver_random()){}
    CHECK(sync_[0].accepted==1 && sync_[0].rejected==1,"only one competing purchase accepted");
    for(int n=0;n<NODES;n++)CHECK(towns[n].gold==5500 && towns[n].wagon_sold[2],"one shared payment and stock removal %d",n);
    printf("shop: stale identity/price refused; competing purchases pay only once on every copy\n");
}

int main(void) {
    test_shop_orders();
    test_observed();
    test_concurrent();
    test_lost_message();
    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
