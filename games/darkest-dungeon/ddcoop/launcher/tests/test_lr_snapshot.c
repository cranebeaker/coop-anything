/* Failure injection for the exact storage path used by prepare/restore. No Steam or game required. */
#include "lr_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #x);                                                                \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)
typedef struct {
    lr_snapshot state, backup;
    int writes, removes, backups, fail_write, fail_remove, fail_backup, fail_read, corrupt_write;
    char last_log[256];
} mock;
static int index_of(mock *m, const char *name) {
    for (uint32_t i = 0; i < m->state.count; i++)
        if (!strcmp(m->state.files[i].name, name)) return (int)i;
    return -1;
}
static int list(void *ctx, int (*visit)(void *, const char *), void *vctx) {
    mock *m = ctx;
    for (uint32_t i = 0; i < m->state.count; i++)
        if (!visit(vctx, m->state.files[i].name)) return -1;
    return (int)m->state.count;
}
static int read_(void *ctx, const char *name, uint8_t **out, size_t *len) {
    mock *m = ctx;
    if (m->fail_read) {
        m->fail_read = 0;
        return 0;
    }
    int i = index_of(m, name);
    if (i < 0) return 0;
    lr_snapshot_file *f = &m->state.files[i];
    *out = malloc(f->size ? f->size : 1);
    if (!*out) return 0;
    memcpy(*out, f->data, f->size);
    *len = f->size;
    return 1;
}
static int erase(mock *m, const char *name) {
    int i = index_of(m, name);
    if (i < 0) return 0;
    m->state.total -= m->state.files[i].size;
    free(m->state.files[i].data);
    m->state.files[i] = m->state.files[--m->state.count];
    return 1;
}
static int write_(void *ctx, const char *name, const uint8_t *data, size_t len) {
    mock *m = ctx;
    CHECK(m->backups > 0);
    m->writes++;
    if (m->fail_write == m->writes) return 0;
    if (index_of(m, name) >= 0) erase(m, name);
    int ok = lr_snapshot_add(&m->state, name, data, len);
    if (ok && m->corrupt_write && len) {
        m->state.files[m->state.count - 1].data[0] ^= 1;
        m->corrupt_write = 0;
    }
    return ok;
}
static int remove_(void *ctx, const char *name) {
    mock *m = ctx;
    CHECK(m->backups > 0);
    m->removes++;
    if (m->fail_remove == m->removes) return 0;
    return erase(m, name);
}
static int backup_(void *ctx, const lr_snapshot *s) {
    mock *m = ctx;
    m->backups++;
    if (m->fail_backup) return 0;
    for (uint32_t i = 0; i < s->count; i++)
        CHECK(lr_snapshot_add(&m->backup, s->files[i].name, s->files[i].data, s->files[i].size));
    return 1;
}
static void log_(void *ctx, const char *line) {
    mock *m = ctx;
    snprintf(m->last_log, sizeof m->last_log, "%s", line);
}
static void add(lr_snapshot *s, const char *name, const char *value) {
    CHECK(lr_snapshot_add(s, name, (const uint8_t *)value, strlen(value)));
}
static int has(mock *m, const char *name, const char *value) {
    int i = index_of(m, name);
    return i >= 0 && m->state.files[i].size == strlen(value) && !memcmp(m->state.files[i].data, value, strlen(value));
}
static mock *original(void) {
    mock *m = calloc(1, sizeof *m);
    add(&m->state, "persist.game.json", "guest original");
    add(&m->state, "stale.json", "keep in backup");
    return m;
}
static void dispose(mock *m) {
    lr_snapshot_free(&m->state);
    lr_snapshot_free(&m->backup);
    free(m);
}
static int last_unread;
static int run(mock *m, const lr_snapshot *want) {
    lr_snapshot_io io = {m, list, read_, write_, remove_, backup_, log_};
    last_unread = -1;
    return lr_snapshot_apply_ex(want, &io, &last_unread);
}
int main(void) {
    lr_snapshot *want = calloc(1, sizeof *want), *decoded = calloc(1, sizeof *decoded);
    /* Slots 1..9 and nothing else; the game's restore area is not one of them. */
    {
        static const struct { const char *name; int slot; } good[] = {{"profile_0", 1}, {"profile_2", 3}, {"profile_8", 9}, {"steam_profile_0", 1}, {"steam_profile_8", 9}};
        static const char *const bad[] = {"profile_9", "steam_profile_9", "profile_10", "profile_", "profile_a", "", "../profile_2", "profile_2/", "steam_steam_profile_1", "PROFILE_1"};
        for (size_t i = 0; i < sizeof good / sizeof good[0]; i++) {
            int slot = 0;
            CHECK(lr_slot_folder_ok(good[i].name, &slot) && slot == good[i].slot);
        }
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) CHECK(!lr_slot_folder_ok(bad[i], NULL));
        CHECK(!lr_slot_folder_ok(NULL, NULL));
    }
    want->user = 123;
    add(want, "persist.game.json", "host current");
    add(want, "persist.roster.json", "four heroes");
    /* A snapshot without a slot, or for the restore area, is not written. */
    CHECK(!lr_snapshot_write("snapshot-test.bin", want));
    strcpy(want->folder, "profile_9");
    CHECK(!lr_snapshot_write("snapshot-test.bin", want));
    strcpy(want->folder, "profile_0");
    CHECK(!lr_snapshot_add(want, "../profile_9/bad.json", NULL, 0));
    CHECK(!lr_snapshot_add(want, "PERSIST.GAME.JSON", NULL, 0));
    CHECK(lr_snapshot_write("snapshot-test.bin", want));
    CHECK(lr_snapshot_read("snapshot-test.bin", decoded) && decoded->count == 2 && decoded->user == 123 && !strcmp(decoded->folder, "profile_0"));
    lr_snapshot_free(decoded);
    {
        uint8_t *v2 = NULL, *v1;
        size_t len = 0;
        CHECK(lr_read_file("snapshot-test.bin", 10000, &v2, &len));
        /* A file altered to name the restore area is refused. */
        CHECK(v2[20] == 9 && v2[21 + 8] == '0');
        v2[21 + 8] = '9';
        CHECK(lr_write_file("snapshot-bad.bin", v2, len));
        CHECK(!lr_snapshot_read("snapshot-bad.bin", decoded));
        v2[21 + 8] = '0';
        /* A recovery file of an older kit, still waiting: those were all slot 3. */
        v1 = malloc(len);
        memcpy(v1, v2, 20);
        memcpy(v1, "DDCSAVE1", 8);
        memcpy(v1 + 20, v2 + 30, len - 30);
        CHECK(lr_write_file("snapshot-old.bin", v1, len - 10));
        CHECK(lr_snapshot_read("snapshot-old.bin", decoded) && decoded->count == 2 && !strcmp(decoded->folder, "profile_2"));
        lr_snapshot_free(decoded);
        free(v1);
        free(v2);
    }
    uint8_t *b = NULL;
    size_t n = 0;
    CHECK(lr_read_file("snapshot-test.bin", 10000, &b, &n));
    for (size_t k = 0; k < n; k++) {
        CHECK(lr_write_file("snapshot-bad.bin", b, k));
        CHECK(!lr_snapshot_read("snapshot-bad.bin", decoded));
    }
    b[n - 1] ^= 1;
    CHECK(lr_write_file("snapshot-bad.bin", b, n));
    CHECK(!lr_snapshot_read("snapshot-bad.bin", decoded));
    free(b);
    mock *m = original();
    CHECK(run(m, want));
    CHECK(m->backups == 1 && m->state.count == 2 && has(m, "persist.game.json", "host current") &&
          index_of(m, "stale.json") < 0);
    dispose(m);
    m = original();
    m->fail_backup = 1;
    CHECK(!run(m, want));
    CHECK(!m->writes && !m->removes && has(m, "persist.game.json", "guest original"));
    CHECK(last_unread == 0); /* read fine, failed later: not a case for trying again */
    dispose(m);
    m = original();
    m->fail_read = 1;
    CHECK(!run(m, want));
    CHECK(!m->writes && !m->backups);
    CHECK(last_unread == 1); /* Steam did not serve the slot: nothing written, so the launcher may try again */
    /* The next try, once Steam serves it, goes through. */
    CHECK(run(m, want) && last_unread == 0 && has(m, "persist.game.json", "host current"));
    dispose(m);
    for (int fail = 1; fail <= 2; fail++) {
        m = original();
        m->fail_write = fail;
        CHECK(!run(m, want));
        CHECK(m->state.count == 2 && has(m, "persist.game.json", "guest original") &&
              has(m, "stale.json", "keep in backup"));
        CHECK(strstr(m->last_log, "rollback verified"));
        dispose(m);
    }
    m = original();
    m->corrupt_write = 1;
    CHECK(!run(m, want));
    CHECK(has(m, "persist.game.json", "guest original") && has(m, "stale.json", "keep in backup"));
    dispose(m);
    m = original();
    m->fail_remove = 1;
    CHECK(!run(m, want));
    CHECK(has(m, "persist.game.json", "guest original") && has(m, "stale.json", "keep in backup"));
    dispose(m);
    m = calloc(1, sizeof *m);
    add(&m->state, "persist.game.json", "host current");
    add(&m->state, "persist.roster.json", "four heroes");
    CHECK(run(m, want));
    CHECK(!m->writes && !m->backups);
    dispose(m);
    lr_snapshot empty = {0};
    m = original();
    CHECK(run(m, &empty) && !m->state.count);
    dispose(m);
    lr_delete_file("snapshot-test.bin");
    lr_delete_file("snapshot-bad.bin");
    lr_snapshot_free(want);
    lr_snapshot_free(decoded);
    free(want);
    free(decoded);
    printf("test_lr_snapshot: %s (%d failures)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
