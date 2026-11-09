#include "dd_provisions.h"
#include <limits.h>
#include <string.h>

#define ITEM_SIZE 0x5a8u
#define DEFINITION_MAX 16384u
typedef struct {
    dd_provisions_read_fn read;
    void *ctx;
    const char *stage;
    uint64_t image, library, definitions[2];
    unsigned empty_types_count;
    uint32_t empty_types[DD_PROVISIONS_STOCK_MAX + DD_PROVISIONS_BAG_MAX];
} reader;
typedef struct {
    uint64_t begin, end, capacity;
} span;
static int get(reader *r, uint64_t at, void *out, size_t n) {
    return at && at <= UINT64_MAX - n && r->read(r->ctx, at, out, n);
}
static int pointer(reader *r, uint64_t at, uint64_t *v) {
    return get(r, at, v, sizeof *v) && *v <= 0x00007fffffffffffULL;
}
static int vector_ok(const span *v, uint32_t stride, uint32_t cap) {
    return v->capacity <= 0x00007fffffffffffULL && v->end >= v->begin && v->capacity >= v->end &&
           (v->end - v->begin) % stride == 0 && (v->capacity - v->begin) % stride == 0 &&
           (v->capacity - v->begin) / stride <= cap && (v->begin || !v->capacity);
}
static dd_provisions_result definitions_unchanged(reader *r) {
    uint64_t library = 0, after[2];
    if (!pointer(r, r->image + 0x118e0f8, &library) || !get(r, r->library, after, sizeof after))
        return DD_PROVISIONS_UNREADABLE;
    return library == r->library && !memcmp(after, r->definitions, sizeof after) ? DD_PROVISIONS_OK
                                                                                 : DD_PROVISIONS_CHANGED;
}
/* The normal game identifies items by the PAIR (type,id), and food's ID is the empty string.
 * Accept zero ID only if that exact pair exists in the loaded, bounded item-definition library.
 * Do not reinterpret a positive-quantity food stack as an empty slot or invent an ID for it. */
static dd_provisions_result empty_id_known(reader *r, uint32_t type) {
    for (unsigned i = 0; i < r->empty_types_count; i++)
        if (r->empty_types[i] == type) return DD_PROVISIONS_OK;
    if (!r->library) {
        if (!pointer(r, r->image + 0x118e0f8, &r->library) || !r->library ||
            !get(r, r->library, r->definitions, sizeof r->definitions))
            return DD_PROVISIONS_UNREADABLE;
        uint64_t begin = r->definitions[0], end = r->definitions[1];
        if (end > 0x00007fffffffffffULL || end < begin || (end - begin) % 8 ||
            (end - begin) / 8 > DEFINITION_MAX || (!begin && end))
            return DD_PROVISIONS_LAYOUT;
    }
    int found = 0;
    for (uint64_t at = r->definitions[0]; at < r->definitions[1]; at += 8) {
        uint64_t def = 0;
        uint32_t def_type = 0, def_id = 0;
        if (!pointer(r, at, &def) || !def || !get(r, def + 0x40, &def_type, 4))
            return DD_PROVISIONS_UNREADABLE;
        if (def_type != type) continue;
        if (!get(r, def + 0x84, &def_id, 4)) return DD_PROVISIONS_UNREADABLE;
        if (!def_id) {
            found = 1;
            break;
        }
    }
    dd_provisions_result stable = definitions_unchanged(r);
    if (stable != DD_PROVISIONS_OK) return stable;
    if (!found || r->empty_types_count >= DD_PROVISIONS_STOCK_MAX + DD_PROVISIONS_BAG_MAX)
        return DD_PROVISIONS_LAYOUT;
    r->empty_types[r->empty_types_count++] = type;
    return DD_PROVISIONS_OK;
}
static dd_provisions_result inventory(reader *r, uint64_t inv, dd_provisions_item *items, uint32_t cap,
                                      uint32_t *slots, uint32_t *occupied, int is_bag) {
    span before, after;
    uint32_t reported = 0, actual = 0;
    r->stage = is_bag ? "bag-vector" : "stock-vector";
    if (!get(r, inv + 8, &before, sizeof before)) return DD_PROVISIONS_UNREADABLE;
    if (!vector_ok(&before, ITEM_SIZE, cap)) return DD_PROVISIONS_LAYOUT;
    *slots = (uint32_t)((before.end - before.begin) / ITEM_SIZE);
    r->stage = is_bag ? "bag-count" : "stock-count";
    if (!get(r, inv + 0x24, &reported, 4)) return DD_PROVISIONS_UNREADABLE;
    if (reported > *slots) return DD_PROVISIONS_LAYOUT;
    for (uint32_t i = 0; i < *slots; i++) {
        r->stage = is_bag ? "bag-item" : "stock-item";
        uint64_t at = before.begin + (uint64_t)i * ITEM_SIZE;
        int32_t qty = 0;
        if (!get(r, at, &qty, 4)) return DD_PROVISIONS_UNREADABLE;
        if (qty < 0 || qty > 1000000) return DD_PROVISIONS_LAYOUT;
        if (!qty) continue; /* Freed slots may retain identity bytes. Canonical empty is all zero. */
        items[i].quantity = qty;
        if (!get(r, at + 0x44, &items[i].type, 4) || !get(r, at + 0x88, &items[i].id, 4))
            return DD_PROVISIONS_UNREADABLE;
        if (!items[i].type) return DD_PROVISIONS_LAYOUT;
        if (!items[i].id) {
            r->stage = is_bag ? "bag-empty-id-definition" : "stock-empty-id-definition";
            dd_provisions_result known = empty_id_known(r, items[i].type);
            if (known != DD_PROVISIONS_OK) return known;
        }
        actual++;
    }
    uint32_t reported_after = 0;
    r->stage = is_bag ? "bag-recheck" : "stock-recheck";
    if (!get(r, inv + 8, &after, sizeof after) || !get(r, inv + 0x24, &reported_after, 4))
        return DD_PROVISIONS_UNREADABLE;
    if (memcmp(&before, &after, sizeof before) || reported_after != reported || actual != reported)
        return DD_PROVISIONS_CHANGED;
    *occupied = actual;
    return DD_PROVISIONS_OK;
}
static dd_provisions_result capture(reader *r, uint64_t image, dd_provisions_snapshot *s) {
    uint8_t mode;
    uint64_t manager = 0, device = 0, campaign = 0, roster[2], town = 0, vt = 0;
    char name[0x80];
    r->stage = "normal-mode";
    if (!get(r, image + 0x2a6d4bb, &mode, 1)) return DD_PROVISIONS_UNREADABLE;
    if (mode) return DD_PROVISIONS_OTHER_MODE;
    r->stage = "save-device";
    if (!pointer(r, image + 0x1185248, &manager)) return DD_PROVISIONS_UNREADABLE;
    if (!manager) return DD_PROVISIONS_WAIT;
    if (!pointer(r, manager + 0x38, &device)) return DD_PROVISIONS_UNREADABLE;
    if (!device) return DD_PROVISIONS_WAIT;
    if (!get(r, device + 0x20, name, sizeof name)) return DD_PROVISIONS_UNREADABLE;
    if (!memchr(name, 0, sizeof name)) return DD_PROVISIONS_LAYOUT;
    /* Any campaign slot (the host chooses it in the launcher; the runtime is only active in that one). Never profile 9, the game's restore area. */
    {
        const char *digit = !strncmp(name, "steam_profile_", 14) ? name + 14 : !strncmp(name, "profile_", 8) ? name + 8 : NULL;
        if (!digit || digit[0] < '0' || digit[0] > '8' || digit[1]) return DD_PROVISIONS_SLOT;
    }
    r->stage = "campaign";
    if (!pointer(r, image + 0x118e138, &campaign)) return DD_PROVISIONS_UNREADABLE;
    if (!campaign) return DD_PROVISIONS_WAIT;
    r->stage = "roster-vector";
    if (!get(r, campaign + 0x20, roster, sizeof roster)) return DD_PROVISIONS_UNREADABLE;
    if (!roster[0] || roster[1] < roster[0] || (roster[1] - roster[0]) % 8 || roster[1] - roster[0] > 64 * 8)
        return DD_PROVISIONS_LAYOUT;
    if (roster[0] == roster[1]) return DD_PROVISIONS_WAIT;
    r->stage = "town-type";
    if (!pointer(r, image + 0x118e360, &town)) return DD_PROVISIONS_UNREADABLE;
    if (!town) return DD_PROVISIONS_WAIT;
    if (!pointer(r, town, &vt)) return DD_PROVISIONS_UNREADABLE;
    if (vt != image + 0xec37a8) return DD_PROVISIONS_LAYOUT;
    /* TownDisplay ctor 6da730 adjusts this by +17a8 before calling 6dad90.
     * That subobject's ctor embeds ProvisionSelect at +1fc8: total +3770.
     * The prior +1fc8-only assumption incorrectly treated the subobject as TownDisplay. */
    uint64_t panel = town + 0x3770, views[2];
    r->stage = "provision-panel-type";
    if (!pointer(r, panel, &vt)) return DD_PROVISIONS_UNREADABLE;
    if (vt != image + 0xebdde8) return DD_PROVISIONS_LAYOUT;
    r->stage = "provision-panel-state";
    if (!get(r, panel + 0x60, &s->panel_visible, 1) || !get(r, panel + 0x80, views, sizeof views) ||
        !get(r, campaign + 0x1838, &s->context_1838, 4))
        return DD_PROVISIONS_UNREADABLE;
    if (s->panel_visible > 1) return DD_PROVISIONS_LAYOUT;
    for (unsigned i = 0; i < 2; i++) {
        if (!views[i]) continue;
        r->stage = i ? "bag-view-type" : "stock-view-type";
        if (!pointer(r, views[i], &vt)) return DD_PROVISIONS_UNREADABLE;
        if (vt != image + 0xee1420) return DD_PROVISIONS_LAYOUT;
    }
    s->stock_view = views[0] != 0;
    s->bag_view = views[1] != 0;
    dd_provisions_result result = inventory(r, campaign + 0x1618, s->stock, DD_PROVISIONS_STOCK_MAX,
                                            &s->stock_slots, &s->stock_occupied, 0);
    if (result != DD_PROVISIONS_OK) return result;
    result =
        inventory(r, campaign + 0x15b0, s->bag, DD_PROVISIONS_BAG_MAX, &s->bag_slots, &s->bag_occupied, 1);
    if (result != DD_PROVISIONS_OK) return result;
    uint64_t wallet[2], wallet_after[2];
    r->stage = "wallet-vector";
    if (!get(r, campaign + 0xe70, wallet, sizeof wallet)) return DD_PROVISIONS_UNREADABLE;
    if (wallet[1] < wallet[0] || (wallet[1] - wallet[0]) % 0x48 ||
        (wallet[1] - wallet[0]) / 0x48 > DD_PROVISIONS_CURRENCY_MAX || (!wallet[0] && wallet[1]))
        return DD_PROVISIONS_LAYOUT;
    s->currencies = (uint32_t)((wallet[1] - wallet[0]) / 0x48);
    r->stage = "wallet-item";
    for (uint32_t i = 0; i < s->currencies; i++) {
        uint64_t at = wallet[0] + (uint64_t)i * 0x48;
        if (!get(r, at, &s->wallet[i].quantity, 4) || !get(r, at + 0x44, &s->wallet[i].type, 4))
            return DD_PROVISIONS_UNREADABLE;
        if (s->wallet[i].quantity < 0 || !s->wallet[i].type) return DD_PROVISIONS_LAYOUT;
        for (uint32_t j = 0; j < i; j++)
            if (s->wallet[j].type == s->wallet[i].type) return DD_PROVISIONS_LAYOUT;
    }
    uint64_t campaign_after = 0, town_after = 0, manager_after = 0, device_after = 0;
    char name_after[0x80];
    r->stage = "roots-recheck";
    if (!get(r, campaign + 0xe70, wallet_after, sizeof wallet_after) ||
        !pointer(r, image + 0x118e138, &campaign_after) || !pointer(r, image + 0x118e360, &town_after) ||
        !pointer(r, image + 0x1185248, &manager_after) || !pointer(r, manager + 0x38, &device_after) ||
        !get(r, device + 0x20, name_after, sizeof name_after))
        return DD_PROVISIONS_UNREADABLE;
    if (campaign_after != campaign || town_after != town || manager_after != manager ||
        device_after != device || memcmp(wallet, wallet_after, sizeof wallet) ||
        memcmp(name, name_after, sizeof name))
        return DD_PROVISIONS_CHANGED;
    if (r->empty_types_count) {
        r->stage = "definitions-recheck";
        dd_provisions_result stable = definitions_unchanged(r);
        if (stable != DD_PROVISIONS_OK) return stable;
    }
    r->stage = "complete";
    return DD_PROVISIONS_OK;
}
dd_provisions_result dd_provisions_capture(dd_provisions_read_fn read, void *ctx, uint64_t image,
                                           dd_provisions_snapshot *out) {
    return dd_provisions_capture_ex(read, ctx, image, out, NULL);
}
dd_provisions_result dd_provisions_capture_ex(dd_provisions_read_fn read, void *ctx, uint64_t image,
                                              dd_provisions_snapshot *out, const char **stage) {
    if (stage) *stage = "arguments";
    if (!out) return DD_PROVISIONS_UNREADABLE;
    memset(out, 0, sizeof *out);
    if (!read || !image || image > UINT64_MAX - 0x3000000) return DD_PROVISIONS_UNREADABLE;
    reader r = {0};
    r.read = read;
    r.ctx = ctx;
    r.stage = "arguments";
    r.image = image;
    dd_provisions_result result = capture(&r, image, out);
    if (stage) *stage = r.stage;
    if (result != DD_PROVISIONS_OK) memset(out, 0, sizeof *out);
    return result;
}
const char *dd_provisions_result_name(dd_provisions_result r) {
    switch (r) {
    case DD_PROVISIONS_OK:
        return "ready";
    case DD_PROVISIONS_WAIT:
        return "waiting-for-town";
    case DD_PROVISIONS_SLOT:
        return "refused-other-slot";
    case DD_PROVISIONS_OTHER_MODE:
        return "refused-other-mode";
    case DD_PROVISIONS_UNREADABLE:
        return "unreadable";
    case DD_PROVISIONS_LAYOUT:
        return "unrecognised-layout";
    case DD_PROVISIONS_CHANGED:
        return "changed-during-capture";
    default:
        return "invalid-status";
    }
}
