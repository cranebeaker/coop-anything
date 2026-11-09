"""Generate src/addresses.inc: every mapped game function with the first bytes found at its address.

The framework compares these bytes with the running game at startup. If they differ (another game
build), it searches the code for the same bytes; if that is not unique either, the mod stays off.
Every pattern is checked to be unique in .text of this build before anything is written.

Usage: python3 -I gen_address_table.py <Darkest.exe> <out.inc>
"""
import struct
import sys

IMAGE_BASE = 0x140000000
PATTERN_LEN = 48  # 32 was not unique for provisions_buy (also at 0x1404fa480)

# name, virtual address, what it is (see games/darkest-dungeon/NOTES.md in coop-anything)
FUNCTIONS = [
    ("activity_assign_hero", 0x1405360c0, "Building::Activity assign hero (activity, result, heroId); slot in activity+0x90"),
    ("activity_slot_widget_ctor", 0x1407e1790, "ActivityHeroSlotInterface ctor (widget, activity, slotIndex); activity discovery for placement sync"),
    ("buy_upgrade", 0x14059ad80, "Upgrade purchase (request {u32 tree, u8 code, u32 heroId}) -> 1 on success"),
    ("wallet_add", 0x140568500, "Estate wallet add (wallet, type, amount)"),
    ("wallet_deduct", 0x140568600, "Estate wallet deduct (wallet, type, amount); does not refuse"),
    ("wallet_notify", 0x140567990, "Normal wallet change notifications"),
    ("hero_toggle_combat", 0x1405d4f70, "Hero selected combat skill toggle; learned level checked by co-op"),
    ("hero_toggle_camping", 0x1405d3dd0, "Hero selected camping skill toggle; learned level checked by co-op"),
    ("character_refresh_skills", 0x1408002d0, "Normal CharacterDisplay skill widgets rebind (display, Hero); deferred past click callback"),
    ("hero_commit_name", 0x1407fe720, "Normal character name commit; bounded input string at context+0x14f"),
    ("roster_change_location", 0x14058b500, "Roster::ChangeLocation(roster, Entry, location), normal party statuses"),
    ("party_set_slot", 0x1407e3820, "Normal QuestSelectHeroSlotInterface SetHero; rank at +0x230"),
    ("inventory_swap_slot",0x1405df8f0,"Inventory native Item swap and modified event"),
    ("item_copy",0x1403cdf60,"Item copy constructor; owns C++ containers"),
    ("item_clear",0x1405de910,"Item clear preserving valid container construction"),
    ("item_destroy",0x1403ce100,"Item component destructor, no object deallocation"),
    ("actor_refresh_trinkets",0x140481a40,"Normal actor trinket stats refresh"),
    ("trinket_pickup_hero",0x1407ecdf0,"EquipmentSlot source drag pickup"),
    ("trinket_pickup_inventory",0x1407ee320,"InventorySlot source drag pickup"),
    ("trinket_finish_hero",0x1407ecf00,"EquipmentSlot source drag finish"),
    ("trinket_cancel_hero",0x1407ed150,"EquipmentSlot source drag cancel"),
    ("trinket_finish_inventory",0x1407ee410,"InventorySlot source drag finish"),
    ("trinket_cancel_inventory",0x1407ee6a0,"InventorySlot source drag cancel"),
    ("trinket_drop_hero",0x1407ed600,"EquipmentSlot secondary destination drop"),
    ("trinket_drop_inventory",0x1407eebb0,"InventorySlot secondary destination drop"),
    ("trinket_area_hero",0x1407efc60,"Normal hero trinket-area drop"),
    ("trinket_area_inventory",0x1407efe00,"Normal inventory trinket-area drop"),
    ("trinket_sort_button",0x1406ad650,"RealmInventory sort button callback; co-op guard until bulk transaction exists"),
    ("trinket_unequip_all_dialog",0x1406a9480,"RealmInventory unequip-all dialog; co-op guard, use individual moves"),
    ("hero_dismiss", 0x1406b46b0, "Dismiss (this, heroId): fires HeroDismissedEvent"),
    ("roster_list_update", 0x1406b4e30, "RosterList update (panel, seconds): removes the hero whose row hero_dismiss marked"),
    ("recruit_event_send", 0x1406c54d0, "Dispatch HeroRecruited (bus, event); the Stagecoach callback 6c3c10 calls it"),
    ("event_clock", 0x140372340, "Timestamp for a dispatched event"),
    ("save_roster", 0x140585a60, "Writer of persist.roster, called by the Stagecoach callback and the dismissal"),
    ("save_progression", 0x14056d090, "Writer of persist.progression, called by the Stagecoach callback"),
    ("save_town", 0x140591540, "Writer of persist.town, called by the Stagecoach callback and the dismissal"),
    ("save_raid", 0x1403d3110, "Writer of persist.raid/map, called by the Stagecoach callback and the dismissal"),
    ("hero_recruit", 0x1406c3c10, "Stagecoach recruit UI handler: fires HeroRecruitedEvent"),
    ("recruit_panel_show", 0x1406c17b0, "BaseHeroRecruitDisplay normal show lifecycle; constructs genuine hero rows"),
    ("recruit_panel_hide", 0x1406c1a70, "BaseHeroRecruitDisplay hide lifecycle; destroys hero rows"),
    ("roster_ui_drop", 0x1406b3ee0, "Roster drop acceptance; guest Stagecoach request before model mutation"),
    ("roster_add_entry", 0x140589ab0, "Roster::AddEntry clones an Entry, notifies roster listeners, returns clone Entry"),
    ("roster_recruit_count", 0x14058b1f0, "Game count of ordinary heroes consuming roster space"),
    ("roster_capacity", 0x14058b2e0, "Game roster capacity including Stagecoach upgrades"),
    ("provisions_buy", 0x1406960c0, "Provisions buy (runtime test 4)"),
    ("provisions_return", 0x1407ebcf0, "Provisions return (itemStack, n)"),
    ("wagon_buy", 0x140693090, "Nomad Wagon buy (runtime test 4)"),
    ("trinket_price", 0x14059b540, "Adjusted Nomad price (basePrice, modifierType) -> gold"),
    ("wagon_sell", 0x1406a77b0, "Nomad Wagon sell"),
    ("heirloom_exchange", 0x1406868f0, "Heirloom exchange confirm"),
    ("monster_ai_select_skill", 0x1405aa490, "Battle::GetMonsterAISelectSkill (brain)"),
    ("party_on_raid_start", 0x140602cd0, "Party::OnRaidStart (party, restoring, params, quests, index)"),
    ("darkest_raid_start", 0x1403d7e80, "Darkest::RaidStart (app)"),
    ("rng_raw_uniform", 0x14028f9f0, "prng_minstd core step"),
    ("rng_named_uniform", 0x14028fcd0, "prng_minstd_wrapped core step"),
    ("rng_named_seed", 0x14028fc40, "prng_minstd_wrapped seed"),
    ("campaign_current", 0x140556520, "Campaign::Current() -> g_pCurrent"),
    ("provision_return_click", 0x1406978b0, "normal provision return callback; genuine bag capture"),
    ("provision_cancel", 0x1406e0250, "refund+clear both provision inventories"),
    ("supply_fill_free", 0x14057b100, "Free items for the mission into the bag; the panel's opening 694df0 calls it (also a demo raid, 3e4e50)"),
    ("supply_fill_stock", 0x14057b7c0, "Shop stock for the mission; only the panel's opening 694df0 calls it"),
    ("provision_enter", 0x140694df0, "genuine ProvisionSelect OnEnter"),
    ("fe_start_button", 0x140424150, "Title screen's button: runs the game's check of Steam's saves and goes on. Functor with the screen at +8"),
    ("fe_new_slot", 0x14041b330, "What an empty save slot's new_game button runs (functor 423c80), with the screen and the slot"),
    ("fe_load_slot", 0x14041c390, "What a save slot's load button runs (functor 425090, slots 0..8), with the screen and the slot. Not 41be60, the Circus's"),
    ("provision_open", 0x1406e7a70, "Normal provisions button callback; local functor holds TownDisplay at +8; no Circus path"),
    ("provision_back", 0x1406e85e0, "Back button callback of the provisions panel; calls provision_cancel, then leaves"),
    ("town_depart", 0x1406e7c70, "Normal departure: sets the app's next state to 0x14, which starts the raid. Not the Circus path 6e3390/6debf0/6dfb40"),
    ("raid_move_input", 0x14075b530, "RaidDisplay reads the walking keys and moves the party (display, out direction)"),
    ("raid_use_prop", 0x1407747c0, "RaidDisplay player uses a prop (display, prop, a, b)"),
    ("raid_use_trap", 0x140774410, "RaidDisplay::InteractWithTrap: clicking a trap to disarm it (display, prop)"),
    ("battle_skill_button", 0x14071f1e0, "A skill button in a fight: picks or unpicks through 5cdf70 (functor, slot at +14)"),
    ("battle_skill_pick", 0x1405cdf70, "Hero: set the chosen skill (hero, slot or -1, Rules or not)"),
    ("battle_target_enemy", 0x1405abae0, "Battle: add a target for a skill on the enemy (battle, skill, actor, target); called by the button at 707850"),
    ("battle_target_friend", 0x1405ab6d0, "Battle: add a target for a skill on the party (battle, skill, actor, target); called by the button at 707ae0"),
    ("battle_move", 0x1405b9540, "Battle: move to a rank, or pass at its own rank (battle, hero, rank, -, 0)"),
    ("rules_uniform", 0x14028fcd0, "prng_minstd_wrapped core step, for the log of who draws Rules"),
    ("curio_item", 0x140712e40, "Overlay_Event: put an item in the curio slot (overlay, item)"),
    ("bark_moving", 0x14075b970, "A bark on starting or stopping to walk; picks its hero with a Rules number"),
    ("tutorial_show", 0x140820ae0, "TutorialPopup::PreChildrenShowInternal"),
    ("panel_close", 0x1407a8c80, "A close button several panels share (functor, panel at +8)"),
    ("quest_choice_one", 0x1407506c0, "After a finished quest: the first button (functor, panel at +8)"),
    ("quest_choice_two", 0x1407504e0, "After a finished quest: the second button (functor, panel at +8)"),
    ("results_setup", 0x1407a0170, "RaidResultsDisplay::InitializeInternal"),
    ("results_next", 0x1407a2440, "RaidResultsDisplay: move on, at the last step to town"),
    ("item_clear", 0x1405de910, "Inventory item: empty this slot"),
    ("bag_shop_drop", 0x1407ef6d0, "An item dropped on the shop: sells it back"),
    ("hero_equipment_levels", 0x1405d5670, "Hero: weapon and armour levels (+132c, +1330) from the purchased %s.weapon/%s.armour upgrades; the Blacksmith callback 66aa20 calls it"),
    ("hero_skill_levels", 0x1405d3c20, "Hero: skill levels from the purchased %s.%s upgrades; the Guild callback 673210 calls it"),
    ("provision_stock_drop", 0x140694ab0, "Provisions stock slot: a bag item dropped on it sells back (7ebcf0); else the InventorySlot drop 7eebb0"),
    ("provision_stack_return", 0x140696a20, "Provisions bag callback at +a18 (Do_call 697870): merges a stack, then returns the rest (7ebcf0)"),
    ("bag_drop", 0x1407ed600, "InventorySlotInterface: something dropped here"),
    ("bag_source_a", 0x1407ed150, "InventorySlotInterface: the source after a drag"),
    ("bag_source_b", 0x1407ecf00, "InventorySlotInterface: the source after a drag"),
    ("bag_throw_a", 0x140737c20, "Panel_Inventory: throw an item away"),
    ("bag_throw_b", 0x140737070, "Panel_Inventory: throw an item away, confirmed"),
    ("retreat_attempt", 0x1407499a0, "QuestInfo::AttemptBattleRetreat (the panel at display+3590)"),
    ("camp_meal", 0x140709cf0, "Overlay_Camp: the meal button's work"),
    ("camp_skill", 0x1405c7420, "Camping::PerformCampingSkill"),
    ("camp_leave", 0x14070b7c0, "Overlay_Camp: the Leave button (functor, overlay at +8)"),
    ("item_use", 0x140429750, "DungeonController::HeroUseItem (controller, hero, inventory, slot, 1)"),
    ("torch_change", 0x1406155f0, "Raid: change the torch (raid, amount, 1)"),
    ("loot_take_all", 0x140717d80, "OverlayLoot button: take all (functor, sets +218)"),
    ("loot_close", 0x140717d40, "OverlayLoot button: close (functor, sets +219)"),
    ("loot_take_item", 0x1407171d0, "OverlayLoot button: one item (+10 place, +20 loot, +28 widget)"),
    ("game_assert", 0x1402357e0, "The game's assert report (expression, function, file, line, format, ...): reached from 1025 places"),
    ("curio_tool", 0x14070ff50, "Curio overlay: use a tool on it (shovel on rubble). Capture: overlay at +0, a copy of the item at +8"),
    ("curio_button", 0x140714360, "Curio overlay button: runs the curio with the picked item (functor with the overlay at +8)"),
    ("overlay_use", 0x140714770, "Curio overlay button: interacts with the open prop and closes (functor with the overlay at +8)"),
    ("party_select", 0x140606450, "Party selects a hero (party, index)"),
    ("raid_travel", 0x140770080, "RaidDisplay move to another area (display, dest, from area, flip, extra)"),
    ("party_move", 0x140604af0, "Party move (party, direction); the game's input passes (+-1,0,0)"),
    ("raid_map_generate", 0x1405f1ff0, "Raid map generator (a, b, c); seeds a local prng from rand_s at 0x5f2164"),
    ("hunger_show", 0x140712080, "Normal Overlay_Event::ShowHunger (overlay, RaidDisplay), per-frame prompt identity"),
    ("hunger_eat", 0x140713bf0, "Hunger eat callback (std::_Func: overlay +8, food float +10)"),
    ("hunger_refuse", 0x140712800, "Hunger refusal callback (capture: overlay +0, food float +8)"),
    ("quest_return", 0x14074fdd0, "Completed-quest seal stateless callback, state9; hooked through its specific vtable slot"),
]


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    first = pe + 24 + opt_size
    return [struct.unpack_from("<8sIIII", data, first + i * 40) for i in range(count)]


def file_offset(secs, rva):
    for _, vsize, va, rsize, raw in secs:
        if va <= rva < va + max(vsize, rsize):
            return rva - va + raw
    raise ValueError(hex(rva))


def main():
    exe, out = sys.argv[1:3]
    data = open(exe, "rb").read()
    secs = sections(data)
    lines = ["/* Generated by tools/gen_address_table.py from Darkest.exe (Steam, x64,",
             " * sha256 f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12). Do not edit. */"]
    text = [s for s in secs if s[0].rstrip(b"\0") == b".text"][0]
    code = data[text[4]:text[4] + text[3]]
    for name, va, what in FUNCTIONS:
        off = file_offset(secs, va - IMAGE_BASE)
        pattern = data[off:off + PATTERN_LEN]
        first = code.find(pattern)
        if first < 0 or code.find(pattern, first + 1) >= 0:
            sys.exit(f"{name}: the {PATTERN_LEN} bytes at 0x{va:x} are not unique in .text; nothing written")
        hexbytes = ", ".join(f"0x{b:02x}" for b in pattern)
        lines.append(f"ADDR({name}, 0x{va:x}ULL, {{{hexbytes}}}) /* {what} */")
    open(out, "w", newline="\n").write("\n".join(lines) + "\n")
    print(f"{len(FUNCTIONS)} addresses written to {out}")


if __name__ == "__main__":
    main()
