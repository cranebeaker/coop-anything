#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lr_snapshot.h"
#include "lr_prepare_win.h"

typedef void *(*iface_fn)(void);
typedef struct {
    void *remote;
    FILE *report;
    char backup[LR_PATH_MAX];
    char folder[LR_NAME_MAX + 1]; /* Checked by lr_snapshot_read: one campaign slot, never profile_9. */
    uint64_t user;
    int (*count)(void *);
    const char *(*name)(void *, int, int32_t *);
    int32_t (*size)(void *, const char *);
    bool (*exists)(void *, const char *);
    int32_t (*read)(void *, const char *, void *, int32_t);
    bool (*write)(void *, const char *, const void *, int32_t);
    bool (*remove)(void *, const char *);
} steam_save;
static int game_running(void) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return 1; /* cannot prove the game is closed */
    PROCESSENTRY32W e = {.dwSize = sizeof e};
    int running = 0;
    for (BOOL more = Process32FirstW(h, &e); more; more = Process32NextW(h, &e))
        if (!_wcsicmp(e.szExeFile, L"Darkest.exe")) {
            running = 1;
            break;
        }
    CloseHandle(h);
    return running;
}
static void report(void *ctx, const char *line) {
    steam_save *s = ctx;
    fprintf(s->report, "%s\n", line);
    fflush(s->report);
}
/* The only target this adapter can construct is a root file in the one slot of the snapshot. */
static int target(const steam_save *s, char path[128], const char *name) {
    if (!lr_snapshot_name_ok(name)) return 0;
    snprintf(path, 128, "%s/%s", s->folder, name);
    return 1;
}
static int list(void *ctx, int (*visit)(void *, const char *), void *vctx) {
    steam_save *s = ctx;
    int total = s->count(s->remote), n = 0;
    if (total < 0 || total > 100000) return -1;
    for (int i = 0; i < total; i++) {
        int32_t size = 0;
        const char *name = s->name(s->remote, i, &size);
        if (!name) return -1;
        size_t fl = strlen(s->folder);
        if (strncmp(name, s->folder, fl) || (name[fl] != '/' && name[fl] != '\\')) continue;
        name += fl + 1;
        char logical[LR_SNAPSHOT_NAME_MAX + 1];
        if (strlen(name) >= sizeof logical) return -1;
        snprintf(logical, sizeof logical, "%s", name);
        for (char *c = logical; *c; c++)
            if (*c == '\\') *c = '/';
        if (!lr_snapshot_name_ok(logical) || !visit(vctx, logical)) return -1;
        if (++n > LR_MAX_FILES) return -1;
    }
    return n;
}
static int read_file(void *ctx, const char *name, uint8_t **out, size_t *len) {
    steam_save *s = ctx;
    char path[128];
    *out = NULL;
    *len = 0;
    if (!target(s, path, name) || !s->exists(s->remote, path)) {
        report(ctx,"A listed file of the slot is not visible through Steam FileExists");
        return 0;
    }
    int32_t n = s->size(s->remote, path);
    if (n < 0 || (uint32_t)n > LR_MAX_FILE_SIZE) return 0;
    uint8_t *b = malloc(n ? (size_t)n : 1);
    if (!b) return 0;
    if (n && s->read(s->remote, path, b, n) != n) {
        free(b);
        return 0;
    }
    *out = b;
    *len = (size_t)n;
    return 1;
}
static int write_file(void *ctx, const char *name, const uint8_t *data, size_t len) {
    steam_save *s = ctx;
    char path[128];
    return target(s, path, name) && len <= LR_MAX_FILE_SIZE && s->write(s->remote, path, data, (int32_t)len);
}
static int remove_file(void *ctx, const char *name) {
    steam_save *s = ctx;
    char path[128];
    return target(s, path, name) && s->remove(s->remote, path);
}
static int backup(void *ctx, const lr_snapshot *original) {
    steam_save *s = ctx;
    if (game_running()) {
        report(ctx, "Game opened during preparation; no save writes attempted");
        return 0;
    }
    lr_snapshot *copy = malloc(sizeof *copy);
    if (!copy) return 0;
    *copy = *original;
    copy->user = s->user;
    snprintf(copy->folder, sizeof copy->folder, "%s", s->folder);
    int ok = lr_snapshot_write(s->backup, copy);
    free(copy);
    report(ctx, ok ? "Steam storage original backed up and verified" : "Steam storage original backup failed");
    return ok;
}
static int utf8(const wchar_t *w, char *out, size_t cap) {
    return WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL) > 0;
}
#define LOAD(member, export_name)                                                                                      \
    do {                                                                                                               \
        FARPROC p = GetProcAddress(dll, export_name);                                                                  \
        if (!p) {                                                                                                      \
            report(&s, "Required Steam storage API is missing");                                                       \
            goto done;                                                                                                 \
        }                                                                                                              \
        _Static_assert(sizeof s.member == sizeof p, "function pointer size");                                          \
        memcpy(&s.member, &p, sizeof p);                                                                               \
    } while (0)
int lr_prepare_win_main(int argc, wchar_t **argv) {
    if (argc != 6) return 2;
    steam_save s = {0};
    int result = 1, initialized = 0;
    HMODULE dll = NULL;
    bool (*init)(void) = NULL;
    void (*shutdown)(void) = NULL;
    iface_fn remote = NULL, utils = NULL, user = NULL;
    uint32_t (*appid)(void *) = NULL;
    uint64_t (*steamid)(void *) = NULL;
    lr_snapshot *want = calloc(1, sizeof *want);
    s.report = _wfopen(argv[5], L"w");
    if (!s.report) {
        free(want);
        return 2;
    }
    if (!_wcsicmp(argv[3], argv[4])) {
        report(&s, "Recovery output must differ from the input snapshot");
        goto done;
    }
    if (game_running()) {
        report(&s, "Game is still running; preparation refused");
        goto done;
    }
    char bundle[LR_PATH_MAX];
    if (!want || !utf8(argv[3], bundle, sizeof bundle) || !utf8(argv[4], s.backup, sizeof s.backup) ||
        !lr_snapshot_read(bundle, want)) {
        report(&s, "Snapshot is missing or invalid; no Steam writes attempted");
        goto done;
    }
    if (!want->user) {
        report(&s, "Snapshot has no Steam owner; no writes attempted");
        goto done;
    }
    wchar_t path[LR_PATH_MAX];
    if (wcslen(argv[2]) + 19 >= LR_PATH_MAX) goto done;
    swprintf(path, LR_PATH_MAX, L"%ls\\steam_api64.dll", argv[2]);
    dll = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) {
        report(&s, "Could not load the installed game's Steam API");
        goto done;
    }
#define GETLOCAL(var, name)                                                                                            \
    do {                                                                                                               \
        FARPROC p = GetProcAddress(dll, name);                                                                         \
        if (!p) goto done;                                                                                             \
        memcpy(&var, &p, sizeof p);                                                                                    \
    } while (0)
    GETLOCAL(init, "SteamAPI_Init");
    GETLOCAL(shutdown, "SteamAPI_Shutdown");
    GETLOCAL(remote, "SteamAPI_SteamRemoteStorage_v016");
    GETLOCAL(utils, "SteamAPI_SteamUtils_v010");
    GETLOCAL(user, "SteamAPI_SteamUser_v021");
    GETLOCAL(appid, "SteamAPI_ISteamUtils_GetAppID");
    GETLOCAL(steamid, "SteamAPI_ISteamUser_GetSteamID");
    if (!init()) {
        report(&s, "Steam storage initialization failed; no writes attempted");
        goto done;
    }
    initialized = 1;
    void *u = utils(), *usr = user();
    s.remote = remote();
    if (!u || !usr || !s.remote || appid(u) != 262060 || steamid(usr) != want->user) {
        report(&s, "Steam game or account does not match; no writes attempted");
        goto done;
    }
    s.user = want->user;
    snprintf(s.folder, sizeof s.folder, "%s", want->folder);
    report(&s, "Steam identity verified: Darkest Dungeon (262060), expected local account");
    LOAD(count, "SteamAPI_ISteamRemoteStorage_GetFileCount");
    LOAD(name, "SteamAPI_ISteamRemoteStorage_GetFileNameAndSize");
    LOAD(size, "SteamAPI_ISteamRemoteStorage_GetFileSize");
    LOAD(exists, "SteamAPI_ISteamRemoteStorage_FileExists");
    LOAD(read, "SteamAPI_ISteamRemoteStorage_FileRead");
    LOAD(write, "SteamAPI_ISteamRemoteStorage_FileWrite");
    LOAD(remove, "SteamAPI_ISteamRemoteStorage_FileDelete");
    lr_snapshot_io io = {&s, list, read_file, write_file, remove_file, backup, report};
    int unread = 0;
    if (lr_snapshot_apply_ex(want, &io, &unread)) result = 0;
    else if (unread) result = LR_PREPARE_UNREAD;
done:
    report(&s, result == LR_PREPARE_UNREAD ? "PREPARE NOT YET: Steam does not serve the slot yet; nothing written, try again shortly"
               : result ? "PREPARE FAILED: game must stay closed" : "PREPARE OK: slot verified through Steam storage");
    if (initialized) shutdown();
    if (dll) FreeLibrary(dll);
    if (want) {
        lr_snapshot_free(want);
        free(want);
    }
    fclose(s.report);
    return result;
}
