/* Test-only Steam ABI substitute. All save data lives in this process's memory. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define API __declspec(dllexport)
typedef struct {
    char name[100];
    uint8_t data[256];
    int32_t len;
} file;
static file files[16];
static int count, writes, deletes, bad_target;
/* Any campaign slot may be written; the game's restore area never. */
static int slot_name(const char *name) {
    return !strncmp(name, "profile_", 8) && name[8] >= '0' && name[8] <= '8' && name[9] == '/';
}
static void add(const char *name, const char *data) {
    file *f = &files[count++];
    snprintf(f->name, sizeof f->name, "%s", name);
    f->len = (int32_t)strlen(data);
    memcpy(f->data, data, (size_t)f->len);
}
static int find(const char *name) {
    for (int i = 0; i < count; i++)
        if (!strcmp(files[i].name, name)) return i;
    return -1;
}
API bool SteamAPI_Init(void) {
    if (getenv("DDCOOP_FAKE_BORROWED")) {
        add("profile_2/persist.game.json", "host current");
        add("profile_2/persist.roster.json", "heroes");
        add("profile_2/backup/persist.game.json", "host current");
        add("profile_2/backup/persist.roster.json", "heroes");
    } else {
        add("profile_2/persist.game.json", "guest original");
        add("profile_2/stale.json", "stale");
        add("profile_2/backup/old.json", "nested");
    }
    add("profile_0/keep.json", "protected0");
    add("profile_9/restore.json", "protected9");
    return true;
}
API void SteamAPI_Shutdown(void) {
    const char *path = getenv("DDCOOP_FAKE_TRACE");
    FILE *f = path ? fopen(path, "w") : NULL;
    if (!f) return;
    fprintf(f, "writes=%d deletes=%d bad_target=%d\n", writes, deletes, bad_target);
    for (int i = 0; i < count; i++)
        fprintf(f, "%s=%.*s\n", files[i].name, (int)files[i].len, files[i].data);
    fclose(f);
}
API void *SteamAPI_SteamRemoteStorage_v016(void) { return (void *)1; }
API void *SteamAPI_SteamUtils_v010(void) { return (void *)2; }
API void *SteamAPI_SteamUser_v021(void) { return (void *)3; }
API uint32_t SteamAPI_ISteamUtils_GetAppID(void *p) {
    (void)p;
    const char *s = getenv("DDCOOP_FAKE_BAD_APP");
    return s ? 480 : 262060;
}
API uint64_t SteamAPI_ISteamUser_GetSteamID(void *p) {
    (void)p;
    const char *s = getenv("DDCOOP_FAKE_BAD_USER");
    return s ? 999 : 123;
}
API int SteamAPI_ISteamRemoteStorage_GetFileCount(void *p) {
    (void)p;
    return count;
}
API const char *SteamAPI_ISteamRemoteStorage_GetFileNameAndSize(void *p, int i, int32_t *n) {
    (void)p;
    if (i < 0 || i >= count) return "";
    *n = files[i].len;
    return files[i].name;
}
API int32_t SteamAPI_ISteamRemoteStorage_GetFileSize(void *p, const char *name) {
    (void)p;
    int i = find(name);
    return i < 0 ? 0 : files[i].len;
}
API bool SteamAPI_ISteamRemoteStorage_FileExists(void *p, const char *name) {
    (void)p;
    return find(name) >= 0;
}
API int32_t SteamAPI_ISteamRemoteStorage_FileRead(void *p, const char *name, void *out, int32_t n) {
    (void)p;
    int i = find(name);
    if (getenv("DDCOOP_FAKE_UNREAD")) return 0; /* Steam does not serve the file yet */
    if (i < 0 || n != files[i].len) return 0;
    memcpy(out, files[i].data, (size_t)n);
    return n;
}
API bool SteamAPI_ISteamRemoteStorage_FileWrite(void *p, const char *name, const void *data, int32_t n) {
    (void)p;
    const char *base = name + 10;
    if (!strncmp(base, "backup/", 7)) base += 7;
    if (!slot_name(name) || strchr(base, '/') || n < 0 || n > 256) {
        bad_target++;
        return false;
    }
    int i = find(name);
    if (i < 0) {
        i = count++;
        snprintf(files[i].name, sizeof files[i].name, "%s", name);
    }
    memcpy(files[i].data, data, (size_t)n);
    files[i].len = n;
    writes++;
    return true;
}
API bool SteamAPI_ISteamRemoteStorage_FileDelete(void *p, const char *name) {
    (void)p;
    const char *base = name + 10;
    if (!strncmp(base, "backup/", 7)) base += 7;
    if (!slot_name(name) || strchr(base, '/')) {
        bad_target++;
        return false;
    }
    int i = find(name);
    if (i < 0) return false;
    files[i] = files[--count];
    deletes++;
    return true;
}
