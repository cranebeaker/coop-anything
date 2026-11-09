/* End-to-end test over real TCP (loopback): host + 3 guests run the town sync (dd_town) through the
 * encrypted session (dd_session) and the transport (dd_net); a 4th guest with the wrong password must be
 * turned away while the others play on. All four copies of the fake Hamlet must end identical.
 *   gcc -std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       -Isrc -Itests -o build/test_net tests/test_net.c src/dd_net.c src/dd_session.c src/dd_crypto_openssl.c \
 *       src/dd_town.c src/dd_proto.c -lcrypto -lpthread
 *   setarch x86_64 -R build/test_net */
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "dd_net.h"
#include "dd_town.h"
#include "fake_town.h"

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

#define GUESTS 3
#define ACTIONS_PER_GUEST 300
#define TIMEOUT 5000
static const char *k_password = "test-password-7f3a9c";
static int g_port;

static uint32_t next_rnd(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }

static dd_action random_action(uint32_t *s) {
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = (uint8_t)(1 + next_rnd(s) % (DD_ACT_COUNT_ - 1));
    switch (a.kind) {
    case DD_ACT_ASSIGN_HERO:
        a.u.assign.activity = (uint8_t)(next_rnd(s) % DD_ACTIVITY_COUNT_);
        a.u.assign.slot = (uint8_t)(next_rnd(s) % 3);
        a.u.assign.hero_id = 1 + next_rnd(s) % 47;
        break;
    case DD_ACT_BUY_UPGRADE:
        a.u.upgrade.tree = 1 + next_rnd(s) % 5;
        a.u.upgrade.code = (uint8_t)('0' + next_rnd(s) % 4);
        a.u.upgrade.hero_id = next_rnd(s) % 12;
        break;
    case DD_ACT_RECRUIT:
    case DD_ACT_DISMISS: a.u.hero.hero_id = 1 + next_rnd(s) % 47; break;
    case DD_ACT_WAGON_BUY:
    case DD_ACT_WAGON_SELL:
        a.u.items.slot=(uint16_t)(next_rnd(s)%8);a.u.items.count=1;
        a.u.items.type=1;a.u.items.id=a.u.items.slot+1;a.u.items.cost=500;
        break;
    case DD_ACT_RENAME: a.u.rename.hero_id=1;strcpy(a.u.rename.before,"Hero");strcpy(a.u.rename.after,"Hero B");break;
    case DD_ACT_SKILL: a.u.skill.hero_id=1;a.u.skill.after=1;break;
    case DD_ACT_TRINKET_MOVE: a.u.move.src_type=1;a.u.move.src_id=1;a.u.move.dst_slot=1;break;
    case DD_ACT_PARTY: a.u.party.before[0]=1;a.u.party.after[1]=1;break;
    case DD_ACT_HEIRLOOM_EXCHANGE:
        a.u.exchange.src_type = 1 + (int)(next_rnd(s) % 3);
        a.u.exchange.dst_type = 1 + a.u.exchange.src_type % 3;
        a.u.exchange.src_amount = 1 + (int)(next_rnd(s) % 6);
        a.u.exchange.dst_amount = 1 + (int)(next_rnd(s) % 3);
        break;
    default:
        a.u.items.slot = (uint16_t)(next_rnd(s) % 8);
        a.u.items.count = (uint16_t)(1 + next_rnd(s) % 4);
    }
    return a;
}

/* ---------------- host ---------------- */
static town host_town;
static dd_town host_sync;
static dd_conn host_conns[GUESTS];
static int host_refused;

static void host_send(void *net, int to, const dd_msg *m) {
    (void)net;
    if (to == DD_TOWN_ALL) {
        for (int g = 0; g < GUESTS; g++) dd_conn_send(&host_conns[g], m);
    } else if (to >= 1 && to <= GUESTS) {
        dd_conn_send(&host_conns[to - 1], m);
    }
}

static void *host_main(void *arg) {
    dd_socket l = *(dd_socket *)arg;
    int good = 0;
    /* 4 connections arrive: 3 with the right password, one without. */
    for (int i = 0; i < GUESTS + 1 && good < GUESTS + 1; i++) {
        dd_socket s = dd_net_accept(l, TIMEOUT);
        if (s == DD_NO_SOCKET) break;
        dd_conn c;
        dd_conn_init(&c, s);
        if (dd_conn_handshake_host(&c, k_password, TIMEOUT) && good < GUESTS) {
            host_conns[good++] = c;
        } else {
            host_refused++;
            dd_conn_free(&c);
        }
    }
    CHECK(good == GUESTS, "host accepted %d guests", good);
    town_init(&host_town);
    dd_town_init(&host_sync, &host_town, town_apply, NULL, host_send);
    uint32_t r = 99;
    int done = 0, actions = 0;
    while (done < good) {
        for (int g = 0; g < good; g++) {
            dd_msg m;
            dd_poll_result p = dd_conn_poll(&host_conns[g], 5, &m);
            if (p == DD_POLL_CLOSED) { done = good; CHECK(0, "guest %d dropped", g + 1); break; }
            if (p != DD_POLL_MSG) continue;
            if (m.type == DD_MSG_TOWN_REQUEST) dd_town_host_request(&host_sync, g + 1, &m);
            else if (m.type == DD_MSG_PING) done++;
        }
        if (actions < 200 && next_rnd(&r) % 4 == 0) { /* the host plays too */
            dd_action a = random_action(&r);
            dd_town_host_local(&host_sync, &a);
            actions++;
        }
    }
    dd_msg fin;
    memset(&fin, 0, sizeof fin);
    fin.type = DD_MSG_PONG;
    for (int g = 0; g < good; g++) dd_conn_send(&host_conns[g], &fin);
    return NULL;
}

/* ---------------- guests ---------------- */
typedef struct {
    int index;
    const char *password;
    town t;
    dd_town sync;
    dd_conn conn;
    int connected, finished;
    long applied, rejected;
} guest;

static void guest_send(void *net, int to, const dd_msg *m) {
    (void)to;
    dd_conn_send(&((guest *)net)->conn, m);
}

static void *guest_main(void *arg) {
    guest *g = arg;
    town_init(&g->t);
    dd_town_init(&g->sync, &g->t, town_apply, g, guest_send);
    if (!dd_conn_init(&g->conn, dd_net_connect("127.0.0.1", g_port, TIMEOUT))) return NULL;
    g->connected = dd_conn_handshake_guest(&g->conn, g->password, TIMEOUT);
    if (!g->connected) {
        dd_conn_free(&g->conn);
        return NULL;
    }
    uint32_t r = 1000u + (uint32_t)g->index * 7919u;
    int sent = 0;
    for (;;) {
        if (sent < ACTIONS_PER_GUEST) {
            dd_action a = random_action(&r);
            dd_town_guest_local(&g->sync, &a);
            if (++sent == ACTIONS_PER_GUEST) {
                dd_msg ping;
                memset(&ping, 0, sizeof ping);
                ping.type = DD_MSG_PING;
                dd_conn_send(&g->conn, &ping);
            }
        }
        dd_msg m;
        dd_poll_result p = dd_conn_poll(&g->conn, sent < ACTIONS_PER_GUEST ? 0 : TIMEOUT, &m);
        if (p == DD_POLL_CLOSED) break;
        if (p != DD_POLL_MSG) continue;
        if (m.type == DD_MSG_TOWN_APPLY) g->applied += dd_town_guest_apply(&g->sync, &m);
        else if (m.type == DD_MSG_TOWN_REJECT) g->rejected++;
        else if (m.type == DD_MSG_PONG) { g->finished = 1; break; }
    }
    dd_conn_free(&g->conn);
    return NULL;
}

int main(void) {
    dd_net_startup();
    dd_socket l = DD_NO_SOCKET;
    for (g_port = 27400; g_port < 27500 && l == DD_NO_SOCKET; g_port++) l = dd_net_listen(g_port, 0);
    g_port--;
    CHECK(l != DD_NO_SOCKET, "listen");

    pthread_t host, threads[GUESTS + 1];
    static guest guests[GUESTS + 1];
    pthread_create(&host, NULL, host_main, &l);
    for (int i = 0; i <= GUESTS; i++) {
        guests[i].index = i + 1;
        guests[i].password = i == 1 ? "wrong password" : k_password; /* guest 2 knows the wrong password */
    }
    /* the impostor first, then the three real guests */
    pthread_create(&threads[1], NULL, guest_main, &guests[1]);
    pthread_join(threads[1], NULL);
    for (int i = 0; i <= GUESTS; i++)
        if (i != 1) pthread_create(&threads[i], NULL, guest_main, &guests[i]);
    for (int i = 0; i <= GUESTS; i++)
        if (i != 1) pthread_join(threads[i], NULL);
    pthread_join(host, NULL);
    dd_net_close(l);

    CHECK(!guests[1].connected, "the guest with the wrong password must be refused");
    CHECK(host_refused == 1, "host refused %d connections", host_refused);
    uint32_t h = town_hash(&host_town);
    long applied = 0, rejected = 0;
    for (int i = 0; i <= GUESTS; i++) {
        if (i == 1) continue;
        CHECK(guests[i].connected && guests[i].finished, "guest %d finished", guests[i].index);
        CHECK(!guests[i].sync.needs_resync, "guest %d needs a resync", guests[i].index);
        CHECK(town_hash(&guests[i].t) == h, "guest %d's Hamlet differs from the host's", guests[i].index);
        CHECK(guests[i].t.min_gold >= 0 && !hero_in_two_slots(&guests[i].t), "guest %d invariants", guests[i].index);
        applied += guests[i].applied;
        rejected += guests[i].rejected;
    }
    printf("tcp: host accepted %u actions, refused %u; guests applied %ld, were refused %ld; wrong password refused; "
           "all Hamlets identical\n", host_sync.accepted, host_sync.rejected, applied, rejected);
    dd_net_cleanup();
    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
