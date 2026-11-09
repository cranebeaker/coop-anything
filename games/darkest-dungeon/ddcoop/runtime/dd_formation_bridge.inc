/* Normal dungeon default-order button AND its keyboard shortcut converge on
 * RaidDisplay::RestoreDefaultPartyOrder at76f060. Native code sets rank masks,
 * updates the party vector immediately and schedules the presentation itself.
 * No foreign pointers, direct rank/vector writes or battle AI hooks are used. */
typedef void(__fastcall *formation_native_fn)(void *);
typedef uint8_t(__fastcall *formation_default_fn)(void *, uint64_t *);
static formation_native_fn g_formation_native;
static formation_default_fn g_formation_default;
static uint32_t g_formation_request;
static uint64_t g_formation_wait;
static void formation_reset(void) { g_formation_request = 0; g_formation_wait = 0; }
static int formation_ids(uint64_t raid, uint32_t ids[4]) {
    uint64_t begin=0,end=0;
    memset(ids,0,16);
    if(!safe_read((uintptr_t)raid+8,&begin,8) || !safe_read((uintptr_t)raid+16,&end,8) ||
       end<begin || (end-begin)%8 || (end-begin)/8>4 || end==begin)return 0;
    for(unsigned i=0;i<(end-begin)/8;i++) {
        uint64_t hero=0;
        if(!safe_read((uintptr_t)begin+i*8,&hero,8))return 0;
        if(hero && (!safe_read((uintptr_t)hero+0x137c,&ids[i],4) || !ids[i] || ids[i]==UINT32_MAX))return 0;
        for(unsigned j=0;j<i;j++)if(ids[i] && ids[i]==ids[j])return 0;
    }
    return ids[0] || ids[1] || ids[2] || ids[3];
}
static int formation_animating(const walk_view *v) {
    uint8_t busy=1;
    return !safe_read((uintptr_t)v->display+0x4119,&busy,1) || busy;
}
static int formation_busy(void) {
    walk_view v;
    return g_formation_request || (g_walk_view(&v) && formation_animating(&v));
}
static int formation_ready(const walk_view *v) {
    uint64_t now=GetTickCount64(); uint8_t blocked=1;
    return !v->battle && v->state>1 && v->state!=6 && v->state!=7 && !formation_animating(v) &&
        safe_read((uintptr_t)v->display+0x4118,&blocked,1) && !blocked &&
        (!g_walk_door_pending || now>=g_walk_door_pending) && now>=g_walk_door_until;
}
static int formation_default_ids(const walk_view *v, uint32_t ids[4]) {
    uint64_t heroes[4]={0};
    if(!g_formation_default)return 0;
    (void)g_formation_default((void *)(uintptr_t)v->raid,heroes);
    memset(ids,0,16);
    for(unsigned i=0;i<4;i++)if(heroes[i] && !safe_read((uintptr_t)heroes[i]+0x137c,&ids[i],4))return 0;
    return 1; /* Caller validates uniqueness and exact membership against current party. */
}
static int formation_apply(const dd_action *a) {
    walk_view v; uint32_t ids[4],expected[4],after[4];
    if(!g_formation_native || !g_walk_view(&v) || !formation_ready(&v) ||
       v.area!=a->u.formation.area || !formation_ids(v.raid,ids))return 0;
    if(!memcmp(ids,a->u.formation.after,16))return g_cfg.role==DD_ROLE_GUEST;
    if(memcmp(ids,a->u.formation.before,16) || !formation_default_ids(&v,expected) ||
       memcmp(expected,a->u.formation.after,16)) {
        dd_log("Formation: stale party or different native default; refused (role=%s)",dd_role_name(g_cfg.role));
        return 0;
    }
    g_formation_native((void *)(uintptr_t)v.display);
    if(!formation_ids(v.raid,after) || memcmp(after,a->u.formation.after,16)) {
        dd_log("Formation: native reset did not produce expected IDs; shared actions stop");
        runtime_sync_stop(1);return 0;
    }
    dd_log("Formation: default order applied area=%08x ids=%u,%u,%u,%u (role=%s)",v.area,after[0],after[1],after[2],after[3],dd_role_name(g_cfg.role));
    return 1;
}
static void __fastcall hook_formation_reset(void *display) {
    walk_view v;dd_action a={0};
    if(!g_formation_native)return;
    if(g_game_apply_depth || g_cfg.role==DD_ROLE_OFF || !InterlockedCompareExchange(&g_campaign_active,0,0)) {
        g_formation_native(display);return;
    }
    if(runtime_shared_blocked() || runtime_waiting_for_peer() || !walk_shared() ||
       !g_walk_view(&v) || (uintptr_t)display!=v.display || !formation_ready(&v))return;
    a.kind=DD_ACT_FORMATION_RESET;a.u.formation.area=v.area;
    if(!formation_ids(v.raid,a.u.formation.before) || !formation_default_ids(&v,a.u.formation.after))return;
    if(!memcmp(a.u.formation.before,a.u.formation.after,16))return;
    if(!dd_action_valid(&a)) {
        dd_log("Formation: default order has different or unknown members; not shared");
        shop_notice("The default party order could not be verified. Nothing moved; send the logs.");
        return;
    }
    dd_log("Formation: reset requested area=%08x before=%u,%u,%u,%u after=%u,%u,%u,%u (role=%s)",v.area,
        a.u.formation.before[0],a.u.formation.before[1],a.u.formation.before[2],a.u.formation.before[3],
        a.u.formation.after[0],a.u.formation.after[1],a.u.formation.after[2],a.u.formation.after[3],dd_role_name(g_cfg.role));
    if(g_cfg.role==DD_ROLE_GUEST)g_formation_request=dd_town_guest_local(&g_town,&a);
    else (void)dd_town_host_local(&g_town,&a);
}
static void formation_request_done(uint32_t request) {
    if(request && request==g_formation_request)g_formation_request=0;
}
static int formation_gate(const dd_msg *m) {
    if((m->type!=DD_MSG_TOWN_REQUEST && m->type!=DD_MSG_TOWN_APPLY) ||
       m->u.town.action.kind!=DD_ACT_FORMATION_RESET || !dd_action_valid(&m->u.town.action))return 0;
    if(g_cfg.role==DD_ROLE_GUEST ? (m->type!=DD_MSG_TOWN_APPLY || m->seq!=g_town.applied_seq+1) : m->type!=DD_MSG_TOWN_REQUEST)return 0;
    walk_view v;uint64_t now=GetTickCount64();
    if(!g_formation_wait)g_formation_wait=now+15000;
    if((!g_walk_view(&v) || !formation_ready(&v) || v.area!=m->u.town.action.u.formation.area) && now<g_formation_wait)return 1;
    g_formation_wait=0;return 0;
}
static int formation_bind(void) {
    g_formation_default=(formation_default_fn)hamlet_address("formation_default");
    return g_formation_default!=NULL;
}
