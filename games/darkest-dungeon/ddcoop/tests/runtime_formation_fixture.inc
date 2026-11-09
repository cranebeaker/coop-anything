static unsigned fm_calls;
static int fm_partial;
static uint8_t __fastcall fm_default(void *raid,uint64_t *out) {
    CHECK(raid==wk_raid_lists);
    for(unsigned i=0;i<4;i++)out[i]=(uintptr_t)bt_heroes[i];
    return !memcmp(out,bt_party,32);
}
static void __fastcall fm_native(void *display) {
    CHECK(display==wk_display);fm_calls++;
    if(fm_partial)return;
    for(unsigned i=0;i<4;i++) {
        bt_party[i]=(uintptr_t)bt_heroes[i];
        uint32_t mask=1u<<i;memcpy(bt_heroes[i]+0x1088,&mask,4);
    }
    wk_display[0x4119]=1; /* Real native code keeps this set until presentation finishes. */
}
static void fm_reset(int role) {
    it_reset(role);
    wk_display[0x4118]=wk_display[0x4119]=0;
    g_formation_default=fm_default;g_formation_native=fm_native;fm_calls=fm_partial=0;
    uint64_t p=bt_party[0];bt_party[0]=bt_party[1];bt_party[1]=p;
}
static void formation_checks(void) {
    dd_msg order,request,stop;uint32_t ids[4];
    /* Host button/shortcut: same native entry, one mutation then ordered apply. */
    fm_reset(DD_ROLE_HOST);
    hook_formation_reset(wk_display);
    CHECK(fm_calls==1 && sent==1 && g_town.applied_seq==1 && last_message.u.town.action.kind==DD_ACT_FORMATION_RESET);
    order=last_message;
    CHECK(formation_ids(wk_view.raid,ids) && ids[0]==11 && ids[1]==12 && wk_display[0x4119]);
    hook_formation_reset(wk_display);CHECK(fm_calls==1 && sent==1);
    float direction[3]={1,0,0};unsigned before=wk_natives;
    hook_raid_move(wk_display,direction);
    CHECK(!direction[0] && wk_natives==before); /* Walking waits for the reorder animation. */
    wk_display[0x4119]=0;
    hook_formation_reset(wk_display);CHECK(fm_calls==1 && sent==1); /* Already default. */
    fm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && g_town.applied_seq==1 && formation_ids(wk_view.raid,ids) && ids[0]==11 && ids[1]==12);

    /* Guest initiated: no local change before approval, no duplicate request. */
    fm_reset(DD_ROLE_GUEST);
    hook_formation_reset(wk_display);request=last_message;
    hook_formation_reset(wk_display);
    CHECK(!fm_calls && sent==1 && g_formation_request && request.type==DD_MSG_TOWN_REQUEST && formation_ids(wk_view.raid,ids) && ids[0]==12);
    fm_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();order=last_message;
    CHECK(fm_calls==1 && order.type==DD_MSG_TOWN_APPLY && order.u.town.request_id==request.u.town.request_id);
    fm_reset(DD_ROLE_GUEST);
    g_formation_request=request.u.town.request_id;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && !g_formation_request && g_town.applied_seq==1);

    /* Both click: first order wins. A stale second request cannot reorder twice. */
    fm_reset(DD_ROLE_HOST);hook_formation_reset(wk_display);
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
    CHECK(g_incoming.count==1 && fm_calls==1);
    wk_display[0x4119]=0;dd_runtime_poll();
    CHECK(!g_incoming.count && fm_calls==1 && last_message.type==DD_MSG_TOWN_REJECT && !g_town.needs_resync);

    /* Delayed client presentation and combat end: wait, then apply once. */
    fm_reset(DD_ROLE_GUEST);wk_display[0x4119]=1;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();CHECK(!fm_calls && g_incoming.count==1);
    wk_display[0x4119]=0;wk_view.battle=34;dd_runtime_poll();CHECK(!fm_calls && g_incoming.count==1);
    wk_view.battle=0;dd_runtime_poll();CHECK(fm_calls==1 && g_town.applied_seq==1);

    /* No mutation for missing member/default, stale before-order or wrong area. */
    fm_reset(DD_ROLE_HOST);
    uint32_t wrong=99;memcpy(bt_heroes[0]+0x137c,&wrong,4);
    CHECK(!runtime_apply(NULL,&order.u.town.action) && !fm_calls);
    fm_reset(DD_ROLE_GUEST);wk_view.area++;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();CHECK(g_incoming.count==1 && !fm_calls);
    g_formation_wait=1;dd_runtime_poll();
    CHECK(g_town.needs_resync && !fm_calls && !g_town.applied_seq && queue_peek(&g_outgoing,&stop) && stop.type==DD_MSG_SYNC_STOP);

    /* Partial native result is never broadcast/ACKed; neither peer may continue. */
    fm_reset(DD_ROLE_HOST);fm_partial=1;
    hook_formation_reset(wk_display);
    CHECK(fm_calls==1 && !sent && !g_town.applied_seq && g_town.needs_resync && queue_peek(&g_outgoing,&stop) && stop.type==DD_MSG_SYNC_STOP);
    fm_reset(DD_ROLE_GUEST);fm_partial=1;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && !g_town.applied_seq && g_town.needs_resync);

    /* Equal rights, but only outside combat/loading/doors and in the chosen slot. */
    for(int role=DD_ROLE_HOST;role<=DD_ROLE_GUEST;role++) {
        fm_reset(role);wk_view.battle=29;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
        wk_view.battle=0;g_walk_door_until=GetTickCount64()+1000;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
        g_walk_door_until=0;wk_view.state=6;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
    }
    fm_reset(DD_ROLE_OFF);hook_formation_reset(wk_display);CHECK(fm_calls==1 && !sent);
    fm_reset(DD_ROLE_HOST);g_campaign_active=0;hook_formation_reset(wk_display);CHECK(fm_calls==1 && !sent);
    fm_reset(DD_ROLE_HOST);g_session_ready=0;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
    fm_reset(DD_ROLE_HOST);g_formation_default=NULL;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
    g_formation_native=NULL;wk_display[0x4119]=0;
    puts("Formation reset: HOST/GUEST intent before mutation, exact persistent IDs/native default, one ordered native call, crossed/duplicate clicks, delayed animation/combat, area/slot/session guards, native partial failure and pre-travel divergence stop verified");
}
