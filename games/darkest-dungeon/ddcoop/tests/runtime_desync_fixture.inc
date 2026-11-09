/* Regressions from a two-PC run: delayed item use, unchecked repair, and a
 * battle choice silently acknowledged for a different party entry. */
#include "dd_focus.h"
static void desync_regressions(void) {
    dd_action choice, friendly, use;
    uint64_t functor[3] = {0};
    void *battle = wk_raid_lists + 0x4898;
    bt_reset(DD_ROLE_HOST);
    hook_battle_skill_button(functor);
    choice = last_message.u.town.action;
    CHECK(choice.u.battle.actor_id == 13);
    hook_battle_friend(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_party[0]);
    friendly = last_message.u.town.action;
    CHECK(friendly.u.battle.target_id == 11);

    /* The same actor and target moved to other vector entries. Use their IDs. */
    bt_reset(DD_ROLE_GUEST);
    { uint64_t p = bt_party[0]; bt_party[0] = bt_party[2]; bt_party[2] = p; }
    wk_push(1, &choice);
    dd_runtime_poll();
    CHECK(bt_picks == 1 && bt_last_hero == bt_heroes[2] && g_town.applied_seq == 1);
    wk_push(2, &friendly);
    dd_runtime_poll();
    CHECK(bt_friends == 1 && bt_last_hero == bt_heroes[2] && bt_last_target == bt_heroes[0]);

    /* Same turn numbers, different actual actor: no native call or silent ACK. */
    bt_reset(DD_ROLE_GUEST);
    bt_turns[3] = bt_party[0];
    wk_push(1, &choice);
    dd_runtime_poll();
    CHECK(!bt_picks && !g_town.applied_seq && g_town.needs_resync);
    dd_msg stop = {0};
    CHECK(queue_peek(&g_outgoing, &stop) && stop.type == DD_MSG_SYNC_STOP && stop.u.load.slot == 1);
    runtime_sync_stop(1);
    CHECK(g_outgoing.count == 1); /* One stop message, never an echo loop. */

    /* The stop travels outside the FIFO: it cannot wait behind an animation. */
    bt_reset(DD_ROLE_HOST);
    dd_msg request = {0};
    request.type = DD_MSG_TOWN_REQUEST; request.u.town.request_id = 1;
    request.u.town.action = choice;
    bt_at(2, 1, 0x1c);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && !g_town.needs_resync);
    InterlockedExchange(&g_peer_sync_stop, 1);
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !bt_picks && !sent && runtime_shared_blocked());
    CHECK(dd_runtime_input_stopped());
    { uint32_t event[16] = {0x401}; CHECK(dd_gameplay_input(event));
      event[0] = 0x300; CHECK(dd_gameplay_input(event));
      event[0] = 0x651; CHECK(dd_gameplay_input(event));
      event[0] = 0x100; CHECK(!dd_gameplay_input(event)); /* SDL_QUIT */
      event[0] = 0x200; CHECK(!dd_gameplay_input(event)); /* window close */ }
    g_campaign_active = 0; CHECK(!dd_runtime_input_stopped()); g_campaign_active = 1;

    /* Neither player can consume an item while a door is pending. */
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        it_reset(role);
        g_walk_door_pending = GetTickCount64() + 2000;
        CHECK(!hook_item_use((void *)(uintptr_t)0x1234, bt_heroes[2], wk_raid_lists + DD_WALK_RAID_INVENTORY, 3, 1));
        CHECK(!it_uses && !sent);
        g_walk_door_pending = 0;
        g_walk_door_until = GetTickCount64() + 2000;
        CHECK(!hook_item_use((void *)(uintptr_t)0x1234, bt_heroes[2], wk_raid_lists + DD_WALK_RAID_INVENTORY, 3, 1));
        CHECK(!it_uses && !sent);
    }
    it_reset(DD_ROLE_HOST);
    hook_item_use((void *)(uintptr_t)0x1234, bt_heroes[2], wk_raid_lists + DD_WALK_RAID_INVENTORY, 3, 1);
    use = last_message.u.town.action;
    CHECK(use.u.item.op == DD_ITEM_OP_USE && !use.u.item.state);
    it_reset(DD_ROLE_GUEST);
    wk_view.pos += 500; /* Walking pixels must not hold an inventory click. */
    wk_push(1, &use);
    dd_runtime_poll();
    CHECK(it_uses == 1 && g_town.applied_seq == 1 && !g_incoming.count);
    it_reset(DD_ROLE_GUEST);
    wk_view.battle = DD_BATTLE_CHOOSE_SKILL_STATE;
    wk_push(1, &use);
    dd_runtime_poll();
    CHECK(!it_uses && g_incoming.count == 1);
    g_item_wait_until = 1;
    dd_runtime_poll();
    CHECK(!it_uses && !g_town.applied_seq && g_town.needs_resync && queue_peek(&g_outgoing, &stop) && stop.type == DD_MSG_SYNC_STOP);

    /* The associated Rules prefix has the same phase guard, no pixel gate. */
    uint8_t rules[16] = {0};
    uint32_t seed = 1234;
    it_reset(DD_ROLE_HOST);
    g_rules_object = rules;
    memcpy(rules + 8, &seed, 4);
    hook_item_use((void *)(uintptr_t)0x1234, bt_heroes[2], wk_raid_lists + DD_WALK_RAID_INVENTORY, 3, 1);
    CHECK(sent == 2 && sent_messages[0].u.town.action.u.item.flag == 1);
    dd_action prefix = sent_messages[0].u.town.action;
    it_reset(DD_ROLE_GUEST);
    g_rules_object = rules;
    wk_view.pos += 500;
    wk_push(1, &prefix);
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 1 && !g_incoming.count);
    g_rules_object = NULL;

    /* A repair is not complete until its expected final bag hash matches. */
    dd_msg repair[DD_BAG_SLOTS + 2];
    it_reset(DD_ROLE_HOST);
    it_bag_set();
    it_bag_slot(it_bag, 0, 2, 11, 21);
    bag_send_all(&wk_view);
    unsigned count = sent;
    CHECK(count == 10 && sent_messages[0].u.town.action.u.item.op == DD_ITEM_OP_BAG_CHECK);
    memcpy(repair, sent_messages, count * sizeof *repair);
    for (int broken = 0; broken < 2; broken++) {
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        if (!broken) it_bag_slot(it_bag, 2, 1, 11, 21); /* Same kind can be copied/moved. */
        g_item_clear = it_clear;
        wk_push(1, &repair[0].u.town.action);
        dd_runtime_poll();
        CHECK(g_bag_repairing && runtime_shared_blocked());
        for (unsigned i = 1; i < count; i++) wk_push(i + 1, &repair[i].u.town.action);
        dd_runtime_poll();
        if (broken) CHECK(g_town.needs_resync && g_town.applied_seq < count && queue_peek(&g_outgoing, &stop) && stop.type == DD_MSG_SYNC_STOP);
        else CHECK(!g_town.needs_resync && g_town.applied_seq == count && !bag_repair_busy() && it_count(it_bag, 0) == 2);
    }
    bt_unwire();
    puts("Desync regressions: stable actor/target IDs after reorder, wrong actor stops both, stop bypasses waiting FIFO, door item guards, no walking-pixel delay, late combat use refused, final bag repair hash verified");
}
