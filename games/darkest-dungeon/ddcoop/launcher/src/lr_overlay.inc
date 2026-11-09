/* Kept separate from session/save logic. UI commands go through the existing lr_core handlers. */
#include "../../src/dd_overlay_wire.h"
static HANDLE g_overlay_mapping;
static dd_overlay_snapshot *g_overlay_view;
static DWORD g_overlay_game;
static UINT g_overlay_message;
static void overlay_close(void) {
    if (g_overlay_view) UnmapViewOfFile(g_overlay_view);
    if (g_overlay_mapping) CloseHandle(g_overlay_mapping);
    g_overlay_view = NULL;
    g_overlay_mapping = NULL;
    g_overlay_game = 0;
}
static void overlay_publish(void) {
    DWORD pid = G.game ? GetProcessId(G.game) : 0;
    if (pid != g_overlay_game) overlay_close();
    if (!pid) return;
    if (!g_overlay_view) {
        wchar_t name[96];
        swprintf(name, 96, L"Local\\DDCoopOverlay-%lu", pid);
        g_overlay_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                               sizeof(dd_overlay_snapshot), name);
        if (!g_overlay_mapping) return;
        g_overlay_view = MapViewOfFile(g_overlay_mapping, FILE_MAP_WRITE, 0, 0, sizeof(dd_overlay_snapshot));
        if (!g_overlay_view) {
            overlay_close();
            return;
        }
        g_overlay_game = pid;
    }
    dd_overlay_snapshot next = {0};
    next.magic = DD_OVERLAY_MAGIC;
    next.version = DD_OVERLAY_VERSION;
    next.size = sizeof next;
    next.launcher_pid = GetCurrentProcessId();
    next.launcher_window = (uintptr_t)G.wnd;
    next.updated = now_ms();
    snprintf(next.role, sizeof next.role, "%s",
             G.core.role == LR_ROLE_HOST    ? "HOST"
             : G.core.role == LR_ROLE_GUEST ? "GAST"
                                            : "DDCoop");
    snprintf(next.status, sizeof next.status, "%s",
             G.core.error[0] && G.core.state == LR_ST_FAILED ? G.core.error : G.core.status);
    snprintf(next.runtime, sizeof next.runtime, "%s", G.runtime_line);
    for (unsigned i = 0; i < DD_OVERLAY_CHECKS; i++) {
        next.check_ok[i] = G.ok[i];
        snprintf(next.checks[i], sizeof next.checks[i], "%s", G.check_text[i]);
    }
    int me = G.core.role == LR_ROLE_HOST ? LR_PLAYER_HOST : LR_PLAYER_GUEST;
    /* Party first, just like the launcher table. IDs remain the authority when a sorted row is clicked. */
    int order[LR_MAX_HEROES];
    uint8_t flags[LR_MAX_HEROES];
    (void)lr_owners_live_order(&G.core.owners, G.live_party_ids, G.live_party_count, order, flags);
    for (int position = 0; position < G.core.owners.count && next.hero_count < DD_OVERLAY_HEROES; position++) {
        int i = order[position];
        const lr_hero *h = &G.core.owners.heroes[i];
        int party = flags[i];
        dd_overlay_hero *row = &next.heroes[next.hero_count++];
        row->id = h->id;
        row->party = (uint32_t)party;
        row->mine = G.core.owners.owner[i] == me;
        snprintf(row->name, sizeof row->name, "%s", h->name);
        snprintf(row->cls, sizeof row->cls, "%s", h->cls);
        snprintf(row->owner, sizeof row->owner, "%s",
                 row->mine                                  ? "you"
                 : G.core.owners.owner[i] == LR_PLAYER_HOST ? "host"
                                                            : "friend");
    }
    InterlockedIncrement((volatile LONG *)&g_overlay_view->sequence);
    memcpy((uint8_t *)g_overlay_view + sizeof next.sequence, (uint8_t *)&next + sizeof next.sequence,
           sizeof next - sizeof next.sequence);
    MemoryBarrier();
    InterlockedIncrement((volatile LONG *)&g_overlay_view->sequence);
}
static void overlay_command(WPARAM command, LPARAM value) {
    if (!G.game || !session_active()) return;
    switch (command) {
    case DD_OVERLAY_LAUNCHER:
        ShowWindow(G.wnd, SW_RESTORE);
        SetForegroundWindow(G.wnd);
        break;
    case DD_OVERLAY_LOGS:
        do_logs();
        break;
    case DD_OVERLAY_COPY_CODE:
        do_copy_code();
        break;
    case DD_OVERLAY_CLAIM:
    case DD_OVERLAY_RELEASE:
        if ((uintptr_t)value <= UINT32_MAX && lr_owners_find(&G.core.owners, (uint32_t)value) >= 0) {
            lr_core_claim(&G.core, (uint32_t)value, command == DD_OVERLAY_CLAIM);
            G.heroes_shown = 0xffffffffu;
        }
        break;
    }
}
