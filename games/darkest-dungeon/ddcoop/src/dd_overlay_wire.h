/* Local launcher -> game UI snapshot. No save data, credentials, Steam IDs or peer pointers. */
#ifndef DD_OVERLAY_WIRE_H
#define DD_OVERLAY_WIRE_H
#include <stdint.h>
#define DD_OVERLAY_MAGIC 0x314f4344u
#define DD_OVERLAY_VERSION 1
#define DD_OVERLAY_HEROES 64
#define DD_OVERLAY_CHECKS 8
#define DD_OVERLAY_MESSAGE L"DDCoopOverlayCommand-v1"
enum { DD_OVERLAY_LAUNCHER = 1, DD_OVERLAY_LOGS, DD_OVERLAY_COPY_CODE, DD_OVERLAY_CLAIM, DD_OVERLAY_RELEASE };
typedef struct {
    uint32_t id, party, mine;
    char name[64], cls[48], owner[16];
} dd_overlay_hero;
typedef struct {
    volatile int32_t sequence;
    uint32_t magic, version, size, launcher_pid, hero_count;
    uint64_t updated, launcher_window;
    char role[16], status[256], runtime[200];
    int32_t check_ok[DD_OVERLAY_CHECKS];
    char checks[DD_OVERLAY_CHECKS][200];
    dd_overlay_hero heroes[DD_OVERLAY_HEROES];
} dd_overlay_snapshot;
#endif
