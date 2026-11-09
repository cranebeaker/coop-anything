/* Real normal callback dispatch + intent acknowledgement + seeded departure.
 * A closed HOST screen is represented by the background shop, as in owner21:54. */
static int em_read(depart_view *v) { return dp_view(v) && !v->raid; }
static void em_reset(int role) {
    dp_reset(role);
    g_prepare_read = em_read;
    dp_world(0xa001, 8);
    g_supply_quest_known = 1; g_supply_quest_index = 1; g_supply_quest_key = 0xa001;
    g_supply_bg = role == DD_ROLE_HOST;
}
static void embark_transactions(void) {
    void **slot = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(slot != NULL);
    if (!slot) return;
    slot[0] = slot[1] = (void *)dp_depart;
    DWORD old = 0;
    CHECK(VirtualProtect(slot, 4096, PAGE_READONLY, &old));
    CHECK(!embark_bind_slot(slot, (uintptr_t)dp_start));
    CHECK(embark_bind_slot(slot, (uintptr_t)dp_depart));
    CHECK(embark_bind_slot(slot, (uintptr_t)dp_depart));
    CHECK(!embark_bind_slot(slot + 1, (uintptr_t)dp_depart) && slot[1] == (void *)dp_depart);

    em_reset(DD_ROLE_GUEST);
    ((town_depart_fn)*slot)(); ((town_depart_fn)*slot)();
    CHECK(sent == 1 && !dp_departs && !dp_starts && !dp_maps && g_embark_request);
    dd_msg request = last_message, decoded;
    CHECK(request.type == DD_MSG_TOWN_REQUEST && request.u.town.action.kind == DD_ACT_EMBARK);
    uint8_t wire[512]; size_t used = 0, length = dd_encode(&request, wire, sizeof wire);
    CHECK(length && dd_decode(wire, length, &decoded, &used) == DD_DECODE_OK && used == length);
    em_reset(DD_ROLE_HOST);
    CHECK(g_supply_bg && queue_push(&g_incoming, &decoded)); dd_runtime_poll();
    CHECK(dp_departs == 1 && dp_party_calls == 1 && !dp_maps && sent == 1 && g_embark_leaving);
    dd_msg ack = last_message;
    CHECK(ack.type == DD_MSG_TOWN_APPLY && ack.u.town.action.kind == DD_ACT_EMBARK && ack.seq == 1);
    /* Repeated local callbacks or a crossed guest request cannot start twice. */
    ((town_depart_fn)*slot)();
    CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
    CHECK(dp_departs == 1 && g_town.rejected == 1);
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && sent == 3 && last_message.u.town.action.kind == DD_ACT_DEPARTURE);
    dd_msg leave = last_message;
    CHECK(leave.seq == 2 && dp_map_seed[0] == leave.u.town.action.u.departure.seed);
    uint32_t host_rules = dp_rules_seed;
    em_reset(DD_ROLE_GUEST); ((town_depart_fn)*slot)();
    CHECK(queue_push(&g_incoming, &ack)); dd_runtime_poll();
    CHECK(!dp_departs && !dp_maps && g_embark_request); /* Ack is not permission to invent a map. */
    CHECK(queue_push(&g_incoming, &leave)); dd_runtime_poll();
    CHECK(dp_departs == 1 && !g_embark_request && !dp_maps && !g_town.needs_resync);
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && !dp_native_calls && dp_map_seed[0] == leave.u.town.action.u.departure.seed && dp_rules_seed == host_rules);

    /* HOST input uses the same preflight and GAST follows without clicking. */
    em_reset(DD_ROLE_HOST); ((town_depart_fn)*slot)(); ((town_depart_fn)*slot)();
    CHECK(dp_departs == 1 && sent == 1);
    ack = last_message; hook_raid_start(dp_app); leave = last_message;
    em_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &ack) && queue_push(&g_incoming, &leave)); dd_runtime_poll();
    CHECK(dp_departs == 1 && !g_town.needs_resync && !dp_maps);
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && dp_map_seed[0] == leave.u.town.action.u.departure.seed);

    /* Pending purchases finish before sending the single departure request. */
    em_reset(DD_ROLE_GUEST); g_supply_request = 55;
    ((town_depart_fn)*slot)(); ((town_depart_fn)*slot)();
    CHECK(g_embark_local_pending && !sent && !dp_departs);
    g_supply_request = 0; embark_poll(); embark_poll();
    CHECK(sent == 1 && g_embark_request && !g_embark_local_pending && !dp_departs);
    dd_msg reject = {0}; reject.type = DD_MSG_TOWN_REJECT;
    reject.u.reject.request_id = g_embark_request; reject.u.reject.reason = DD_REJECT_NOT_ALLOWED;
    CHECK(queue_push(&g_incoming, &reject)); dd_runtime_poll();
    CHECK(!g_embark_request && !dp_departs);

    /* Stale/unknown mission, invalid party, wrong mode, missing seed binding. */
    for (unsigned bad = 0; bad < 5; bad++) {
        em_reset(DD_ROLE_HOST);
        if (bad == 0) g_supply_quest_key++;
        if (bad == 1) dp_party_ok = 0;
        if (bad == 2) dp_in_raid = 1;
        if (bad == 3) g_rand_s_native = NULL;
        if (bad == 4) g_supply_quest_known = 0;
        CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
        CHECK(!dp_departs && !dp_maps && g_town.rejected == 1);
    }
    em_reset(DD_ROLE_GUEST); g_session_ready = 0;
    ((town_depart_fn)*slot)(); CHECK(!sent && !dp_departs);
    em_reset(DD_ROLE_GUEST); g_campaign_active = 0;
    ((town_depart_fn)*slot)(); CHECK(!sent && dp_departs == 1);
    em_reset(DD_ROLE_OFF); ((town_depart_fn)*slot)(); CHECK(!sent && dp_departs == 1);
    embark_unbind(); CHECK(*slot == (void *)dp_depart);
    VirtualFree(slot, 0, MEM_RELEASE);
    g_prepare_read = prepare_read;
    g_depart_view = depart_read; g_depart_party = depart_party;
    g_town_depart = NULL; g_rules_reseed = NULL; g_rules_object = NULL;
    g_rand_s_native = NULL; g_raid_start = NULL; g_raid_map = NULL;
    puts("Embark parity: protected callback, GAST first/HOST background, HOST first, approval before map, equal seeds, pending buys, double/crossed clicks and refusal guards verified");
}
