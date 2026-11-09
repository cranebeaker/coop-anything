/* Hero ownership table: roster updates, claim rules, ini text and restore, and the LR_ROSTER body codec
 * (round trips, strict refusals, 1,000,000 fuzzed inputs; run under ASan by launcher/build-linux.sh). */
#include "lr_owners.h"
#include "../../src/dd_party_wire.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static lr_hero hero(uint32_t id, const char *name, const char *cls, int32_t xp, int32_t status) {
    lr_hero h;
    memset(&h, 0, sizeof h);
    h.id = id;
    snprintf(h.name, sizeof h.name, "%s", name);
    snprintf(h.cls, sizeof h.cls, "%s", cls);
    h.xp = xp;
    h.status = status;
    return h;
}

static int same(const lr_owners *a, const lr_owners *b) {
    if (a->count != b->count || a->revision != b->revision) return 0;
    for (int i = 0; i < a->count; i++) {
        const lr_hero *x = &a->heroes[i], *y = &b->heroes[i];
        if (x->id != y->id || x->xp != y->xp || x->status != y->status || strcmp(x->name, y->name) ||
            strcmp(x->cls, y->cls) || a->owner[i] != b->owner[i])
            return 0;
    }
    return 1;
}

static void test_party_view(void) {
    lr_owners o;lr_owners_init(&o);
    lr_hero h[]={hero(1,"A","crusader",0,1),hero(2,"B","vestal",0,0),hero(3,"C","houndmaster",0,2),hero(4,"D","leper",0,0)};
    CHECK(lr_owners_set_roster(&o,h,4));
    CHECK(lr_owners_claim(&o,4,LR_PLAYER_GUEST,1));
    int view[LR_MAX_HEROES];uint32_t rev=o.revision;
    CHECK(lr_owners_display_order(&o,view)==2 && view[0]==1 && view[1]==3 && view[2]==0 && view[3]==2);
    CHECK(o.revision==rev && o.heroes[0].id==1 && o.owner[3]==LR_PLAYER_GUEST);
    /* Claim a displayed row by its hero ID; reordering never changes another hero's owner. */
    CHECK(lr_owners_claim(&o,o.heroes[view[0]].id,LR_PLAYER_GUEST,1));
    CHECK(o.owner[1]==LR_PLAYER_GUEST && o.owner[0]==LR_PLAYER_HOST && o.owner[3]==LR_PLAYER_GUEST);
    o.heroes[1].status=1;o.heroes[2].status=0;
    CHECK(lr_owners_display_order(&o,view)==2 && o.heroes[view[0]].id==3 && o.heroes[view[1]].id==4);
    o.count=5;for(int i=0;i<5;i++)o.heroes[i].status=0;
    CHECK(lr_owners_display_order(&o,view)==-1 && view[0]==0 && view[4]==4);
    lr_owners_init(&o);CHECK(lr_owners_display_order(&o,view)==0);
    printf("Legacy saved-status sort retained; live UI uses persistent-ID membership instead\n");
}

static void test_live_party(void) {
    lr_owners o; lr_owners_init(&o);
    uint32_t ids[6] = {1, 2, 7, 8, 15, 16};
    lr_hero heroes[6];
    for (unsigned i = 0; i < 6; i++) heroes[i] = hero(ids[i], "Hero", "crusader", 0, i == 5 ? 0 : 1);
    CHECK(lr_owners_set_roster(&o, heroes, 6));
    lr_owners before = o;
    uint32_t live[4] = {15, 7, 2, 1};
    int order[LR_MAX_HEROES]; uint8_t flags[LR_MAX_HEROES];
    CHECK(lr_owners_live_order(&o, live, 4, order, flags) == 4);
    CHECK(o.heroes[order[0]].id == 15 && o.heroes[order[3]].id == 1 &&
          !flags[3] && !flags[5] && flags[0] && flags[1] && flags[2] && flags[4]);
    CHECK(!memcmp(&o, &before, sizeof o)); /* Status0 hero16 is NOT the dungeon party. */
    CHECK(lr_owners_claim(&o, o.heroes[order[1]].id, LR_PLAYER_GUEST, 1));
    CHECK(o.owner[2] == LR_PLAYER_GUEST && o.owner[5] == LR_PLAYER_HOST);
    uint32_t swapped[4] = {1, 2, 15, 7};
    CHECK(lr_owners_live_order(&o, swapped, 4, order, flags) == 4 && o.heroes[order[0]].id == 1 && flags[4]);
    live[3] = 999;
    CHECK(lr_owners_live_order(&o, live, 4, order, flags) == -1);
    for (unsigned i = 0; i < 6; i++) CHECK(!flags[i] && order[i] == (int)i);
    live[3] = live[0]; CHECK(lr_owners_live_order(&o, live, 4, order, flags) == -1);
    CHECK(lr_owners_live_order(&o, NULL, -1, order, flags) == -1 && !flags[5]);
    CHECK(lr_owners_live_order(&o, NULL, 0, order, flags) == 0 && !flags[5]);
    CHECK(lr_owners_live_order(&o, live, 5, order, flags) == -1);

    dd_party_snapshot s = {0};
    s.magic = DD_PARTY_MAGIC; s.version = DD_PARTY_VERSION; s.size = sizeof s;
    s.pid = 321; s.updated = 1000; s.mode = DD_PARTY_RAID; s.count = 4;
    memcpy(s.ids, swapped, sizeof s.ids);
    CHECK(sizeof s == 56 && dd_party_valid(&s, 321, 1000));
    CHECK(!dd_party_valid(&s, 123, 1000) && !dd_party_valid(&s, 321, 999) && !dd_party_valid(&s, 321, 4001));
    s.sequence = 1; CHECK(!dd_party_valid(&s, 321, 1000)); s.sequence = 0;
    s.ids[3] = s.ids[0]; CHECK(!dd_party_valid(&s, 321, 1000)); s.ids[3] = swapped[3];
    s.count = 5; CHECK(!dd_party_valid(&s, 321, 1000)); s.count = 4;
    s.version++; CHECK(!dd_party_valid(&s, 321, 1000)); s.version--;
#ifdef _WIN32
    dd_party_snapshot shared = {0}, copy;
    dd_party_write(&shared, &s);
    CHECK(dd_party_read(&shared, 321, 1000, &copy) && !memcmp(copy.ids, swapped, sizeof swapped));
    shared.sequence |= 1;
    CHECK(!dd_party_read(&shared, 321, 1000, &copy) && !copy.count);
#endif
    puts("Live party: actual IDs override saved status, rank changes redraw, stable claims, unknown/stale/duplicate/PID/torn packets remove markers");
}
int main(void) {
    test_party_view();
    test_live_party();
    static lr_owners o, d, keep;
    uint8_t buf[16000];
    char ini[16384];

    /* roster: all heroes start with the host, revision 1 */
    lr_owners_init(&o);
    lr_hero r1[] = {hero(1, "Reynauld", "crusader", 6, 0), hero(2, "Dismas", "highwayman", 4, 1),
                    hero(16, "Neufmarch\xc3\xa9", "occultist", 2, 2)};
    CHECK(lr_owners_set_roster(&o, r1, 3) && o.revision == 1 && o.count == 3);
    for (int i = 0; i < 3; i++) CHECK(o.owner[i] == LR_PLAYER_HOST);
    CHECK(!lr_owners_set_roster(&o, r1, 3) && o.revision == 1); /* same list: no change */

    /* claims: tick = mine, untick mine = the other player's, untick someone else's = nothing */
    CHECK(lr_owners_claim(&o, 2, LR_PLAYER_GUEST, 1) && o.owner[1] == LR_PLAYER_GUEST && o.revision == 2);
    CHECK(!lr_owners_claim(&o, 2, LR_PLAYER_GUEST, 1) && o.revision == 2);  /* already the guest's */
    CHECK(!lr_owners_claim(&o, 1, LR_PLAYER_GUEST, 0));                     /* not the guest's to give away */
    CHECK(lr_owners_claim(&o, 2, LR_PLAYER_HOST, 1) && o.owner[1] == LR_PLAYER_HOST); /* the host takes it back */
    CHECK(lr_owners_claim(&o, 16, LR_PLAYER_HOST, 0) && o.owner[2] == LR_PLAYER_GUEST); /* host hands it over */
    CHECK(lr_owners_claim(&o, 16, LR_PLAYER_GUEST, 0) && o.owner[2] == LR_PLAYER_HOST); /* guest gives it back */
    CHECK(!lr_owners_claim(&o, 99, LR_PLAYER_GUEST, 1));                    /* unknown hero */
    CHECK(!lr_owners_claim(&o, 1, 2, 1) && !lr_owners_claim(&o, 1, -1, 1)); /* bad player */
    CHECK(lr_owners_claim(&o, 2, LR_PLAYER_GUEST, 1));
    uint32_t rev = o.revision;

    /* a new roster keeps owners of heroes still there; a recruit is the host's; a dead hero is gone */
    lr_hero r2[] = {hero(1, "Reynauld", "crusader", 8, 0), hero(2, "Dismas", "highwayman", 4, 1),
                    hero(31, "Corviser", "bounty_hunter", 0, 1)};
    CHECK(lr_owners_set_roster(&o, r2, 3) && o.revision == rev + 1);
    CHECK(o.owner[0] == LR_PLAYER_HOST && o.owner[1] == LR_PLAYER_GUEST && o.owner[2] == LR_PLAYER_HOST);
    CHECK(lr_owners_find(&o, 16) == -1 && lr_owners_find(&o, 31) == 2);

    /* a hero from a damaged save is cleaned so it always encodes */
    lr_owners dirty;
    lr_owners_init(&dirty);
    lr_hero bad[] = {hero(5, "a\tb", "Bad-Class!", -3, 9), hero(5, "dup", "vestal", 1, 1), hero(6, "x", "", 1, 1)};
    CHECK(lr_owners_set_roster(&dirty, bad, 3) && dirty.count == 2);
    CHECK(strcmp(dirty.heroes[0].name, "a?b") == 0 && strcmp(dirty.heroes[0].cls, "_ad__lass_") == 0);
    CHECK(dirty.heroes[0].xp == 0 && dirty.heroes[0].status == -1 && strcmp(dirty.heroes[1].cls, "unknown") == 0);
    CHECK(lr_owners_encode(&dirty, buf, sizeof buf) > 0);

    /* ini: header, one line per hero; restoring it gives the same owners */
    size_t n = lr_owners_ini(&o, LR_PLAYER_HOST, ini, sizeof ini);
    CHECK(n > 0 && strlen(ini) == n);
    CHECK(strstr(ini, "version=1\r\n") && strstr(ini, "hero.1=0\r\n") && strstr(ini, "hero.2=1\r\n") &&
          strstr(ini, "hero.31=0\r\n") && strstr(ini, "; Dismas, highwayman\r\n"));
    char rev_line[32];
    snprintf(rev_line, sizeof rev_line, "revision=%u\r\n", (unsigned)o.revision);
    CHECK(strstr(ini, rev_line) != NULL);
    CHECK(strstr(ini, "\r\nwritten_by=host\r\n") && lr_owners_ini_from_host(ini, n));
    char gini[16384];
    size_t gn = lr_owners_ini(&o, LR_PLAYER_GUEST, gini, sizeof gini);
    CHECK(gn > 0 && strstr(gini, "written_by=guest") && !lr_owners_ini_from_host(gini, gn));
    CHECK(lr_owners_ini(&o, LR_PLAYER_HOST, ini, 40) == 0); /* does not fit */
    n = lr_owners_ini(&o, LR_PLAYER_HOST, ini, sizeof ini);
    lr_owners fresh;
    lr_owners_init(&fresh);
    lr_owners_set_roster(&fresh, r2, 3);
    CHECK(lr_owners_apply_ini(&fresh, ini, n) && fresh.owner[1] == LR_PLAYER_GUEST && fresh.owner[0] == LR_PLAYER_HOST);
    CHECK(!lr_owners_apply_ini(&fresh, ini, n)); /* nothing new */
    const char *odd = "hero.1=1\nhero.2=7\nhero.31=1 \nhero.=1\nhero.1x=0\nxhero.31=1\nhero.99=1\nhero.31=";
    CHECK(lr_owners_apply_ini(&fresh, odd, strlen(odd)) && fresh.owner[0] == LR_PLAYER_GUEST);
    CHECK(fresh.owner[1] == LR_PLAYER_GUEST && fresh.owner[2] == LR_PLAYER_HOST);
    const char *unix_lf = "hero.31=1";
    CHECK(lr_owners_apply_ini(&fresh, unix_lf, strlen(unix_lf)) && fresh.owner[2] == LR_PLAYER_GUEST);

    /* codec: round trip, refusals, every truncation */
    n = lr_owners_encode(&o, buf, sizeof buf);
    CHECK(n > 1);
    CHECK(lr_owners_decode(buf, n, o.revision, &d) && same(&o, &d));
    lr_owners empty;
    lr_owners_init(&empty);
    CHECK(lr_owners_encode(&empty, buf + 8000, 100) == 1 && lr_owners_decode(buf + 8000, 1, 7, &d) && d.count == 0);
    n = lr_owners_encode(&o, buf, sizeof buf);
    CHECK(lr_owners_encode(&o, buf, 10) == 0);
    keep = d;
    for (size_t k = 0; k < n; k++) CHECK(!lr_owners_decode(buf, k, 1, &d));
    CHECK(same(&keep, &d)); /* refused input leaves out untouched */
    uint8_t extra[16001];
    memcpy(extra, buf, n);
    extra[n] = 0;
    CHECK(!lr_owners_decode(extra, n + 1, 1, &d)); /* trailing byte */
    uint8_t t[16000];
    memcpy(t, buf, n);
    t[5] = 2;  /* owner = player 2 */
    CHECK(!lr_owners_decode(t, n, 1, &d));
    memcpy(t, buf, n);
    t[6] = 4;  /* status 3 */
    CHECK(!lr_owners_decode(t, n, 1, &d));
    memcpy(t, buf, n);
    t[0] = LR_MAX_HEROES + 1;
    CHECK(!lr_owners_decode(t, n, 1, &d));
    memcpy(t, buf, n);
    t[10] = '\n'; /* control character in the first name */
    CHECK(!lr_owners_decode(t, n, 1, &d));
    memcpy(t, buf, n);
    t[1] = 9; /* first id 9 > second id 2: not ascending */
    CHECK(!lr_owners_decode(t, n, 1, &d));
    memcpy(t, buf, n);
    t[n - 1] = 'X'; /* class letters are [a-z0-9_] */
    CHECK(!lr_owners_decode(t, n, 1, &d));

    /* a full table fits one launcher message */
    lr_owners full;
    lr_owners_init(&full);
    static lr_hero many[LR_MAX_HEROES];
    for (int i = 0; i < LR_MAX_HEROES; i++) {
        many[i] = hero((uint32_t)i + 1, "", "", 70000, 2);
        memset(many[i].name, 'n', LR_HERO_NAME_MAX);
        memset(many[i].cls, 'c', LR_HERO_CLASS_MAX);
    }
    CHECK(lr_owners_set_roster(&full, many, LR_MAX_HEROES) && full.count == LR_MAX_HEROES && full.heroes[0].xp == 0xFFFF);
    n = lr_owners_encode(&full, buf, sizeof buf);
    CHECK(n > 0 && n <= sizeof buf && lr_owners_decode(buf, n, full.revision, &d) && same(&full, &d));

    /* fuzz: mutations of a valid body and random bytes; anything accepted re-encodes to the same bytes */
    n = lr_owners_encode(&o, buf, sizeof buf);
    srand(777);
    long accepted = 0;
    for (int round = 0; round < 1000000; round++) {
        size_t len;
        if (round % 2) {
            len = n;
            memcpy(t, buf, n);
            for (int e = 1 + rand() % 3; e > 0; e--) t[(size_t)rand() % n] = (uint8_t)rand();
        } else {
            len = (size_t)rand() % 64;
            for (size_t k = 0; k < len; k++) t[k] = (uint8_t)(rand() % 4 ? rand() % 8 : rand());
        }
        if (lr_owners_decode(t, len, 1, &d)) {
            uint8_t again[16000];
            size_t m = lr_owners_encode(&d, again, sizeof again);
            CHECK(m == len && memcmp(again, t, len) == 0);
            accepted++;
        }
    }
    printf("fuzz: 1000000 inputs, %ld accepted, all re-encode exactly\n", accepted);

    if (failures) {
        printf("test_lr_owners: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_lr_owners: all passed\n");
    return 0;
}
