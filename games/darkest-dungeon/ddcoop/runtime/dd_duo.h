/* Two copies of the game on one PC, for testing without a second player. Only used when built with DD_DUO; never a player kit.
 *
 * Three things stop two copies working side by side on one PC. Each is handled from outside the game's code:
 * - A starting copy looks for a window with the game's title, brings it forward and exits (370e0b). So each copy renames its own.
 * - Both copies read one ddcoop.ini. So the second gets its role from the environment.
 * - A copy without focus ignores its input logic. That one is handled for every session now: dd_focus.h. */
#ifndef DD_DUO_H
#define DD_DUO_H

#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "dd_config.h"

static dd_role dd_duo_role(const char *value, dd_role from_ini) {
    return value && !strcmp(value, "guest") ? DD_ROLE_GUEST : from_ini;
}
static const char *dd_duo_title(dd_role role) {
    return role == DD_ROLE_GUEST ? "Darkest Dungeon - DDCoop GAST" : "Darkest Dungeon - DDCoop HOST";
}
/* Roughly 60 FPS per process instead of two uncapped renderers. Keep existing
 * slower/vsync pacing, never busy-spin or catch up after loading pauses. */
static DWORD dd_duo_frame_wait(uint64_t now, uint64_t previous) {
    if (!previous || now < previous || now - previous >= 16) return 0;
    return (DWORD)(16 - (now - previous));
}
/* Every visible window of this process whose title is not yet the given one. Returns how many it changed. */
typedef struct {
    const char *title;
    unsigned changed;
} dd_duo_rename_job;
static BOOL CALLBACK dd_duo_rename_one(HWND window, LPARAM value) {
    dd_duo_rename_job *job = (dd_duo_rename_job *)value;
    DWORD process = 0;
    char now[64] = "";
    GetWindowThreadProcessId(window, &process);
    if (process == GetCurrentProcessId() && IsWindowVisible(window) && GetWindowTextA(window, now, sizeof now) &&
        strcmp(now, job->title) && SetWindowTextA(window, job->title))
        job->changed++;
    return TRUE;
}
static unsigned dd_duo_rename(const char *title) {
    dd_duo_rename_job job = {title, 0};
    EnumWindows(dd_duo_rename_one, (LPARAM)&job);
    return job.changed;
}

#endif
