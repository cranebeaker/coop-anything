#include "lr_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SEP '\\'

static wchar_t *wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = malloc((size_t)n * sizeof *w);
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static DWORD attrs(const char *path) {
    wchar_t *w = wide(path);
    DWORD a = w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES;
    free(w);
    return a;
}

int lr_dir_exists(const char *path) {
    DWORD a = attrs(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int lr_file_exists(const char *path) {
    DWORD a = attrs(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int make_dir(const char *path) {
    wchar_t *w = wide(path);
    int ok = w && (CreateDirectoryW(w, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    free(w);
    return ok;
}

int lr_list_files(const char *dir, int (*fn)(void *ctx, const char *name), void *ctx) {
    char pattern[LR_PATH_MAX];
    lr_join(pattern, sizeof pattern, dir, "*");
    wchar_t *w = wide(pattern);
    if (!w) return -1;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(w, &fd);
    free(w);
    if (h == INVALID_HANDLE_VALUE) return lr_dir_exists(dir) ? 0 : -1;
    int count = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char name[LR_PATH_MAX];
        if (!WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof name, NULL, NULL)) continue;
        count++;
        if (!fn(ctx, name)) break;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return count;
}

static FILE *open_file(const char *path, const char *mode) {
    wchar_t *w = wide(path), wm[8];
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 8);
    FILE *f = w ? _wfopen(w, wm) : NULL;
    free(w);
    return f;
}

static int replace_file(const char *from, const char *to) {
    wchar_t *a = wide(from), *b = wide(to);
    int ok = a && b && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    free(a);
    free(b);
    return ok;
}
int lr_sync_file(const char *path) {
    wchar_t *w = wide(path);
    HANDLE f = w ? CreateFileW(w, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL) : INVALID_HANDLE_VALUE;
    free(w);
    if (f == INVALID_HANDLE_VALUE) return 0;
    int ok = FlushFileBuffers(f) != 0;
    CloseHandle(f);
    return ok;
}

int lr_delete_file(const char *path) {
    wchar_t *w = wide(path);
    int ok = w && DeleteFileW(w);
    free(w);
    return ok;
}
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define SEP '/'

int lr_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int lr_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int make_dir(const char *path) { return mkdir(path, 0755) == 0 || lr_dir_exists(path); }

int lr_list_files(const char *dir, int (*fn)(void *ctx, const char *name), void *ctx) {
    DIR *d = opendir(dir);
    if (!d) return -1;
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        char path[LR_PATH_MAX];
        lr_join(path, sizeof path, dir, e->d_name);
        if (!lr_file_exists(path)) continue;
        count++;
        if (!fn(ctx, e->d_name)) break;
    }
    closedir(d);
    return count;
}

static FILE *open_file(const char *path, const char *mode) { return fopen(path, mode); }
static int replace_file(const char *from, const char *to) { return rename(from, to) == 0; }
int lr_sync_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int ok = fsync(fileno(f)) == 0;
    fclose(f);
    return ok;
}
int lr_delete_file(const char *path) { return unlink(path) == 0; }
#endif

void lr_join(char *out, size_t cap, const char *a, const char *b) {
    size_t n = strlen(a);
    if (n > 0 && a[n - 1] != '/' && a[n - 1] != '\\')
        snprintf(out, cap, "%s%c%s", a, SEP, b);
    else
        snprintf(out, cap, "%s%s", a, b);
}

int lr_make_dirs(const char *path) {
    char buf[LR_PATH_MAX];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/' && *p != '\\') continue;
        if (p[-1] == ':') continue; /* "C:\" */
        char c = *p;
        *p = 0;
        if (!lr_dir_exists(buf)) make_dir(buf);
        *p = c;
    }
    return make_dir(buf);
}

int lr_read_file(const char *path, size_t max, uint8_t **out, size_t *len) {
    *out = NULL;
    *len = 0;
    FILE *f = open_file(path, "rb");
    if (!f) return 0;
    size_t cap = 65536, n = 0;
    uint8_t *buf = malloc(cap);
    while (buf) {
        if (n == cap) {
            if (cap > max) break;
            uint8_t *p = realloc(buf, cap * 2);
            if (!p) break;
            buf = p;
            cap *= 2;
        }
        size_t r = fread(buf + n, 1, cap - n, f);
        n += r;
        if (r == 0) {
            int ok = !ferror(f) && n <= max;
            fclose(f);
            if (!ok) {
                free(buf);
                return 0;
            }
            *out = buf;
            *len = n;
            return 1;
        }
    }
    fclose(f);
    free(buf);
    return 0;
}

int lr_write_file(const char *path, const uint8_t *data, size_t len) {
    char tmp[LR_PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s.ddcoop-tmp", path);
    FILE *f = open_file(tmp, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, len, f) == len;
    ok = (fclose(f) == 0) && ok;
    if (ok) ok = replace_file(tmp, path);
    if (!ok) lr_delete_file(tmp);
    return ok;
}

typedef struct {
    const char *from, *to;
    int ok;
} copy_ctx;

static int copy_one(void *p, const char *name) {
    copy_ctx *c = p;
    char a[LR_PATH_MAX], b[LR_PATH_MAX];
    uint8_t *data;
    size_t len;
    lr_join(a, sizeof a, c->from, name);
    lr_join(b, sizeof b, c->to, name);
    if (!lr_read_file(a, 256u * 1024 * 1024, &data, &len)) {
        c->ok = 0;
        return 0;
    }
    if (!lr_write_file(b, data, len)) c->ok = 0;
    free(data);
    return c->ok;
}

int lr_copy_dir_files(const char *from, const char *to) {
    if (!lr_make_dirs(to)) return 0;
    copy_ctx c = {from, to, 1};
    if (lr_list_files(from, copy_one, &c) < 0) return 0;
    return c.ok;
}
