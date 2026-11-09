/* The completed-quest seal is a stateless callback, separate from the two
 * completion-popup choices. Its native body schedules game state9 (results).
 * Hook its specific std::_Func vtable slot: the 4-byte prologue is followed by
 * RIP-relative code, so the ordinary byte-copy trampoline is unsuitable. */
typedef void(__fastcall *quest_return_fn)(void *);
static quest_return_fn g_quest_return;
static void **g_quest_return_slot;
static int g_quest_return_done;
static uint32_t g_quest_return_request;
static void quest_return_reset(void) {
    g_quest_return_done = 0;
    g_quest_return_request = 0;
    g_results_display = g_results_asked_at = 0;
}
static int quest_return_ready(const walk_view *v) {
    uint32_t choice = 0;
    /* DrawQuestSeal itself renders this button only after choosing to keep
     * exploring a completed mission (QuestInfo+14c == 2). */
    return !v->battle && safe_read((uintptr_t)v->display + DD_ITEM_QUEST_INFO + 0x14c, &choice, 4) && choice == 2;
}
static int quest_return_apply(const dd_action *a) {
    walk_view v;
    if (g_quest_return_done) return g_cfg.role == DD_ROLE_GUEST; /* Guest acknowledgements are harmless; late requests refuse. */
    if (!g_quest_return || !g_walk_view(&v) || !quest_return_ready(&v)) {
        dd_log("Quest return: completed mission not ready here (role=%s)", dd_role_name(g_cfg.role));
        return 0;
    }
    (void)a; /* This callback captures no object or pointers from the sender. */
    if (g_cfg.role == DD_ROLE_HOST) results_send_rules();
    g_quest_return_done = 1;
    dd_log("Quest return: leaving completed dungeon together (role=%s)", dd_role_name(g_cfg.role));
    g_quest_return(NULL);
    return 1;
}
static void __fastcall hook_quest_return(void *functor) {
    walk_view v;
    dd_action a;
    if (!g_quest_return) return;
    if (g_game_apply_depth || g_cfg.role == DD_ROLE_OFF || !InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        g_quest_return(functor);
        return;
    }
    if (!walk_shared()) { (void)runtime_shared_blocked(); return; }
    if (g_quest_return_done || g_quest_return_request || !g_walk_view(&v) || !quest_return_ready(&v) ||
        !item_order(DD_ITEM_OP_QUEST_RETURN, &a, &v)) return;
    dd_log("Quest return: completion seal chosen (role=%s)", dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) g_quest_return_request = dd_town_guest_local(&g_town, &a);
    else (void)dd_town_host_local(&g_town, &a);
}
static void quest_return_request_done(uint32_t request) {
    if (request && request == g_quest_return_request) g_quest_return_request = 0;
}
static int quest_return_gate_ready(const walk_view *v) {
    return g_quest_return_done || quest_return_ready(v);
}
/* Publish the native function before the replacement can be called. A different
 * slot, unexpected native value, or a competing modification is never replaced. */
static int quest_return_bind_slot(void **slot, uintptr_t expected) {
    void *value = NULL;
    if (!slot || !expected || !safe_read((uintptr_t)slot, &value, sizeof value)) return 0;
    if (g_quest_return_slot)
        return slot == g_quest_return_slot && value == (void *)hook_quest_return && (uintptr_t)g_quest_return == expected;
    if ((uintptr_t)value != expected || expected == (uintptr_t)hook_quest_return) return 0;
    DWORD old = 0, ignored = 0;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) return 0;
    g_quest_return = (quest_return_fn)expected;
    value = InterlockedCompareExchangePointer((PVOID volatile *)slot, (void *)hook_quest_return, (void *)expected);
    VirtualProtect(slot, sizeof *slot, old, &ignored);
    if ((uintptr_t)value != expected) { g_quest_return = NULL; return 0; }
    g_quest_return_slot = slot;
    return 1;
}
static int quest_return_bind(void) {
    uintptr_t slot = image_base() + 0xecc4c8u; /* DrawQuestSeal lambda_1 vtable +10, normal game only. */
    uintptr_t expected = hamlet_address("quest_return");
    if (!image_range(slot, sizeof(void *)) || !quest_return_bind_slot((void **)slot, expected)) return 0;
    dd_log("Quest return: completed-mission seal callback ready");
    return 1;
}
