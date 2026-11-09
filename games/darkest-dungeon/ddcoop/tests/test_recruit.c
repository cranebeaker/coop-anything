#include "dd_recruit.h"
#include <stdio.h>
#include <string.h>
/* Isolated snapshot fixture: no game, saves or game writes. */
static unsigned char memory[0x3100000];
static const uint64_t image = 0x10000000, campaign = 0x13000000, head = 0x13010000, node = 0x13010100,
                      coach = 0x13011000, children = 0x13012000, store = 0x13013000, panel = 0x13020000,
                      rows = 0x13021000, row0 = 0x13022000, row1 = 0x13022800, controller0 = 0x13023000,
                      controller1 = 0x13023100, roster = 0x13030000, hero0 = 0x13040000, hero1 = 0x13042000,
                      regular = 0x13044000, entries = 0x13080000;
static int failures, reads;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            printf("FAIL line %d: %s\n", __LINE__, #c);                                                      \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static int read_memory(void *unused, uint64_t at, void *out, size_t n) {
    (void)unused;
    reads++;
    if (at < image || at - image > sizeof memory || n > sizeof memory - (size_t)(at - image)) return 0;
    memcpy(out, memory + (size_t)(at - image), n);
    return 1;
}
static void p64(uint64_t at, uint64_t value) { memcpy(memory + (size_t)(at - image), &value, 8); }
static void p32(uint64_t at, uint32_t value) { memcpy(memory + (size_t)(at - image), &value, 4); }
static void p8(uint64_t at, uint8_t value) { memory[(size_t)(at - image)] = value; }
static void vec(uint64_t at, uint64_t begin, unsigned bytes) {
    p64(at, begin);
    p64(at + 8, begin + bytes);
}
static int open(dd_recruit *s) { return dd_recruit_open(s, read_memory, NULL, image, campaign); }
static void setup(void) {
    memset(memory, 0, sizeof memory);
    reads = 0;
    p64(campaign + 0x11f8, head);
    p8(head + 0x19, 1);
    p64(head + 8, node);
    p64(node, head);
    p64(node + 0x10, head);
    p64(node + 0x28, coach);
    p64(coach, image + 0xe9d088);
    vec(coach + 0xa8, children, 8);
    p64(children, store);
    p64(store, image + 0xe9cfd0);
    vec(store + 0x68, entries, 2 * 0x15b0);
    p32(entries + 0x137c, 40);
    p8(entries + 0x15a8, 1);
    p32(entries + 0x15b0 + 0x137c, 41);
    p8(entries + 0x15b0 + 0x15a8, 1);
    p64(panel, image + 0xec0cb0);
    p64(panel + 0x80, store);
    vec(panel + 0x350, rows, 32);
    p64(rows, row0);
    p64(rows + 16, row1);
    p64(row0, image + 0xebcc00);
    p64(row1, image + 0xebcc00);
    p64(row0 + 0x158, controller0);
    p64(row1 + 0x158, controller1);
    p32(row0 + 0x58, 1);
    p32(row1 + 0x58, 1);
    p64(controller0, image + 0xee0990);
    p64(controller1, image + 0xee0990);
    p64(controller0 + 0x28, entries);
    p64(controller1 + 0x28, entries + 0x15b0);
    p32(hero0 + 0x137c, 999);
    p32(hero1 + 0x137c, 998);
    p32(hero1 + 0x1384, 41);
    p32(regular + 0x1384, 1);
    vec(campaign + 0x20, roster, 8);
    p64(roster, regular);
}
static void normal(void) {
    setup();
    dd_recruit s;
    dd_recruit_choice c;
    unsigned count;
    CHECK(open(&s) && s.store == store);
    CHECK(dd_recruit_panel_ready(&s, panel));
    CHECK(dd_recruit_resolve(&s, panel, 41, &c) && c.id == 41 && c.hero == entries + 0x15b0 && c.index == 1 &&
          c.available && c.enabled);
    CHECK(dd_recruit_roster_has(&s, 41, &count) == 0 && count == 1);
    /* Ordinals can move; identity must select the matching genuine row. */
    p64(rows, row1);
    p64(rows + 16, row0);
    CHECK(dd_recruit_resolve(&s, panel, 41, &c) && c.index == 0 && c.hero == entries + 0x15b0);
    p32(row1 + 0x58, 0);
    CHECK(dd_recruit_resolve(&s, panel, 41, &c) && !c.enabled); /* full roster: caller must refuse */
    p8(entries + 0x15b0 + 0x15a8, 0);
    vec(campaign + 0x20, roster, 16);
    p64(roster + 8, hero1);
    CHECK(dd_recruit_candidate(&s, 41, &c) && !c.available);
    CHECK(!dd_recruit_resolve(&s, panel, 41, &c)); /* same candidate cannot be recruited twice */
    CHECK(dd_recruit_roster_has(&s, 41, &count) == 1 && count == 2);
    vec(panel + 0x350, rows, 0);
    CHECK(!dd_recruit_panel_ready(&s, panel));
}
static void invalid(void) {
    dd_recruit s;
    dd_recruit_choice c;
    unsigned count;
    setup();
    p8(image + 0x2a6d4bb, 1);
    CHECK(!open(&s));
    setup();
    p64(node, node);
    CHECK(!open(&s));
    CHECK(reads < 1500);
    setup();
    p64(store, image + 0xe8ebe0);
    CHECK(!open(&s)); /* shard store excluded */
    setup();
    p64(store + 0x70, entries + 65 * 0x15b0);
    CHECK(!open(&s));
    setup();
    CHECK(open(&s));
    p64(panel + 0x80, store + 8);
    CHECK(!dd_recruit_panel_ready(&s, panel));
    setup();
    CHECK(open(&s));
    p64(panel, image + 0xeb2968);
    CHECK(!dd_recruit_panel_ready(&s, panel));
    setup();
    CHECK(open(&s));
    p32(entries + 0x15b0 + 0x137c, 40);
    CHECK(!dd_recruit_candidate(&s, 40, &c));
    setup();
    CHECK(open(&s));
    p8(entries + 0x15a8, 2);
    CHECK(!dd_recruit_candidate(&s, 40, &c));
    setup();
    CHECK(open(&s));
    p64(controller0 + 0x28, UINT64_MAX - 10);
    CHECK(!dd_recruit_resolve(&s, panel, 40, &c));
    setup();
    CHECK(open(&s));
    p64(row0, image + 0xeae350);
    CHECK(!dd_recruit_resolve(&s, panel, 40, &c));
    setup();
    CHECK(open(&s));
    p32(hero0 + 0x137c, 40);
    p64(controller0 + 0x28, hero0);
    CHECK(!dd_recruit_resolve(&s, panel, 40, &c)); /* same ID is not the same source Entry */
    setup();
    CHECK(open(&s));
    p64(panel + 0x358, rows + 65 * 16);
    CHECK(!dd_recruit_panel_ready(&s, panel));
    setup();
    CHECK(open(&s));
    p64(campaign + 0x28, roster + 65 * 8);
    CHECK(dd_recruit_roster_has(&s, 40, &count) == -1);
}
int main(void) {
    normal();
    invalid();
    printf("Stagecoach snapshot: candidate identity, normal model, genuine rows, "
           "capacity/duplicate/corruption guards: %s (%d failures)\n",
           failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
