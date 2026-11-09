/* Local runtime -> launcher membership only. Display data never grants authority. */
#ifndef DD_PARTY_WIRE_H
#define DD_PARTY_WIRE_H
#include <stdint.h>
#define DD_PARTY_MAGIC 0x31504344u
#define DD_PARTY_VERSION 1u
#define DD_PARTY_FRESH_MS 3000u
enum { DD_PARTY_UNKNOWN = 0, DD_PARTY_TOWN = 1, DD_PARTY_RAID = 2 };
typedef struct {
    volatile int32_t sequence;
    uint32_t magic, version, size, pid, mode, count, reserved;
    uint64_t updated;
    uint32_t ids[4];
} dd_party_snapshot;
static inline int dd_party_valid(const dd_party_snapshot *s, uint32_t pid, uint64_t now) {
    if (!s || (s->sequence & 1) || s->magic != DD_PARTY_MAGIC || s->version != DD_PARTY_VERSION ||
        s->size != sizeof *s || !pid || s->pid != pid || s->reserved || s->mode > DD_PARTY_RAID ||
        s->count > 4 || (s->mode == DD_PARTY_UNKNOWN && s->count) ||
        now < s->updated || now - s->updated > DD_PARTY_FRESH_MS) return 0;
    for (unsigned i = 0; i < 4; i++) {
        if (i >= s->count) { if (s->ids[i]) return 0; continue; }
        if (!s->ids[i] || s->ids[i] == UINT32_MAX) return 0;
        for (unsigned j = 0; j < i; j++) if (s->ids[j] == s->ids[i]) return 0;
    }
    return 1;
}
#ifdef _WIN32
#include <windows.h>
#include <string.h>
static inline int dd_party_read(const dd_party_snapshot *shared, uint32_t pid, uint64_t now, dd_party_snapshot *out) {
    if (!shared || !out) return 0;
    for (unsigned attempt = 0; attempt < 3; attempt++) {
        int32_t before = shared->sequence;
        if (before & 1) continue;
        MemoryBarrier();
        memcpy(out, shared, sizeof *out);
        MemoryBarrier();
        if (before == shared->sequence && out->sequence == before && dd_party_valid(out, pid, now)) return 1;
    }
    memset(out, 0, sizeof *out);
    return 0;
}
static inline void dd_party_write(dd_party_snapshot *shared, const dd_party_snapshot *next) {
    InterlockedIncrement((volatile LONG *)&shared->sequence);
    memcpy((uint8_t *)shared + sizeof shared->sequence, (const uint8_t *)next + sizeof next->sequence,
           sizeof *next - sizeof next->sequence);
    MemoryBarrier();
    InterlockedIncrement((volatile LONG *)&shared->sequence);
}
#endif
#endif
