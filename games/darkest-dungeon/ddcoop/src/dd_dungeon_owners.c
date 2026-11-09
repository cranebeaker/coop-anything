/* Bounded parser for the existing launcher ownership contract. No game/OS/file access. */
#include "dd_dungeon.h"
#include <string.h>
typedef struct {
    const char *p;
    size_t n;
} span;
static span trim(span s) {
    while (s.n && (s.p[0] == ' ' || s.p[0] == '\t')) {
        s.p++;
        s.n--;
    }
    while (s.n && (s.p[s.n - 1] == ' ' || s.p[s.n - 1] == '\t' || s.p[s.n - 1] == '\r'))
        s.n--;
    return s;
}
static int equal(span s, const char *v) { return s.n == strlen(v) && !memcmp(s.p, v, s.n); }
static int number(span s, uint32_t *out) {
    if (!s.n) return 0;
    uint32_t n = 0;
    for (size_t i = 0; i < s.n; i++) {
        unsigned digit = (unsigned char)s.p[i] - '0';
        if (digit > 9 || n > (UINT32_MAX - digit) / 10) return 0;
        n = n * 10 + digit;
    }
    *out = n;
    return 1;
}
static int line(const char *text, size_t len, size_t *at, span *key, span *value) {
    size_t begin = *at;
    while (*at < len && text[*at] != '\n')
        (*at)++;
    span s = trim((span){text + begin, *at - begin});
    if (*at < len) (*at)++;
    if (!s.n || s.p[0] == ';' || s.p[0] == '#') return 0;
    const char *e = memchr(s.p, '=', s.n);
    if (!e) {
        *key = s;
        *value = (span){NULL, 0};
        return 1;
    }
    *key = trim((span){s.p, (size_t)(e - s.p)});
    *value = trim((span){e + 1, s.n - (size_t)(e - s.p) - 1});
    return 1;
}
int dd_dng_owners_parse(const char *text, size_t len, const uint32_t party[4], uint8_t owners[4],
                        uint32_t *revision) {
    memset(owners, 0, 4);
    *revision = 0;
    if (!text) return len ? -1 : 0;
    if (len > 16384 || memchr(text, 0, len)) return -1;
    if (len >= 3 && (unsigned char)text[0] == 0xef && (unsigned char)text[1] == 0xbb &&
        (unsigned char)text[2] == 0xbf) {
        text += 3;
        len -= 3;
    }
    span key, value;
    int host = 0, writers = 0;
    for (size_t at = 0; at < len;)
        if (line(text, len, &at, &key, &value) && equal(key, "written_by")) {
            if (++writers > 1) return -1;
            host = equal(value, "host");
        }
    if (!host) return 0; /* A stale guest/other writer never assigns authority to this host. */
    uint32_t ids[64], rev = 0;
    uint8_t selected[4] = {0};
    unsigned count = 0, versions = 0, revisions = 0;
    for (size_t at = 0; at < len;) {
        if (!line(text, len, &at, &key, &value)) continue;
        uint32_t n = 0;
        if (equal(key, "version")) {
            if (++versions > 1 || !number(value, &n) || n != 1) return -1;
        } else if (equal(key, "revision")) {
            if (++revisions > 1 || !number(value, &rev)) return -1;
        } else if (key.n >= 5 && !memcmp(key.p, "hero.", 5)) {
            uint32_t id = 0;
            if (!number((span){key.p + 5, key.n - 5}, &id) || !id || !number(value, &n) || n > 1 ||
                count == 64)
                return -1;
            for (unsigned i = 0; i < count; i++)
                if (ids[i] == id) return -1;
            ids[count++] = id;
            for (unsigned i = 0; i < 4; i++)
                if (party[i] && party[i] == id) selected[i] = (uint8_t)n;
        }
    }
    if (versions != 1) return -1;
    memcpy(owners, selected, 4);
    *revision = rev;
    return 1;
}
