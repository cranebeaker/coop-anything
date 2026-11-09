/* Membership from this launcher's tracked game only. This never edits owners. */
#include "../../src/dd_party_wire.h"
static HANDLE g_live_party_mapping;
static const dd_party_snapshot *g_live_party_view;
static DWORD g_live_party_pid;
static void live_party_close(void) {
    if (g_live_party_view) UnmapViewOfFile(g_live_party_view);
    if (g_live_party_mapping) CloseHandle(g_live_party_mapping);
    g_live_party_view = NULL; g_live_party_mapping = NULL; g_live_party_pid = 0;
}
static void live_party_refresh(void) {
    DWORD pid = G.game ? GetProcessId(G.game) : 0;
    if (pid != g_live_party_pid) live_party_close();
    if (pid && !g_live_party_view) {
        wchar_t name[96];
        swprintf(name, 96, L"Local\\DDCoopParty-%lu", pid);
        g_live_party_mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
        if (g_live_party_mapping)
            g_live_party_view = MapViewOfFile(g_live_party_mapping, FILE_MAP_READ, 0, 0, sizeof(dd_party_snapshot));
        if (g_live_party_view) g_live_party_pid = pid;
        else live_party_close();
    }
    dd_party_snapshot next = {0};
    int count = -1;
    if (pid && dd_party_read(g_live_party_view, pid, now_ms(), &next) && next.mode != DD_PARTY_UNKNOWN)
        count = (int)next.count;
    if (count < 0) memset(next.ids, 0, sizeof next.ids);
    if (G.live_party_count != count || memcmp(G.live_party_ids, next.ids, sizeof next.ids)) {
        G.live_party_count = count;
        memcpy(G.live_party_ids, next.ids, sizeof next.ids);
        G.heroes_shown = 0xffffffffu; /* Party changes redraw independently of owner revisions. */
    }
}
