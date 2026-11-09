#include "dd_recruit.h"
#include <string.h>

static int rd(const dd_recruit *s, uint64_t at, void *out, size_t n) {
    return at && at <= UINT64_MAX - n && s->read(s->ctx, at, out, n);
}
static int ptr(const dd_recruit *s, uint64_t at, uint64_t *out) { return rd(s, at, out, 8); }
static int vec(const dd_recruit *s, uint64_t at, unsigned stride, unsigned cap, uint64_t *begin,
               unsigned *count) {
    uint64_t end;
    if (!ptr(s, at, begin) || !ptr(s, at + 8, &end) || end < *begin || (end - *begin) % stride ||
        (end - *begin) / stride > cap || (!*begin && end))
        return 0;
    *count = (unsigned)((end - *begin) / stride);
    return 1;
}
int dd_recruit_open(dd_recruit *s, dd_recruit_read_fn read, void *ctx, uint64_t image, uint64_t campaign) {
    memset(s, 0, sizeof *s);
    s->read = read;
    s->ctx = ctx;
    s->image = image;
    s->campaign = campaign;
    uint8_t alternate = 0, nil = 0;
    uint64_t head = 0, root = 0, coach = 0;
    if (!read || !campaign || !rd(s, image + 0x2a6d4bb, &alternate, 1) || alternate ||
        !ptr(s, campaign + 0x11f8, &head) || !head || !rd(s, head + 0x19, &nil, 1) || !nil ||
        !ptr(s, head + 8, &root))
        return 0;
    uint64_t pending[128];
    unsigned n = 0, seen = 0;
    if (root != head) pending[n++] = root;
    while (n) {
        uint64_t node = pending[--n], left, right, model, vt;
        if (++seen > 128 || !node || node == head || !rd(s, node + 0x19, &nil, 1) || nil ||
            !ptr(s, node, &left) || !ptr(s, node + 0x10, &right) || !ptr(s, node + 0x28, &model) || !model ||
            !ptr(s, model, &vt))
            return 0;
        if (vt == image + 0xe9d088) {
            if (coach) return 0;
            coach = model;
        }
        if (left != head) {
            if (n == 128) return 0;
            pending[n++] = left;
        }
        if (right != head) {
            if (n == 128) return 0;
            pending[n++] = right;
        }
    }
    uint64_t children;
    unsigned count;
    if (!coach || !vec(s, coach + 0xa8, 8, 64, &children, &count)) return 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t child, vt;
        if (!ptr(s, children + i * 8, &child)) return 0;
        if (!child) continue;
        if (!ptr(s, child, &vt)) return 0;
        if (vt == image + 0xe9cfd0) {
            if (s->store) return 0;
            s->store = child;
        }
    }
    uint64_t entries;
    return s->store && vec(s, s->store + 0x68, 0x15b0, DD_RECRUIT_MAX_CANDIDATES, &entries, &count);
}
int dd_recruit_candidate(const dd_recruit *s, uint32_t id, dd_recruit_choice *out) {
    uint64_t begin;
    unsigned count;
    int found = 0;
    memset(out, 0, sizeof *out);
    if (!id || !s->store || !vec(s, s->store + 0x68, 0x15b0, DD_RECRUIT_MAX_CANDIDATES, &begin, &count))
        return 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t entry = begin + (uint64_t)i * 0x15b0;
        uint32_t key;
        uint8_t available;
        if (!rd(s, entry + 0x137c, &key, 4) || !rd(s, entry + 0x15a8, &available, 1) || available > 1)
            return 0;
        if (key == id) {
            if (found) return 0;
            found = 1;
            out->id = id;
            out->entry = entry;
            out->available = available;
        }
    }
    return found;
}
int dd_recruit_panel_ready(const dd_recruit *s, uint64_t panel) {
    uint64_t vt = 0, store = 0, begin;
    unsigned count;
    return panel && ptr(s, panel, &vt) && vt == s->image + 0xec0cb0 && ptr(s, panel + 0x80, &store) &&
           store == s->store && vec(s, panel + 0x350, 16, DD_RECRUIT_MAX_CANDIDATES, &begin, &count) &&
           count > 0;
}
int dd_recruit_resolve(const dd_recruit *s, uint64_t panel, uint32_t id, dd_recruit_choice *out) {
    if (!dd_recruit_candidate(s, id, out) || !out->available || !dd_recruit_panel_ready(s, panel)) return 0;
    uint64_t begin;
    unsigned count;
    if (!vec(s, panel + 0x350, 16, DD_RECRUIT_MAX_CANDIDATES, &begin, &count)) return 0;
    int found = 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t row, vt, controller, hero = 0;
        uint32_t key = 0, enabled = 0;
        if (!ptr(s, begin + i * 16, &row)) return 0;
        if (!row) continue;
        if (!ptr(s, row, &vt) || vt != s->image + 0xebcc00 || !ptr(s, row + 0x158, &controller)) return 0;
        if (!controller) continue;
        if (!ptr(s, controller + 0x28, &hero)) return 0;
        if (!hero) continue;
        if (!rd(s, hero + 0x137c, &key, 4)) return 0;
        if (key != id) continue;
        if (found || hero != out->entry || !ptr(s, controller, &vt) || vt != s->image + 0xee0990 ||
            !rd(s, row + 0x58, &enabled, 4))
            return 0;
        found = 1;
        out->hero = hero;
        out->index = i;
        out->enabled = enabled != 0;
    }
    return found;
}
int dd_recruit_roster_has(const dd_recruit *s, uint32_t id, unsigned *count) {
    uint64_t begin;
    int found = 0;
    if (!vec(s, s->campaign + 0x20, 8, 64, &begin, count)) return -1;
    for (unsigned i = 0; i < *count; i++) {
        uint64_t hero;
        uint32_t key;
        /* Live roster wrapper embeds the Entry at +8; its ID is +0x1384, not Entry+0x137c. */
        if (!ptr(s, begin + i * 8, &hero) || !hero || !rd(s, hero + 0x1384, &key, 4)) return -1;
        if (key == id) found = 1;
    }
    return found;
}
