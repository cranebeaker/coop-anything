/* Isolated memory fixture. Never reads or writes a game process or save. */
#include "dd_shop.h"
#include <stdio.h>
#include <string.h>

static unsigned char memory[0x4000000];
static const uint64_t image = 0x10000000, campaign = 0x13000000, head = 0x13010000, node = 0x13010100,
                      wagon = 0x13011000, children = 0x13012000, supply = 0x13013000,
                      stock_items = 0x13020000, estate_items = 0x13100000, library = 0x13040000,
                      definitions = 0x13041000, def1 = 0x13050000, def2 = 0x13051000, wallet = 0x13060000;
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
static void put(uint64_t at, const void *value, size_t n) { memcpy(memory + (size_t)(at - image), value, n); }
static void p64(uint64_t at, uint64_t value) { put(at, &value, 8); }
static void p32(uint64_t at, uint32_t value) { put(at, &value, 4); }
static void p8(uint64_t at, uint8_t value) { put(at, &value, 1); }
static void vec(uint64_t at, uint64_t begin, unsigned bytes) {
    p64(at, begin);
    p64(at + 8, begin + bytes);
}
static void item(uint64_t at, uint32_t id, uint32_t quantity) {
    p32(at, quantity);
    p32(at + 0x44, 123);
    p32(at + 0x88, id);
}
static void setup(void) {
    memset(memory, 0, sizeof memory);
    p64(campaign + 0x11f8, head);
    p8(head + 0x19, 1);
    p64(head + 8, node);
    p64(node, head);
    p64(node + 0x10, head);
    p64(node + 0x28, wagon);
    p64(wagon, image + 0xe9c338);
    vec(wagon + 0xa8, children, 8);
    p64(children, supply);
    p64(supply, image + 0xe9c2e0);
    vec(supply + 0x70, stock_items, 3 * 0x5a8);
    item(stock_items, 101, 1);
    item(stock_items + 0x5a8, 0, 0);
    item(stock_items + 2 * 0x5a8, 102, 1);
    /* The base game's trinket_storage config reserves 9999 slots, including empty ones.
     * Match that layout even when the campaign owns only two populated stacks. */
    vec(campaign + 0xe90, estate_items, 9999 * 0x5a8);
    item(estate_items, 101, 2);
    item(estate_items + 0x5a8, 102, 1);
    p64(image + 0x118e0f8, library);
    vec(library, definitions, 16);
    p64(definitions, def1);
    p64(definitions + 8, def2);
    p32(def1 + 0x40, 123);
    p32(def1 + 0x84, 101);
    p32(def1 + 0x9c, 500);
    p32(def1 + 0xa4, 50);
    p32(def2 + 0x40, 123);
    p32(def2 + 0x84, 102);
    p32(def2 + 0x9c, 750);
    p32(def2 + 0xa4, 75);
    p32(image + 0x2a6f868, 123);
    p32(image + 0x2bb41f0, 99);
    vec(campaign + 0xe70, wallet, 0x48);
    p32(wallet, 6000);
    p32(wallet + 0x44, 99);
    reads = 0;
}
static dd_shop_result open(dd_shop *s) { return dd_shop_open(s, read_memory, NULL, image, campaign); }
static void normal(void) {
    setup();
    dd_shop s;
    dd_shop_item i;
    int32_t n;
    CHECK(open(&s) == DD_SHOP_READY);
    CHECK(s.stock == supply + 0x68 && s.estate == campaign + 0xe88);
    CHECK(dd_shop_visible(&s, 1, &i) && i.physical == 2 && i.id == 102 && i.base_price == 750);
    CHECK(dd_shop_slot(&s, 2, &i) && i.visible == 1 && i.quantity == 1);
    CHECK(!dd_shop_visible(&s, 2, &i) && !dd_shop_slot(&s, 1, &i));
    CHECK(dd_shop_owned(&s, 123, 101, &n) && n == 2);
    CHECK(dd_shop_gold(&s, &n) && n == 6000);
    CHECK(dd_shop_estate_slot(&s, 1, &i) && i.physical == 1 && i.id == 102 && i.base_sale == 75);
    CHECK(!dd_shop_estate_slot(&s, 0, &i)); /* sale callback clears whole stack; never accept quantity >1 */
    p32(estate_items + 0x5a8 + 0x44, 456);
    CHECK(!dd_shop_estate_slot(&s, 1, &i));
    p32(estate_items + 0x5a8 + 0x44, 123);
    p32(def2 + 0xa4, 0);
    CHECK(!dd_shop_estate_slot(&s, 1, &i));
    p32(def2 + 0xa4, 75);
    CHECK(!dd_shop_estate_slot(&s, 9999, &i));
    /* Buy slot zero. Slot two's visible ordinal changes, stable physical index/identity does not. */
    p32(stock_items, 0);
    p32(estate_items, 3);
    p32(wallet, 5500);
    CHECK(dd_shop_quantity(&s, 0, &n) && n == 0);
    CHECK(!dd_shop_slot(&s, 0, &i));
    CHECK(dd_shop_slot(&s, 2, &i) && i.visible == 0 && i.id == 102);
    CHECK(dd_shop_visible(&s, 0, &i) && i.physical == 2);
    CHECK(dd_shop_owned(&s, 123, 101, &n) && n == 3);
    CHECK(dd_shop_gold(&s, &n) && n == 5500);
    vec(wagon + 0xa8, children, 16);
    p64(children, 0);
    p64(children + 8, supply);
    CHECK(open(&s) == DD_SHOP_READY); /* optional child entries may be empty */
}
static void invalid(void) {
    dd_shop s;
    dd_shop_item i;
    int32_t n;
    setup();
    p8(image + 0x2a6d4bb, 1);
    CHECK(open(&s) == DD_SHOP_OTHER_MODE);
    setup();
    p64(campaign + 0x11f8, 0);
    CHECK(open(&s) == DD_SHOP_BAD_TREE);
    setup();
    p64(node, node);
    CHECK(open(&s) == DD_SHOP_BAD_TREE);
    CHECK(reads < 2000);
    setup();
    p64(wagon, image + 0xe8db79);
    CHECK(open(&s) == DD_SHOP_NO_WAGON);
    setup();
    p64(wagon + 0xb0, children + 9);
    CHECK(open(&s) == DD_SHOP_BAD_CHILDREN);
    setup();
    p64(supply, image + 0xe8dbe8);
    CHECK(open(&s) == DD_SHOP_NO_SUPPLY); /* exclude shard shop */
    setup();
    p64(supply + 0x78, stock_items + 129 * 0x5a8);
    CHECK(open(&s) == DD_SHOP_BAD_STOCK);
    setup();
    p64(supply + 0x78, stock_items - 8);
    CHECK(open(&s) == DD_SHOP_BAD_STOCK);
    setup();
    CHECK(open(&s) == DD_SHOP_READY);
    p32(def1 + 0x84, 999);
    CHECK(!dd_shop_slot(&s, 0, &i));
    setup();
    CHECK(open(&s) == DD_SHOP_READY);
    p32(stock_items, 0xffffffff);
    CHECK(!dd_shop_visible(&s, 0, &i));
    setup();
    CHECK(open(&s) == DD_SHOP_READY);
    p32(def1 + 0x9c, 0xffffffff);
    CHECK(!dd_shop_slot(&s, 0, &i));
    setup();
    CHECK(open(&s) == DD_SHOP_READY);
    p64(library + 8, definitions + 20000 * 8);
    CHECK(!dd_shop_slot(&s, 0, &i));
    setup();
    CHECK(open(&s) == DD_SHOP_READY);
    p64(campaign + 0xe98, estate_items + 1);
    CHECK(!dd_shop_owned(&s, 123, 101, &n));
    setup();
    CHECK(open(&s) == DD_SHOP_READY);
    p32(wallet, 0xffffffff);
    CHECK(!dd_shop_gold(&s, &n));
    setup();
    CHECK(open(&s) == DD_SHOP_READY);
    p64(campaign + 0xe78, wallet + 33 * 0x48);
    CHECK(!dd_shop_gold(&s, &n));
    setup();
    p64(node + 0x28, UINT64_MAX - 100);
    CHECK(open(&s) == DD_SHOP_BAD_TREE);
}
static void storage_capacity(void) {
    setup();
    dd_shop s;
    int32_t quantity = -1;
    CHECK(open(&s) == DD_SHOP_READY);
    item(estate_items + 9998ull * 0x5a8, 101, 3);
    CHECK(dd_shop_owned(&s, 123, 101, &quantity) && quantity == 5);
    /* Include the last slot and reject corrupt quantities or a vector above the supported limit. */
    p32(estate_items + 9998ull * 0x5a8, 0xffffffff);
    CHECK(!dd_shop_owned(&s, 123, 101, &quantity));
    p32(estate_items + 9998ull * 0x5a8, 3);
    p64(campaign + 0xe98, estate_items + 10000ull * 0x5a8);
    CHECK(!dd_shop_owned(&s, 123, 101, &quantity));
}
int main(void) {
    normal();
    invalid();
    storage_capacity();
    printf("Nomad memory adapter: mapping, stable slots, wallet/inventory and malformed models: %s (%d "
           "failures)\n",
           failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
