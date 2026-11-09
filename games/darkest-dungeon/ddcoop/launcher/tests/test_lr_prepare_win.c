/* Runs only against fake_storage_api.dll in an isolated build folder, never the real Steam/game DLL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lr_snapshot.h"
#include "lr_prepare_win.h"
static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #x);                                                                \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)
static DWORD run(const char *exe, const char *dir, const char *bundle) {
    char cmd[8192];
    snprintf(cmd, sizeof cmd, "\"%s\" --prepare-save \"%s\" \"%s\" \"%s\\before.bin\" \"%s\\report.log\"", exe, dir,
             bundle, dir, dir);
    STARTUPINFOA si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        CHECK(0);
        return 99;
    }
    CloseHandle(pi.hThread);
    CHECK(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
    DWORD code = 99;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return code;
}
static char *text(const char *path) {
    uint8_t *b = NULL;
    size_t n = 0;
    if (!lr_read_file(path, 65536, &b, &n)) return NULL;
    char *s = malloc(n + 1);
    memcpy(s, b, n);
    s[n] = 0;
    free(b);
    return s;
}
int main(int argc, char **argv) {
    if (argc != 3) {
        puts("test_lr_prepare_win <test DDCoop.exe> <folder containing fake steam_api64.dll>");
        return 2;
    }
    char dir[LR_PATH_MAX], exe[LR_PATH_MAX], bundle[LR_PATH_MAX], trace[LR_PATH_MAX];
    GetFullPathNameA(argv[1], sizeof exe, exe, NULL);
    GetFullPathNameA(argv[2], sizeof dir, dir, NULL);
    lr_join(bundle, sizeof bundle, dir, "wanted.bin");
    lr_join(trace, sizeof trace, dir, "trace.txt");
    SetEnvironmentVariableA("DDCOOP_FAKE_TRACE", trace);
    lr_snapshot *s = calloc(1, sizeof *s);
    s->user = 123;
    strcpy(s->folder, "profile_2");
    CHECK(lr_snapshot_add(s, "persist.game.json", (const uint8_t *)"host current", 12));
    CHECK(lr_snapshot_add(s, "persist.roster.json", (const uint8_t *)"heroes", 6));
    CHECK(lr_snapshot_add(s, "backup/persist.game.json", (const uint8_t *)"host current", 12));
    CHECK(lr_snapshot_add(s, "backup/persist.roster.json", (const uint8_t *)"heroes", 6));
    CHECK(lr_snapshot_write(bundle, s));
    lr_snapshot_free(s);
    free(s);
    CHECK(run(exe, dir, bundle) == 0);
    char *v = text(trace);
    CHECK(v && strstr(v, "bad_target=0") && strstr(v, "persist.game.json=host current") && !strstr(v, "stale.json="));
    CHECK(v && strstr(v, "profile_0/keep.json=protected0") && strstr(v, "profile_9/restore.json=protected9") &&
          strstr(v, "profile_2/backup/persist.game.json=host current") && !strstr(v, "profile_2/backup/old.json="));
    free(v);
    char before[LR_PATH_MAX], original[LR_PATH_MAX];
    lr_join(before, sizeof before, dir, "before.bin");
    lr_join(original, sizeof original, dir, "original.bin");
    s = calloc(1, sizeof *s);
    CHECK(lr_snapshot_read(before, s) && s->count == 3);
    CHECK(lr_snapshot_write(original, s));
    lr_snapshot_free(s);
    free(s);
    SetEnvironmentVariableA("DDCOOP_FAKE_BORROWED", "1");
    CHECK(run(exe, dir, original) == 0);
    v = text(trace);
    CHECK(v && strstr(v, "profile_2/persist.game.json=guest original") &&
          strstr(v, "profile_2/backup/old.json=nested") && !strstr(v, "persist.roster.json="));
    CHECK(v && strstr(v, "bad_target=0") && strstr(v, "profile_9/restore.json=protected9"));
    free(v);
    SetEnvironmentVariableA("DDCOOP_FAKE_BORROWED", NULL);
    /* Another slot: only that one changes. */
    {
        char other[LR_PATH_MAX];
        uint8_t *raw = NULL;
        size_t rawn = 0;
        lr_join(other, sizeof other, dir, "other.bin");
        s = calloc(1, sizeof *s);
        s->user = 123;
        strcpy(s->folder, "profile_1");
        CHECK(lr_snapshot_add(s, "persist.game.json", (const uint8_t *)"chosen slot", 11));
        CHECK(lr_snapshot_write(other, s));
        lr_snapshot_free(s);
        free(s);
        CHECK(run(exe, dir, other) == 0);
        v = text(trace);
        CHECK(v && strstr(v, "bad_target=0") && strstr(v, "profile_1/persist.game.json=chosen slot"));
        CHECK(v && strstr(v, "profile_2/persist.game.json=guest original") && strstr(v, "profile_2/stale.json=stale") && strstr(v, "profile_0/keep.json=protected0") && strstr(v, "profile_9/restore.json=protected9"));
        free(v);
        /* A bundle altered to name the restore area: nothing is written. */
        CHECK(lr_read_file(other, 100000, &raw, &rawn) && raw[21 + 8] == '1');
        raw[21 + 8] = '9';
        CHECK(lr_write_file(other, raw, rawn));
        free(raw);
        lr_delete_file(trace);
        CHECK(run(exe, dir, other) != 0);
        v = text(trace); /* Refused before Steam is even opened: no trace at all. */
        CHECK(!v);
        free(v);
    }
    /* Steam does not serve the slot yet (a save the game just wrote): its own exit code, so the launcher may try again; nothing written. */
    SetEnvironmentVariableA("DDCOOP_FAKE_UNREAD", "1");
    CHECK(run(exe, dir, bundle) == LR_PREPARE_UNREAD);
    v = text(trace);
    CHECK(v && strstr(v, "writes=0 deletes=0 bad_target=0") && strstr(v, "profile_2/persist.game.json=guest original"));
    free(v);
    SetEnvironmentVariableA("DDCOOP_FAKE_UNREAD", NULL);
    SetEnvironmentVariableA("DDCOOP_FAKE_BAD_APP", "1");
    CHECK(run(exe, dir, bundle) == 1); /* any other refusal is final */
    v = text(trace);
    CHECK(v && strstr(v, "writes=0 deletes=0 bad_target=0"));
    free(v);
    SetEnvironmentVariableA("DDCOOP_FAKE_BAD_APP", NULL);
    SetEnvironmentVariableA("DDCOOP_FAKE_BAD_USER", "1");
    CHECK(run(exe, dir, bundle) != 0);
    v = text(trace);
    CHECK(v && strstr(v, "writes=0 deletes=0 bad_target=0"));
    free(v);
    SetEnvironmentVariableA("DDCOOP_FAKE_BAD_USER", NULL);
    printf("test_lr_prepare_win: %s (%d failures)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
