#include "lr_roster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DSON_HEADER 0x40

typedef struct {
    const uint8_t *buf;
    size_t len;
    const uint8_t *data;
    uint32_t data_len;
    const uint8_t *m1, *m2; /* object table (16 bytes per entry), field table (12 bytes per entry) */
    uint32_t n1, n2;
} dson;

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Is [off, off + n) inside a buffer of len bytes? */
static int in_range(size_t len, uint64_t off, uint64_t n) {
    return off <= len && n <= len - off;
}

static int dson_open(dson *d, const uint8_t *buf, size_t len) {
    memset(d, 0, sizeof *d);
    if (len < DSON_HEADER || get32(buf) != 0xB101u) return 0;
    d->n1 = get32(buf + 0x14);
    uint32_t m1_off = get32(buf + 0x18);
    d->n2 = get32(buf + 0x2c);
    uint32_t m2_off = get32(buf + 0x30);
    d->data_len = get32(buf + 0x38);
    uint32_t data_off = get32(buf + 0x3c);
    if (d->n1 == 0 || d->n2 == 0 || !in_range(len, m1_off, (uint64_t)d->n1 * 16) ||
        !in_range(len, m2_off, (uint64_t)d->n2 * 12) || !in_range(len, data_off, d->data_len))
        return 0;
    d->buf = buf;
    d->len = len;
    d->m1 = buf + m1_off;
    d->m2 = buf + m2_off;
    d->data = buf + data_off;
    return 1;
}

static uint32_t field_off(const dson *d, uint32_t f) { return get32(d->m2 + 12 * f + 4); }
static uint32_t field_info(const dson *d, uint32_t f) { return get32(d->m2 + 12 * f + 8); }
static int field_is_obj(const dson *d, uint32_t f) { return field_info(d, f) & 1; }
static uint32_t field_obj(const dson *d, uint32_t f) { return (field_info(d, f) >> 11) & 0xFFFFF; }

/* The field's name (NUL terminated in the data block); NULL if it does not fit. */
static const char *field_name(const dson *d, uint32_t f, uint32_t *name_len) {
    uint32_t off = field_off(d, f), n = (field_info(d, f) >> 2) & 0x1FF;
    if (n == 0 || !in_range(d->data_len, off, n) || d->data[off + n - 1] != 0) return NULL;
    *name_len = n;
    return (const char *)d->data + off;
}

/* The field's value bytes: from after its name up to the next field (fields are stored in data order).
 * Values of 4 bytes or more are 4-aligned relative to the data block. */
static int field_value(const dson *d, uint32_t f, const uint8_t **p, uint32_t *n) {
    uint32_t name_len;
    if (!field_name(d, f, &name_len)) return 0;
    uint64_t start = (uint64_t)field_off(d, f) + name_len;
    uint64_t end = f + 1 < d->n2 ? field_off(d, f + 1) : d->data_len;
    if (end < start || end > d->data_len) return 0;
    uint64_t pad = (4 - start % 4) % 4;
    if (end - start >= pad + 4) start += pad;
    *p = d->data + start;
    *n = (uint32_t)(end - start);
    return 1;
}

/* Field index of the direct child called name of object field f, or -1. */
static int64_t child(const dson *d, uint32_t f, const char *name) {
    if (f >= d->n2 || !field_is_obj(d, f)) return -1;
    uint32_t obj = field_obj(d, f);
    if (obj >= d->n1) return -1;
    uint32_t direct = get32(d->m1 + 16 * obj + 8);
    uint64_t i = (uint64_t)f + 1;
    for (uint32_t k = 0; k < direct && i < d->n2; k++) {
        uint32_t name_len;
        const char *nm = field_name(d, (uint32_t)i, &name_len);
        if (!nm) return -1;
        if (strcmp(nm, name) == 0) return (int64_t)i;
        if (field_is_obj(d, (uint32_t)i)) {
            uint32_t o = field_obj(d, (uint32_t)i);
            if (o >= d->n1) return -1;
            i += 1 + (uint64_t)get32(d->m1 + 16 * o + 12);
        } else {
            i++;
        }
    }
    return -1;
}

/* Field 0 is the root object; names[0] is its name. */
static int64_t path(const dson *d, const char *const *names, int count) {
    uint32_t name_len;
    const char *root = field_name(d, 0, &name_len);
    if (!root || strcmp(root, names[0]) != 0) return -1;
    int64_t f = 0;
    for (int i = 1; i < count && f >= 0; i++) f = child(d, (uint32_t)f, names[i]);
    return f;
}

static int value_int(const dson *d, int64_t f, int32_t *v) {
    const uint8_t *p;
    uint32_t n;
    if (f < 0 || field_is_obj(d, (uint32_t)f) || !field_value(d, (uint32_t)f, &p, &n) || n != 4) return 0;
    *v = (int32_t)get32(p);
    return 1;
}

/* A length-prefixed value: strings (length includes the NUL) and embedded files. */
static int value_blob(const dson *d, int64_t f, const uint8_t **out, uint32_t *out_len) {
    const uint8_t *p;
    uint32_t n;
    if (f < 0 || field_is_obj(d, (uint32_t)f) || !field_value(d, (uint32_t)f, &p, &n) || n < 4) return 0;
    uint32_t k = get32(p);
    if (k > n - 4) return 0;
    *out = p + 4;
    *out_len = k;
    return 1;
}

static void value_text(const dson *d, int64_t f, char *out, size_t cap) {
    const uint8_t *p;
    uint32_t n;
    out[0] = 0;
    if (!value_blob(d, f, &p, &n) || n == 0 || p[n - 1] != 0) return;
    size_t i = 0;
    for (; i + 1 < cap && i + 1 < n; i++) out[i] = (p[i] < 0x20 || p[i] == 0x7f) ? '?' : (char)p[i];
    out[i] = 0;
}

static int parse_id(const char *s, uint32_t *id) {
    uint64_t v = 0;
    if (!*s) return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (uint64_t)(*s - '0');
        if (v > 0xFFFFFFFFu) return 0;
    }
    *id = (uint32_t)v;
    return 1;
}

static int read_hero(const uint8_t *buf, uint32_t len, lr_hero *h) {
    dson d;
    if (!dson_open(&d, buf, len)) return 0;
    static const char *const actor_name[] = {"base_root", "actor", "name"};
    static const char *const cls[] = {"base_root", "heroClass"};
    static const char *const xp[] = {"base_root", "resolveXp"};
    static const char *const status[] = {"base_root", "roster.status"};
    value_text(&d, path(&d, actor_name, 3), h->name, sizeof h->name);
    value_text(&d, path(&d, cls, 2), h->cls, sizeof h->cls);
    if (!h->cls[0]) return 0;
    if (!value_int(&d, path(&d, xp, 2), &h->xp)) h->xp = 0;
    if (!value_int(&d, path(&d, status, 2), &h->status)) h->status = -1;
    return 1;
}

static int by_id(const void *a, const void *b) {
    uint32_t x = ((const lr_hero *)a)->id, y = ((const lr_hero *)b)->id;
    return (x > y) - (x < y);
}

int lr_roster_parse(const uint8_t *buf, size_t len, lr_hero *out, int cap) {
    dson d;
    if (!dson_open(&d, buf, len)) return -1;
    static const char *const heroes_path[] = {"base_root", "heroes"};
    int64_t heroes = path(&d, heroes_path, 2);
    if (heroes < 0 || !field_is_obj(&d, (uint32_t)heroes) || field_obj(&d, (uint32_t)heroes) >= d.n1) return -1;
    uint32_t obj = field_obj(&d, (uint32_t)heroes);
    uint32_t direct = get32(d.m1 + 16 * obj + 8);
    uint64_t i = (uint64_t)heroes + 1;
    int count = 0;
    for (uint32_t k = 0; k < direct && i < d.n2; k++) {
        uint32_t name_len, f = (uint32_t)i;
        const char *nm = field_name(&d, f, &name_len);
        if (!nm || !field_is_obj(&d, f) || field_obj(&d, f) >= d.n1) return -1;
        i += 1 + (uint64_t)get32(d.m1 + 16 * field_obj(&d, f) + 12);
        lr_hero h;
        memset(&h, 0, sizeof h);
        int64_t raw = child(&d, f, "hero_file_data");
        raw = raw >= 0 ? child(&d, (uint32_t)raw, "raw_data") : -1;
        const uint8_t *p;
        uint32_t n;
        if (count < cap && parse_id(nm, &h.id) && value_blob(&d, raw, &p, &n) && read_hero(p, n, &h))
            out[count++] = h;
    }
    qsort(out, (size_t)count, sizeof *out, by_id);
    return count;
}

int lr_hero_level(int32_t xp) {
    /* campaign/roster/roster.variables.json, resolve_level_thresholds */
    static const int32_t at[] = {2, 8, 14, 24, 36, 48};
    int level = 0;
    while (level < 6 && xp >= at[level]) level++;
    return level;
}

/* ---- persist.game.json ---- */

static int value_bool(const dson *d, int64_t f, int *v) {
    const uint8_t *p;
    uint32_t n;
    if (f < 0 || field_is_obj(d, (uint32_t)f) || !field_value(d, (uint32_t)f, &p, &n) || n < 1) return 0;
    *v = p[0] != 0;
    return 1;
}

static int dlc_name_ok(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_')) return 0;
    return 1;
}

static int has_text(const uint8_t *buf, size_t len, const char *s) {
    size_t n = strlen(s);
    for (size_t i = 0; i + n <= len; i++)
        if (memcmp(buf + i, s, n) == 0) return 1;
    return 0;
}

int lr_game_parse(const uint8_t *buf, size_t len, lr_game_info *out) {
    memset(out, 0, sizeof *out);
    size_t i = 0;
    while (i < len && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\r' || buf[i] == '\n')) i++;
    if (i < len && buf[i] == '{') {
        /* The game writes binary; text is the untouched template it copies into a new campaign's folder. */
        out->text = 1;
        out->tutorial = has_text(buf, len, "\"tutorial\"");
        return 1;
    }
    dson d;
    if (!dson_open(&d, buf, len)) return 0;
    static const char *const root[] = {"base_root"};
    static const char *const inraid[] = {"base_root", "inraid"};
    static const char *const dungeon[] = {"base_root", "raiddungeon"};
    static const char *const dlc[] = {"base_root", "dlc"};
    static const char *const estate[] = {"base_root", "estatename"};
    if (path(&d, root, 1) != 0) return 0;
    value_text(&d, path(&d, estate, 2), out->estate, sizeof out->estate);
    int raid = 0;
    char where[32];
    value_bool(&d, path(&d, inraid, 2), &raid);
    value_text(&d, path(&d, dungeon, 2), where, sizeof where);
    out->tutorial = raid && strcmp(where, "tutorial") == 0;
    int64_t list = path(&d, dlc, 2);
    if (list < 0 || !field_is_obj(&d, (uint32_t)list) || field_obj(&d, (uint32_t)list) >= d.n1) return 1;
    uint32_t direct = get32(d.m1 + 16 * field_obj(&d, (uint32_t)list) + 8);
    uint64_t f = (uint64_t)list + 1;
    for (uint32_t k = 0; k < direct && f < d.n2; k++) {
        uint32_t name_len;
        if (!field_name(&d, (uint32_t)f, &name_len)) return 1;
        uint64_t next = f + 1;
        if (field_is_obj(&d, (uint32_t)f)) {
            if (field_obj(&d, (uint32_t)f) >= d.n1) return 1;
            next += get32(d.m1 + 16 * field_obj(&d, (uint32_t)f) + 12);
            char name[LR_DLC_NAME_MAX + 1];
            value_text(&d, child(&d, (uint32_t)f, "name"), name, sizeof name);
            if (out->dlc_count < LR_MAX_DLC && dlc_name_ok(name))
                snprintf(out->dlc[out->dlc_count++], sizeof out->dlc[0], "%s", name);
        }
        f = next;
    }
    return 1;
}

const char *lr_dlc_title(const char *name) {
    /* The folder names under <game>\dlc are <Steam app id>_<name>; flagellant and districts are parts of
     * The Crimson Court (dlc\580100_crimson_court\features\...). */
    static const char *const titles[][2] = {
        {"musketeer", "The Musketeer"},           {"crimson_court", "The Crimson Court"},
        {"flagellant", "The Crimson Court"},      {"districts", "The Crimson Court"},
        {"shieldbreaker", "The Shieldbreaker"},   {"color_of_madness", "The Color of Madness"},
        {"arena_mp", "The Butcher's Circus"},
    };
    for (size_t i = 0; i < sizeof titles / sizeof *titles; i++)
        if (strcmp(name, titles[i][0]) == 0) return titles[i][1];
    return name;
}
