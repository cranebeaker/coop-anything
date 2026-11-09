/* Test helper: writes files in Darkest Dungeon's binary save format (DSON), enough for a roster with
 * embedded hero files. Used by test_lr_roster.c and test_lr_core.c. */
#ifndef DSON_WRITER_H
#define DSON_WRITER_H

#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t data[16384];
    uint32_t dlen;
    uint32_t f_off[512], f_info[512];
    int nf;
    int32_t o[128][4]; /* parent, field, direct children, all children */
    int no;
    int stack[16], sp;
} dson_writer;

static void dw_put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void dw_field(dson_writer *w, const char *name, uint32_t flags) {
    uint32_t n = (uint32_t)strlen(name) + 1;
    w->f_off[w->nf] = w->dlen;
    w->f_info[w->nf] = flags | n << 2;
    w->nf++;
    memcpy(w->data + w->dlen, name, n);
    w->dlen += n;
    if (w->sp) w->o[w->stack[w->sp - 1]][2]++;
}

static void dw_align(dson_writer *w) {
    while (w->dlen % 4) w->data[w->dlen++] = 0;
}

static void dw_begin(dson_writer *w, const char *name) {
    int obj = w->no++;
    dw_field(w, name, 1u | (uint32_t)obj << 11);
    w->o[obj][0] = w->sp ? w->stack[w->sp - 1] : -1;
    w->o[obj][1] = w->nf - 1;
    w->o[obj][2] = w->o[obj][3] = 0;
    w->stack[w->sp++] = obj;
}

static void dw_end(dson_writer *w) {
    int obj = w->stack[--w->sp];
    w->o[obj][3] = w->nf - (w->o[obj][1] + 1);
}

static void dw_int(dson_writer *w, const char *name, int32_t v) {
    dw_field(w, name, 0);
    dw_align(w);
    dw_put32(w->data + w->dlen, (uint32_t)v);
    w->dlen += 4;
}

static void dw_blob(dson_writer *w, const char *name, const void *p, uint32_t n) {
    dw_field(w, name, 0);
    dw_align(w);
    dw_put32(w->data + w->dlen, n);
    memcpy(w->data + w->dlen + 4, p, n);
    w->dlen += 4 + n;
}

static void dw_str(dson_writer *w, const char *name, const char *s) { dw_blob(w, name, s, (uint32_t)strlen(s) + 1); }

static void dw_bool(dson_writer *w, const char *name, int v) {
    dw_field(w, name, 0);
    w->data[w->dlen++] = (uint8_t)v;
}

static size_t dw_finish(const dson_writer *w, uint8_t *out) {
    uint32_t m1 = 0x40, m2 = m1 + 16u * (uint32_t)w->no, data = m2 + 12u * (uint32_t)w->nf;
    memset(out, 0, data);
    dw_put32(out, 0xB101);
    dw_put32(out + 0x08, 0x40);
    dw_put32(out + 0x10, 16u * (uint32_t)w->no);
    dw_put32(out + 0x14, (uint32_t)w->no);
    dw_put32(out + 0x18, m1);
    dw_put32(out + 0x2c, (uint32_t)w->nf);
    dw_put32(out + 0x30, m2);
    dw_put32(out + 0x38, w->dlen);
    dw_put32(out + 0x3c, data);
    for (int i = 0; i < w->no; i++)
        for (int k = 0; k < 4; k++) dw_put32(out + m1 + 16 * i + 4 * k, (uint32_t)w->o[i][k]);
    for (int i = 0; i < w->nf; i++) {
        dw_put32(out + m2 + 12 * i, 0x12345678u); /* name hash: the reader does not use it */
        dw_put32(out + m2 + 12 * i + 4, w->f_off[i]);
        dw_put32(out + m2 + 12 * i + 8, w->f_info[i]);
    }
    memcpy(out + data, w->data, w->dlen);
    return data + w->dlen;
}

/* One hero's embedded file (roster.status, actor.name, heroClass, resolveXp, plus fields the reader skips). */
static size_t dw_hero_file(uint8_t *out, const char *name, const char *cls, int32_t xp, int32_t status) {
    static dson_writer w;
    memset(&w, 0, sizeof w);
    dw_begin(&w, "base_root");
    dw_int(&w, "roster.status", status);
    dw_str(&w, "roster.building_name", "");
    dw_begin(&w, "actor");
    dw_str(&w, "name", name);
    dw_bool(&w, "combat_ready", 0);
    dw_begin(&w, "buff_group");
    dw_end(&w);
    dw_end(&w);
    dw_str(&w, "heroClass", cls);
    dw_int(&w, "resolveXp", xp);
    dw_bool(&w, "visited_deaths_door", 1);
    dw_end(&w);
    return dw_finish(&w, out);
}

typedef struct {
    const char *id; /* the key under "heroes"; not numeric = skipped by the reader */
    const char *name, *cls;
    int32_t xp, status;
} dw_hero;

/* A persist.roster.json with these heroes, in the given order. out must hold 64 KB. */
static size_t dw_roster(uint8_t *out, const dw_hero *heroes, int count) {
    static dson_writer w;
    static uint8_t h[4096];
    memset(&w, 0, sizeof w);
    dw_begin(&w, "base_root");
    dw_int(&w, "version", 513);
    dw_begin(&w, "heroes");
    for (int i = 0; i < count; i++) {
        size_t n = dw_hero_file(h, heroes[i].name, heroes[i].cls, heroes[i].xp, heroes[i].status);
        dw_begin(&w, heroes[i].id);
        dw_begin(&w, "hero_file_data");
        dw_blob(&w, "raw_data", h, (uint32_t)n);
        dw_end(&w);
        dw_end(&w);
    }
    dw_end(&w);
    dw_int(&w, "dismissed_hero_count", 1);
    dw_end(&w);
    return dw_finish(&w, out);
}

/* A persist.game.json shaped like the game's: inraid, raiddungeon, presented_dlc (not checked), dlc. */
static size_t dw_game(uint8_t *out, int inraid, const char *dungeon, const char *const *dlc, int dlc_count) {
    static dson_writer w;
    memset(&w, 0, sizeof w);
    dw_begin(&w, "base_root");
    dw_int(&w, "version", 2);
    dw_int(&w, "totalelapsed", 0);
    dw_bool(&w, "inraid", inraid);
    dw_str(&w, "raiddungeon", dungeon);
    dw_str(&w, "estatename", "Darkest");
    dw_begin(&w, "presented_dlc");
    dw_begin(&w, "dlc");
    dw_begin(&w, "0");
    dw_str(&w, "name", "arena_mp");
    dw_end(&w);
    dw_end(&w);
    dw_end(&w);
    dw_bool(&w, "dlc_init", 1);
    dw_begin(&w, "dlc");
    for (int i = 0; i < dlc_count; i++) {
        char key[8];
        key[0] = (char)('0' + i % 10);
        key[1] = 0;
        dw_begin(&w, key);
        dw_str(&w, "name", dlc[i]);
        dw_int(&w, "source", 4);
        dw_end(&w);
    }
    dw_end(&w);
    dw_end(&w);
    return dw_finish(&w, out);
}

#endif
