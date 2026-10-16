#include "steam_abi.h"
#include <dlfcn.h>
#include <string.h>
int steam_load(struct steam_api *a,const char *path) {
    memset(a,0,sizeof *a);a->library=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if(!a->library)return -1;
#define LOAD(alias,name,ret,args) do { \
    void *symbol=dlsym(a->library,"SteamAPI_" #name); \
    _Static_assert(sizeof a->alias==sizeof symbol,"POSIX function pointers"); \
    if(!symbol){steam_unload(a);return -1;}memcpy(&a->alias,&symbol,sizeof symbol); \
} while(0);
    STEAM_FUNCTIONS(LOAD)
#undef LOAD
    return 0;
}
void steam_unload(struct steam_api *a) {
    if(a->library)dlclose(a->library);
    memset(a,0,sizeof *a);
}
