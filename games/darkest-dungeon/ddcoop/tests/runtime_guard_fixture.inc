/* Guard rails: nothing that would change one copy only, and an honest connection state. */
static void guard_transactions(void) {
    /* Supplies before the connection: refused with a message each time; nothing changes. */
    sw_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_session_seen = 0;
    sw_click_buy(0);
    CHECK(!sw_buys && *sw_gold() == 1000 && !sent && alerts == 1);
    sw_click_buy(0);
    CHECK(!sw_buys && alerts == 2);
    sw_click_return(0);
    CHECK(!sw_returns && alerts == 3);
    /* Leaving the panel stays possible: nothing was bought. */
    hook_supply_back(NULL);
    CHECK(sw_backs == 1 && sw_cancels == 1 && !sent);
    /* Same for the guest. */
    sw_reset(DD_ROLE_GUEST);
    g_session_ready = 0;
    g_session_seen = 0;
    sw_click_buy(0);
    CHECK(!sw_buys && !sent && alerts == 1);
    /* Connected: the same action goes through, and the panel says so. */
    sw_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_session_seen = 0;
    dd_runtime_poll();
    CHECK(overlay_state == DD_OVERLAY_WAITING);
    sw_click_buy(0);
    CHECK(!sw_buys && alerts == 1);
    g_session_ready = g_session_seen = 1;
    dd_runtime_poll();
    CHECK(overlay_state == DD_OVERLAY_CONNECTED && !g_shop_notice[0]);
    sw_click_buy(0);
    CHECK(sw_buys == 1 && sent == 1);
    /* Without a co-op session the game is the game. */
    sw_reset(DD_ROLE_OFF);
    g_session_ready = 0;
    sw_click_buy(0);
    CHECK(sw_buys == 1 && !alerts);
    /* Before the campaign is loaded nothing is refused either. */
    sw_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_campaign_active = 0;
    CHECK(!runtime_waiting_for_peer() && !alerts);
    /* A native action the bridge itself runs is never refused. */
    sw_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_game_apply_depth = 1;
    CHECK(!runtime_waiting_for_peer());
    g_game_apply_depth = 0;
    /* A lost connection reads as lost, also when a socket comes back. */
    sw_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_session_seen = 1;
    dd_runtime_poll();
    CHECK(overlay_state == DD_OVERLAY_LOST);
    g_session_lost = 0;
    /* A host that keeps refusing the guest: told once, after a few tries, gone when connected. */
    sw_reset(DD_ROLE_GUEST);
    g_session_ready = 0;
    g_session_seen = 0;
    g_host_refused_count = DD_HOST_REFUSED_NOTICE - 1;
    dd_runtime_poll();
    CHECK(overlay_state == DD_OVERLAY_WAITING && !alerts);
    g_host_refused_count = DD_HOST_REFUSED_NOTICE;
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(overlay_state == DD_OVERLAY_REFUSED && alerts == 1 && !g_town.needs_resync);
    g_host_refused_count = 0;
    /* A guest that never reaches a host (nothing listens: the host's game runs alone): told after twenty seconds, gone when connected. */
    sw_reset(DD_ROLE_GUEST);
    g_session_ready = 0;
    g_session_seen = 0;
    g_transport_started_at = 0;
    dd_runtime_poll();
    CHECK(overlay_state == DD_OVERLAY_WAITING && !alerts);
    g_transport_started_at = (LONG64)GetTickCount64();
    dd_runtime_poll();
    CHECK(!alerts);
    g_transport_started_at = (LONG64)(GetTickCount64() - DD_HOST_ABSENT_MS);
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(alerts == 1 && !strcmp(g_shop_notice, g_host_absent) && !g_town.needs_resync);
    g_session_ready = g_session_seen = 1;
    dd_runtime_poll();
    CHECK(overlay_state == DD_OVERLAY_CONNECTED && !g_shop_notice[0]);
    /* Not for the host, and not before the campaign is loaded. */
    sw_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_session_seen = 0;
    dd_runtime_poll();
    CHECK(!alerts);
    sw_reset(DD_ROLE_GUEST);
    g_session_ready = 0;
    g_session_seen = 0;
    g_campaign_active = 0;
    runtime_publish_state();
    CHECK(!alerts);
    g_transport_started_at = 0;
    /* Upgrades and placements are also held. */
    sw_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_session_seen = 0;
    CHECK(runtime_waiting_for_peer() && alerts == 1);
    puts("Guard rails: real actions before the connection refused with a message each time, game's own load and "
         "local Back unaffected, connection state honest");
}
