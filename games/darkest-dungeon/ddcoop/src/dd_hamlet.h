/* Bounded normal-campaign reads and validation for host-authoritative Hamlet commands. */
#ifndef DD_HAMLET_H
#define DD_HAMLET_H
#include <stddef.h>
#include <stdint.h>
#include "dd_trinket.h"
#define DD_HAMLET_HEROES 64
#define DD_HAMLET_NAME 64
#define DD_HAMLET_SKILLS 32
typedef int (*dd_hamlet_read_fn)(void *, uint64_t, void *, size_t);
typedef struct {
    dd_hamlet_read_fn read;
    void *ctx;
    uint64_t image, campaign;
} dd_hamlet;
typedef struct {
    uint64_t entry, hero;
    uint32_t id, location;
} dd_hamlet_hero;
int dd_hamlet_open(dd_hamlet *, dd_hamlet_read_fn, void *, uint64_t, uint64_t);
/* 1=found, 0=absent, -1=invalid snapshot. Hero is Entry+8. */
int dd_hamlet_find(const dd_hamlet *, uint32_t, dd_hamlet_hero *);
int dd_hamlet_hero_pointer(const dd_hamlet *, uint64_t, dd_hamlet_hero *);
int dd_hamlet_name(const dd_hamlet *, const dd_hamlet_hero *, char out[DD_HAMLET_NAME]);
int dd_hamlet_name_valid(const char name[DD_HAMLET_NAME]);
/* 0=combat, 1=camping; rejects unknown/unlearned indices and malformed/duplicate selected skills. */
int dd_hamlet_skill(const dd_hamlet *, const dd_hamlet_hero *, unsigned, unsigned, int *active);
int dd_hamlet_skill_mask(const dd_hamlet *, const dd_hamlet_hero *, unsigned, uint32_t *);
int dd_hamlet_currency(const dd_hamlet *, uint32_t, int32_t *);
int dd_hamlet_exchange_valid(uint32_t, int32_t, uint32_t, int32_t);
/* Pinned normal TownDisplay embedded panel; rechecks root/child vtables. -1=unavailable, 0=unknown kind. */
int dd_hamlet_panel(const dd_hamlet *, uint32_t, uint64_t *);
int dd_hamlet_party(const dd_hamlet *, uint32_t out[4]); /* Old IDs: identity/uniqueness, even if now busy. */
int dd_hamlet_party_valid(const dd_hamlet *, const uint32_t ids[4]); /* NEW destination: all available. */
int dd_hamlet_party_slots(const dd_hamlet *, uint64_t slots[4]);
/* 1=genuine complete slots, 0=normal panel not built yet, -1=invalid/unreadable model. */
int dd_hamlet_party_slots_status(const dd_hamlet *, uint64_t slots[4]);
/* 0 = estate; DD_TRINKET_BAG/LOOT = current raid inventories, bounded to 64 slots.
 * Other owners are persistent hero IDs, Hero+0x1280, at most two slots. */
int dd_hamlet_inventory(const dd_hamlet *, uint32_t, uint64_t *, unsigned *);
int dd_hamlet_inventory_owner(const dd_hamlet *, uint64_t, uint32_t *);
int dd_hamlet_item(const dd_hamlet *, uint64_t, unsigned, uint32_t *, uint32_t *, uint32_t *);
/* Class restrictions and duplicate equipped IDs are checked against the final destination. */
int dd_hamlet_can_equip(const dd_hamlet *, uint32_t, unsigned, uint32_t, unsigned);
#endif
