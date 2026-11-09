#include "lr_owners.h"

#include <stdio.h>
#include <string.h>

void lr_owners_init(lr_owners *o) { memset(o, 0, sizeof *o); }

int lr_owners_find(const lr_owners *o, uint32_t id) {
    for (int i = 0; i < o->count; i++)
        if (o->heroes[i].id == id) return i;
    return -1;
}

int lr_owners_display_order(const lr_owners *o,int order[LR_MAX_HEROES]) {
    if(o->count<0 || o->count>LR_MAX_HEROES)return -1;
    int party=0,n=0;
    for(int i=0;i<o->count;i++) {order[i]=i;party+=o->heroes[i].status==0;}
    if(party>4)return -1;
    for(int pass=0;pass<2;pass++)
        for(int i=0;i<o->count;i++)
            if((o->heroes[i].status==0)==(pass==0))order[n++]=i;
    return party;
}
int lr_owners_live_order(const lr_owners *o, const uint32_t ids[4], int count,
                         int order[LR_MAX_HEROES], uint8_t flags[LR_MAX_HEROES]) {
    memset(flags, 0, LR_MAX_HEROES);
    if (o->count < 0 || o->count > LR_MAX_HEROES) return -1;
    for (int i = 0; i < o->count; i++) order[i] = i;
    if (count < 0 || count > 4 || (count && !ids)) return -1;
    int ranks[4] = {0};
    for (int r = 0; r < count; r++) {
        ranks[r] = lr_owners_find(o, ids[r]);
        if (!ids[r] || ranks[r] < 0) return -1;
        for (int earlier = 0; earlier < r; earlier++) if (ranks[earlier] == ranks[r]) return -1;
    }
    int n = 0;
    for (int r = 0; r < count; r++) { flags[ranks[r]] = 1; order[n++] = ranks[r]; }
    for (int i = 0; i < o->count; i++) if (!flags[i]) order[n++] = i;
    return count;
}

static int name_ok(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        if ((uint8_t)s[i] < 0x20 || s[i] == 0x7f) return 0;
    return 1;
}

static int class_ok(const char *s, size_t n) {
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_')) return 0;
    return 1;
}

/* Makes a hero from the save safe to send: names without control characters, classes [a-z0-9_]. */
static void clean(lr_hero *h) {
    h->name[sizeof h->name - 1] = 0;
    h->cls[sizeof h->cls - 1] = 0;
    for (char *c = h->name; *c; c++)
        if ((uint8_t)*c < 0x20 || *c == 0x7f) *c = '?';
    for (char *c = h->cls; *c; c++)
        if (!class_ok(c, 1)) *c = '_';
    if (!h->cls[0]) snprintf(h->cls, sizeof h->cls, "unknown");
    if (h->status < -1 || h->status > 2) h->status = -1;
    if (h->xp < 0) h->xp = 0;
    if (h->xp > 0xFFFF) h->xp = 0xFFFF;
}

int lr_owners_set_roster(lr_owners *o, const lr_hero *heroes, int count) {
    lr_owners next;
    memset(&next, 0, sizeof next);
    for (int i = 0; i < count && next.count < LR_MAX_HEROES; i++) {
        if (i > 0 && heroes[i].id <= heroes[i - 1].id) continue; /* lr_roster_parse sorts; skip duplicates */
        lr_hero h = heroes[i];
        clean(&h);
        int old = lr_owners_find(o, h.id);
        next.heroes[next.count] = h;
        next.owner[next.count] = old >= 0 ? o->owner[old] : LR_PLAYER_HOST;
        next.count++;
    }
    int changed = next.count != o->count || memcmp(next.heroes, o->heroes, sizeof next.heroes[0] * (size_t)next.count) ||
                  memcmp(next.owner, o->owner, (size_t)next.count) || o->revision == 0;
    next.revision = o->revision + (changed ? 1 : 0);
    *o = next;
    return changed;
}

int lr_owners_claim(lr_owners *o, uint32_t id, int player, int claim) {
    int i = lr_owners_find(o, id);
    if (i < 0 || player < 0 || player >= LR_MAX_PLAYERS) return 0;
    int to;
    if (claim) {
        to = player;
    } else {
        if (o->owner[i] != player) return 0;
        to = player == LR_PLAYER_HOST ? LR_PLAYER_GUEST : LR_PLAYER_HOST;
    }
    if (o->owner[i] == to) return 0;
    o->owner[i] = (uint8_t)to;
    o->revision++;
    return 1;
}

int lr_owners_apply_ini(lr_owners *o, const char *text, size_t len) {
    int changed = 0;
    size_t i = 0;
    while (i < len) {
        size_t end = i;
        while (end < len && text[end] != '\n') end++;
        /* hero.<id>=<player>, nothing else on the line (a trailing \r is fine) */
        const char *line = text + i;
        size_t ll = end - i;
        if (ll && line[ll - 1] == '\r') ll--;
        if (ll > 7 && memcmp(line, "hero.", 5) == 0) {
            size_t k = 5;
            uint64_t id = 0;
            while (k < ll && k < 15 && line[k] >= '0' && line[k] <= '9') id = id * 10 + (uint64_t)(line[k++] - '0');
            int player = k + 2 == ll && k > 5 && line[k] == '=' ? line[k + 1] - '0' : -1;
            int idx = id <= 0xFFFFFFFFu ? lr_owners_find(o, (uint32_t)id) : -1;
            if (player >= 0 && player < LR_MAX_PLAYERS && idx >= 0 && o->owner[idx] != player) {
                o->owner[idx] = (uint8_t)player;
                changed = 1;
            }
        }
        i = end + 1;
    }
    if (changed) o->revision++;
    return changed;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Body layout: count (1 byte), then per hero in ascending id order:
 *   id (4), owner (1), status + 1 (1), resolve xp (2), name length (1), name, class length (1), class */
size_t lr_owners_encode(const lr_owners *o, uint8_t *out, size_t cap) {
    if (o->count < 0 || o->count > LR_MAX_HEROES || cap < 1) return 0;
    size_t n = 0;
    out[n++] = (uint8_t)o->count;
    for (int i = 0; i < o->count; i++) {
        const lr_hero *h = &o->heroes[i];
        size_t name = strlen(h->name), cls = strlen(h->cls);
        if (n + 10 + name + cls > cap || name > LR_HERO_NAME_MAX || cls > LR_HERO_CLASS_MAX) return 0;
        put32(out + n, h->id);
        out[n + 4] = o->owner[i];
        out[n + 5] = (uint8_t)(h->status + 1);
        out[n + 6] = (uint8_t)h->xp;
        out[n + 7] = (uint8_t)(h->xp >> 8);
        out[n + 8] = (uint8_t)name;
        memcpy(out + n + 9, h->name, name);
        n += 9 + name;
        out[n++] = (uint8_t)cls;
        memcpy(out + n, h->cls, cls);
        n += cls;
    }
    return n;
}

int lr_owners_decode(const uint8_t *buf, size_t len, uint32_t revision, lr_owners *out) {
    static lr_owners t; /* ~6 KB; the launcher is single threaded */
    memset(&t, 0, sizeof t);
    if (len < 1 || buf[0] > LR_MAX_HEROES) return 0;
    t.count = buf[0];
    t.revision = revision;
    size_t n = 1;
    for (int i = 0; i < t.count; i++) {
        if (len - n < 10) return 0;
        lr_hero *h = &t.heroes[i];
        h->id = get32(buf + n);
        uint8_t owner = buf[n + 4], status = buf[n + 5];
        h->xp = (int32_t)(buf[n + 6] | buf[n + 7] << 8);
        size_t name = buf[n + 8];
        if (owner >= LR_MAX_PLAYERS || status > 3 || name > LR_HERO_NAME_MAX || len - n - 9 < name + 1) return 0;
        if (i > 0 && h->id <= t.heroes[i - 1].id) return 0;
        if (!name_ok((const char *)buf + n + 9, name)) return 0;
        memcpy(h->name, buf + n + 9, name);
        n += 9 + name;
        size_t cls = buf[n++];
        if (cls > LR_HERO_CLASS_MAX || len - n < cls || !class_ok((const char *)buf + n, cls)) return 0;
        memcpy(h->cls, buf + n, cls);
        n += cls;
        h->status = (int32_t)status - 1;
        t.owner[i] = owner;
    }
    if (n != len) return 0;
    *out = t;
    return 1;
}

size_t lr_owners_ini(const lr_owners *o, int writer, char *out, size_t cap) {
    size_t n = 0;
    int w = snprintf(out, cap,
                     "; DDCoop hero ownership. Written by DDCoop.exe, read by the runtime at embark.\r\n"
                     "; hero.<roster id>=<player>: 0 = host, 1 = guest. A hero that is not listed belongs to the host.\r\n"
                     "version=1\r\nwritten_by=%s\r\nrevision=%u\r\n",
                     writer == LR_PLAYER_HOST ? "host" : "guest", (unsigned)o->revision);
    if (w < 0 || (size_t)w >= cap) return 0;
    n = (size_t)w;
    for (int i = 0; i < o->count; i++) {
        w = snprintf(out + n, cap - n, "; %s, %s\r\nhero.%u=%u\r\n", o->heroes[i].name, o->heroes[i].cls,
                     (unsigned)o->heroes[i].id, (unsigned)o->owner[i]);
        if (w < 0 || (size_t)w >= cap - n) return 0;
        n += (size_t)w;
    }
    return n;
}

int lr_owners_ini_from_host(const char *text, size_t len) {
    static const char key[] = "\nwritten_by=host\r";
    size_t k = sizeof key - 1;
    for (size_t i = 0; i + k <= len; i++)
        if (memcmp(text + i, key, k) == 0) return 1;
    return 0;
}
