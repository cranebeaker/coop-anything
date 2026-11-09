/* Stable wire endpoints. A raid bag/loot is not a persistent hero ID. */
#ifndef DD_TRINKET_H
#define DD_TRINKET_H
#include <stdint.h>
#define DD_TRINKET_BAG UINT32_MAX
#define DD_TRINKET_LOOT (UINT32_MAX - 1u)
#define DD_TRINKET_IS_HERO(owner) ((owner) != 0 && (owner) < DD_TRINKET_LOOT)
#define DD_TRINKET_SLOTS(owner) ((owner) >= DD_TRINKET_LOOT ? 64u : ((owner) ? 2u : 9999u))
#endif
