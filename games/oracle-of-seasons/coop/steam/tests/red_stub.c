#include "steam.h"
int sa_open(struct sa *s,const char *p,const struct sa_config *c,uint64_t n){(void)s;(void)p;(void)c;(void)n;return -1;}
int sa_host(struct sa *s,uint64_t n){(void)s;(void)n;return -1;}
int sa_join(struct sa *s,uint64_t l,uint64_t n){(void)s;(void)l;(void)n;return -1;}
int sa_invite(struct sa *s){(void)s;return -1;}
int sa_send(struct sa *s,const void *p,size_t z,uint64_t n){(void)s;(void)p;(void)z;(void)n;return -1;}
int sa_poll(struct sa *s,uint64_t n,struct sa_event *e){(void)s;(void)n;(void)e;return -1;}
void sa_close(struct sa *s){(void)s;}
