#include "dd_hamlet.h"
#include "dd_text.h"
#include <string.h>
static int rd(const dd_hamlet *s, uint64_t p, void *out, size_t n) {
    return p && p <= UINT64_MAX - n && s->read(s->ctx, p, out, n);
}
static int ptr(const dd_hamlet *s, uint64_t p, uint64_t *v) { return rd(s, p, v, 8); }
static int word(const dd_hamlet *s, uint64_t p, uint32_t *v) { return rd(s, p, v, 4); }
static int vector(const dd_hamlet *s, uint64_t p, unsigned stride, unsigned cap, uint64_t *begin,
                  unsigned *count) {
    uint64_t end = 0;
    if (!ptr(s, p, begin) || !ptr(s, p + 8, &end) || end < *begin || (end - *begin) % stride ||
        (end - *begin) / stride > cap || (!*begin && end))
        return 0;
    *count = (unsigned)((end - *begin) / stride);
    return 1;
}
int dd_hamlet_open(dd_hamlet *s, dd_hamlet_read_fn read, void *ctx, uint64_t image, uint64_t campaign) {
    memset(s, 0, sizeof *s);
    s->read = read;
    s->ctx = ctx;
    s->image = image;
    s->campaign = campaign;
    uint8_t mode = 1;
    uint64_t begin = 0;
    unsigned count = 0;
    return read && campaign && rd(s, image + 0x2a6d4bb, &mode, 1) && !mode &&
           vector(s, campaign + 0x20, 8, DD_HAMLET_HEROES, &begin, &count);
}
int dd_hamlet_find(const dd_hamlet *s, uint32_t id, dd_hamlet_hero *out) {
    memset(out, 0, sizeof *out);
    uint64_t begin = 0;
    unsigned count = 0;
    int found = 0;
    if (!id || !vector(s, s->campaign + 0x20, 8, DD_HAMLET_HEROES, &begin, &count)) return -1;
    for (unsigned i = 0; i < count; i++) {
        uint64_t entry = 0;
        uint32_t key = 0, duplicate = 0, location = 0;
        if (!ptr(s, begin + 8 * i, &entry) || !entry || !word(s, entry, &key) ||
            !word(s, entry + 0x1384, &duplicate) || key != duplicate || !key ||
            !word(s, entry + 0x15b0, &location) || location > 16)
            return -1;
        if (key == id) {
            if (found) return -1;
            found = 1;
            out->entry = entry;
            out->hero = entry + 8;
            out->id = id;
            out->location = location;
        }
    }
    return found;
}
int dd_hamlet_hero_pointer(const dd_hamlet *s, uint64_t pointer, dd_hamlet_hero *out) {
    uint32_t id = 0;
    return pointer && word(s, pointer + 0x137c, &id) && dd_hamlet_find(s, id, out) == 1 &&
           out->hero == pointer;
}
int dd_hamlet_name_valid(const char name[DD_HAMLET_NAME]) { return dd_utf8_name_valid(name, DD_HAMLET_NAME); }
int dd_hamlet_name(const dd_hamlet *s, const dd_hamlet_hero *h, char out[DD_HAMLET_NAME]) {
    return rd(s, h->hero + 8, out, DD_HAMLET_NAME) && dd_hamlet_name_valid(out);
}
int dd_hamlet_skill(const dd_hamlet *s, const dd_hamlet_hero *h, unsigned camping, unsigned index,
                    int *active) {
    uint64_t learned = 0, selected = 0;
    unsigned learned_n = 0, selected_n = 0;
    uint32_t level = 0;
    if (camping > 1 || index >= DD_HAMLET_SKILLS ||
        !vector(s, h->hero + (camping ? 0x13b0 : 0x13e0), 4, DD_HAMLET_SKILLS, &learned, &learned_n) ||
        index >= learned_n || !word(s, learned + 4 * index, &level) || level > 4 ||
        !vector(s, h->hero + (camping ? 0x1398 : 0x13c8), 4, 8, &selected, &selected_n))
        return 0;
    uint32_t ids[8] = {0};
    *active = 0;
    for (unsigned i = 0; i < selected_n; i++) {
        if (!word(s, selected + 4 * i, &ids[i]) || ids[i] >= learned_n) return 0;
        for (unsigned j = 0; j < i; j++)
            if (ids[j] == ids[i]) return 0;
        if (ids[i] == index) *active = 1;
    }
    return 1;
}
int dd_hamlet_skill_mask(const dd_hamlet *s, const dd_hamlet_hero *h, unsigned camping, uint32_t *mask) {
    uint64_t begin = 0;
    unsigned count = 0;
    *mask = 0;
    if (camping > 1 || !vector(s, h->hero + (camping ? 0x1398 : 0x13c8), 4, 8, &begin, &count)) return 0;
    for (unsigned i = 0; i < count; i++) {
        uint32_t index = 0;
        int active = 0;
        if (!word(s, begin + 4 * i, &index) || index >= 32 ||
            !dd_hamlet_skill(s, h, camping, index, &active) || !active || (*mask & (1u << index)))
            return 0;
        *mask |= 1u << index;
    }
    return 1;
}
int dd_hamlet_currency(const dd_hamlet *s, uint32_t type, int32_t *amount) {
    uint64_t begin = 0;
    unsigned count = 0;
    int found = 0;
    if (!type || !vector(s, s->campaign + 0xe70, 0x48, 32, &begin, &count)) return 0;
    for (unsigned i = 0; i < count; i++) {
        uint32_t key = 0, value = 0;
        if (!word(s, begin + i * 0x48 + 0x44, &key)) return 0;
        if (key == type) {
            if (found || !word(s, begin + i * 0x48, &value) || value > 0x7fffffff) return 0;
            found = 1;
            *amount = (int32_t)value;
        }
    }
    return found;
}
int dd_hamlet_exchange_valid(uint32_t from, int32_t amount, uint32_t to, int32_t received) {
    /* Supported normal base-game heirloom_exchange.json. Modified/DLC rates are not guessed. */
    static const uint32_t types[] = {14924810, 504499961, 15176862, 798419517}; /* bust portrait deed crest */
    static const uint8_t rates[12][4] = {{0, 3, 1, 1}, {0, 3, 2, 2}, {0, 2, 3, 3}, {1, 2, 0, 3},
                                         {1, 2, 2, 3}, {1, 1, 3, 3}, {2, 3, 0, 2}, {2, 3, 1, 1},
                                         {2, 2, 3, 3}, {3, 3, 0, 1}, {3, 6, 1, 1}, {3, 3, 2, 1}};
    if (amount <= 0 || received <= 0 || from == to) return 0;
    for (unsigned i = 0; i < 12; i++)
        if (from == types[rates[i][0]] && to == types[rates[i][2]])
            return amount % rates[i][1] == 0 && (int64_t)(amount / rates[i][1]) * rates[i][3] == received;
    return 0;
}
int dd_hamlet_panel(const dd_hamlet *s, uint32_t vt_rva, uint64_t *out) {
    uint64_t town = 0, vt = 0;
    uint32_t offset = 0;
    uint8_t mode = 1;
    *out = 0;
    /* Pinned TownDisplay constructor 6ce2c0 -> 6ce970. These are embedded normal-mode panels,
     * not members of the save-screen registry at 117d9c0. Resolve afresh on every use. */
    switch (vt_rva) {
    case 0xec00a0:
        offset = 0x2f48;
        break; /* RosterList */
    case 0xeba728:
        offset = 0x3640;
        break; /* EmbarkParty */
    case 0xee2ae8:
        offset = 0x38c0;
        break; /* CharacterDisplay */
    default:
        return 0;
    }
    if (!rd(s, s->image + 0x2a6d4bb, &mode, 1) || mode || !ptr(s, s->image + 0x118e360, &town) || !town ||
        !ptr(s, town, &vt) || vt != s->image + 0xec37a8 || town > UINT64_MAX - offset ||
        !ptr(s, town + offset, &vt) || vt != s->image + vt_rva)
        return -1;
    *out = town + offset;
    return 1;
}
static int party_ids_valid(const dd_hamlet *s, const uint32_t ids[4], int available) {
    for (unsigned i = 0; i < 4; i++)
        if (ids[i]) {
            dd_hamlet_hero hero;
            if (dd_hamlet_find(s, ids[i], &hero) != 1 || (available && hero.location > 1)) return 0;
            for (unsigned j = 0; j < i; j++)
                if (ids[i] == ids[j]) return 0;
        }
    return 1;
}
int dd_hamlet_party(const dd_hamlet *s, uint32_t out[4]) {
    uint64_t begin = 0;
    unsigned count = 0;
    if (!vector(s, s->campaign, 4, 4, &begin, &count) || count != 4) return 0;
    for (unsigned i = 0; i < 4; i++) {
        if (!word(s, begin + 4 * i, &out[i])) return 0;
        if (out[i] == UINT32_MAX) out[i] = 0;
    }
    /* The remembered last-party vector can still name a hero now in a building. Reading its
     * identity must succeed so the normal expedition UI can clear it. Availability is a
     * constraint on a NEW destination, not on the old vector we are trying to repair. */
    return party_ids_valid(s, out, 0);
}
int dd_hamlet_party_valid(const dd_hamlet *s, const uint32_t ids[4]) { return party_ids_valid(s, ids, 1); }
int dd_hamlet_party_slots_status(const dd_hamlet *s, uint64_t slots[4]) {
    uint64_t panel = 0, begin = 0;
    unsigned count = 0, seen = 0;
    memset(slots, 0, 4 * sizeof *slots);
    if (dd_hamlet_panel(s, 0xeba728, &panel) != 1 || !vector(s, panel + 0x88, 8, 4, &begin, &count))
        return -1;
    if (count != 4) return 0; /* Native panel construction may not have populated all four rows. */
    for (unsigned i = 0; i < count; i++) {
        uint64_t widget = 0, controller = 0, vt = 0;
        uint32_t rank = 0;
        if (!ptr(s, begin + 8 * i, &widget)) return -1;
        if (!widget) return 0;
        if (!ptr(s, widget, &vt)) return -1;
        if (!vt) return 0;
        if (vt != s->image + 0xebcc00 || !ptr(s, widget + 0x158, &controller)) return -1;
        if (!controller) return 0;
        if (!ptr(s, controller, &vt)) return -1;
        if (!vt) return 0;
        if (vt != s->image + 0xee0838 || !word(s, controller + 0x230, &rank) || rank >= 4 ||
            (seen & (1u << rank)))
            return -1;
        slots[rank] = controller;
        seen |= 1u << rank;
    }
    return seen == 15;
}
int dd_hamlet_party_slots(const dd_hamlet *s, uint64_t slots[4]) {
    return dd_hamlet_party_slots_status(s, slots) == 1;
}
int dd_hamlet_inventory(const dd_hamlet *s, uint32_t hero_id, uint64_t *inventory, unsigned *count) {
    uint64_t begin = 0;
    if (hero_id >= DD_TRINKET_LOOT) {
        uint64_t raid = 0;
        if (!ptr(s, s->image + 0x118e2c8, &raid) || !raid) return 0;
        *inventory = raid + (hero_id == DD_TRINKET_BAG ? 0x218 : 0x5240);
    } else if (!hero_id)
        *inventory = s->campaign + 0xe88;
    else {
        dd_hamlet_hero hero;
        if (dd_hamlet_find(s, hero_id, &hero) != 1) return 0;
        *inventory = hero.hero + 0x1280;
    }
    return vector(s, *inventory + 8, 0x5a8, DD_TRINKET_SLOTS(hero_id), &begin, count);
}
int dd_hamlet_inventory_owner(const dd_hamlet *s, uint64_t inventory, uint32_t *owner) {
    uint64_t actual = 0;
    unsigned count = 0;
    *owner = 0;
    if (inventory == s->campaign + 0xe88) return dd_hamlet_inventory(s, 0, &actual, &count);
    for (unsigned i = 0; i < 2; i++) {
        uint32_t candidate = i ? DD_TRINKET_LOOT : DD_TRINKET_BAG;
        if (dd_hamlet_inventory(s, candidate, &actual, &count) && actual == inventory) {
            *owner = candidate;
            return 1;
        }
    }
    uint64_t begin = 0;
    unsigned heroes = 0;
    if (!vector(s, s->campaign + 0x20, 8, DD_HAMLET_HEROES, &begin, &heroes)) return 0;
    for (unsigned i = 0; i < heroes; i++) {
        uint64_t entry = 0;
        uint32_t id = 0;
        if (!ptr(s, begin + 8 * i, &entry) || !word(s, entry, &id)) return 0;
        if (entry + 8 + 0x1280 == inventory && dd_hamlet_inventory(s, id, &actual, &count)) {
            *owner = id;
            return 1;
        }
    }
    return 0;
}
int dd_hamlet_item(const dd_hamlet *s, uint64_t inventory, unsigned slot, uint32_t *type, uint32_t *id,
                   uint32_t *quantity) {
    uint64_t begin = 0;
    unsigned count = 0;
    uint32_t owner = 0;
    if (!dd_hamlet_inventory_owner(s, inventory, &owner) ||
        !vector(s, inventory + 8, 0x5a8, DD_TRINKET_SLOTS(owner), &begin, &count) || slot >= count ||
        !word(s, begin + (uint64_t)slot * 0x5a8, quantity) || *quantity > (owner >= DD_TRINKET_LOOT ? 0x7fffffffu : 1u))
        return 0;
    *type = *id = 0;
    return !*quantity || (word(s, begin + (uint64_t)slot * 0x5a8 + 0x44, type) && *type &&
                          word(s, begin + (uint64_t)slot * 0x5a8 + 0x88, id) && *id);
}
int dd_hamlet_can_equip(const dd_hamlet *s, uint32_t owner, unsigned slot, uint32_t id,
                        unsigned ignore_slot) {
    if (!DD_TRINKET_IS_HERO(owner) || !id) return 1;
    dd_hamlet_hero hero;
    uint64_t inventory = 0, definitions = 0, klass = 0;
    unsigned n = 0, definition_n = 0;
    uint32_t class_id = 0;
    if (dd_hamlet_find(s, owner, &hero) != 1 || hero.location > 1 ||
        !dd_hamlet_inventory(s, owner, &inventory, &n) || slot >= n || !ptr(s, hero.hero + 0x1320, &klass) ||
        !klass || !word(s, klass + 0xd4, &class_id) ||
        !vector(s, s->image + 0x2c3c570, 0x488, 8192, &definitions, &definition_n))
        return 0;
    int allowed = 0, found = 0;
    for (unsigned i = 0; i < definition_n; i++) {
        uint32_t key = 0;
        if (!word(s, definitions + i * 0x488 + 0x40, &key)) return 0;
        if (key != id) continue;
        uint64_t classes = 0;
        unsigned count = 0;
        if (found++ || !vector(s, definitions + i * 0x488 + 0x2d8, 4, 64, &classes, &count)) return 0;
        allowed = count == 0;
        for (unsigned j = 0; j < count; j++) {
            uint32_t candidate = 0;
            if (!word(s, classes + 4 * j, &candidate)) return 0;
            if (candidate == class_id) allowed = 1;
        }
    }
    if (!found || !allowed) return 0;
    for (unsigned i = 0; i < n; i++)
        if (i != slot && i != ignore_slot) {
            uint32_t type = 0, key = 0, q = 0;
            if (!dd_hamlet_item(s, inventory, i, &type, &key, &q) || (q && key == id)) return 0;
        }
    return 1;
}
