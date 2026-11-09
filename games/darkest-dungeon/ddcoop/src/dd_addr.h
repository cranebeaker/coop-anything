/* Game addresses: the expected location of every mapped function plus the bytes found there in the
 * build the mod was made for. dd_addr_resolve() checks each one against the running game's code and,
 * when a game update moved a function, finds it again by its bytes - but only if they occur exactly
 * once. Anything unresolved keeps the mod off. Platform independent (works on a memory buffer), so it
 * is unit tested on Linux (tests/test_addr.c). */
#ifndef DD_ADDR_H
#define DD_ADDR_H

#include <stddef.h>
#include <stdint.h>

#define DD_IMAGE_BASE 0x140000000ULL
#define DD_PATTERN_LEN 48

typedef enum {
    DD_ADDR_UNRESOLVED = 0,
    DD_ADDR_EXACT,     /* bytes match at the expected address */
    DD_ADDR_RELOCATED, /* found exactly once elsewhere in the code */
    DD_ADDR_MISSING,   /* not found */
    DD_ADDR_AMBIGUOUS, /* found more than once: refuse to guess */
} dd_addr_status;

typedef struct {
    const char *name;
    uint64_t va;                      /* address in the build the mod was made for */
    uint8_t pattern[DD_PATTERN_LEN];  /* bytes at that address in that build */
    uint64_t resolved_va;             /* where it is in the running game (0 if unresolved) */
    dd_addr_status status;
} dd_addr;

/* The table generated from Darkest.exe (src/addresses.inc). */
extern dd_addr dd_addresses[];
extern const size_t dd_address_count;

/* Resolves every entry against a copy of the game's code.
 *   code      the code bytes, as mapped in memory
 *   code_va   the virtual address of code[0] in the build's address space (DD_IMAGE_BASE + section RVA)
 * Returns the number of entries that are EXACT or RELOCATED. */
size_t dd_addr_resolve(dd_addr *table, size_t count, const uint8_t *code, size_t code_size, uint64_t code_va);

/* Looks an entry up by name; NULL if it does not exist or did not resolve. */
const dd_addr *dd_addr_find(const dd_addr *table, size_t count, const char *name);

const char *dd_addr_status_name(dd_addr_status s);

#endif
