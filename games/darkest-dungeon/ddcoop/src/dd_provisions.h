/* Build 28385 ONLY. Read-only measurement of the normal pre-departure supply model.
 * This is not a wire format or a native mutation API; no engine pointers leave the reader. */
#ifndef DD_PROVISIONS_H
#define DD_PROVISIONS_H
#include <stddef.h>
#include <stdint.h>

#define DD_PROVISIONS_STOCK_MAX 128
#define DD_PROVISIONS_BAG_MAX 64
#define DD_PROVISIONS_CURRENCY_MAX 32
typedef int (*dd_provisions_read_fn)(void *ctx, uint64_t address, void *out, size_t length);
typedef struct {
    /* Occupied identity is (type,id); food legitimately uses id=0. Quantity=0 means empty. */
    uint32_t type, id;
    int32_t quantity;
} dd_provisions_item;
typedef struct {
    uint32_t type;
    int32_t quantity;
} dd_provisions_currency;
typedef struct {
    uint32_t context_1838; /* Meaning deliberately unassigned until measured. */
    uint32_t stock_slots, bag_slots, currencies;
    uint32_t stock_occupied, bag_occupied;
    uint8_t panel_visible, stock_view, bag_view, reserved;
    dd_provisions_item stock[DD_PROVISIONS_STOCK_MAX], bag[DD_PROVISIONS_BAG_MAX];
    dd_provisions_currency wallet[DD_PROVISIONS_CURRENCY_MAX];
} dd_provisions_snapshot;
typedef enum {
    DD_PROVISIONS_OK = 0,
    DD_PROVISIONS_WAIT,       /* No fully loaded normal town yet. */
    DD_PROVISIONS_SLOT,       /* Not a campaign slot (0..8). No inventory read. */
    DD_PROVISIONS_OTHER_MODE, /* No inventory read. */
    DD_PROVISIONS_UNREADABLE,
    DD_PROVISIONS_LAYOUT,
    DD_PROVISIONS_CHANGED /* Root/header changed during capture; retry. */
} dd_provisions_result;
/* Caller verifies exact EXE hash BEFORE invoking this build-specific reader.
 * Output is zeroed on every failure. Confirm two consecutive equal snapshots for live logging.
 * Polling/header checks reduce torn observations, but do NOT constitute an atomic game snapshot. */
dd_provisions_result dd_provisions_capture(dd_provisions_read_fn read, void *ctx, uint64_t image,
                                           dd_provisions_snapshot *out);
/* Optional stage identifies the failed check without exposing addresses or raw memory.
 * Returned string is a static literal, also for argument errors; never supplied by the game. */
dd_provisions_result dd_provisions_capture_ex(dd_provisions_read_fn read, void *ctx, uint64_t image,
                                              dd_provisions_snapshot *out, const char **stage);
const char *dd_provisions_result_name(dd_provisions_result result);
#endif
