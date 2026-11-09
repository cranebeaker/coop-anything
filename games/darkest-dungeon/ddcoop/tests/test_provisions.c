/* Isolated model tests: no game, Steam, saves or injection. */
#include "dd_provisions.h"
#include <stdio.h>
#include <string.h>

static unsigned char memory[0x4000000];
static const uint64_t image = 0x10000000, campaign = 0x13000000, town = 0x13010000, manager = 0x13020000,
                      device = 0x13020100, roster = 0x13021000, stock = 0x13100000, bag = 0x13200000,
                      wallet = 0x13300000, stock_view = 0x13310000, bag_view = 0x13310100;
/* Native constructor: TownDisplay +17a8 subobject, then its ProvisionSelect +1fc8.
 * Keep this fixture independent of the production reader offset. */
static const uint64_t panel = 0x13013770;
static const uint64_t library = 0x13320000, definitions = 0x13320100, food_definition = 0x13321000,
                      other_definition = 0x13322000;
/* Original base.supply.inventory.items.darkest: .type "provision" .id "" (food).
 * The empty string hashes to zero. This is an occupied stack, not an empty slot. */
#define FOOD_TYPE 0x72f14069u
static int failures, reads, fail_at, race, race_read, target_read, denied_inventory;
static uint64_t target;
static const char *last_stage;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            printf("FAIL line %d: %s\n", __LINE__, #c);                                                      \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static void put(uint64_t at, const void *v, size_t n) { memcpy(memory + (size_t)(at - image), v, n); }
static void p64(uint64_t at, uint64_t v) { put(at, &v, 8); }
static void p32(uint64_t at, uint32_t v) { put(at, &v, 4); }
static void p8(uint64_t at, uint8_t v) { put(at, &v, 1); }
static int rd(void *unused, uint64_t at, void *out, size_t n) {
    (void)unused;
    reads++;
    if (fail_at && reads == fail_at) return 0;
    if (at >= stock && at < bag + 0x10000) denied_inventory++;
    if (at < image || at - image > sizeof memory || n > sizeof memory - (size_t)(at - image)) return 0;
    if (race && at == target && ++target_read == race_read) memory[(size_t)(at - image)] ^= 8;
    memcpy(out, memory + (size_t)(at - image), n);
    return 1;
}
static void inv(uint64_t at, uint64_t start, unsigned slots, unsigned occupied) {
    p64(at + 8, start);
    p64(at + 0x10, start + slots * 0x5a8);
    p64(at + 0x18, start + slots * 0x5a8);
    p32(at + 0x24, occupied);
}
static void item(uint64_t at, uint32_t qty, uint32_t type, uint32_t id) {
    p32(at, qty);
    p32(at + 0x44, type);
    p32(at + 0x88, id);
}
static void setup(void) {
    memset(memory, 0, sizeof memory);
    p64(image + 0x1185248, manager);
    p64(manager + 0x38, device);
    put(device + 0x20, "steam_profile_2", 16);
    p64(image + 0x118e138, campaign);
    p64(campaign + 0x20, roster);
    p64(campaign + 0x28, roster + 4 * 8);
    p64(image + 0x118e360, town);
    p64(town, image + 0xec37a8);
    /* The old mistaken location is a different panel, not ProvisionSelect. */
    p64(town + 0x1fc8, image + 0xec00a0);
    p64(panel, image + 0xebdde8);
    p8(panel + 0x60, 1);
    p64(panel + 0x80, stock_view);
    p64(panel + 0x88, bag_view);
    p64(stock_view, image + 0xee1420);
    p64(bag_view, image + 0xee1420);
    p32(campaign + 0x1838, 91);
    inv(campaign + 0x1618, stock, 4, 2);
    inv(campaign + 0x15b0, bag, 8, 1);
    item(stock, 12, 55, 101);
    item(stock + 2 * 0x5a8, 8, 55, 102);
    item(bag + 3 * 0x5a8, 4, 55, 101);
    /* Empty native slots can retain an identity. Do not report it as an owned item. */
    item(bag, 0, 999, 999);
    p64(campaign + 0xe70, wallet);
    p64(campaign + 0xe78, wallet + 2 * 0x48);
    p32(wallet, 6000);
    p32(wallet + 0x44, 900);
    p32(wallet + 0x48, 100);
    p32(wallet + 0x48 + 0x44, 901);
    reads = fail_at = race = target_read = denied_inventory = 0;
    race_read = 2;
}
static void setup_food(void) {
    setup();
    /* Actual owner14a closed snapshot had14 stock/16 bag slots and73010 gold. */
    inv(campaign + 0x1618, stock, 14, 2);
    inv(campaign + 0x15b0, bag, 16, 1);
    p32(wallet, 73010);
    p32(wallet + 0x44, 0x00eed472);
    item(stock, 24, FOOD_TYPE, 0);
    item(bag + 3 * 0x5a8, 4, FOOD_TYPE, 0);
    p64(image + 0x118e0f8, library);
    p64(library, definitions);
    p64(library + 8, definitions + 16);
    p64(definitions, food_definition);
    p64(definitions + 8, other_definition);
    p32(food_definition + 0x40, FOOD_TYPE);
    p32(food_definition + 0x84, 0);
    p32(other_definition + 0x40, 55);
    p32(other_definition + 0x84, 102);
}
static dd_provisions_result capture(dd_provisions_snapshot *s) {
    memset(s, 0xa5, sizeof *s);
    last_stage = NULL;
    return dd_provisions_capture_ex(rd, NULL, image, s, &last_stage);
}
static int zero(const dd_provisions_snapshot *s) {
    dd_provisions_snapshot z;
    memset(&z, 0, sizeof z);
    return !memcmp(s, &z, sizeof z);
}
int main(void) {
    dd_provisions_snapshot s, before;
    setup();
    CHECK(capture(&s) == DD_PROVISIONS_OK);
    CHECK(last_stage && !strcmp(last_stage, "complete"));
    CHECK(dd_provisions_capture(rd, NULL, image, &s) == DD_PROVISIONS_OK);
    CHECK(s.panel_visible && s.stock_view && s.bag_view && s.context_1838 == 91);
    CHECK(s.stock_slots == 4 && s.stock_occupied == 2 && s.bag_slots == 8 && s.bag_occupied == 1);
    CHECK(s.stock[0].id == 101 && s.stock[0].quantity == 12 && !s.stock[1].id);
    CHECK(s.bag[3].quantity == 4 && !s.bag[0].type && !s.bag[0].id);
    CHECK(s.currencies == 2 && s.wallet[0].quantity == 6000 && s.wallet[1].type == 901);
    before = s;
    /* Actual purchase/refund/cancel model deltas, retaining physical slot holes. */
    item(stock, 11, 55, 101);
    item(bag + 3 * 0x5a8, 5, 55, 101);
    p32(wallet, 5925);
    CHECK(capture(&s) == DD_PROVISIONS_OK && s.stock[0].quantity == 11 && s.bag[3].quantity == 5);
    CHECK(s.wallet[0].quantity == 5925 && memcmp(&s, &before, sizeof s));
    item(stock, 12, 55, 101);
    item(bag + 3 * 0x5a8, 4, 55, 101);
    p32(wallet, 6000);
    CHECK(capture(&s) == DD_PROVISIONS_OK && !memcmp(&s, &before, sizeof s));
    inv(campaign + 0x1618, stock, 0, 0);
    inv(campaign + 0x15b0, bag, 0, 0);
    p8(panel + 0x60, 0);
    p64(panel + 0x80, 0);
    p64(panel + 0x88, 0);
    CHECK(capture(&s) == DD_PROVISIONS_OK && !s.stock_slots && !s.bag_slots && !s.panel_visible);
    setup_food();
    CHECK(capture(&s) == DD_PROVISIONS_OK);
    CHECK(s.stock[0].quantity == 24 && s.stock[0].type == FOOD_TYPE && s.stock[0].id == 0);
    CHECK(s.bag[3].quantity == 4 && s.bag[3].type == FOOD_TYPE && s.bag[3].id == 0);
    CHECK(s.stock_slots == 14 && s.bag_slots == 16 && s.wallet[0].quantity == 73010);
    item(stock, 23, FOOD_TYPE, 0);
    item(bag + 3 * 0x5a8, 5, FOOD_TYPE, 0);
    p32(wallet, 73010 - 75);
    CHECK(capture(&s) == DD_PROVISIONS_OK && s.stock[0].quantity == 23 && s.bag[3].quantity == 5 &&
          s.wallet[0].quantity == 72935);
    item(stock, 24, FOOD_TYPE, 0);
    item(bag + 3 * 0x5a8, 4, FOOD_TYPE, 0);
    p32(wallet, 73010);
    CHECK(capture(&s) == DD_PROVISIONS_OK && s.wallet[0].quantity == 73010);
    for (unsigned i = 0; i < 14; i++)
        p32(stock + i * 0x5a8, 0);
    for (unsigned i = 0; i < 16; i++)
        p32(bag + i * 0x5a8, 0);
    p32(campaign + 0x163c, 0);
    p32(campaign + 0x15d4, 0);
    p8(panel + 0x60, 0);
    CHECK(capture(&s) == DD_PROVISIONS_OK && !s.stock_occupied && !s.bag_occupied && !s.stock[0].type &&
          !s.bag[3].type && s.wallet[0].quantity == 73010);
    /* Every failure on the real-food definition path also discards the complete result. */
    setup_food();
    CHECK(capture(&s) == DD_PROVISIONS_OK);
    int food_reads = reads;
    for (int i = 1; i <= food_reads; i++) {
        setup_food();
        fail_at = i;
        CHECK(capture(&s) != DD_PROVISIONS_OK && zero(&s));
    }
    setup_food();
    item(stock, 24, 55, 0); /* Known type but this empty-ID pair is NOT defined. */
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    CHECK(!strcmp(last_stage, "stock-empty-id-definition"));
    setup_food();
    p32(food_definition + 0x84, 1);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    setup_food();
    item(stock, 24, 0, 0);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    CHECK(!strcmp(last_stage, "stock-item"));
    setup_food();
    p64(library + 8, definitions + 8 * 16385);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    setup_food();
    p64(library + 8, definitions + 1);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    setup_food();
    p64(definitions, 0);
    CHECK(capture(&s) == DD_PROVISIONS_UNREADABLE && zero(&s));
    setup_food();
    item(bag + 3 * 0x5a8, 4, 55, 0);
    p32(other_definition + 0x84, 0);
    CHECK(capture(&s) == DD_PROVISIONS_OK && s.stock[0].id == 0 && s.bag[3].id == 0);
    const uint64_t definition_targets[] = {image + 0x118e0f8, library};
    for (unsigned i = 0; i < sizeof definition_targets / sizeof definition_targets[0]; i++) {
        for (int j = 2; j <= 3; j++) {
            setup_food();
            race = 1;
            race_read = j;
            target = definition_targets[i];
            CHECK(capture(&s) == DD_PROVISIONS_CHANGED && zero(&s));
        }
    }
    /* Every individual read failure must discard the partial result. */
    setup();
    CHECK(capture(&s) == DD_PROVISIONS_OK);
    int total = reads;
    for (int i = 1; i <= total; i++) {
        setup();
        fail_at = i;
        CHECK(capture(&s) != DD_PROVISIONS_OK && zero(&s));
        CHECK(last_stage && strcmp(last_stage, "arguments") && strcmp(last_stage, "complete"));
    }
    for (unsigned slot = 0; slot <= 9; slot++) {
        setup();
        char name[32];
        snprintf(name, sizeof name, "steam_profile_%u", slot);
        memset(memory + (size_t)(device + 0x20 - image), 0, 128);
        put(device + 0x20, name, strlen(name) + 1);
        CHECK(capture(&s) == (slot != 9 ? DD_PROVISIONS_OK : DD_PROVISIONS_SLOT));
        if (slot == 9) CHECK(!denied_inventory && zero(&s));
    }
    setup();
    p8(image + 0x2a6d4bb, 1);
    CHECK(capture(&s) == DD_PROVISIONS_OTHER_MODE && !denied_inventory && zero(&s));
    setup();
    p64(image + 0x118e138, 0);
    CHECK(capture(&s) == DD_PROVISIONS_WAIT && zero(&s));
    setup();
    p64(town, image + 0x123);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    CHECK(!strcmp(last_stage, "town-type"));
    setup();
    p64(panel, image + 0x123);
    p64(town + 0x1fc8, image + 0xebdde8); /* A false match at the OLD address must not be accepted. */
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT);
    CHECK(!strcmp(last_stage, "provision-panel-type") && !denied_inventory);
    setup();
    p64(bag_view, image + 0x123);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT);
    CHECK(!strcmp(last_stage, "bag-view-type"));
    setup();
    inv(campaign + 0x15b0, bag, DD_PROVISIONS_BAG_MAX + 1, 1);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    CHECK(!strcmp(last_stage, "bag-vector"));
    setup();
    inv(campaign + 0x1618, stock, DD_PROVISIONS_STOCK_MAX + 1, 2);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    setup();
    inv(campaign + 0x1618, stock, DD_PROVISIONS_STOCK_MAX, DD_PROVISIONS_STOCK_MAX);
    inv(campaign + 0x15b0, bag, DD_PROVISIONS_BAG_MAX, DD_PROVISIONS_BAG_MAX);
    for (unsigned i = 0; i < DD_PROVISIONS_STOCK_MAX; i++)
        item(stock + i * 0x5a8, 1, 55, 101 + i);
    for (unsigned i = 0; i < DD_PROVISIONS_BAG_MAX; i++)
        item(bag + i * 0x5a8, 1, 55, 301 + i);
    CHECK(capture(&s) == DD_PROVISIONS_OK && s.stock_occupied == DD_PROVISIONS_STOCK_MAX &&
          s.bag_occupied == DD_PROVISIONS_BAG_MAX && s.bag[DD_PROVISIONS_BAG_MAX - 1].id == 364);
    setup();
    p64(campaign + 0x1628, stock - 1);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT);
    setup();
    p32(bag + 3 * 0x5a8, 0xffffffff);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT);
    setup();
    p32(campaign + 0x163c, 3);
    CHECK(capture(&s) == DD_PROVISIONS_CHANGED);
    setup();
    p32(wallet + 0x48 + 0x44, 900);
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT);
    setup();
    p64(campaign + 0xe78, wallet + 0x48 * (DD_PROVISIONS_CURRENCY_MAX + 1));
    CHECK(capture(&s) == DD_PROVISIONS_LAYOUT && zero(&s));
    setup();
    p64(image + 0x118e138, UINT64_MAX - 0x100);
    CHECK(capture(&s) == DD_PROVISIONS_UNREADABLE);
    const uint64_t targets[] = {campaign + 0x1620, campaign + 0x15b8, campaign + 0xe70, image + 0x118e138,
                                image + 0x118e360, image + 0x1185248, manager + 0x38,   device + 0x20};
    for (unsigned i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        setup();
        race = 1;
        target = targets[i];
        CHECK(capture(&s) == DD_PROVISIONS_CHANGED && zero(&s));
    }
    CHECK(dd_provisions_capture(NULL, NULL, image, &s) == DD_PROVISIONS_UNREADABLE && zero(&s));
    CHECK(dd_provisions_capture(rd, NULL, UINT64_MAX, &s) == DD_PROVISIONS_UNREADABLE);
    const char *stage = NULL;
    CHECK(dd_provisions_capture_ex(rd, NULL, image, NULL, &stage) == DD_PROVISIONS_UNREADABLE);
    CHECK(stage && !strcmp(stage, "arguments"));
    printf("provisions: bounded purchase/refund/cancel observations, every read failure, slot/mode gates and "
           "races: %s\n",
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
