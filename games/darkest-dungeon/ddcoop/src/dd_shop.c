#include "dd_shop.h"
#include <limits.h>
#include <string.h>

#define ITEM_BYTES 0x5a8u
static int bytes(const dd_shop *s, uint64_t p, void *out, size_t n) {
    return p && p <= UINT64_MAX - n && s->read(s->ctx, p, out, n);
}
static int ptr(const dd_shop *s, uint64_t p, uint64_t *v) { return bytes(s, p, v, sizeof *v); }
static int u32(const dd_shop *s, uint64_t p, uint32_t *v) { return bytes(s, p, v, sizeof *v); }
static int vector(const dd_shop *s, uint64_t at, unsigned stride, unsigned cap, uint64_t *begin,
                  unsigned *count) {
    uint64_t end;
    if (!ptr(s, at, begin) || !ptr(s, at + 8, &end) || end < *begin || (end - *begin) % stride ||
        (end - *begin) / stride > cap || (!*begin && end))
        return 0;
    *count = (unsigned)((end - *begin) / stride);
    return 1;
}
dd_shop_result dd_shop_open(dd_shop *s, dd_shop_read_fn read, void *ctx, uint64_t image, uint64_t campaign) {
    memset(s, 0, sizeof *s);
    s->read = read;
    s->ctx = ctx;
    s->image = image;
    s->campaign = campaign;
    uint8_t alternate = 0;
    if (!campaign || !bytes(s, image + 0x2a6d4bb, &alternate, 1)) return DD_SHOP_UNREADABLE;
    if (alternate) return DD_SHOP_OTHER_MODE;
    /* Buildings manager map: head at Campaign+0x1138+0xc0; MSVC node value at +0x28.
     * NomadWagon's child-item vector is +0xa8/+0xb0; only normal TrinketSupply is accepted. */
    uint64_t head = 0, root = 0, wagon = 0;
    uint8_t nil = 0;
    if (!ptr(s, campaign + 0x11f8, &head) || !head || !bytes(s, head + 0x19, &nil, 1) || !nil ||
        !ptr(s, head + 8, &root))
        return DD_SHOP_BAD_TREE;
    uint64_t pending[128];
    unsigned n = 0, seen = 0;
    if (root != head) pending[n++] = root;
    while (n) {
        uint64_t node = pending[--n], left = 0, right = 0, model = 0, vt = 0;
        if (++seen > 128 || !node || node == head || !bytes(s, node + 0x19, &nil, 1) || nil ||
            !ptr(s, node, &left) || !ptr(s, node + 0x10, &right) || !ptr(s, node + 0x28, &model) || !model ||
            !ptr(s, model, &vt))
            return DD_SHOP_BAD_TREE;
        if (vt == image + 0xe9c338) {
            if (wagon) return DD_SHOP_BAD_TREE;
            wagon = model;
        }
        if (left != head) {
            if (n == 128) return DD_SHOP_BAD_TREE;
            pending[n++] = left;
        }
        if (right != head) {
            if (n == 128) return DD_SHOP_BAD_TREE;
            pending[n++] = right;
        }
    }
    if (!wagon) return DD_SHOP_NO_WAGON;
    uint64_t children = 0;
    unsigned count = 0;
    if (!vector(s, wagon + 0xa8, 8, 64, &children, &count)) return DD_SHOP_BAD_CHILDREN;
    for (unsigned i = 0; i < count; i++) {
        uint64_t child = 0, vt = 0;
        if (!ptr(s, children + i * 8, &child)) return DD_SHOP_BAD_CHILDREN;
        if (!child) continue; /* the game's own child iterator skips empty entries */
        if (!ptr(s, child, &vt)) return DD_SHOP_BAD_CHILDREN;
        if (vt == image + 0xe9c2e0) {
            if (s->stock) return DD_SHOP_BAD_CHILDREN;
            s->stock = child + 0x68;
        }
    }
    if (!s->stock) return DD_SHOP_NO_SUPPLY;
    uint64_t begin = 0;
    if (!vector(s, s->stock + 8, ITEM_BYTES, DD_SHOP_MAX_STOCK, &begin, &count)) return DD_SHOP_BAD_STOCK;
    s->estate = campaign + 0xe88;
    return DD_SHOP_READY;
}
static int definition(const dd_shop *s, dd_shop_item *item) {
    uint64_t lib = 0, begin = 0;
    unsigned count = 0;
    if (!ptr(s, s->image + 0x118e0f8, &lib) || !lib || !vector(s, lib, 8, 16384, &begin, &count)) return 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t def = 0;
        uint32_t type = 0, id = 0;
        if (!ptr(s, begin + i * 8, &def) || !def || !u32(s, def + 0x40, &type) || !u32(s, def + 0x84, &id))
            return 0;
        if (type == item->type && id == item->id) {
            uint32_t cost = 0;
            if (!u32(s, def + 0x9c, &cost) || cost > INT32_MAX) return 0;
            item->base_price = (int32_t)cost;
            /* Some definitions cannot be sold. That must not change purchase eligibility. */
            item->base_sale = u32(s, def + 0xa4, &cost) && cost <= INT32_MAX ? (int32_t)cost : 0;
            return 1;
        }
    }
    return 0;
}
int dd_shop_slot(const dd_shop *s, uint16_t physical, dd_shop_item *out) {
    uint64_t begin = 0;
    unsigned count = 0;
    memset(out, 0, sizeof *out);
    if (!s->stock || !vector(s, s->stock + 8, ITEM_BYTES, DD_SHOP_MAX_STOCK, &begin, &count) ||
        physical >= count)
        return 0;
    unsigned visible = 0;
    for (unsigned i = 0; i <= physical; i++) {
        uint32_t quantity = 0;
        if (!u32(s, begin + (uint64_t)i * ITEM_BYTES, &quantity) || quantity > INT32_MAX) return 0;
        if (i == physical) {
            out->stack = begin + (uint64_t)i * ITEM_BYTES;
            out->physical = physical;
            out->visible = (uint16_t)visible;
            out->quantity = (int32_t)quantity;
            return quantity > 0 && u32(s, out->stack + 0x44, &out->type) &&
                   u32(s, out->stack + 0x88, &out->id) && out->type && out->id && definition(s, out);
        }
        if (quantity) visible++;
    }
    return 0;
}
int dd_shop_visible(const dd_shop *s, uint16_t visible, dd_shop_item *out) {
    uint64_t begin = 0;
    unsigned count = 0;
    if (!s->stock || !vector(s, s->stock + 8, ITEM_BYTES, DD_SHOP_MAX_STOCK, &begin, &count)) return 0;
    unsigned j = 0;
    for (unsigned i = 0; i < count; i++) {
        uint32_t q = 0;
        if (!u32(s, begin + (uint64_t)i * ITEM_BYTES, &q) || q > INT32_MAX) return 0;
        if (q && j++ == visible) return dd_shop_slot(s, (uint16_t)i, out);
    }
    return 0;
}
int dd_shop_estate_slot(const dd_shop *s, uint16_t physical, dd_shop_item *out) {
    uint64_t begin = 0;
    unsigned count = 0;
    uint32_t trinket_type = 0, quantity = 0;
    memset(out, 0, sizeof *out);
    if (!s->estate || !vector(s, s->estate + 8, ITEM_BYTES, DD_SHOP_MAX_STORAGE, &begin, &count) ||
        physical >= count || !u32(s, s->image + 0x2a6f868, &trinket_type) || !trinket_type)
        return 0;
    out->stack = begin + (uint64_t)physical * ITEM_BYTES;
    out->physical = physical;
    if (!u32(s, out->stack, &quantity) || quantity != 1 || !u32(s, out->stack + 0x44, &out->type) ||
        out->type != trinket_type || !u32(s, out->stack + 0x88, &out->id) || !out->id ||
        !definition(s, out) || out->base_sale <= 0)
        return 0;
    out->quantity = 1;
    return 1;
}
int dd_shop_estate_quantity(const dd_shop *s, uint16_t physical, int32_t *quantity) {
    uint64_t begin = 0;
    unsigned count = 0;
    uint32_t q = 0;
    if (!s->estate || !vector(s, s->estate + 8, ITEM_BYTES, DD_SHOP_MAX_STORAGE, &begin, &count) ||
        physical >= count || !u32(s, begin + (uint64_t)physical * ITEM_BYTES, &q) || q > INT32_MAX)
        return 0;
    *quantity = (int32_t)q;
    return 1;
}
int dd_shop_quantity(const dd_shop *s, uint16_t physical, int32_t *quantity) {
    uint64_t begin = 0;
    unsigned count = 0;
    uint32_t q = 0;
    if (!s->stock || !vector(s, s->stock + 8, ITEM_BYTES, DD_SHOP_MAX_STOCK, &begin, &count) ||
        physical >= count || !u32(s, begin + (uint64_t)physical * ITEM_BYTES, &q) || q > INT32_MAX)
        return 0;
    *quantity = (int32_t)q;
    return 1;
}
int dd_shop_owned(const dd_shop *s, uint32_t type, uint32_t id, int32_t *quantity) {
    uint64_t begin = 0;
    unsigned count = 0;
    uint64_t total = 0;
    if (!s->estate || !vector(s, s->estate + 8, ITEM_BYTES, DD_SHOP_MAX_STORAGE, &begin, &count)) return 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t item = begin + (uint64_t)i * ITEM_BYTES;
        uint32_t q = 0, t = 0, key = 0;
        if (!u32(s, item, &q) || q > INT32_MAX) return 0;
        if (!q) continue;
        if (!u32(s, item + 0x44, &t) || !u32(s, item + 0x88, &key)) return 0;
        if (t == type && key == id) total += q;
    }
    if (total > INT32_MAX) return 0;
    *quantity = (int32_t)total;
    return 1;
}
int dd_shop_gold(const dd_shop *s, int32_t *gold) {
    uint64_t begin = 0;
    unsigned count = 0;
    uint32_t gold_type = 0;
    if (!u32(s, s->image + 0x2bb41f0, &gold_type) ||
        !vector(s, s->campaign + 0xe70, 0x48, 32, &begin, &count))
        return 0;
    for (unsigned i = 0; i < count; i++) {
        uint32_t type = 0, q = 0;
        if (!u32(s, begin + i * 0x48 + 0x44, &type) || !u32(s, begin + i * 0x48, &q)) return 0;
        if (type == gold_type) {
            if (q > INT32_MAX) return 0;
            *gold = (int32_t)q;
            return 1;
        }
    }
    return 0;
}
const char *dd_shop_result_name(dd_shop_result r) {
    static const char *names[] = {"ready",
                                  "unreadable campaign",
                                  "other game mode",
                                  "invalid building map",
                                  "Nomad model missing",
                                  "invalid child models",
                                  "normal supply missing",
                                  "invalid stock vector"};
    return (unsigned)r < sizeof names / sizeof names[0] ? names[r] : "unknown";
}
