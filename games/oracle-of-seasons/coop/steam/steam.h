#ifndef SEASONS_STEAM_H
#define SEASONS_STEAM_H
#include "steam_abi.h"
#define SA_MAX_PAYLOAD 4096u
struct sa_config { char game[32],build[64],protocol[16]; uint64_t timeout_ms; };
enum sa_event_type { SA_NONE,SA_LOBBY_READY,SA_PARTNER_READY,SA_JOIN_REQUEST,SA_MESSAGE,SA_FAILURE };
struct sa_event { enum sa_event_type type;uint64_t lobby,peer;unsigned player;size_t size;unsigned char data[SA_MAX_PAYLOAD]; };
/* One initialized context per process. Storage cannot move/copy while active. */
struct sa {
    struct steam_api api;struct sa_config config;
    void *match,*friends,*messages;int pipe,initialized,state,host;
    uint64_t self,lobby,owner,peer,pending,target,deadline,last_rx,now,send_deadline;
    size_t queued;bool has_output;unsigned char output[SA_MAX_PAYLOAD];
};
int sa_open(struct sa *,const char *library,const struct sa_config *,uint64_t now);
int sa_host(struct sa *,uint64_t now);
int sa_join(struct sa *,uint64_t lobby,uint64_t now);
int sa_invite(struct sa *);
/* One copied output slot. 1 queued, 0 busy, -1 invalid. */
int sa_send(struct sa *,const void *,size_t,uint64_t now);
/* 1 event, 0 pending, -1 terminal failure. Pump regularly even when idle. */
int sa_poll(struct sa *,uint64_t now,struct sa_event *);
void sa_close(struct sa *);
#endif
