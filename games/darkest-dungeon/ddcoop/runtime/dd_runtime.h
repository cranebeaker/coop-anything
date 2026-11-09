/* In-game transport bridge for the first replicated town action. */
#ifndef DD_RUNTIME_H
#define DD_RUNTIME_H

#include <stdint.h>

#include "dd_config.h"

/* Copies settings and verified runtime addresses. No game state is touched here. */
int dd_runtime_init(const dd_config *cfg, const uint8_t build_id[8], uint64_t assign_hero_va,
                    uint64_t slot_widget_ctor_va, uint64_t buy_upgrade_va,
                    uint64_t wagon_buy_va, uint64_t trinket_price_va,
                    uint64_t recruit_va,uint64_t recruit_show_va,uint64_t recruit_hide_va,
                    uint64_t roster_add_va,uint64_t roster_count_va,uint64_t roster_capacity_va,uint64_t roster_drop_va,uint64_t sell_va);

/* Call when the loaded campaign has been positively identified as a configured co-op slot. */
void dd_runtime_campaign_active(int active);

/* Called by the local SDL_PollEvent wrapper after the real poll returns, on the game thread. */
void dd_runtime_poll(void);

/* From startup, once the game's build is known: lets the game's start screen skip the opening. */
void dd_runtime_frontend_early(const dd_config *cfg);

int dd_runtime_session_ready(void);

#endif
