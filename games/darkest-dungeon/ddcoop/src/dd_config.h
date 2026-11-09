/* Settings from ddcoop.ini next to Darkest.exe. Missing file or keys give the defaults below. */
#ifndef DD_CONFIG_H
#define DD_CONFIG_H

typedef enum { DD_ROLE_OFF = 0, DD_ROLE_HOST, DD_ROLE_GUEST } dd_role;

typedef struct {
    dd_role role;        /* [net] role = off | host | guest   (default off: plain single player) */
    int port;            /* [net] port                        (default 27360) */
    char host[64];       /* [net] host = address a guest connects to */
    char password[128];  /* [net] password (never logged) */
    char coop_slots[64]; /* [coop] coop_slots = 3   or   3,7   (campaign menu positions 1..9) */
    int verbose;         /* [log] verbose = 0 | 1 */
} dd_config;

/* Reads the file; returns 1 if it existed. */
int dd_config_load(const char *path, dd_config *cfg);
const char *dd_role_name(dd_role r);

#endif
