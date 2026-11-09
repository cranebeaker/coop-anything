/* Read-only, bounded access to the known build's normal Nomad Wagon model.
 * No UI pointers or peer pointers are used; tested with an isolated memory adapter. */
#ifndef DD_SHOP_H
#define DD_SHOP_H
#include <stddef.h>
#include <stdint.h>

#define DD_SHOP_MAX_STOCK 128
/* base.inventory.system_configs.darkest: normal trinket_storage reserves all 9999 slots. */
#define DD_SHOP_MAX_STORAGE 9999
typedef int (*dd_shop_read_fn)(void *ctx, uint64_t address, void *out, size_t len);
typedef struct {
    dd_shop_read_fn read;
    void *ctx;
    uint64_t image, campaign, stock, estate;
} dd_shop;
typedef struct {
    uint16_t physical, visible;
    uint32_t type, id;
    int32_t quantity, base_price, base_sale;
    uint64_t stack;
} dd_shop_item;
typedef enum {
    DD_SHOP_READY = 0,
    DD_SHOP_UNREADABLE,
    DD_SHOP_OTHER_MODE,
    DD_SHOP_BAD_TREE,
    DD_SHOP_NO_WAGON,
    DD_SHOP_BAD_CHILDREN,
    DD_SHOP_NO_SUPPLY,
    DD_SHOP_BAD_STOCK
} dd_shop_result;
dd_shop_result dd_shop_open(dd_shop *out, dd_shop_read_fn read, void *ctx, uint64_t image, uint64_t campaign);
const char *dd_shop_result_name(dd_shop_result r);
int dd_shop_visible(const dd_shop *s, uint16_t visible, dd_shop_item *out);
int dd_shop_slot(const dd_shop *s, uint16_t physical, dd_shop_item *out);
/* Physical estate storage slots; includes only occupied normal trinkets with a positive sale value. */
int dd_shop_estate_slot(const dd_shop *s, uint16_t physical, dd_shop_item *out);
int dd_shop_estate_quantity(const dd_shop *s, uint16_t physical, int32_t *quantity);
int dd_shop_quantity(const dd_shop *s, uint16_t physical, int32_t *quantity);
int dd_shop_owned(const dd_shop *s, uint32_t type, uint32_t id, int32_t *quantity);
int dd_shop_gold(const dd_shop *s, int32_t *gold);
#endif
