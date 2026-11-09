#include "dd_party_wire.h"
static HANDLE g_party_mapping;
static dd_party_snapshot *g_party_view;
static uint64_t g_party_publish_at;
static void party_publish(const walk_view *v) {
    uint64_t now = GetTickCount64();
    if (now < g_party_publish_at) return;
    g_party_publish_at = now + 250;
    if (!g_party_view) {
        wchar_t name[96];
        swprintf(name, 96, L"Local\\DDCoopParty-%lu", GetCurrentProcessId());
        g_party_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(dd_party_snapshot), name);
        if (!g_party_mapping) return;
        g_party_view = MapViewOfFile(g_party_mapping, FILE_MAP_WRITE, 0, 0, sizeof(dd_party_snapshot));
        if (!g_party_view) { CloseHandle(g_party_mapping); g_party_mapping = NULL; return; }
    }
    dd_party_snapshot next = {0};
    next.magic = DD_PARTY_MAGIC; next.version = DD_PARTY_VERSION; next.size = sizeof next;
    next.pid = GetCurrentProcessId(); next.updated = now;
    uint32_t ids[4] = {0};
    if (v) {
        uint64_t heroes[4];
        uint32_t hp[4], stress[4], trinkets[4];
        if (walk_party_read(v->raid, heroes, ids, hp, stress, trinkets)) next.mode = DD_PARTY_RAID;
    } else {
        depart_view town;
        dd_hamlet model;
        if (g_depart_view(&town) && town.town && !town.raid &&
            g_hamlet_open_model(&model) && model.campaign == town.campaign && dd_hamlet_party(&model, ids)) next.mode = DD_PARTY_TOWN;
    }
    if (next.mode)
        for (unsigned i = 0; i < 4; i++) if (ids[i] && ids[i] != UINT32_MAX) next.ids[next.count++] = ids[i];
    if (!dd_party_valid(&next, next.pid, now)) { next.mode = next.count = 0; memset(next.ids, 0, sizeof next.ids); }
    dd_party_write(g_party_view, &next);
}
