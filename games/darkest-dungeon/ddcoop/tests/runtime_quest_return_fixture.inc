/* Dispatch through the real replacement vtable slot, then the real FIFO/wire.
 * Only the native SetNextState callback and result-screen methods are faked. */
static unsigned qr_calls;
static uint32_t qr_state, qr_rules_at_return;
static uint8_t qr_rules[0x20];
static void __fastcall qr_native(void *unused) {
    (void)unused;
    qr_calls++;
    qr_state = 9;
    qr_rules_at_return = battle_rules();
}
static void qr_ready(int ready_here) {
    uint32_t choice = ready_here ? 2 : 0;
    memcpy(wk_display + DD_ITEM_QUEST_INFO + 0x14c, &choice, 4);
}
static void qr_reset(int role) {
    bt_reset(role);
    items_reset();
    wk_view.battle = 0;
    qr_ready(1);
    memset(qr_rules, 0, sizeof qr_rules);
    g_rules_object = qr_rules;
    uint32_t seed = role == DD_ROLE_HOST ? 0x12345678u : 0x87654321u;
    memcpy(qr_rules + 8, &seed, 4);
    qr_calls = qr_state = qr_rules_at_return = 0;
}
static void quest_return_transactions(void) {
    void **table = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(table != NULL);
    if (!table) return;
    void **slot = table + 2;
    *slot = (void *)qr_native;
    table[3] = (void *)qr_native;
    DWORD old = 0;
    CHECK(VirtualProtect(table, 4096, PAGE_READONLY, &old));
    CHECK(!quest_return_bind_slot(slot, (uintptr_t)it_all) && *slot == (void *)qr_native);
    CHECK(quest_return_bind_slot(slot, (uintptr_t)qr_native));
    CHECK(*slot == (void *)hook_quest_return && g_quest_return == qr_native);
    CHECK(quest_return_bind_slot(slot, (uintptr_t)qr_native));
    CHECK(!quest_return_bind_slot(table + 3, (uintptr_t)qr_native) && table[3] == (void *)qr_native);
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery(slot, &mbi, sizeof mbi) && mbi.Protect == PAGE_READONLY);

    /* HOST: one native transition, with Rules preceding the return order. */
    qr_reset(DD_ROLE_HOST);
    ((quest_return_fn)*slot)(NULL);
    ((quest_return_fn)*slot)(NULL);
    CHECK(qr_calls == 1 && qr_state == 9 && g_quest_return_done && sent == 2 && g_town.applied_seq == 2);
    dd_msg rules = sent_messages[0], apply = sent_messages[1];
    CHECK(rules.u.town.action.u.item.op == DD_ITEM_OP_RULES && rules.seq == 1);
    CHECK(apply.type == DD_MSG_TOWN_APPLY && apply.seq == 2 && apply.u.town.action.u.item.op == DD_ITEM_OP_QUEST_RETURN);
    uint8_t wire[DD_PROTO_HEADER + DD_PROTO_MAX_BODY]; size_t used = 0;
    dd_msg decoded;
    size_t length = dd_encode(&apply, wire, sizeof wire);
    CHECK(length && dd_decode(wire, length, &decoded, &used) == DD_DECODE_OK && used == length &&
          decoded.u.town.action.u.item.op == DD_ITEM_OP_QUEST_RETURN);
    qr_reset(DD_ROLE_GUEST); qr_ready(0);
    CHECK(queue_push(&g_incoming, &rules) && queue_push(&g_incoming, &decoded));
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 1 && g_incoming.count == 1 && !qr_calls);
    qr_ready(1); dd_runtime_poll();
    CHECK(qr_calls == 1 && qr_state == 9 && qr_rules_at_return == 0x12345678u &&
          g_town.applied_seq == 2 && !g_town.needs_resync && !g_incoming.count);
    /* The existing shared Next route follows the new seal route. */
    g_results_setup = it_setup; g_results_next = it_next; g_results_can = it_results_can;
    it_can = 1; it_nexts = 0;
    hook_results_setup(it_results, 0, 0, 0);
    dd_msg next = apply; next.seq = 3; next.u.town.action.u.item.op = DD_ITEM_OP_RESULTS_NEXT;
    CHECK(queue_push(&g_incoming, &next)); dd_runtime_poll();
    CHECK(it_nexts == 1 && g_town.applied_seq == 3 && !g_town.needs_resync);
    g_results_setup = NULL; g_results_next = NULL; g_results_can = results_can_native;

    /* GUEST: wait for approval; host waits for quest completion, then orders both. */
    qr_reset(DD_ROLE_GUEST);
    ((quest_return_fn)*slot)(NULL); ((quest_return_fn)*slot)(NULL);
    CHECK(!qr_calls && sent == 1 && g_quest_return_request);
    dd_msg request = last_message;
    CHECK(request.type == DD_MSG_TOWN_REQUEST && dd_action_valid(&request.u.town.action));
    qr_reset(DD_ROLE_HOST); qr_ready(0);
    CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
    CHECK(!qr_calls && !sent && g_incoming.count == 1);
    qr_ready(1); dd_runtime_poll();
    CHECK(qr_calls == 1 && qr_state == 9 && sent == 2 && !g_incoming.count);
    rules = sent_messages[0]; apply = sent_messages[1];
    CHECK(apply.u.town.request_id == request.u.town.request_id && apply.seq == 2);
    /* A crossed or late second request cannot schedule results twice. */
    CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
    CHECK(qr_calls == 1 && g_town.rejected == 1 && last_message.type == DD_MSG_TOWN_REJECT);
    qr_reset(DD_ROLE_GUEST); ((quest_return_fn)*slot)(NULL);
    CHECK(queue_push(&g_incoming, &rules) && queue_push(&g_incoming, &apply)); dd_runtime_poll();
    CHECK(qr_calls == 1 && !g_quest_return_request && !g_town.needs_resync && qr_rules_at_return == 0x12345678u);
    apply.seq = 3;
    CHECK(queue_push(&g_incoming, &apply)); dd_runtime_poll();
    CHECK(qr_calls == 1 && g_town.applied_seq == 3 && !g_town.needs_resync);
    /* A new raid is allowed to return once again; no old results display survives. */
    quest_return_reset();
    CHECK(!g_quest_return_done && !g_quest_return_request && !g_results_display);
    apply.seq = 4;
    CHECK(queue_push(&g_incoming, &apply)); dd_runtime_poll();
    CHECK(qr_calls == 2 && g_town.applied_seq == 4);

    qr_reset(DD_ROLE_GUEST); ((quest_return_fn)*slot)(NULL);
    dd_msg reject = {0}; reject.type = DD_MSG_TOWN_REJECT;
    reject.u.reject.request_id = g_quest_return_request; reject.u.reject.reason = DD_REJECT_NOT_ALLOWED;
    CHECK(queue_push(&g_incoming, &reject)); dd_runtime_poll();
    CHECK(!g_quest_return_request && !qr_calls);
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        qr_reset(role); qr_ready(0); ((quest_return_fn)*slot)(NULL);
        CHECK(!qr_calls && !sent);
        qr_ready(1); wk_view.battle = DD_BATTLE_CHOOSE_SKILL_STATE;
        ((quest_return_fn)*slot)(NULL); CHECK(!qr_calls && !sent);
        wk_view.battle = 0; g_session_ready = 0;
        ((quest_return_fn)*slot)(NULL); CHECK(!qr_calls && !sent);
        qr_reset(role); g_campaign_active = 0;
        ((quest_return_fn)*slot)(NULL); CHECK(qr_calls == 1 && !sent);
    }
    qr_reset(DD_ROLE_OFF); ((quest_return_fn)*slot)(NULL);
    CHECK(qr_calls == 1 && !sent);
    dd_action bad = apply.u.town.action;
    bad.u.item.state = 1; CHECK(!dd_action_valid(&bad));
    bad.u.item.state = 0; bad.u.item.hero = 0; CHECK(!dd_action_valid(&bad));
    bad.u.item.hero = 255; bad.u.item.flag = 1; CHECK(!dd_action_valid(&bad));
    CHECK(VirtualProtect(table, 4096, PAGE_READWRITE, &old));
    *slot = (void *)qr_native;
    g_quest_return_slot = NULL; g_quest_return = NULL;
    VirtualFree(table, 0, MEM_RELEASE);
    g_rules_object = NULL;
    g_walk_view = walk_read;
    bt_unwire();
    puts("Quest seal: exact vtable dispatch, both directions, Rules before results, delayed readiness, shared Next, first click, refusals, reset and passive campaigns verified");
}
