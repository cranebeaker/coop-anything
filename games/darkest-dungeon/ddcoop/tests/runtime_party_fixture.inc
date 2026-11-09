/* Actual live reader/publication and a read-only launcher-shaped mapping. */
static int lp_town(depart_view *v) {
    memset(v, 0, sizeof *v);
    v->town = 1; v->campaign = (uintptr_t)world_campaign;
    return 1;
}
static void party_publication_checks(void) {
    wk_reset(DD_ROLE_HOST);
    wk_party_state(30.0f, 40.0f, 0x600);
    uint32_t ids[4] = {1, 2, 15, 7};
    for (unsigned i = 0; i < 4; i++) putint(wk_heroes[i] + 0x137c, ids[i]);
    g_party_publish_at = 0;
    party_publish(&wk_view);
    wchar_t name[96];
    swprintf(name, 96, L"Local\\DDCoopParty-%lu", GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
    CHECK(mapping != NULL);
    const dd_party_snapshot *view = mapping ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(dd_party_snapshot)) : NULL;
    CHECK(view != NULL);
    if (view) {
        dd_party_snapshot report;
        CHECK(dd_party_read(view, GetCurrentProcessId(), GetTickCount64(), &report) &&
              report.mode == DD_PARTY_RAID && report.count == 4 && !memcmp(report.ids, ids, sizeof ids));
        uint64_t swap = wk_party[0]; wk_party[0] = wk_party[3]; wk_party[3] = swap;
        g_party_publish_at = 0; party_publish(&wk_view);
        CHECK(dd_party_read(view, GetCurrentProcessId(), GetTickCount64(), &report) &&
              report.ids[0] == 7 && report.ids[3] == 1); /* Formation, not owner mask. */
        CHECK(!dd_party_read(view, GetCurrentProcessId() + 1, GetTickCount64(), &report));
        int (*original_view)(depart_view *) = g_depart_view;
        g_depart_view = dp_view; dp_in_raid = 1;
        g_party_publish_at = 0; party_publish(NULL);
        CHECK(dd_party_read(view, GetCurrentProcessId(), GetTickCount64(), &report) &&
              report.mode == DD_PARTY_UNKNOWN && !report.count && !report.ids[0]);
        dp_in_raid = 0;
        hm_reset(DD_ROLE_HOST); g_depart_view = lp_town;
        hm_party[0] = 3; hm_party[1] = 1;
        g_party_publish_at = 0; party_publish(NULL);
        CHECK(dd_party_read(view, GetCurrentProcessId(), GetTickCount64(), &report) &&
              report.mode == DD_PARTY_TOWN && report.count == 2 && report.ids[0] == 3 && report.ids[1] == 1);
        g_depart_view = original_view;
        UnmapViewOfFile(view);
    }
    if (mapping) CloseHandle(mapping);
    CHECK(!wk_moves && !sent); /* Publication cannot move the party or change ownership. */
    puts("Live party publication: actual raid persistent IDs/ranks, read-only consumer, current PID and unknown/loading clears verified");
}
