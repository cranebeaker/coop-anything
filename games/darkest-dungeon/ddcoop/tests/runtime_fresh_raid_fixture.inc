static unsigned fresh_restores,fresh_old_raids,fresh_new_raids;
static uint8_t __fastcall fresh_restore(void) { fresh_restores++;return 1; }
static void __fastcall fresh_reseed(void *p,uint32_t seed) { CHECK(p==dp_rules && seed);dp_rules_seed=seed; }
static uintptr_t __fastcall fresh_start(void *app) {
    CHECK(app==dp_app);dp_starts++;
    if(hook_raid_restore()) { fresh_old_raids++;return 99; }
    fresh_new_raids++;return hook_raid_map(NULL,NULL,NULL);
}
static void fresh_raid_checks(void) {
    dd_action orders[2];
    dp_reset(DD_ROLE_HOST);
    g_raid_restore=fresh_restore;g_raid_start=fresh_start;g_rules_reseed=fresh_reseed;
    fresh_restores=fresh_old_raids=fresh_new_raids=0;
    memset(dp_app+0x1720,0,8);
    hook_raid_start(dp_app); /* Initial borrowed mid-fight save must still restore. */
    CHECK(fresh_restores==1 && fresh_old_raids==1 && !fresh_new_raids && !g_departure_fresh);
    for(unsigned raid=0;raid<2;raid++) {
        dp_world(0x71717171+raid,20+raid*4);
        hook_raid_start(dp_app);orders[raid]=last_message.u.town.action;
        CHECK(fresh_restores==1 && fresh_new_raids==raid+1 && dp_maps==raid+1 && !g_departure_fresh);
        CHECK(orders[raid].kind==DD_ACT_DEPARTURE && orders[raid].u.departure.seed==dp_map_seed[raid]);
    }
    CHECK(orders[0].u.departure.seed!=orders[1].u.departure.seed);
    dp_reset(DD_ROLE_GUEST);
    g_raid_restore=fresh_restore;g_raid_start=fresh_start;g_rules_reseed=fresh_reseed;
    fresh_restores=fresh_old_raids=fresh_new_raids=0;
    memset(dp_app+0x1720,0,8);hook_raid_start(dp_app);
    CHECK(fresh_restores==1 && fresh_old_raids==1);
    for(unsigned raid=0;raid<2;raid++) {
        /* The checkpoint on disk never changes on this -nosave guest. */
        dp_world(orders[raid].u.departure.quest,orders[raid].u.departure.heroes[0]);
        CHECK(departure_apply(&orders[raid]));hook_raid_start(dp_app);
        CHECK(fresh_restores==1 && fresh_old_raids==1 && fresh_new_raids==raid+1 && dp_maps==raid+1);
        CHECK(dp_map_seed[raid]==orders[raid].u.departure.seed && dp_rules_seed==orders[raid].u.departure.rules);
        CHECK(!g_departure_fresh && !g_departure_seed && !g_town.needs_resync);
    }
    CHECK(hook_raid_restore()==1 && fresh_restores==2); /* Scope never leaks. */
    dp_reset(DD_ROLE_GUEST);g_raid_restore=fresh_restore;g_raid_start=fresh_start;g_rules_reseed=fresh_reseed;
    fresh_restores=fresh_new_raids=0;CHECK(departure_apply(&orders[0]));dp_world(0x999,20);
    hook_raid_start(dp_app);CHECK(!fresh_restores && !fresh_new_raids && g_town.needs_resync);
    dp_reset(DD_ROLE_OFF);g_raid_restore=fresh_restore;g_raid_start=fresh_start;
    fresh_restores=0;hook_raid_start(dp_app);CHECK(fresh_restores==1 && !g_departure_fresh);
    g_raid_restore=NULL;g_raid_start=dp_start;g_rules_reseed=dp_reseed;
    puts("Fresh raid lifecycle: initial checkpoint restores; two later approved raids use fresh maps on host/guest despite unchanged guest disk; matching seeds/party, scope cleanup, wrong mission refusal and passive restore verified");
}
