#ifndef DD_TEXT_H
#define DD_TEXT_H
#include <stddef.h>
/* NUL-terminated UTF-8 without ASCII controls, bounded by a fixed wire/game field. */
static inline int dd_utf8_name_valid(const char *text, size_t cap) {
    const unsigned char *p = (const unsigned char *)text;
    size_t i = 0;
    if (!cap || !p[0]) return 0;
    while (i < cap && p[i]) {
        unsigned c = p[i++], need = 0, min = 0;
        if (c < 0x20 || c == 0x7f) return 0;
        if (c < 0x80) continue;
        if (c >= 0xc2 && c <= 0xdf) {
            need = 1;
            min = 0x80;
            c &= 0x1f;
        } else if (c >= 0xe0 && c <= 0xef) {
            need = 2;
            min = 0x800;
            c &= 0xf;
        } else if (c >= 0xf0 && c <= 0xf4) {
            need = 3;
            min = 0x10000;
            c &= 7;
        } else
            return 0;
        while (need--) {
            if (i >= cap || (p[i] & 0xc0) != 0x80) return 0;
            c = (c << 6) | (p[i++] & 0x3f);
        }
        if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return 0;
    }
    return i < cap;
}
#endif
