/* Normal provisions button, audited in exact build28385 at 6e7a70.
 * It reads a local functor's +8 TownDisplay, opens its embedded ProvisionSelect
 * through 7f6a50, then selects app state 0x0a. No Circus function is used.
 * Only the copy that clicked opens its screen. The order opens the shop on the other copy, in its model: see dd_supplies_bridge.inc. */
typedef void(__fastcall *prepare_open_fn)(void *);
static prepare_open_fn g_prepare_open;
static uint64_t g_prepare_wait_until;
/* This copy clicked: the host while it applies its own order, the guest for as long as its request can be on its way. */
static int g_prepare_host_click;
static uint64_t g_prepare_own_until;
#define DD_PREPARE_OWN_MS 5000u
static void prepare_reset(void) {
    g_prepare_wait_until = 0;
    g_prepare_host_click = 0;
    g_prepare_own_until = 0;
}
static int prepare_read(depart_view *v) {
    uint64_t town_type = 0, panel_type = 0;
    uint8_t circus = 1;
    return g_depart_view(v) && v->town && !v->raid &&
        safe_read(image_base() + 0x2a6d4bb, &circus, 1) && !circus &&
        safe_read((uintptr_t)v->town, &town_type, 8) && town_type == image_base() + 0xec37a8 &&
        safe_read((uintptr_t)v->town + 0x3770, &panel_type, 8) && panel_type == image_base() + 0xebdde8;
}
static int (*g_prepare_read)(depart_view *) = prepare_read;
static int prepare_apply(const dd_action *a) {
    depart_view v;
    uint32_t state = 0;
    if (!g_prepare_open || !g_prepare_read(&v) ||
        !safe_read((uintptr_t)v.town + 0x37c8, &state, 4)) return 0;
    /* An already open panel must not be refilled or have its mission changed. */
    if (state >= 1 && state <= 4) return v.quest == (int32_t)a->u.prepare.index && v.key == a->u.prepare.key;
    int own = g_prepare_host_click || GetTickCount64() < g_prepare_own_until;
    if (own) g_prepare_own_until = 0;
    if (!depart_set_quest(&v, a->u.prepare.index, a->u.prepare.key)) return 0;
    g_supply_quest_known = 1;
    g_supply_quest_index = a->u.prepare.index;
    g_supply_quest_key = a->u.prepare.key;
    dd_log("Prepare: shop for mission %u key=%08x (role=%s, %s)", a->u.prepare.index, a->u.prepare.key, dd_role_name(g_cfg.role), own ? "opening here" : "in the background");
    if (!own) return supply_bg_begin();
    uint64_t functor[2] = {0, v.town};
    g_prepare_open(functor);
    return 1;
}
static void __fastcall hook_prepare_open(void *functor) {
    depart_view v;
    uint64_t town = 0;
    if (!g_prepare_open) return;
    if (g_game_apply_depth || !supply_active()) {
        if (!runtime_shared_blocked()) g_prepare_open(functor);
        return;
    }
    if (!g_prepare_read(&v) || !safe_read((uintptr_t)functor + 8, &town, 8) || town != v.town || (!g_supply_quest_known && (v.quest < 0 || !v.key))) {
        shop_notice("Het voorraadscherm kon niet gezamenlijk worden geopend. Kies eerst een missie en probeer opnieuw.");
        return;
    }
    if (supply_cancel_pending()) {
        dd_log("Prepare: a refund is still on its way; this click is ignored");
        return;
    }
    dd_action a = {0};
    a.kind = DD_ACT_PREPARE;
    /* A shop is open: join it, whatever mission this player has selected meanwhile. */
    a.u.prepare.index = g_supply_quest_known ? g_supply_quest_index : (uint32_t)v.quest;
    a.u.prepare.key = g_supply_quest_known ? g_supply_quest_key : v.key;
    if (g_cfg.role == DD_ROLE_GUEST) {
        g_prepare_own_until = GetTickCount64() + DD_PREPARE_OWN_MS;
        dd_town_guest_local(&g_town, &a);
    } else {
        g_prepare_host_click = 1;
        if (!dd_town_host_local(&g_town, &a))
            shop_notice("Het voorraadscherm kon niet worden geopend. Controleer de missie en stuur de logs bij een fout.");
        g_prepare_host_click = 0;
    }
}
static int prepare_gate(const dd_msg *m) {
    if ((m->type != DD_MSG_TOWN_REQUEST && m->type != DD_MSG_TOWN_APPLY) ||
        m->u.town.action.kind != DD_ACT_PREPARE || !dd_action_valid(&m->u.town.action)) return 0;
    if (g_supply_cancel_local) return 1; /* That refund goes first: this order then fills an empty shop. */
    depart_view v;
    if (g_prepare_read(&v)) { g_prepare_wait_until = 0; return 0; }
    /* Do not delay a stale/forged request sent from an already running raid. */
    if (g_depart_view(&v) && v.raid) { g_prepare_wait_until = 0; return 0; }
    uint64_t now = GetTickCount64();
    if (!g_prepare_wait_until) g_prepare_wait_until = now + 30000;
    if (now <= g_prepare_wait_until) return 1;
    g_prepare_wait_until = 0;
    return 0; /* Normal failed apply/rejection; never silently acknowledge. */
}
