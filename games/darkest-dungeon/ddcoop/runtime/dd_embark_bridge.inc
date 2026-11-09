/* Normal TownDisplay::ProgressForwardInternal completion callback. The callback
 * only schedules state14; RaidStart later supplies the authoritative seeds.
 * Intercept BEFORE scheduling, so a guest cannot start a private map first. */
static void **g_embark_slot;
static int g_embark_leaving, g_embark_local_pending;
static uint32_t g_embark_request;
static uint64_t g_embark_wait_until;
static dd_action g_embark_local;
static void embark_reset(void) {
    g_embark_leaving = g_embark_local_pending = 0;
    g_embark_request = 0;
    g_embark_wait_until = 0;
    memset(&g_embark_local, 0, sizeof g_embark_local);
}
static int embark_apply(const dd_action *a) {
    /* This is the acknowledgement of an intent, never seeds supplied by GAST.
     * GAST stays in town until the following DEPARTURE from HOST's RaidStart. */
    if (g_cfg.role == DD_ROLE_GUEST) return 1;
    depart_view v;
    if (g_cfg.role != DD_ROLE_HOST || g_embark_leaving || !supply_active() ||
        !g_town_depart || !g_rand_s_native || !g_rules_reseed ||
        !g_prepare_read(&v) || v.raid || !g_supply_quest_known ||
        a->u.prepare.index != g_supply_quest_index || a->u.prepare.key != g_supply_quest_key ||
        provisions_busy() || hamlet_party_busy() || hamlet_dismiss_busy() ||
        !depart_set_quest(&v, a->u.prepare.index, a->u.prepare.key) || !g_depart_party()) {
        dd_log("Embark: requested departure refused before scheduling");
        return 0;
    }
    g_embark_leaving = 1;
    dd_log("Embark: host starts shared mission %u key=%08x", a->u.prepare.index, a->u.prepare.key);
    g_town_depart();
    return 1;
}
static void embark_request_done(uint32_t request) {
    if (request && request == g_embark_request) {
        g_embark_request = 0;
        shop_notice("Het gezamenlijke vertrek is geweigerd. Controleer de missie en party; stuur de logs als dit blijft gebeuren.");
    }
}
static void embark_poll(void) {
    if (!g_embark_local_pending) return;
    if (!supply_active()) { g_embark_local_pending = 0; return; }
    if (GetTickCount64() > g_embark_wait_until) {
        g_embark_local_pending = 0;
        shop_notice("Het vertrek wachtte te lang op een gedeelde actie. Sluit beide spellen en start de sessie opnieuw; stuur de logs mee.");
        return;
    }
    if (runtime_shared_blocked()) return;
    g_embark_local_pending = 0;
    dd_log("Embark: shared departure chosen (role=%s)", dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) g_embark_request = dd_town_guest_local(&g_town, &g_embark_local);
    else if (!dd_town_host_local(&g_town, &g_embark_local))
        shop_notice("Het gezamenlijke vertrek kon niet worden gestart. Controleer de missie en party; stuur de logs mee.");
}
static void __fastcall hook_town_depart(void) {
    if (!g_town_depart) return;
    if (g_game_apply_depth || g_cfg.role == DD_ROLE_OFF || !InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        g_town_depart(); return;
    }
    if (!supply_active()) { (void)runtime_shared_blocked(); return; }
    if (g_embark_leaving || g_embark_request || g_embark_local_pending) return;
    depart_view v;
    if (!g_prepare_read(&v) || v.raid || !g_supply_quest_known) {
        shop_notice("Het vertrek kon hier niet worden gecontroleerd. Open de voorraad voor dezelfde missie en probeer opnieuw.");
        return;
    }
    memset(&g_embark_local, 0, sizeof g_embark_local);
    g_embark_local.kind = DD_ACT_EMBARK;
    g_embark_local.u.prepare.index = g_supply_quest_index;
    g_embark_local.u.prepare.key = g_supply_quest_key;
    g_embark_wait_until = GetTickCount64() + 30000;
    g_embark_local_pending = 1;
    embark_poll();
}
/* Like the quest seal, state14's four-byte prologue is followed by a RIP load.
 * Replace only this normal callback's Do_call slot, never its instruction bytes. */
static int embark_bind_slot(void **slot, uintptr_t expected) {
    void *value = NULL;
    if (!slot || !expected || !safe_read((uintptr_t)slot, &value, sizeof value)) return 0;
    if (g_embark_slot)
        return g_embark_slot == slot && value == (void *)hook_town_depart && (uintptr_t)g_town_depart == expected;
    if ((uintptr_t)value != expected || expected == (uintptr_t)hook_town_depart) return 0;
    DWORD old = 0, ignored = 0;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) return 0;
    g_town_depart = (town_depart_fn)expected;
    value = InterlockedCompareExchangePointer((PVOID volatile *)slot, (void *)hook_town_depart, (void *)expected);
    VirtualProtect(slot, sizeof *slot, old, &ignored);
    if ((uintptr_t)value != expected) { g_town_depart = NULL; return 0; }
    g_embark_slot = slot;
    return 1;
}
static void embark_unbind(void) {
    DWORD old = 0, ignored = 0;
    if (!g_embark_slot || !VirtualProtect(g_embark_slot, sizeof *g_embark_slot, PAGE_READWRITE, &old)) return;
    InterlockedCompareExchangePointer((PVOID volatile *)g_embark_slot, (void *)g_town_depart, (void *)hook_town_depart);
    VirtualProtect(g_embark_slot, sizeof *g_embark_slot, old, &ignored);
    g_embark_slot = NULL;
}
static int embark_bind(void) {
    uintptr_t slot = image_base() + 0xec3e00u;
    if (!image_range(slot, sizeof(void *)) || !embark_bind_slot((void **)slot, hamlet_address("town_depart"))) return 0;
    dd_log("Embark: normal departure callback ready for host and guest");
    return 1;
}
