/* End to end: two launcher cores over an in-memory "Steam" (with backpressure and optional corruption),
 * fake games on real 127.0.0.1 TCP, and real save folders on disk.
 *
 * The host's fake game stands in for the co-op runtime: it starts listening on the ini's port only after a
 * "loading" delay, and answers every byte XOR 0x5A. The guest's fake game connects to the guest's relay port
 * once the core says "launch", pushes 2 MB through and checks every answer. */
#include "lr_core.h"
#include "lr_snapshot.h"

#include "dson_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static uint64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/* ---- fake Steam ---- */

typedef struct qmsg {
    uint64_t from, to;
    int channel;
    size_t len;
    struct qmsg *next;
    uint8_t data[];
} qmsg;

static qmsg *q_head, *q_tail;
static int backpressure;   /* every Nth send says "full" (0 = never) */
static int corrupt_chunks; /* flip a byte in the next SAVE_CHUNK */
static long send_count;

typedef struct {
    uint64_t me;
    int game_running;
    int allow_all;
    int no_dlc;     /* this node has no DLC installed */
    lr_core core;
} node;

static int fake_send(void *ctx, uint64_t peer, int channel, const uint8_t *buf, size_t len) {
    node *n = ctx;
    if (backpressure && ++send_count % backpressure == 0) return 0;
    qmsg *m = malloc(sizeof *m + len);
    m->from = n->me;
    m->to = peer;
    m->channel = channel;
    m->len = len;
    m->next = NULL;
    memcpy(m->data, buf, len);
    if (corrupt_chunks && len > 20 && buf[3] == LR_SAVE_CHUNK) {
        m->data[len - 1] ^= 1;
        corrupt_chunks = 0;
    }
    if (q_tail) q_tail->next = m; else q_head = m;
    q_tail = m;
    return 1;
}

static int allowed(void *ctx, uint64_t peer) {
    node *n = ctx;
    return n->allow_all || peer != 3; /* peer 3 is not in the lobby */
}

static int game_running(void *ctx) { return ((node *)ctx)->game_running; }
static int has_dlc(void *ctx, const char *name) { (void)name; return !((node *)ctx)->no_dlc; }

static void fake_random(void *ctx, uint8_t *buf, size_t len) {
    (void)ctx;
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)rand();
}

static void quiet_log(void *ctx, const char *line) {
    (void)ctx;
    if (getenv("LR_TEST_VERBOSE")) printf("  [%llu] %s\n", (unsigned long long)((node *)ctx)->me, line);
}

static node *nodes[8];

static void deliver(void) {
    while (q_head) {
        qmsg *m = q_head;
        q_head = m->next;
        if (!q_head) q_tail = NULL;
        if (m->to < 8 && nodes[m->to]) lr_core_on_message(&nodes[m->to]->core, m->from, m->channel, m->data, m->len, now_ms());
        free(m);
    }
}

static void drop_queue(void) {
    while (q_head) {
        qmsg *m = q_head;
        q_head = m->next;
        free(m);
    }
    q_tail = NULL;
}

/* ---- disk ---- */

static char root[512];

static void write_text(const char *dir, const char *name, const char *text) {
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, dir, name);
    CHECK(lr_write_file(path, (const uint8_t *)text, strlen(text)));
}

static char *read_text(const char *path) {
    uint8_t *data;
    size_t len;
    if (!lr_read_file(path, 1 << 26, &data, &len)) return NULL;
    char *s = realloc(data, len + 1);
    s[len] = 0;
    return s;
}

static int same_file(const char *a, const char *b) {
    uint8_t *x, *y;
    size_t lx, ly;
    if (!lr_read_file(a, 1 << 26, &x, &lx)) return 0;
    if (!lr_read_file(b, 1 << 26, &y, &ly)) { free(x); return 0; }
    int same = lx == ly && memcmp(x, y, lx) == 0;
    free(x);
    free(y);
    return same;
}

static void setup_node(node *n, uint64_t me, const char *name) {
    memset(n, 0, sizeof *n);
    n->me = me;
    lr_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.send = fake_send;
    cfg.ctx = n;
    cfg.allowed = allowed;
    cfg.game_running = game_running;
    cfg.has_dlc = has_dlc;
    cfg.random = fake_random;
    cfg.log = quiet_log;
    cfg.build = 0x4d78fbfa;
    cfg.kit = 0xaff98e68;
    cfg.coop_slot = 3;
    char base[LR_PATH_MAX], mods[LR_PATH_MAX];
    lr_join(base, sizeof base, root, name);
    lr_join(mods, sizeof mods, base, "mods");
    lr_make_dirs(mods);
    lr_join(cfg.ini_path, sizeof cfg.ini_path, mods, "ddcoop.ini");
    lr_join(cfg.heroes_path, sizeof cfg.heroes_path, mods, "ddcoop-heroes.ini");
    lr_join(cfg.remote_dir, sizeof cfg.remote_dir, base, "remote");
    lr_join(cfg.backup_root, sizeof cfg.backup_root, base, "backups");
    lr_make_dirs(cfg.remote_dir);
    lr_core_init(&n->core, &cfg);
    nodes[me] = n;
}

/* ---- fake games ---- */

typedef struct {
    lr_sock listener, conn;
    int port;
    uint64_t listen_at;
    int saw_eof;
    long echoed;
} host_game;

static void host_game_tick(host_game *g) {
    if (g->listener == LR_NOSOCK && now_ms() >= g->listen_at) g->listener = lr_listen_local(g->port, NULL);
    if (g->listener == LR_NOSOCK) return;
    if (g->conn == LR_NOSOCK) g->conn = lr_accept_nb(g->listener);
    if (g->conn == LR_NOSOCK || g->saw_eof) return;
    uint8_t buf[8192];
    int n = lr_recv_nb(g->conn, buf, sizeof buf);
    if (n < 0) {
        g->saw_eof = 1;
        lr_close(g->conn);
        return;
    }
    for (int i = 0; i < n; i++) buf[i] ^= 0x5A;
    int off = 0;
    while (off < n) { /* the test socket buffers are large; spin until written */
        int w = lr_send_nb(g->conn, buf + off, (size_t)(n - off));
        if (w < 0) return;
        off += w;
    }
    g->echoed += n;
}

#define PUSH_BYTES (2 * 1024 * 1024)

typedef struct {
    lr_sock s;
    int connected;
    long sent, got;
    int bad;
} guest_game;

static uint8_t pattern(long i) { return (uint8_t)(i * 31 + (i >> 9)); }

static void guest_game_tick(guest_game *g) {
    if (!g->connected) {
        int c = lr_connect_check(g->s);
        if (c < 0) g->bad = 1;
        if (c != 1) return;
        g->connected = 1;
    }
    if (g->sent < PUSH_BYTES) {
        uint8_t buf[4096];
        size_t n = PUSH_BYTES - g->sent < (long)sizeof buf ? (size_t)(PUSH_BYTES - g->sent) : sizeof buf;
        for (size_t i = 0; i < n; i++) buf[i] = pattern(g->sent + (long)i);
        int w = lr_send_nb(g->s, buf, n);
        if (w > 0) g->sent += w;
    }
    uint8_t in[8192];
    int r = lr_recv_nb(g->s, in, sizeof in);
    for (int i = 0; i < r; i++)
        if ((in[i] ^ 0x5A) != pattern(g->got + i)) g->bad = 1;
    if (r > 0) g->got += r;
}

/* ---- scenario ---- */

static void make_host_campaign(const char *remote) {
    char dir[LR_PATH_MAX];
    lr_join(dir, sizeof dir, remote, "profile_2");
    lr_make_dirs(dir);
    for (int i = 0; i < 26; i++) {
        char name[64], text[128];
        snprintf(name, sizeof name, "persist.part%02d.json", i);
        snprintf(text, sizeof text, "{\"host\": %d}", i);
        write_text(dir, name, text);
    }
    /* one big file, spread over many chunks */
    size_t big = 100000;
    uint8_t *data = malloc(big);
    for (size_t i = 0; i < big; i++) data[i] = (uint8_t)rand();
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, dir, "persist.map.json");
    CHECK(lr_write_file(path, data, big));
    free(data);
    /* the roster, in the game's binary format */
    static uint8_t roster[65536];
    static const dw_hero heroes[] = {{"2", "Dismas", "highwayman", 4, 1}, {"1", "Reynauld", "crusader", 6, 0},
                                     {"8", "Bourneville", "vestal", 13, 2}};
    size_t n = dw_roster(roster, heroes, 3);
    lr_join(path, sizeof path, dir, "persist.roster.json");
    CHECK(lr_write_file(path, roster, n));
    /* in town, with the Musketeer DLC (the game turns it on for anyone who has it) */
    static const char *const dlc[] = {"musketeer"};
    n = dw_game(roster, 0, "", dlc, 1);
    lr_join(path, sizeof path, dir, "persist.game.json");
    CHECK(lr_write_file(path, roster, n));
    /* another campaign that must never travel */
    char other[LR_PATH_MAX];
    lr_join(other, sizeof other, remote, "profile_0");
    lr_make_dirs(other);
    write_text(other, "persist.town.json", "{\"single player\": true}");
}

static void make_guest_campaign(const char *remote) {
    char dir[LR_PATH_MAX];
    lr_join(dir, sizeof dir, remote, "profile_2");
    lr_make_dirs(dir);
    write_text(dir, "persist.part00.json", "{\"guest\": \"own\"}");
    write_text(dir, "persist.extra.json", "{\"guest only\": 1}");
}

static int run_until(node *a, node *b, int (*done)(void *), void *arg, int max_ms) {
    uint64_t end = now_ms() + (uint64_t)max_ms;
    while (now_ms() < end) {
        lr_core_tick(&a->core, now_ms());
        if (b) lr_core_tick(&b->core, now_ms());
        deliver();
        if (done(arg)) return 1;
#ifdef _WIN32
        Sleep(1);
#else
        usleep(200);
#endif
    }
    return 0;
}

static int state_is(void *p) { node *n = p; return n->core.state == LR_ST_GUEST_LAUNCH || n->core.state == LR_ST_FAILED; }
static int failed(void *p) { return ((node *)p)->core.state == LR_ST_FAILED; }
static int never(void *p) { (void)p; return 0; }

static node *g_host, *g_guest;

/* The guest has the host's current table. */
static int owners_match(void *p) {
    (void)p;
    const lr_owners *h = &g_host->core.owners, *g = &g_guest->core.owners;
    return g->revision == h->revision && g->count == h->count && memcmp(g->owner, h->owner, (size_t)h->count) == 0;
}

/* "<id><h|g>": that hero belongs to the host / guest in both tables. */
static int owner_is(void *p) {
    const char *spec = p;
    uint32_t id = (uint32_t)strtoul(spec, NULL, 10);
    int want = spec[strlen(spec) - 1] == 'g' ? LR_PLAYER_GUEST : LR_PLAYER_HOST;
    int hi = lr_owners_find(&g_host->core.owners, id), gi = lr_owners_find(&g_guest->core.owners, id);
    return hi >= 0 && gi >= 0 && g_host->core.owners.owner[hi] == want && g_guest->core.owners.owner[gi] == want;
}

typedef struct { int attempts, sent; } close_sender;
static int backpressured_close(void *ctx,uint64_t peer,int channel,const uint8_t *buf,size_t len) {
    close_sender *sender=ctx;
    lr_msg m;
    CHECK(peer==2 && channel==LR_CH_STREAM);
    CHECK(lr_decode(buf,len,channel,&m) && m.type==LR_STREAM_CLOSE && m.id==77);
    if(++sender->attempts==1)return 0;
    sender->sent++;return 1;
}
static void test_pending_close(void) {
    /* No socket is opened: isolate the queued CLOSE state after a local EOF. */
    for(int receive_close=0;receive_close<2;receive_close++) {
        lr_relay relay;close_sender sender={0};
        CHECK(lr_relay_init(&relay,LR_ROLE_HOST,1,backpressured_close,&sender));
        lr_stream *s=&relay.streams[0];
        s->used=s->connected=s->local_closed=1;s->peer=2;s->id=77;
        lr_msg m={0};m.type=LR_STREAM_CLOSE;m.id=77;
        s->pend_len=lr_encode(&m,s->pend);
        if(receive_close)lr_relay_on_message(&relay,2,&m,0);
        else s->peer_closed=1;
        CHECK(lr_relay_active_streams(&relay)==1);
        lr_relay_tick(&relay,1); /* transport full: keep queued CLOSE, even with peer EOF */
        CHECK(sender.attempts==1 && sender.sent==0 && lr_relay_active_streams(&relay)==1);
        lr_relay_tick(&relay,2);
        CHECK(sender.sent==1 && lr_relay_active_streams(&relay)==0);
        lr_relay_free(&relay);
    }
    printf("relay close: queued EOF survives peer close and backpressure\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    srand(4242);
    lr_sock_startup();
    test_pending_close();
#ifdef _WIN32
    snprintf(root, sizeof root, "build/lrtest-%lu-%llu", GetCurrentProcessId(), (unsigned long long)now_ms());
    CHECK(!lr_dir_exists(root) && lr_make_dirs(root));
#else
    snprintf(root, sizeof root, "build/lrtest-XXXXXX");
    CHECK(mkdtemp(root) != NULL);
#endif

    node host, guest, intruder, guest2;
    setup_node(&host, 1, "host");
    setup_node(&guest, 2, "guest");
    setup_node(&intruder, 3, "intruder");
    setup_node(&guest2, 4, "guest2");
    g_host = &host;
    g_guest = &guest;

    /* 1. no campaign in slot 3: a clear refusal */
    CHECK(!lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.state == LR_ST_FAILED && strstr(host.core.error, "slot 3"));

    make_host_campaign(host.core.cfg.remote_dir);
    make_guest_campaign(guest.core.cfg.remote_dir);

    /* 2. host starts: ini says host, a 32-character password, the chosen port */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    char *ini = read_text(host.core.cfg.ini_path);
    CHECK(ini && strstr(ini, "role=host") && strstr(ini, "bind_any=0") && strstr(ini, "coop_slots=3"));
    CHECK(ini && strstr(ini, host.core.password) && strlen(host.core.password) == LR_PASSWORD_LEN);
    free(ini);

    /* 3. someone outside the lobby is ignored */
    CHECK(lr_core_guest_join(&intruder.core, 1, now_ms()));
    deliver();
    CHECK(host.core.peer == 0 && intruder.core.state == LR_ST_GUEST_HELLO);

    /* 4. the guest joins with Steam's send buffer filling up every 3rd message */
    backpressure = 3;
    CHECK(lr_core_guest_join(&guest.core, 1, now_ms()));
    CHECK(run_until(&host, &guest, state_is, &guest, 10000));
    CHECK(guest.core.state == LR_ST_GUEST_LAUNCH);
    CHECK(strcmp(guest.core.password, host.core.password) == 0);
    char hdir[LR_PATH_MAX], gdir[LR_PATH_MAX];
    lr_join(hdir, sizeof hdir, host.core.cfg.remote_dir, "profile_2");
    lr_join(gdir, sizeof gdir, guest.core.cfg.remote_dir, "profile_2");
    for (int i = 0; i < 26; i++) {
        char name[64], a[LR_PATH_MAX], b[LR_PATH_MAX];
        snprintf(name, sizeof name, "persist.part%02d.json", i);
        lr_join(a, sizeof a, hdir, name);
        lr_join(b, sizeof b, gdir, name);
        CHECK(same_file(a, b));
    }
    char a[LR_PATH_MAX], b[LR_PATH_MAX];
    lr_join(a, sizeof a, hdir, "persist.roster.json");
    lr_join(b, sizeof b, gdir, "persist.roster.json");
    CHECK(same_file(a, b));
    lr_join(a, sizeof a, hdir, "persist.map.json");
    lr_join(b, sizeof b, gdir, "persist.map.json");
    CHECK(same_file(a, b));
    lr_join(b, sizeof b, gdir, "persist.extra.json");
    CHECK(!lr_file_exists(b)); /* the guest's own extra file is gone from the slot ... */
    lr_join(b, sizeof b, guest.core.backup_path, "persist.extra.json");
    CHECK(lr_file_exists(b));  /* ... but kept in the backup, with its own version of part00 */
    lr_join(b, sizeof b, guest.core.backup_path, "persist.part00.json");
    char *own = read_text(b);
    CHECK(own && strstr(own, "own"));
    free(own);
    lr_join(b, sizeof b, guest.core.cfg.remote_dir, "profile_0");
    CHECK(!lr_dir_exists(b)); /* the host's other campaign never travelled */
    ini = read_text(guest.core.cfg.ini_path);
    char port_line[64];
    snprintf(port_line, sizeof port_line, "port=%d", guest.core.game_port);
    CHECK(ini && strstr(ini, "role=guest") && strstr(ini, "host=127.0.0.1") && strstr(ini, port_line));
    CHECK(ini && strstr(ini, host.core.password));
    free(ini);
    CHECK(lr_core_take_launch(&guest.core) && !lr_core_take_launch(&guest.core));

    /* 4b. hero ownership: the host's table (from its campaign) reaches the guest; all heroes start as the host's */
    CHECK(host.core.owners.count == 3 && host.core.owners.revision >= 1);
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));
    CHECK(guest.core.owners.count == 3 && guest.core.owners.heroes[0].id == 1 && guest.core.owners.heroes[2].id == 8);
    CHECK(strcmp(guest.core.owners.heroes[1].name, "Dismas") == 0 && guest.core.owners.heroes[2].status == 2);
    for (int i = 0; i < 3; i++) CHECK(guest.core.owners.owner[i] == LR_PLAYER_HOST);
    CHECK(lr_core_player(&host.core) == LR_PLAYER_HOST && lr_core_player(&guest.core) == LR_PLAYER_GUEST);
    ini = read_text(host.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "written_by=host") && strstr(ini, "hero.1=0") && strstr(ini, "hero.8=0"));
    free(ini);
    ini = read_text(guest.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "written_by=guest") && strstr(ini, "hero.2=0"));
    free(ini);

    /* 4c. the guest ticks Dismas: the host decides, both tables and both files say guest */
    CHECK(lr_core_claim(&guest.core, 2, 1));
    CHECK(run_until(&host, &guest, owner_is, (void *)"2g", 5000));
    ini = read_text(host.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "hero.2=1") && strstr(ini, "hero.1=0"));
    free(ini);
    ini = read_text(guest.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "hero.2=1"));
    free(ini);

    /* 4d. the guest cannot give away the host's hero, nor tick an unknown one: refused, the table comes back */
    uint32_t rev = host.core.owners.revision;
    CHECK(lr_core_claim(&guest.core, 1, 0));
    CHECK(lr_core_claim(&guest.core, 99, 1));
    run_until(&host, &guest, never, NULL, 300);
    CHECK(host.core.owners.revision == rev && guest.core.owners.revision == rev);
    CHECK(guest.core.owners.owner[0] == LR_PLAYER_HOST && guest.core.owners.count == 3);

    /* 4e. the host hands Bourneville over and takes Dismas back */
    CHECK(lr_core_claim(&host.core, 8, 0) && lr_core_claim(&host.core, 2, 1));
    CHECK(!lr_core_claim(&host.core, 2, 1)); /* already the host's */
    CHECK(run_until(&host, &guest, owner_is, (void *)"8g", 5000) && run_until(&host, &guest, owner_is, (void *)"2h", 5000));
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));

    /* 4f. the roster changes while playing (Bourneville died, Corviser recruited): owners kept, new = host */
    CHECK(lr_core_claim(&guest.core, 1, 1));
    CHECK(run_until(&host, &guest, owner_is, (void *)"1g", 5000));
    static uint8_t roster2[65536];
    static const dw_hero heroes2[] = {{"1", "Reynauld", "crusader", 8, 0}, {"2", "Dismas", "highwayman", 4, 1},
                                      {"31", "Corviser", "bounty_hunter", 0, 1}};
    size_t rn = dw_roster(roster2, heroes2, 3);
    lr_core_host_roster(&host.core, roster2, rn);
    lr_core_host_roster(&host.core, (const uint8_t *)"not a roster", 12); /* ignored */
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));
    CHECK(lr_owners_find(&guest.core.owners, 8) == -1 && lr_owners_find(&guest.core.owners, 31) == 2);
    CHECK(guest.core.owners.owner[0] == LR_PLAYER_GUEST && guest.core.owners.owner[1] == LR_PLAYER_HOST &&
          guest.core.owners.owner[2] == LR_PLAYER_HOST && guest.core.owners.heroes[0].xp == 8);

    /* 5. a second friend is told the host is full */
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "1 vriend"));

    /* 6. the games: the host's runtime listens only after 1.5 s of "loading" */
    host_game hg = {LR_NOSOCK, LR_NOSOCK, host.core.game_port, now_ms() + 1500, 0, 0};
    guest_game gg = {lr_connect_local_nb(guest.core.game_port), 0, 0, 0, 0};
    CHECK(gg.s != LR_NOSOCK);
    uint64_t end = now_ms() + 30000;
    while (now_ms() < end && gg.got < PUSH_BYTES && !gg.bad) {
        lr_core_tick(&host.core, now_ms());
        lr_core_tick(&guest.core, now_ms());
        deliver();
        host_game_tick(&hg);
        guest_game_tick(&gg);
    }
    printf("relay: %ld bytes up, %ld echoed back, host game echoed %ld\n", gg.sent, gg.got, hg.echoed);
    CHECK(!gg.bad && gg.got == PUSH_BYTES);
    CHECK(host.core.state == LR_ST_HOST_PLAYING);

    /* 7. the guest's game closes: the host's game must see the end too */
    lr_close(gg.s);
    end = now_ms() + 5000;
    while (now_ms() < end && !hg.saw_eof) {
        lr_core_tick(&host.core, now_ms());
        lr_core_tick(&guest.core, now_ms());
        deliver();
        host_game_tick(&hg);
    }
    CHECK(hg.saw_eof);
    end = now_ms() + 2000;
    while (now_ms() < end && (lr_relay_active_streams(&host.core.relay) || lr_relay_active_streams(&guest.core.relay))) {
        lr_core_tick(&host.core, now_ms());
        lr_core_tick(&guest.core, now_ms());
        deliver();
    }
    CHECK(lr_relay_active_streams(&host.core.relay) == 0 && lr_relay_active_streams(&guest.core.relay) == 0);
    lr_close(hg.listener);
    backpressure = 0;

    /* 8. the guest leaves; the host waits for a new friend */
    lr_core_stop(&guest.core);
    deliver();
    CHECK(host.core.state == LR_ST_HOST_WAITING && host.core.peer == 0);
    ini = read_text(guest.core.cfg.ini_path);
    CHECK(ini && strstr(ini, "role=off"));
    free(ini);

    /* 9. a corrupted chunk: the guest refuses and keeps its own save */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest3");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    corrupt_chunks = 1;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000));
    CHECK(guest2.core.state == LR_ST_FAILED && strstr(guest2.core.error, "onvolledig"));
    lr_join(b, sizeof b, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(a, sizeof a, b, "persist.extra.json");
    CHECK(lr_file_exists(a));
    deliver();
    CHECK(host.core.state == LR_ST_HOST_WAITING && host.core.peer == 0);

    /* 10. a different game build is refused by the host */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest4");
    guest2.core.cfg.build = 0x12345678;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "versies"));

    /* 10b. another DDCoop kit (another runtime) is refused with a message the friend can act on */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest4b");
    guest2.core.cfg.kit = 0x9d760035;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "DDCoop-versies"));

    /* 11. joining with the game already open is refused before anything is touched */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest5");
    guest2.game_running = 1;
    CHECK(!lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(strstr(guest2.core.error, "sluit eerst"));

    /* 11b. a guest without the campaign's DLC is refused before its own save is touched (the game would call
     * the campaign corrupted and start a new one in that slot) */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest5b");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    guest2.no_dlc = 1;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000));
    CHECK(guest2.core.state == LR_ST_FAILED && strstr(guest2.core.error, "The Musketeer (gratis)"));
    lr_join(b, sizeof b, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(a, sizeof a, b, "persist.extra.json");
    CHECK(lr_file_exists(a)); /* own save untouched ... */
    lr_join(a, sizeof a, b, "persist.game.json");
    CHECK(!lr_file_exists(a));
    CHECK(!guest2.core.backup_path[0] && !lr_dir_exists(guest2.core.cfg.backup_root)); /* ... so no backup needed */
    ini = read_text(guest2.core.cfg.ini_path);
    CHECK(!ini || !strstr(ini, "role=guest"));
    free(ini);
    deliver();
    CHECK(host.core.state == LR_ST_HOST_WAITING && host.core.peer == 0 && strstr(host.core.status, "DLC"));

    /* 12. a silent host is noticed after 20 s (clock jumped forward) */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest6");
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    drop_queue();
    lr_core_tick(&guest2.core, now_ms() + LR_PEER_TIMEOUT_MS + 1000);
    CHECK(guest2.core.state == LR_ST_FAILED && strstr(guest2.core.error, "20 s"));

    /* 12b. a game whose session ended takes no new guest, and the friend hears why */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest7");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    lr_core_host_spent(&host.core);
    CHECK(host.core.host_spent && strstr(host.core.status, "opnieuw op Host"));
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "opnieuw op Host") && host.core.peer == 0);
    lr_join(b, sizeof b, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(a, sizeof a, b, "persist.extra.json");
    CHECK(lr_file_exists(a) && !guest2.core.backup_path[0]); /* Nothing of the friend's touched. */

    /* 13. the host stops: ini back to role=off; the heroes file stays */
    lr_core_stop(&host.core);
    ini = read_text(host.core.cfg.ini_path);
    CHECK(ini && strstr(ini, "role=off"));
    free(ini);
    CHECK(host.core.owners.count == 0 && lr_file_exists(host.core.cfg.heroes_path));

    CHECK(lr_core_host_start(&host.core, now_ms()) && !host.core.host_spent); /* A new game: a new session. */
    lr_core_stop(&host.core);
    /* 14. hosting again restores the host's earlier choices (Reynauld stays the guest's) ... */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.owners.count == 3 && host.core.owners.owner[lr_owners_find(&host.core.owners, 1)] == LR_PLAYER_GUEST);
    lr_core_stop(&host.core);
    /* ... but never choices from a file this PC got as a guest of another campaign */
    static const char guest_file[] = "version=1\r\nwritten_by=guest\r\nhero.1=1\r\n";
    CHECK(lr_write_file(host.core.cfg.heroes_path, (const uint8_t *)guest_file, sizeof guest_file - 1));
    CHECK(lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.owners.count == 3);
    for (int i = 0; i < host.core.owners.count; i++) CHECK(host.core.owners.owner[i] == LR_PLAYER_HOST);
    lr_core_stop(&host.core);

    /* 16. Managed preparation preserves the entire original root + backup tree before asking
     * the Windows coordinator to import through Steam. It never overwrites raw Steam files first. */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "managed-guest");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    char managed_dir[LR_PATH_MAX], nested[LR_PATH_MAX], journal[LR_PATH_MAX];
    lr_join(managed_dir, sizeof managed_dir, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(nested, sizeof nested, managed_dir, "backup");
    CHECK(lr_make_dirs(nested));
    write_text(nested, "persist.game.json", "old guest backup");
    lr_join(journal, sizeof journal, root, "guest-original.bin");
    snprintf(guest2.core.cfg.restore_path, sizeof guest2.core.cfg.restore_path, "%s", journal);
    guest2.core.cfg.steam_user = 4;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000) && guest2.core.state == LR_ST_GUEST_LAUNCH);
    lr_snapshot *recovery = calloc(1, sizeof *recovery);
    CHECK(lr_snapshot_read(journal, recovery) && recovery->user == 4 && recovery->count == 3);
    int original_found = 0, backup_found = 0;
    for (uint32_t i = 0; i < recovery->count; i++) {
        if (!strcmp(recovery->files[i].name, "persist.part00.json")) original_found = 1;
        if (!strcmp(recovery->files[i].name, "backup/persist.game.json")) backup_found = 1;
    }
    CHECK(original_found && backup_found);
    lr_snapshot_free(recovery);
    free(recovery);
    lr_join(a, sizeof a, managed_dir, "persist.part00.json");
    own = read_text(a);
    CHECK(own && strstr(own, "own"));
    free(own);
    lr_join(a, sizeof a, nested, "persist.game.json");
    own = read_text(a);
    CHECK(own && !strcmp(own, "old guest backup"));
    free(own);
    lr_core_stop(&guest2.core);
    CHECK(lr_file_exists(journal)); /* stop/crash must not discard recovery */
    lr_core_stop(&host.core);
    drop_queue();

    /* 17. A pending original is never replaced by the next borrowed campaign. */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000) && guest2.core.state == LR_ST_FAILED);
    recovery = calloc(1, sizeof *recovery);
    CHECK(lr_snapshot_read(journal, recovery) && recovery->count == 3);
    lr_snapshot_free(recovery);
    free(recovery);
    lr_core_stop(&host.core);

    /* 15. a campaign still at the start of the tutorial is never hosted: as the game saved it ... */
    char gpath[LR_PATH_MAX];
    static uint8_t game[65536];
    lr_join(gpath, sizeof gpath, hdir, "persist.game.json");
    size_t gn = dw_game(game, 1, "tutorial", NULL, 0);
    CHECK(lr_write_file(gpath, game, gn));
    CHECK(!lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.state == LR_ST_FAILED && strstr(host.core.error, "tutorial") && host.core.file_count == 0);
    /* ... or still the template the game copies into a new campaign */
    static const char tmpl[] = "{\n    \"version\": 2,\n    \"data\": {\n        \"inraid\": true,\n"
                               "        \"raiddungeon\": \"tutorial\"\n    }\n}";
    CHECK(lr_write_file(gpath, (const uint8_t *)tmpl, sizeof tmpl - 1));
    CHECK(!lr_core_host_start(&host.core, now_ms()) && strstr(host.core.error, "tutorial"));
    /* a normal raid is fine */
    gn = dw_game(game, 1, "crypts", NULL, 0);
    CHECK(lr_write_file(gpath, game, gn));
    CHECK(lr_core_host_start(&host.core, now_ms()));
    lr_core_stop(&host.core);

    /* 16. the host chooses from what is there, in menu order; a folder without a campaign is none */
    {
        char d1[LR_PATH_MAX], d9[LR_PATH_MAX], empty[LR_PATH_MAX];
        lr_campaign_entry list[9];
        lr_join(d1, sizeof d1, host.core.cfg.remote_dir, "profile_1");
        lr_join(empty, sizeof empty, host.core.cfg.remote_dir, "profile_3");
        lr_join(d9, sizeof d9, host.core.cfg.remote_dir, "profile_9");
        lr_make_dirs(d1);
        lr_make_dirs(empty);
        lr_make_dirs(d9);
        lr_join(gpath, sizeof gpath, d1, "persist.game.json");
        gn = dw_game(game, 1, "tutorial", NULL, 0);
        CHECK(lr_write_file(gpath, game, gn));
        lr_join(gpath, sizeof gpath, d9, "persist.game.json");
        gn = dw_game(game, 0, "", NULL, 0);
        CHECK(lr_write_file(gpath, game, gn));
        int count = lr_list_campaigns(host.core.cfg.remote_dir, list);
        /* All nine are there; what has no campaign says so. profile_9 is no slot at all. */
        CHECK(count == 2);
        for (int i = 0; i < 9; i++) CHECK(list[i].slot == i + 1);
        CHECK(list[0].empty && !list[0].unreadable && !list[0].tutorial);
        CHECK(!list[1].empty && !list[1].unreadable && list[1].tutorial && !strcmp(list[1].estate, "Darkest"));
        CHECK(!list[2].empty && !list[2].tutorial);
        CHECK(list[3].empty && !list[3].unreadable); /* The folder is there, without a campaign. */
        CHECK(list[8].empty);
        /* A file that cannot be read is not called empty: it may be someone's save. */
        write_text(empty, "persist.game.json", "not a save");
        count = lr_list_campaigns(host.core.cfg.remote_dir, list);
        CHECK(count == 2 && !list[3].empty && list[3].unreadable);
        lr_join(gpath, sizeof gpath, empty, "persist.game.json");
        lr_delete_file(gpath);
        lr_join(gpath, sizeof gpath, d1, "persist.game.json");
        gn = dw_game(game, 1, "crypts", NULL, 0);
        CHECK(lr_write_file(gpath, game, gn));

        /* 17. the host's slot 2 ends up in the guest's slot 2, and the guest's recovery knows that */
        static node guest3;
        char journal3[LR_PATH_MAX], other[LR_PATH_MAX];
        setup_node(&guest3, 5, "guest3");
        lr_join(other, sizeof other, guest3.core.cfg.remote_dir, "profile_1");
        lr_make_dirs(other);
        write_text(other, "persist.part00.json", "own campaign");
        lr_join(journal3, sizeof journal3, root, "guest3-original.bin");
        snprintf(guest3.core.cfg.restore_path, sizeof guest3.core.cfg.restore_path, "%s", journal3);
        host.core.cfg.coop_slot = 2;
        CHECK(lr_core_host_start(&host.core, now_ms()));
        ini = read_text(host.core.cfg.ini_path);
        CHECK(ini && strstr(ini, "coop_slots=2"));
        free(ini);
        CHECK(!strcmp(host.core.save_folder, "profile_1"));
        CHECK(lr_core_guest_join(&guest3.core, 1, now_ms()));
        CHECK(run_until(&host, &guest3, state_is, &guest3, 10000));
        CHECK(guest3.core.state == LR_ST_GUEST_LAUNCH && guest3.core.cfg.coop_slot == 2 && !strcmp(guest3.core.save_folder, "profile_1"));
        CHECK(strstr(guest3.core.status, "slot 2"));
        ini = read_text(guest3.core.cfg.ini_path);
        CHECK(ini && strstr(ini, "coop_slots=2"));
        free(ini);
        {
            lr_snapshot *kept = calloc(1, sizeof *kept);
            CHECK(lr_snapshot_read(journal3, kept) && !strcmp(kept->folder, "profile_1") && kept->count == 1);
            lr_snapshot_free(kept);
            free(kept);
        }
        lr_core_stop(&host.core);
        lr_core_free(&guest3.core);
        lr_delete_file(journal3);
        drop_queue();

        /* 18. a host cannot make a guest replace anything else: the restore area, another slot, or an unknown slot */
        static const struct { const char *folder; uint32_t session; int ok; } hostile[] = {
            {"profile_9", 3, 0}, {"steam_profile_9", 3, 0}, {"profile_0", 3, 0}, {"../profile_2", 3, 0}, {"profile_9", 10, 0}, {"profile_2", 0, 0}, {"profile_8", 9, 1}};
        for (size_t i = 0; i < sizeof hostile / sizeof hostile[0]; i++) {
            uint8_t wire[LR_MAX_MSG];
            lr_msg m;
            setup_node(&guest3, 5, "guest3");
            CHECK(lr_core_guest_join(&guest3.core, 1, now_ms()));
            memset(&m, 0, sizeof m);
            m.type = LR_SESSION;
            m.a = hostile[i].session;
            memset(m.text, 'a', LR_PASSWORD_LEN);
            size_t len = lr_encode(&m, wire);
            CHECK(len > 0);
            lr_core_on_message(&guest3.core, 1, LR_CH_CONTROL, wire, len, now_ms());
            /* An unknown slot does not even decode: no session, and nothing is received without one. */
            CHECK((guest3.core.password[0] != 0) == (hostile[i].session >= 1 && hostile[i].session <= 9));
            memset(&m, 0, sizeof m);
            m.type = LR_SAVE_BEGIN;
            m.a = 1;
            m.b = 1;
            snprintf(m.text, sizeof m.text, "%s", hostile[i].folder);
            len = lr_encode(&m, wire);
            if (len) lr_core_on_message(&guest3.core, 1, LR_CH_CONTROL, wire, len, now_ms());
            CHECK((guest3.core.state == LR_ST_GUEST_RECEIVING) == hostile[i].ok);
            lr_core_free(&guest3.core);
            drop_queue();
        }
    }

    lr_core_free(&intruder.core);
    lr_core_free(&guest.core);
    lr_core_free(&guest2.core);
    drop_queue();
    printf(failures ? "test_lr_core: %d FAILURES\n" : "test_lr_core: all passed\n", failures);
    return failures != 0;
}
