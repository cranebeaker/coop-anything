/* Standalone foundation test: three simulated copies, real proposed packet codec, canonical
 * HMAC-SHA256 fixture digests, deterministic dice and deliberately delayed native completion.
 * This is NOT the Darkest Dungeon engine. It cannot validate live hooks, target layouts or RNG IO. */
#include "dd_dungeon_wire.h"
#include "dd_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
#define OK(x) CHECK((x) == DD_DNG_OK)
#define RUN_ID UINT64_C(0x1000200030004000)
static unsigned checks;
static uint64_t now = 1000;
static uint32_t random_state = 0x9234da7;
static uint32_t rnd(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static uint32_t dice(uint32_t *state) { return *state = (uint32_t)(((uint64_t)*state * 16807) % 2147483647); }
typedef struct {
    uint32_t hp[4], stress[4], monster_hp[2], gold, provisions;
    uint32_t rules, battle, quest, scene, location, encounter, turn, actor_kind, actor;
    unsigned executions, validations;
    int refuse;
} world;
static void word(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++) {
        p[i] = (uint8_t)v;
        v >>= 8;
    }
}
static dd_dng_checkpoint checkpoint(const world *w) {
    /* Canonical scalar encoding, excluding the fixture's counters and padding. A real adapter must
     * cover ALL gameplay state, inventories/statuses/ranks/entities/map/etc., not just these fields. */
    uint32_t words[26];
    size_t n = 0;
    for (unsigned i = 0; i < 4; i++) {
        words[n++] = w->hp[i];
        words[n++] = w->stress[i];
    }
    for (unsigned i = 0; i < 2; i++)
        words[n++] = w->monster_hp[i];
    words[n++] = w->gold;
    words[n++] = w->provisions;
    words[n++] = w->rules;
    words[n++] = w->battle;
    words[n++] = w->quest;
    words[n++] = w->scene;
    words[n++] = w->location;
    words[n++] = w->encounter;
    words[n++] = w->turn;
    words[n++] = w->actor_kind;
    words[n++] = w->actor;
    uint8_t bytes[sizeof words];
    for (size_t i = 0; i < n; i++)
        word(bytes + 4 * i, words[i]);
    dd_dng_checkpoint c = {0};
    c.scene = (uint8_t)w->scene;
    c.location = w->location;
    c.encounter = w->encounter;
    c.turn = w->turn;
    c.active.kind = (uint8_t)w->actor_kind;
    c.active.id = w->actor;
    c.rules_seed = w->rules;
    c.battle_seed = w->battle;
    c.quest_seed = w->quest;
    static const uint8_t domain[] = "DDCoop dungeon fixture only";
    CHECK(dd_hmac_sha256(domain, sizeof domain - 1, bytes, n * 4, c.hash));
    return c;
}
static int native_validate(void *ctx, const dd_dng_command *c) {
    world *w = ctx;
    w->validations++;
    if (w->refuse) return 0;
    if (c->kind == DD_DNG_SKILL)
        return c->argument == 11 && c->target_count == 1 && c->targets[0].kind == DD_DNG_MONSTER &&
               c->targets[0].id == 9001 && w->monster_hp[0] > 0;
    if (c->kind == DD_DNG_MONSTER_SKILL) return c->argument == 22 && c->actor.id == 9001;
    return 1;
}
static void native_execute(world *w, const dd_dng_command *c) {
    w->executions++;
    switch (c->kind) {
    case DD_DNG_SKILL:
        w->monster_hp[0] -= 1 + dice(&w->battle) % 4;
        w->turn++;
        if (c->actor.id < 103) {
            w->actor_kind = DD_DNG_HERO;
            w->actor = c->actor.id + 1;
        } else {
            w->actor_kind = DD_DNG_MONSTER;
            w->actor = 9001;
        }
        break;
    case DD_DNG_MONSTER_SKILL:
        w->hp[0] -= 1 + dice(&w->battle) % 3;
        w->turn++;
        w->actor_kind = DD_DNG_HERO;
        w->actor = 101;
        break;
    case DD_DNG_USE_ITEM:
        w->provisions--;
        w->hp[c->actor.id - 101]++;
        break;
    case DD_DNG_CAMP_SKILL:
        w->stress[c->actor.id - 101] = 0;
        dice(&w->rules);
        break;
    case DD_DNG_MOVE:
        w->location = c->argument;
        dice(&w->rules);
        break;
    case DD_DNG_CURIO:
        w->gold += 1 + dice(&w->quest) % 100;
        break;
    case DD_DNG_TRAP:
        w->stress[c->actor.id - 101] += dice(&w->rules) % 5;
        break;
    case DD_DNG_ENTER_ROOM:
        w->scene = DD_DNG_BATTLE;
        w->encounter = c->argument;
        w->actor_kind = DD_DNG_HERO;
        w->actor = 101;
        w->turn = 1;
        break;
    case DD_DNG_START_CAMP:
        w->scene = DD_DNG_CAMP;
        break;
    case DD_DNG_END_CAMP:
        w->scene = DD_DNG_EXPLORE;
        break;
    case DD_DNG_TAKE_LOOT:
        w->gold += 100;
        w->scene = DD_DNG_EXPLORE;
        break;
    case DD_DNG_FINISH_QUEST:
        w->scene = DD_DNG_RETURNING;
        break;
    case DD_DNG_RETREAT:
        w->scene = DD_DNG_RETURNING;
        w->actor_kind = 0;
        w->actor = 0;
        break;
    case DD_DNG_RETURN_TOWN:
        w->scene = DD_DNG_TOWN;
        break;
    default:
        CHECK(0);
    }
}
static dd_dng_message wire(dd_dng_message m) {
    uint8_t bytes[DD_DNG_WIRE_MAX];
    dd_dng_message decoded;
    size_t len = dd_dng_encode(&m, bytes, sizeof bytes);
    CHECK(len);
    CHECK(dd_dng_decode(bytes, len, &decoded));
    return decoded;
}
typedef struct {
    dd_dungeon nodes[3];
    world worlds[3];
    dd_dng_departure departure;
} simulation;
static dd_dng_departure departure(void) {
    dd_dng_departure d = {0};
    d.quest = 700;
    d.dungeon = 800;
    d.week = 4;
    d.owner_revision = 12;
    d.players = 3;
    d.connected = 7;
    for (unsigned i = 0; i < 4; i++)
        d.heroes[i] = 101 + i;
    d.owners[1] = 1;
    d.owners[2] = 2; /* Future core capacity, not current INI v1 or live transport support. */
    d.map_seed = 0xfedcba98;
    d.rules_seed = 1345;
    d.battle_seed = 2456;
    d.quest_seed = 3567;
    memset(d.content_hash, 0xa3, 32);
    memset(d.town_hash, 0xb4, 32);
    d.item_count = 2;
    d.items[0] = (dd_dng_item){123, 456, 8};
    d.items[1] = (dd_dng_item){123, 789, 4};
    return d;
}
static void prepare(simulation *sim) {
    memset(sim, 0, sizeof *sim);
    sim->departure = departure();
    for (unsigned i = 0; i < 3; i++) {
        world *w = &sim->worlds[i];
        for (unsigned j = 0; j < 4; j++) {
            w->hp[j] = 30;
            w->stress[j] = 8;
        }
        w->monster_hp[0] = 100;
        w->monster_hp[1] = 100;
        w->gold = 500;
        w->provisions = 12;
        w->rules = sim->departure.rules_seed;
        w->battle = sim->departure.battle_seed;
        w->quest = sim->departure.quest_seed;
    }
    dd_dng_checkpoint town = checkpoint(&sim->worlds[0]);
    memcpy(sim->departure.town_hash, town.hash, 32);
    dd_dng_message m = {0};
    m.type = DD_DNG_OFFER;
    m.expedition = RUN_ID;
    m.u.offer = sim->departure;
    m = wire(m);
    for (unsigned i = 0; i < 3; i++) {
        world *w = &sim->worlds[i];
        dd_dungeon_init(&sim->nodes[i], i, native_validate, w);
        CHECK(dd_dungeon_town_allowed(&sim->nodes[i]));
        OK(dd_dungeon_offer(&sim->nodes[i], 0, m.expedition, &m.u.offer, now));
        CHECK(!dd_dungeon_town_allowed(&sim->nodes[i]));
    }
}
static void ready(simulation *sim) {
    dd_dng_checkpoint host_town = checkpoint(&sim->worlds[0]);
    OK(dd_dungeon_local_ready(&sim->nodes[0], RUN_ID, host_town.hash, sim->departure.content_hash));
    for (unsigned i = 1; i < 3; i++) {
        dd_dng_message m = {0};
        m.type = DD_DNG_READY;
        m.expedition = RUN_ID;
        dd_dng_checkpoint town = checkpoint(&sim->worlds[i]);
        memcpy(m.u.ready.town, town.hash, 32);
        memcpy(m.u.ready.content, sim->departure.content_hash, 32);
        OK(dd_dungeon_local_ready(&sim->nodes[i], RUN_ID, town.hash, sim->departure.content_hash));
        m = wire(m);
        OK(dd_dungeon_ready(&sim->nodes[0], i, m.expedition, m.u.ready.town, m.u.ready.content));
    }
}
static void release(simulation *sim) {
    dd_dungeon *host = &sim->nodes[0];
    dd_dng_message m = {0};
    m.expedition = RUN_ID;
    m.sequence = host->sequence;
    unsigned first = 1 + (rnd() % 2);
    for (unsigned j = 0; j < 2; j++) {
        unsigned i = 1 + (first - 1 + j) % 2;
        if (!(host->connected & (1u << i))) continue;
        m.type = DD_DNG_ACK;
        memcpy(m.u.hash, sim->nodes[i].checkpoint.hash, 32);
        m = wire(m);
        OK(dd_dungeon_ack(host, i, m.expedition, m.sequence, m.u.hash));
    }
    m.type = DD_DNG_RELEASE;
    memcpy(m.u.hash, host->checkpoint.hash, 32);
    m = wire(m);
    for (unsigned i = 0; i < 3; i++)
        if (host->connected & (1u << i))
            OK(dd_dungeon_release(&sim->nodes[i], 0, m.expedition, m.sequence, m.u.hash));
}
static void start_scene(simulation *sim, unsigned scene) {
    prepare(sim);
    CHECK(dd_dungeon_launch(&sim->nodes[0], 0, RUN_ID, now) == DD_DNG_BUSY);
    ready(sim);
    dd_dng_message m = {0};
    m.type = DD_DNG_LAUNCH;
    m.expedition = RUN_ID;
    m = wire(m);
    for (unsigned i = 0; i < 3; i++) {
        OK(dd_dungeon_launch(&sim->nodes[i], 0, m.expedition, now));
        sim->worlds[i].scene = scene;
        sim->worlds[i].location = 1;
    }
    m.type = DD_DNG_CHECKPOINT;
    m.u.checkpoint = checkpoint(&sim->worlds[0]);
    m = wire(m);
    OK(dd_dungeon_host_checkpoint(&sim->nodes[0], &m.u.checkpoint, now));
    CHECK(dd_dungeon_release(&sim->nodes[0], 0, RUN_ID, 0, m.u.checkpoint.hash) == DD_DNG_BUSY);
    for (unsigned i = 1; i < 3; i++) {
        OK(dd_dungeon_guest_checkpoint(&sim->nodes[i], 0, RUN_ID, &m.u.checkpoint, now));
        dd_dng_checkpoint actual = checkpoint(&sim->worlds[i]);
        OK(dd_dungeon_guest_complete(&sim->nodes[i], &actual, now));
    }
    release(sim);
}
static void start(simulation *sim) { start_scene(sim, DD_DNG_EXPLORE); }
static dd_dng_command command(const dd_dungeon *host, unsigned kind, uint32_t actor, uint32_t request) {
    dd_dng_command c = {0};
    c.kind = (uint8_t)kind;
    c.request = request;
    c.argument = 11;
    if (actor) c.actor = (dd_dng_entity){kind == DD_DNG_MONSTER_SKILL ? DD_DNG_MONSTER : DD_DNG_HERO, actor};
    if (kind == DD_DNG_SKILL) {
        c.target_count = 1;
        c.targets[0] = (dd_dng_entity){DD_DNG_MONSTER, 9001};
    }
    if (kind == DD_DNG_MONSTER_SKILL) c.argument = 22;
    memcpy(c.before_hash, host->checkpoint.hash, 32);
    return c;
}
static dd_dng_result request(simulation *sim, unsigned from, const dd_dng_command *c) {
    dd_dng_message m = {0};
    m.type = DD_DNG_COMMAND;
    m.expedition = RUN_ID;
    m.sequence = sim->nodes[0].sequence;
    m.u.command = *c;
    m = wire(m);
    return dd_dungeon_request(&sim->nodes[0], from, m.expedition, m.sequence, &m.u.command, now);
}
static dd_dng_event complete_host(simulation *sim) {
    dd_dungeon *host = &sim->nodes[0];
    dd_dng_event e;
    unsigned seq = host->sequence;
    CHECK(host->control == DD_DNG_NATIVE_PENDING);
    native_execute(&sim->worlds[0], &host->pending.command);
    CHECK(host->sequence == seq); /* Even native execution isn't a commit until completed/snapshotted. */
    dd_dng_checkpoint after = checkpoint(&sim->worlds[0]);
    OK(dd_dungeon_host_complete(host, &after, ++now, &e));
    CHECK(host->sequence == seq + 1);
    return e;
}
static void deliver_event(simulation *sim, unsigned to, dd_dng_event event) {
    dd_dungeon *host = &sim->nodes[0], *guest = &sim->nodes[to];
    dd_dng_message m = {0};
    m.type = DD_DNG_EVENT;
    m.expedition = RUN_ID;
    m.sequence = host->sequence;
    m.u.event = event;
    m = wire(m);
    unsigned count = sim->worlds[to].executions;
    OK(dd_dungeon_guest_event(guest, 0, m.expedition, m.sequence, &m.u.event, now));
    CHECK(count == sim->worlds[to].executions); /* Authorization never invokes native behavior itself. */
    CHECK(dd_dungeon_guest_event(guest, 0, m.expedition, m.sequence, &m.u.event, now) == DD_DNG_BUSY);
    native_execute(&sim->worlds[to], &m.u.event.command);
    dd_dng_checkpoint actual = checkpoint(&sim->worlds[to]);
    OK(dd_dungeon_guest_complete(guest, &actual, now));
    CHECK(dd_dungeon_guest_event(guest, 0, m.expedition, m.sequence, &m.u.event, now) == DD_DNG_DUPLICATE);
    CHECK(count + 1 == sim->worlds[to].executions);
}
static void transact(simulation *sim, unsigned from, dd_dng_command c) {
    dd_dungeon *host = &sim->nodes[0];
    unsigned count = sim->worlds[0].executions, seq = host->sequence;
    OK(request(sim, from, &c));
    CHECK(count == sim->worlds[0].executions && seq == host->sequence);
    CHECK(request(sim, from, &c) == DD_DNG_BUSY);
    dd_dng_event e = complete_host(sim);
    CHECK(request(sim, from, &c) == DD_DNG_BUSY); /* Await all clients before another mutation. */
    for (unsigned i = 1; i < 3; i++)
        if (host->connected & (1u << i)) deliver_event(sim, i, e);
    CHECK(dd_dungeon_release(host, 0, RUN_ID, host->sequence, host->checkpoint.hash) == DD_DNG_BUSY);
    release(sim);
    for (unsigned i = 1; i < 3; i++)
        if (host->connected & (1u << i)) {
            dd_dng_checkpoint a = checkpoint(&sim->worlds[0]), b = checkpoint(&sim->worlds[i]);
            CHECK(dd_dng_checkpoint_equal(&a, &b));
        }
    checks++;
}
static void test_expedition(void) {
    simulation sim;
    start(&sim);
    dd_dungeon *h = &sim.nodes[0];
    dd_dng_command c = command(h, DD_DNG_SKILL, 101, 1);
    CHECK(request(&sim, 0, &c) == DD_DNG_PHASE);
    c = command(h, DD_DNG_MOVE, 0, 1);
    CHECK(request(&sim, 1, &c) == DD_DNG_OWNER);
    c = command(h, DD_DNG_USE_ITEM, 102, 2);
    transact(&sim, 1, c);
    c = command(h, DD_DNG_MOVE, 0, 2);
    c.argument = 2;
    transact(&sim, 0, c);
    c = command(h, DD_DNG_CURIO, 103, 1);
    transact(&sim, 2, c);
    c = command(h, DD_DNG_TRAP, 102, 3);
    transact(&sim, 1, c);
    c = command(h, DD_DNG_ENTER_ROOM, 0, 3);
    c.argument = 17;
    transact(&sim, 0, c);
    c = command(h, DD_DNG_SKILL, 101, 4);
    CHECK(request(&sim, 1, &c) == DD_DNG_OWNER);
    c = command(h, DD_DNG_SKILL, 102, 5);
    CHECK(request(&sim, 1, &c) == DD_DNG_PHASE);
    c = command(h, DD_DNG_SKILL, 101, 4);
    transact(&sim, 0, c);
    c = command(h, DD_DNG_SKILL, 102, 6);
    c.argument = 999;
    CHECK(request(&sim, 1, &c) == DD_DNG_NATIVE_REFUSED);
    c = command(h, DD_DNG_SKILL, 102, 7);
    transact(&sim, 1, c);
    c = command(h, DD_DNG_SKILL, 103, 2);
    transact(&sim, 2, c);
    c = command(h, DD_DNG_MONSTER_SKILL, 9001, 3);
    CHECK(request(&sim, 2, &c) == DD_DNG_OWNER);
    c = command(h, DD_DNG_MONSTER_SKILL, 9001, 5);
    transact(&sim, 0, c);
    c = command(h, DD_DNG_RETREAT, 0, 6);
    transact(&sim, 0, c);
    CHECK(dd_dungeon_finish_return(h) == DD_DNG_BUSY);
    c = command(h, DD_DNG_RETURN_TOWN, 0, 7);
    transact(&sim, 0, c);
    for (unsigned i = 0; i < 3; i++) {
        OK(dd_dungeon_finish_return(&sim.nodes[i]));
        CHECK(dd_dungeon_town_allowed(&sim.nodes[i]));
        CHECK(dd_dungeon_offer(&sim.nodes[i], 0, RUN_ID, &sim.departure, now) == DD_DNG_INVALID);
        OK(dd_dungeon_offer(&sim.nodes[i], 0, RUN_ID + 1, &sim.departure, now));
    }
    printf("PASS three-copy expedition: owner turns, host AI/navigation, provisions, RNG, return barriers\n");
}
static void test_failures(void) {
    simulation sim;
    prepare(&sim);
    dd_dungeon *h = &sim.nodes[0];
    uint8_t bad[32];
    memset(bad, 4, 32);
    CHECK(dd_dungeon_launch(&sim.nodes[1], 0, RUN_ID, now) == DD_DNG_BUSY);
    CHECK(dd_dungeon_local_ready(&sim.nodes[1], RUN_ID, bad, sim.departure.content_hash) == DD_DNG_DRIFT);
    prepare(&sim);
    CHECK(dd_dungeon_ready(h, 1, RUN_ID, bad, sim.departure.content_hash) == DD_DNG_DRIFT);
    CHECK(h->control == DD_DNG_RESYNC);
    prepare(&sim);
    CHECK(dd_dungeon_ready(h, 1, RUN_ID, sim.departure.town_hash, bad) == DD_DNG_DRIFT);
    prepare(&sim);
    CHECK(dd_dungeon_offer(&sim.nodes[1], 1, RUN_ID, &sim.departure, now) == DD_DNG_INVALID);
    dd_dungeon_tick(h, h->deadline);
    CHECK(h->control == DD_DNG_RESYNC);
    start(&sim);
    dd_dng_command c = command(h, DD_DNG_MOVE, 0, 1);
    CHECK(dd_dungeon_request(h, 0, RUN_ID - 1, 0, &c, now) == DD_DNG_INVALID);
    CHECK(dd_dungeon_request(h, 0, RUN_ID, 1, &c, now) == DD_DNG_STALE);
    c.before_hash[0] ^= 1;
    CHECK(request(&sim, 0, &c) == DD_DNG_STALE);
    c.before_hash[0] ^= 1;
    h->validate = NULL;
    CHECK(request(&sim, 0, &c) == DD_DNG_NATIVE_REFUSED);
    h->validate = native_validate;
    c.request = 2;
    OK(request(&sim, 0, &c));
    CHECK(dd_dungeon_host_refused(h, 1) == DD_DNG_NATIVE_REFUSED);
    CHECK(h->control == DD_DNG_ACTIVE);
    CHECK(request(&sim, 0, &c) == DD_DNG_DUPLICATE);
    c.request = 3;
    OK(request(&sim, 0, &c));
    CHECK(dd_dungeon_host_refused(h, 0) == DD_DNG_DRIFT);
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    dd_dungeon_tick(h, h->deadline);
    CHECK(h->control == DD_DNG_RESYNC);
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    dd_dng_event e = complete_host(&sim);
    CHECK(dd_dungeon_guest_event(&sim.nodes[1], 1, RUN_ID, 1, &e, now) == DD_DNG_INVALID);
    CHECK(dd_dungeon_guest_event(&sim.nodes[1], 0, RUN_ID, 2, &e, now) == DD_DNG_DRIFT); /* Lost event. */
    CHECK(dd_dungeon_guest_event(&sim.nodes[1], 0, RUN_ID, 1, &e, now) == DD_DNG_DRIFT);
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    e = complete_host(&sim);
    sim.worlds[1].refuse = 1;
    CHECK(dd_dungeon_guest_event(&sim.nodes[1], 0, RUN_ID, 1, &e, now) == DD_DNG_DRIFT);
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    e = complete_host(&sim);
    OK(dd_dungeon_guest_event(&sim.nodes[1], 0, RUN_ID, 1, &e, now));
    native_execute(&sim.worlds[1], &e.command);
    dice(&sim.worlds[1].rules); /* UI consumed extra RNG. */
    dd_dng_checkpoint actual = checkpoint(&sim.worlds[1]);
    CHECK(dd_dungeon_guest_complete(&sim.nodes[1], &actual, now) == DD_DNG_DRIFT);
    CHECK(sim.nodes[1].sequence == 0);
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    e = complete_host(&sim);
    OK(dd_dungeon_guest_event(&sim.nodes[1], 0, RUN_ID, 1, &e, now));
    actual = e.after;
    actual.rules_seed++; /* Even with the same digest, envelope metadata must agree. */
    CHECK(dd_dungeon_guest_complete(&sim.nodes[1], &actual, now) == DD_DNG_DRIFT);
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    e = complete_host(&sim);
    deliver_event(&sim, 1, e);
    deliver_event(&sim, 2, e);
    dd_dungeon_tick(h, h->deadline);
    CHECK(h->control == DD_DNG_RESYNC); /* Missing release barrier ACK. */
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    e = complete_host(&sim);
    deliver_event(&sim, 1, e);
    OK(dd_dungeon_ack(h, 1, RUN_ID, 1, sim.nodes[1].checkpoint.hash));
    CHECK(dd_dungeon_release(h, 0, RUN_ID, 1, h->checkpoint.hash) == DD_DNG_BUSY); /* Peer2 delayed. */
    CHECK(dd_dungeon_ack(h, 2, RUN_ID, 0, h->checkpoint.hash) == DD_DNG_STALE);
    CHECK(dd_dungeon_ack(h, 2, RUN_ID, 1, bad) == DD_DNG_DRIFT);
    start(&sim);
    dd_dungeon_connection_lost(&sim.nodes[1]);
    CHECK(sim.nodes[1].control == DD_DNG_RESYNC);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    CHECK(dd_dungeon_members(h, 0, RUN_ID, 3) == DD_DNG_DRIFT); /* Never drop around queued native work. */
    start(&sim);
    CHECK(dd_dungeon_members(h, 1, RUN_ID, 3) == DD_DNG_INVALID);
    OK(dd_dungeon_members(h, 0, RUN_ID, 3));
    OK(dd_dungeon_members(&sim.nodes[1], 0, RUN_ID, 3));
    CHECK(dd_dungeon_effective_owner(h, 103) == 0);
    CHECK(dd_dungeon_members(h, 0, RUN_ID, 7) == DD_DNG_INVALID);
    c = command(h, DD_DNG_CURIO, 103, 1);
    transact(&sim, 0, c); /* Only the isolated future core supports this fallback. */
    start(&sim);
    h->sequence = UINT32_MAX;
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    dd_dng_checkpoint cp = checkpoint(&sim.worlds[0]);
    CHECK(dd_dungeon_host_complete(h, &cp, now, &e) == DD_DNG_DRIFT);
    start(&sim);
    c = command(h, DD_DNG_MOVE, 0, 1);
    OK(request(&sim, 0, &c));
    cp = checkpoint(&sim.worlds[0]);
    cp.scene = DD_DNG_TOWN;
    CHECK(dd_dungeon_host_complete(h, &cp, now, &e) ==
          DD_DNG_DRIFT); /* No premature return/re-enable Hamlet. */
    printf("PASS mismatches, stale/gap/replay, deadlines, async refusal, disconnect and ACK barriers\n");
}
static void test_owners(void) {
    uint32_t party[4] = {101, 102, 103, 104}, rev = 99;
    uint8_t owners[4] = {1, 1, 1, 1};
    CHECK(dd_dng_owners_parse(NULL, 0, party, owners, &rev) == 0);
    CHECK(!rev && !owners[0] && !owners[3]);
    const char *text = "\xef\xbb\xbfversion=1\r\nwritten_by=host\r\nrevision=7\r\n; "
                       "ignored\r\nhero.102=1\r\nhero.999=1\r\nfuture=yes\r\n";
    CHECK(dd_dng_owners_parse(text, strlen(text), party, owners, &rev) == 1);
    CHECK(rev == 7 && !owners[0] && owners[1] == 1 && !owners[2] && !owners[3]);
    const char *guest = "written_by=guest\nversion=garbage\nhero.102=1000000";
    CHECK(dd_dng_owners_parse(guest, strlen(guest), party, owners, &rev) == 0);
    CHECK(!rev && !owners[1]);
    const char *bad[] = {"written_by=host\n",
                         "written_by=host\nversion=2",
                         "written_by=host\nversion=1\nhero.102=2",
                         "written_by=host\nversion=1\nhero.102=1\nhero.102=0",
                         "written_by=host\nversion=1\nrevision=4294967296",
                         "written_by=host\nversion=1\nhero.-1=1",
                         "written_by=host\nversion=1\nhero.1=+1",
                         "written_by=host\nversion=1\nrevision",
                         "written_by=host\nversion=1\nversion=1",
                         "written_by=host\nwritten_by=guest\nversion=1",
                         "written_by=host\nversion=1\nhero.0=1"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        CHECK(dd_dng_owners_parse(bad[i], strlen(bad[i]), party, owners, &rev) == -1);
        CHECK(!rev && !owners[1]);
    }
    const char nul[] = "written_by=host\nversion=1\0\nhero.102=1";
    CHECK(dd_dng_owners_parse(nul, sizeof nul - 1, party, owners, &rev) == -1);
    char many[2048];
    size_t n = (size_t)sprintf(many, "written_by=host\nversion=1\n");
    for (unsigned i = 0; i < 65; i++)
        n += (size_t)sprintf(many + n, "hero.%u=1\n", i + 1);
    CHECK(dd_dng_owners_parse(many, n, party, owners, &rev) == -1);
    /* The departure receives a COPY. Later launcher/file changes must not change expedition authority. */
    simulation sim;
    start(&sim);
    sim.departure.owners[1] = 0;
    CHECK(dd_dungeon_effective_owner(&sim.nodes[0], 102) == 1);
    for (unsigned i = 0; i < 10000; i++) {
        char input[512];
        size_t len = rnd() % sizeof input;
        for (size_t j = 0; j < len; j++)
            input[j] = (char)rnd();
        int r = dd_dng_owners_parse(input, len, party, owners, &rev);
        CHECK(r >= -1 && r <= 1);
        for (unsigned j = 0; j < 4; j++)
            CHECK(owners[j] <= 1);
    }
    printf("PASS bounded ownership v1 parser, written_by, defaults, frozen table, 10000 malformed inputs\n");
}
static void reject_packet(const uint8_t *p, size_t len) {
    dd_dng_message out;
    memset(&out, 0xa5, sizeof out);
    CHECK(!dd_dng_decode(p, len, &out));
    const uint8_t *bytes = (const uint8_t *)&out;
    for (size_t i = 0; i < sizeof out; i++)
        CHECK(bytes[i] == 0);
}
static void test_codec(void) {
    dd_dng_message messages[10] = {0};
    for (unsigned i = 0; i < 10; i++) {
        messages[i].type = (uint16_t)(i + 1);
        messages[i].expedition = RUN_ID;
    }
    messages[0].u.offer = departure();
    messages[0].u.offer.item_count = 32;
    for (unsigned i = 0; i < 32; i++)
        messages[0].u.offer.items[i] = (dd_dng_item){1, i + 1, i + 1};
    memset(messages[1].u.ready.town, 5, 32);
    memset(messages[1].u.ready.content, 6, 32);
    world w = {0};
    w.rules = 1;
    w.battle = 2;
    w.quest = 3;
    w.scene = DD_DNG_EXPLORE;
    messages[3].u.checkpoint = checkpoint(&w);
    dd_dungeon h;
    dd_dungeon_init(&h, 0, native_validate, &w);
    h.checkpoint = checkpoint(&w);
    messages[4].u.command = command(&h, DD_DNG_SKILL, 101, 1);
    messages[4].u.command.target_count = 8;
    for (unsigned i = 0; i < 8; i++)
        messages[4].u.command.targets[i] = (dd_dng_entity){DD_DNG_MONSTER, 9001 + i};
    messages[5].sequence = 1;
    messages[5].u.event.command = messages[4].u.command;
    messages[5].u.event.after = messages[3].u.checkpoint;
    memset(messages[6].u.hash, 7, 32);
    memset(messages[7].u.hash, 8, 32);
    messages[8].u.reject.request = 12;
    messages[8].u.reject.reason = DD_DNG_OWNER;
    messages[9].u.connected = 7;
    for (unsigned i = 0; i < 10; i++) {
        uint8_t p[DD_DNG_WIRE_MAX], q[DD_DNG_WIRE_MAX];
        dd_dng_message decoded;
        size_t n = dd_dng_encode(&messages[i], p, sizeof p);
        CHECK(n);
        CHECK(p[0] == 'D' && p[1] == 'N' && p[2] == 'G' && p[3] == '1' && p[4] == 1 && p[5] == 0);
        CHECK(p[16] == 0 && p[17] == 0x40 && p[18] == 0 && p[19] == 0x30); /* Golden little-endian run ID. */
        CHECK(dd_dng_decode(p, n, &decoded));
        CHECK(dd_dng_encode(&decoded, q, sizeof q) == n);
        CHECK(!memcmp(p, q, n));
        CHECK(!dd_dng_encode(&messages[i], q, n - 1));
        for (size_t j = 0; j < n; j++)
            reject_packet(p, j);
        p[n] = 0;
        reject_packet(p, n + 1);
        word(p + 8, (uint32_t)(n + 1 - DD_DNG_WIRE_HEADER));
        reject_packet(p, n + 1);
        for (unsigned trial = 0; trial < 1000; trial++) {
            n = dd_dng_encode(&messages[i], p, sizeof p);
            p[rnd() % n] ^= (uint8_t)(1u << (rnd() % 8));
            if (dd_dng_decode(p, n, &decoded)) {
                size_t m = dd_dng_encode(&decoded, q, sizeof q);
                CHECK(m == n);
                CHECK(!memcmp(p, q, n));
            }
        }
    }
    messages[0].u.offer.item_count = 33;
    CHECK(!dd_dng_encode(&messages[0], (uint8_t[1024]){0}, 1024));
    messages[4].u.command.target_count = 9;
    CHECK(!dd_dng_encode(&messages[4], (uint8_t[1024]){0}, 1024));
    for (unsigned trial = 0; trial < 100000; trial++) {
        uint8_t p[DD_DNG_WIRE_MAX + 32];
        size_t n = rnd() % sizeof p;
        for (size_t i = 0; i < n; i++)
            p[i] = (uint8_t)rnd();
        dd_dng_message m;
        CHECK(!dd_dng_decode(p, n, &m));
    }
    printf("PASS all10 packet types,32 provisions/8 targets, exact lengths,10000 mutations,100000 random "
           "packets\n");
}
static void test_food_departure(void) {
    dd_dng_message message = {0}, decoded;
    uint8_t packet[DD_DNG_WIRE_MAX], again[DD_DNG_WIRE_MAX];
    message.type = DD_DNG_OFFER;
    message.expedition = RUN_ID;
    message.u.offer = departure();
    message.u.offer.items[0] = (dd_dng_item){0x72f14069, 0, 12}; /* Real food: provision/empty ID. */
    CHECK(dd_dng_departure_valid(&message.u.offer));
    size_t length = dd_dng_encode(&message, packet, sizeof packet);
    CHECK(length);
    if (length) {
        CHECK(dd_dng_decode(packet, length, &decoded));
        CHECK(decoded.u.offer.items[0].type == 0x72f14069 && decoded.u.offer.items[0].id == 0 &&
              decoded.u.offer.items[0].count == 12);
        CHECK(dd_dng_encode(&decoded, again, sizeof again) == length && !memcmp(packet, again, length));
        dd_dungeon host, guest;
        dd_dungeon_init(&host, 0, NULL, NULL);
        dd_dungeon_init(&guest, 1, NULL, NULL);
        CHECK(dd_dungeon_offer(&host, 0, RUN_ID, &message.u.offer, 1) == DD_DNG_OK);
        CHECK(dd_dungeon_offer(&guest, 0, RUN_ID, &decoded.u.offer, 1) == DD_DNG_OK);
        CHECK(host.departure.items[0].id == 0 && guest.departure.items[0].id == 0);
    }
    message.u.offer.items[0].type = 0;
    CHECK(!dd_dng_departure_valid(&message.u.offer));
    message.u.offer.items[0].type = 0x72f14069;
    message.u.offer.items[0].count = 0;
    CHECK(!dd_dng_departure_valid(&message.u.offer));
    message.u.offer.items[0].count = UINT32_MAX;
    CHECK(!dd_dng_departure_valid(&message.u.offer));
    printf("PASS food departure: empty item ID preserved over codec and host/guest offer; bad type/count "
           "refused\n");
}
static void test_camp_and_validation(void) {
    simulation sim;
    start(&sim);
    dd_dungeon *h = &sim.nodes[0];
    dd_dng_command c = command(h, DD_DNG_START_CAMP, 0, 1);
    transact(&sim, 0, c);
    c = command(h, DD_DNG_CAMP_SKILL, 102, 1);
    transact(&sim, 1, c);
    c = command(h, DD_DNG_END_CAMP, 0, 2);
    transact(&sim, 0, c);
    CHECK(h->checkpoint.scene == DD_DNG_EXPLORE);
    c = command(h, DD_DNG_TAKE_LOOT, 0, 3);
    CHECK(request(&sim, 0, &c) == DD_DNG_PHASE);
    start_scene(&sim, DD_DNG_LOOT);
    c = command(h, DD_DNG_TAKE_LOOT, 0, 1);
    CHECK(request(&sim, 1, &c) == DD_DNG_OWNER);
    transact(&sim, 0, c);
    c = command(h, DD_DNG_FINISH_QUEST, 0, 2);
    transact(&sim, 0, c);
    c = command(h, DD_DNG_RETURN_TOWN, 0, 3);
    transact(&sim, 0, c);
    dd_dng_departure d = departure();
    d.heroes[1] = 101;
    CHECK(!dd_dng_departure_valid(&d));
    d = departure();
    d.connected = 6;
    CHECK(!dd_dng_departure_valid(&d));
    d = departure();
    d.rules_seed = 0;
    CHECK(!dd_dng_departure_valid(&d));
    d = departure();
    d.map_seed = 0;
    CHECK(dd_dng_departure_valid(&d)); /* Map seed is not a Park-Miller state. */
    d = departure();
    d.battle_seed = 2147483647;
    CHECK(!dd_dng_departure_valid(&d));
    d = departure();
    d.heroes[1] = 0;
    CHECK(!dd_dng_departure_valid(&d));
    d.owners[1] = 0;
    CHECK(dd_dng_departure_valid(&d));
    dd_dng_checkpoint cp = h->checkpoint;
    cp.scene = DD_DNG_BATTLE;
    CHECK(!dd_dng_checkpoint_valid(&cp));
    cp = h->checkpoint;
    cp.active = (dd_dng_entity){DD_DNG_HERO, 101};
    CHECK(!dd_dng_checkpoint_valid(&cp));
    c = command(h, DD_DNG_CAMP_SKILL, 102, 2);
    c.target_count = 2;
    c.targets[0] = c.targets[1] = (dd_dng_entity){DD_DNG_HERO, 101};
    CHECK(!dd_dng_command_valid(&c));
    c = command(h, DD_DNG_CAMP_SKILL, 102, 2);
    c.target_count = 1;
    c.targets[0] = (dd_dng_entity){DD_DNG_HERO, 999};
    CHECK(request(&sim, 1, &c) == DD_DNG_INVALID);
    dd_dungeon_init(h, 256, native_validate, &sim.worlds[0]);
    CHECK(h->control == DD_DNG_RESYNC);
    printf("PASS camping, entity/party identity, bounded fields and separate map/stream seed rules\n");
}
static void test_long_explore(void) {
    simulation sim;
    start(&sim);
    uint32_t requests[3] = {0};
    for (unsigned i = 0; i < 1000; i++) {
        unsigned kind = (rnd() % 3), from = kind ? rnd() % 3 : 0;
        dd_dng_command c = command(&sim.nodes[0],
                                   kind == 0   ? DD_DNG_MOVE
                                   : kind == 1 ? DD_DNG_CURIO
                                               : DD_DNG_TRAP,
                                   kind ? 101 + from : 0, ++requests[from]);
        if (!kind) c.argument = 1 + rnd() % 100;
        transact(&sim, from, c);
    }
    printf("PASS1000 mixed-owner exploration actions through codec with interleaved ACK arrival\n");
}
int main(void) {
    test_food_departure();
    test_codec();
    test_owners();
    test_expedition();
    test_failures();
    test_camp_and_validation();
    test_long_explore();
    printf("PASS dungeon foundation (%u committed three-copy fixture actions); NO live game integration "
           "tested\n",
           checks);
    return 0;
}
