#ifndef SEASONS_STEAM_ABI_H
#define SEASONS_STEAM_ABI_H
/* Original minimal declarations of pinned SDK 1.64 Linux x86-64 ABI facts.
 * No SDK implementation bodies. Sources and hashes are recorded in README. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct steam_identity { unsigned char bytes[136]; };
#pragma pack(push,4)
struct steam_callback { int32_t user,kind; unsigned char *data; int32_t size; };
struct steam_completed { uint64_t call; int32_t kind; uint32_t size; };
struct steam_created { int32_t result; uint64_t lobby; };
struct steam_entered { uint64_t lobby; uint32_t permissions; bool locked; uint32_t response; };
struct steam_join_request { uint64_t lobby,friend_id; };
#pragma pack(pop)
struct steam_message {
    void *data; int32_t size; uint32_t connection;
    struct steam_identity peer;
    int64_t user_data,received,number;
    void (*free_data)(struct steam_message *);
    void (*release)(struct steam_message *);
    int32_t channel,flags; int64_t app_data; uint16_t lane,pad;
};
_Static_assert(sizeof(void *)==8,"Linux x86-64 required");
_Static_assert(sizeof(struct steam_callback)==20,"Linux callback pack4");
_Static_assert(offsetof(struct steam_callback,data)==8,"callback pointer offset");
_Static_assert(sizeof(struct steam_created)==12 && offsetof(struct steam_created,lobby)==4,"created pack4");
_Static_assert(sizeof(struct steam_entered)==20 && offsetof(struct steam_entered,response)==16,"entered pack4");
_Static_assert(sizeof(struct steam_completed)==16,"call completed");
_Static_assert(sizeof(struct steam_identity)==136,"identity pack1 storage");
_Static_assert(sizeof(struct steam_message)==216 && offsetof(struct steam_message,peer)==16,"native message layout");
_Static_assert(offsetof(struct steam_message,channel)==192,"native channel offset");
/* alias, export suffix, return type, parameters */
#define STEAM_FUNCTIONS(X) \
 X(init,InitFlat,int,(char *)) \
 X(shutdown,Shutdown,void,(void)) \
 X(pipe,GetHSteamPipe,int,(void)) \
 X(dispatch_init,ManualDispatch_Init,void,(void)) \
 X(run,ManualDispatch_RunFrame,void,(int)) \
 X(next,ManualDispatch_GetNextCallback,bool,(int,struct steam_callback *)) \
 X(free_callback,ManualDispatch_FreeLastCallback,void,(int)) \
 X(result,ManualDispatch_GetAPICallResult,bool,(int,uint64_t,void *,int,int,bool *)) \
 X(user,SteamUser_v023,void *,(void)) \
 X(match,SteamMatchmaking_v009,void *,(void)) \
 X(friends,SteamFriends_v018,void *,(void)) \
 X(messages,SteamNetworkingMessages_SteamAPI_v002,void *,(void)) \
 X(id,ISteamUser_GetSteamID,uint64_t,(void *)) \
 X(logged,ISteamUser_BLoggedOn,bool,(void *)) \
 X(create,ISteamMatchmaking_CreateLobby,uint64_t,(void *,int,int)) \
 X(join,ISteamMatchmaking_JoinLobby,uint64_t,(void *,uint64_t)) \
 X(leave,ISteamMatchmaking_LeaveLobby,void,(void *,uint64_t)) \
 X(set_data,ISteamMatchmaking_SetLobbyData,bool,(void *,uint64_t,const char *,const char *)) \
 X(get_data,ISteamMatchmaking_GetLobbyData,const char *,(void *,uint64_t,const char *)) \
 X(owner,ISteamMatchmaking_GetLobbyOwner,uint64_t,(void *,uint64_t)) \
 X(count,ISteamMatchmaking_GetNumLobbyMembers,int,(void *,uint64_t)) \
 X(member,ISteamMatchmaking_GetLobbyMemberByIndex,uint64_t,(void *,uint64_t,int)) \
 X(limit,ISteamMatchmaking_GetLobbyMemberLimit,int,(void *,uint64_t)) \
 X(joinable,ISteamMatchmaking_SetLobbyJoinable,bool,(void *,uint64_t,bool)) \
 X(invite,ISteamFriends_ActivateGameOverlayInviteDialog,void,(void *,uint64_t)) \
 X(set_id,SteamNetworkingIdentity_SetSteamID64,void,(struct steam_identity *,uint64_t)) \
 X(get_id,SteamNetworkingIdentity_GetSteamID64,uint64_t,(const struct steam_identity *)) \
 X(send,ISteamNetworkingMessages_SendMessageToUser,int,(void *,const struct steam_identity *,const void *,uint32_t,int,int)) \
 X(receive,ISteamNetworkingMessages_ReceiveMessagesOnChannel,int,(void *,int,struct steam_message **,int)) \
 X(accept,ISteamNetworkingMessages_AcceptSessionWithUser,bool,(void *,const struct steam_identity *)) \
 X(close_session,ISteamNetworkingMessages_CloseSessionWithUser,bool,(void *,const struct steam_identity *)) \
 X(release,SteamNetworkingMessage_t_Release,void,(struct steam_message *))
struct steam_api {
    void *library;
#define FIELD(alias,name,ret,args) ret (*alias) args;
    STEAM_FUNCTIONS(FIELD)
#undef FIELD
};
int steam_load(struct steam_api *,const char *);
void steam_unload(struct steam_api *);
#endif
