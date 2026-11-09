#include "lr_snapshot.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SNAP_MAX (LR_MAX_SAVE_TOTAL + LR_MAX_FILES * (LR_SNAPSHOT_NAME_MAX + 13u) + 21u + LR_NAME_MAX)
int lr_slot_folder_ok(const char *folder, int *slot) {
    if (!folder) return 0;
    if (!strncmp(folder, "steam_", 6)) folder += 6;
    if (strncmp(folder, "profile_", 8) || folder[8] < '0' || folder[8] > '8' || folder[9]) return 0;
    if (slot) *slot = folder[8] - '0' + 1;
    return 1;
}
int lr_snapshot_name_ok(const char *name) {
    if (!strncmp(name, "backup/", 7)) return lr_file_name_ok(name + 7);
    return lr_file_name_ok(name);
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t x) {
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(x >> (8 * i));
}
static int same_name(const char *a, const char *b) {
    for (;; a++, b++) {
        unsigned x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
        if (!x) return 1;
    }
}
static const lr_snapshot_file *find(const lr_snapshot *s, const char *name) {
    for (uint32_t i = 0; i < s->count; i++)
        if (same_name(s->files[i].name, name)) return &s->files[i];
    return NULL;
}
void lr_snapshot_free(lr_snapshot *s) {
    for (uint32_t i = 0; i < s->count; i++)
        free(s->files[i].data);
    memset(s, 0, sizeof *s);
}
int lr_snapshot_add(lr_snapshot *s, const char *name, const uint8_t *data, size_t len) {
    if (!lr_snapshot_name_ok(name) || s->count >= LR_MAX_FILES || len > LR_MAX_FILE_SIZE ||
        len > LR_MAX_SAVE_TOTAL - s->total || find(s, name) || (len && !data))
        return 0;
    uint8_t *copy = malloc(len ? len : 1);
    if (!copy) return 0;
    if (len) memcpy(copy, data, len);
    lr_snapshot_file *f = &s->files[s->count++];
    snprintf(f->name, sizeof f->name, "%s", name);
    f->data = copy;
    f->size = (uint32_t)len;
    f->crc = lr_crc32(0, data, len);
    s->total += (uint32_t)len;
    return 1;
}
typedef struct {
    lr_snapshot *s;
    const char *dir;
    const char *prefix;
    int ok;
} dir_ctx;
static int from_file(void *ctx, const char *name) {
    dir_ctx *d = ctx;
    char path[LR_PATH_MAX];
    uint8_t *b = NULL;
    size_t n = 0;
    lr_join(path, sizeof path, d->dir, name);
    char logical[LR_SNAPSHOT_NAME_MAX + 1];
    d->ok = snprintf(logical, sizeof logical, "%s%s", d->prefix, name) < (int)sizeof logical;
    d->ok = d->ok && lr_read_file(path, LR_MAX_FILE_SIZE, &b, &n) && lr_snapshot_add(d->s, logical, b, n);
    free(b);
    return d->ok;
}
int lr_snapshot_from_dir(lr_snapshot *s, uint64_t user, const char *dir) {
    memset(s, 0, sizeof *s);
    s->user = user;
    if (!dir) return 1;
    dir_ctx d = {s, dir, "", 1};
    if (lr_list_files(dir, from_file, &d) < 0 || !d.ok) {
        lr_snapshot_free(s);
        return 0;
    }
    char backup[LR_PATH_MAX];
    lr_join(backup, sizeof backup, dir, "backup");
    if (lr_dir_exists(backup)) {
        d.dir = backup;
        d.prefix = "backup/";
        if (lr_list_files(backup, from_file, &d) < 0 || !d.ok) {
            lr_snapshot_free(s);
            return 0;
        }
    }
    return 1;
}
int lr_snapshot_read(const char *path, lr_snapshot *s) {
    uint8_t *b = NULL;
    size_t n = 0, pos = 20;
    memset(s, 0, sizeof *s);
    if (!lr_read_file(path, SNAP_MAX, &b, &n)) return 0;
    /* A recovery file of an older kit can still be waiting: those were all slot 3. */
    int old = n >= 20 && !memcmp(b, "DDCSAVE1", 8);
    int ok = old || (n >= 21 && !memcmp(b, "DDCSAVE2", 8));
    uint32_t count = ok ? get32(b + 16) : 0;
    if (ok) {
        s->user = (uint64_t)get32(b + 8) | (uint64_t)get32(b + 12) << 32;
        ok = count <= LR_MAX_FILES;
    }
    if (ok && old) {
        snprintf(s->folder, sizeof s->folder, "profile_2");
    } else if (ok) {
        unsigned fl = b[pos++];
        ok = fl && fl <= LR_NAME_MAX && fl <= n - pos && !memchr(b + pos, 0, fl);
        if (ok) {
            memcpy(s->folder, b + pos, fl);
            s->folder[fl] = 0;
            pos += fl;
            ok = lr_slot_folder_ok(s->folder, NULL);
        }
    }
    for (uint32_t i = 0; ok && i < count; i++) {
        if (pos > n || n - pos < 9) {
            ok = 0;
            break;
        }
        unsigned nl = b[pos++];
        uint32_t len = get32(b + pos), crc = get32(b + pos + 4);
        pos += 8;
        if (!nl || nl > LR_SNAPSHOT_NAME_MAX || nl > n - pos || len > n - pos - nl || memchr(b + pos, 0, nl)) {
            ok = 0;
            break;
        }
        char name[LR_SNAPSHOT_NAME_MAX + 1];
        memcpy(name, b + pos, nl);
        name[nl] = 0;
        pos += nl;
        ok = lr_crc32(0, b + pos, len) == crc && lr_snapshot_add(s, name, b + pos, len);
        pos += len;
    }
    ok = ok && pos == n && s->count == count;
    free(b);
    if (!ok) lr_snapshot_free(s);
    return ok;
}
static int equal(const lr_snapshot *a, const lr_snapshot *b) {
    if (a->count != b->count) return 0;
    for (uint32_t i = 0; i < a->count; i++) {
        const lr_snapshot_file *x = &a->files[i], *y = find(b, x->name);
        if (!y || x->size != y->size || memcmp(x->data, y->data, x->size)) return 0;
    }
    return 1;
}
int lr_snapshot_write(const char *path, const lr_snapshot *s) {
    size_t fl = strlen(s->folder), n = 21 + fl, pos = 21 + fl;
    if (!lr_slot_folder_ok(s->folder, NULL) || s->count > LR_MAX_FILES || s->total > LR_MAX_SAVE_TOTAL) return 0;
    for (uint32_t i = 0; i < s->count; i++)
        n += 9 + strlen(s->files[i].name) + s->files[i].size;
    if (n > SNAP_MAX) return 0;
    uint8_t *b = malloc(n);
    if (!b) return 0;
    memcpy(b, "DDCSAVE2", 8);
    put32(b + 8, (uint32_t)s->user);
    put32(b + 12, (uint32_t)(s->user >> 32));
    put32(b + 16, s->count);
    b[20] = (uint8_t)fl;
    memcpy(b + 21, s->folder, fl);
    for (uint32_t i = 0; i < s->count; i++) {
        const lr_snapshot_file *f = &s->files[i];
        size_t nl = strlen(f->name);
        b[pos++] = (uint8_t)nl;
        put32(b + pos, f->size);
        put32(b + pos + 4, lr_crc32(0, f->data, f->size));
        pos += 8;
        memcpy(b + pos, f->name, nl);
        pos += nl;
        memcpy(b + pos, f->data, f->size);
        pos += f->size;
    }
    int ok = lr_write_file(path, b, n) && lr_sync_file(path);
    free(b);
    lr_snapshot *check = calloc(1, sizeof *check);
    if (!check) return 0;
    ok = ok && lr_snapshot_read(path, check) && check->user == s->user && !strcmp(check->folder, s->folder) && equal(s, check);
    lr_snapshot_free(check);
    free(check);
    return ok;
}
static void say(const lr_snapshot_io *io, const char *fmt, ...) {
    if (!io->log) return;
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    io->log(io->ctx, line);
}
typedef struct {
    const lr_snapshot_io *io;
    lr_snapshot *s;
    int ok;
} read_ctx;
static int read_one(void *ctx, const char *name) {
    read_ctx *r = ctx;
    uint8_t *b = NULL;
    size_t n = 0;
    r->ok = lr_snapshot_name_ok(name) && r->io->read(r->io->ctx, name, &b, &n) && lr_snapshot_add(r->s, name, b, n);
    free(b);
    return r->ok;
}
static int read_all(const lr_snapshot_io *io, lr_snapshot *s) {
    read_ctx r = {io, s, 1};
    return io->list(io->ctx, read_one, &r) >= 0 && r.ok;
}
static int import(const lr_snapshot_io *io, const lr_snapshot *want, const lr_snapshot *old, int force) {
    for (uint32_t i = 0; i < want->count; i++) {
        const lr_snapshot_file *f = &want->files[i], *o = find(old, f->name);
        if (!force && o && f->size == o->size && !memcmp(f->data, o->data, f->size)) continue;
        if (!io->write(io->ctx, f->name, f->data, f->size)) return 0;
        say(io, "Steam storage wrote %s: %u bytes", f->name, f->size);
    }
    for (uint32_t i = 0; i < old->count; i++) {
        const char *name = old->files[i].name;
        if (!find(want, name) && !io->remove(io->ctx, name)) return 0;
    }
    return 1;
}
int lr_snapshot_apply(const lr_snapshot *want, const lr_snapshot_io *io) { return lr_snapshot_apply_ex(want, io, NULL); }
int lr_snapshot_apply_ex(const lr_snapshot *want, const lr_snapshot_io *io, int *unread) {
    if (unread) *unread = 0;
    lr_snapshot *before = calloc(1, sizeof *before), *after = calloc(1, sizeof *after);
    if (!before || !after) {
        free(before);
        free(after);
        return 0;
    }
    int ok = read_all(io, before);
    if (!ok) {
        if (unread) *unread = 1;
        say(io, "Steam storage could not read the original slot; no writes attempted");
        goto done;
    }
    if (equal(want, before)) {
        say(io, "Steam storage already matches: %u files, verified byte for byte", want->count);
        goto done;
    }
    if (!io->backup(io->ctx, before)) {
        ok = 0;
        say(io, "Steam storage backup failed; no writes attempted");
        goto done;
    }
    ok = import(io, want, before, 0) && read_all(io, after) && equal(want, after);
    if (ok) {
        say(io, "Steam storage verified: %u files, %u bytes; %s ready", want->count, want->total, want->folder);
        goto done;
    }
    say(io, "Steam storage import failed; restoring the original slot");
    lr_snapshot_free(after);
    /* If listing the failed result fails, include every possible new file in the cleanup table. */
    if (!read_all(io, after)) {
        lr_snapshot_free(after);
        for (uint32_t i = 0; i < want->count; i++)
            lr_snapshot_add(after, want->files[i].name, want->files[i].data, want->files[i].size);
    }
    int restored = import(io, before, after, 1);
    lr_snapshot_free(after);
    restored = restored && read_all(io, after) && equal(before, after);
    say(io, restored ? "Steam storage rollback verified"
                     : "Steam storage ROLLBACK FAILED; recovery bundle retained; do not launch the game");
done:
    lr_snapshot_free(before);
    lr_snapshot_free(after);
    free(before);
    free(after);
    return ok;
}
