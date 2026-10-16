/* Buildable baseline: operations intentionally unimplemented for recorded RED. */
#include "net.h"
int net_listen(struct net_host *h,const char *a,uint16_t p){(void)h;(void)a;(void)p;return -1;}
void net_host_close(struct net_host *h){(void)h;}
int net_accept(struct net_host *h,struct net_peer *p,const struct net_config *c){(void)h;(void)p;(void)c;return -1;}
int net_connect(struct net_peer *p,const char *a,uint16_t n,const struct net_config *c){(void)p;(void)a;(void)n;(void)c;return -1;}
int net_pump(struct net_peer *p,struct net_event *e){(void)p;(void)e;return -1;}
int net_send(struct net_peer *p,enum net_kind k,uint64_t s,const void *b,size_t n){(void)p;(void)k;(void)s;(void)b;(void)n;return -1;}
void net_peer_close(struct net_peer *p){(void)p;}
