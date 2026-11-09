/* Host authority for town actions (platform independent; tested with a fake town in tests/test_town.c).
 *
 * Flow (NOTES.md, "Town sync v2"):
 *   guest: a local action becomes a TOWN_REQUEST; the guest does NOT apply it itself (no prediction).
 *   host:  runs the game's own function for it. Accepted -> TOWN_APPLY with the next apply sequence number
 *          to everyone (the requester included); refused -> TOWN_REJECT to the requester only.
 *   everyone: applies TOWN_APPLY strictly in sequence order. A gap means a message was lost: that copy
 *          stops applying and asks for a resync (the host's save) instead of drifting silently.
 * The host's own actions take the same path (it is one of the players), minus the network. */
#ifndef DD_TOWN_H
#define DD_TOWN_H

#include <stdint.h>

#include "dd_proto.h"

/* Runs one action against the local game. Returns 1 if the game accepted and applied it, 0 if refused.
 * On the host this decides; on a guest the same action must succeed (identical state), and a refusal
 * there means the copies have drifted. */
typedef int (*dd_town_apply_fn)(void *game, const dd_action *a);

/* Hands an encoded message to the network: to = player index, or DD_TOWN_ALL for every guest. */
#define DD_TOWN_ALL (-1)
typedef void (*dd_town_send_fn)(void *net, int to, const dd_msg *m);

typedef struct {
    void *game;
    dd_town_apply_fn apply;
    void *net;
    dd_town_send_fn send;
    uint32_t next_apply_seq; /* host: number for the next accepted action */
    uint32_t applied_seq;    /* everyone: last apply sequence applied here */
    uint32_t next_request;   /* guest: request ids */
    int needs_resync;        /* a gap or a refused APPLY: stop applying, reload the host's save */
    uint32_t accepted, rejected;
} dd_town;

void dd_town_init(dd_town *t, void *game, dd_town_apply_fn apply, void *net, dd_town_send_fn send);

/* Host: a guest (index from_player) asked for an action. */
void dd_town_host_request(dd_town *t, int from_player, const dd_msg *request);

/* Host: the host's own player did an action. Returns 1 if the game accepted it. */
int dd_town_host_local(dd_town *t, const dd_action *a);

/* Host: the host's own game already did this action (seen after the fact, e.g. a hero locked into a
 * building); broadcasts it without applying it again. */
void dd_town_host_observed(dd_town *t, const dd_action *a);

/* Guest: the local player did an action; sends the request. Returns the request id. */
uint32_t dd_town_guest_local(dd_town *t, const dd_action *a);

/* Guest: a TOWN_APPLY arrived from the host. Returns 1 if applied. */
int dd_town_guest_apply(dd_town *t, const dd_msg *apply);

#endif
