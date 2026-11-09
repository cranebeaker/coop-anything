#include "dd_town.h"

#include <string.h>

void dd_town_init(dd_town *t, void *game, dd_town_apply_fn apply, void *net, dd_town_send_fn send) {
    memset(t, 0, sizeof *t);
    t->game = game;
    t->apply = apply;
    t->net = net;
    t->send = send;
    t->next_apply_seq = 1;
    t->next_request = 1;
}

static void broadcast_apply(dd_town *t, uint32_t request_id, const dd_action *a) {
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = DD_MSG_TOWN_APPLY;
    m.seq = t->next_apply_seq++;
    m.u.town.request_id = request_id;
    m.u.town.action = *a;
    t->applied_seq = m.seq;
    t->accepted++;
    t->send(t->net, DD_TOWN_ALL, &m);
}

void dd_town_host_request(dd_town *t, int from_player, const dd_msg *request) {
    const dd_action *a = &request->u.town.action;
    dd_msg r;
    memset(&r, 0, sizeof r);
    r.type = DD_MSG_TOWN_REJECT;
    r.u.reject.request_id = request->u.town.request_id;
    if (!dd_action_valid(a)) {
        r.u.reject.reason = DD_REJECT_INVALID;
    } else if (t->needs_resync) {
        r.u.reject.reason = DD_REJECT_BUSY;
    } else if (t->apply(t->game, a)) {
        broadcast_apply(t, request->u.town.request_id, a);
        return;
    } else {
        r.u.reject.reason = DD_REJECT_NOT_ALLOWED;
    }
    t->rejected++;
    t->send(t->net, from_player, &r);
}

int dd_town_host_local(dd_town *t, const dd_action *a) {
    if (!dd_action_valid(a) || t->needs_resync || !t->apply(t->game, a)) return 0;
    broadcast_apply(t, 0, a);
    return 1;
}

void dd_town_host_observed(dd_town *t, const dd_action *a) {
    if (dd_action_valid(a) && !t->needs_resync) broadcast_apply(t, 0, a);
}

uint32_t dd_town_guest_local(dd_town *t, const dd_action *a) {
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = DD_MSG_TOWN_REQUEST;
    m.u.town.request_id = t->next_request++;
    m.u.town.action = *a;
    t->send(t->net, 0, &m);
    return m.u.town.request_id;
}

int dd_town_guest_apply(dd_town *t, const dd_msg *apply) {
    if (t->needs_resync) return 0;
    if (apply->seq != t->applied_seq + 1) { /* lost or repeated message: never guess */
        t->needs_resync = 1;
        return 0;
    }
    if (!t->apply(t->game, &apply->u.town.action)) { /* the host's game accepted it, ours refused: drift */
        t->needs_resync = 1;
        return 0;
    }
    t->applied_seq = apply->seq;
    t->accepted++;
    return 1;
}
