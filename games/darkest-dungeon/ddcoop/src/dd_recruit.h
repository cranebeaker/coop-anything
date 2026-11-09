/* Bounded, read-only normal Stagecoach and genuine live display validation. */
#ifndef DD_RECRUIT_H
#define DD_RECRUIT_H
#include <stddef.h>
#include <stdint.h>
#define DD_RECRUIT_MAX_CANDIDATES 64
typedef int (*dd_recruit_read_fn)(void *ctx, uint64_t address, void *out, size_t length);
typedef struct {
    dd_recruit_read_fn read;
    void *ctx;
    uint64_t image, campaign, store;
} dd_recruit;
typedef struct {
    /* entry: persistent candidate; hero: the same Entry resolved through its genuine UI row. */
    uint64_t entry, hero;
    uint32_t id, index;
    int available, enabled;
} dd_recruit_choice;
int dd_recruit_open(dd_recruit *out, dd_recruit_read_fn read, void *ctx, uint64_t image, uint64_t campaign);
int dd_recruit_candidate(const dd_recruit *s, uint32_t id, dd_recruit_choice *out);
int dd_recruit_panel_ready(const dd_recruit *s, uint64_t panel);
int dd_recruit_resolve(const dd_recruit *s, uint64_t panel, uint32_t id, dd_recruit_choice *out);
/* The campaign's live Hero pointer vector. Returns -1 for an invalid/unreadable vector. */
int dd_recruit_roster_has(const dd_recruit *s, uint32_t id, unsigned *count);
#endif
