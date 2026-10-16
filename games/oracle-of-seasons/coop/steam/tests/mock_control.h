#ifndef MOCK_CONTROL_H
#define MOCK_CONTROL_H
#include <stdint.h>
struct mock_options { uint64_t self,owner,peer;int fd;uint16_t remote_port;int init_error,logged_out,async_failure,no_completion,bad_metadata; };
struct mock_stats { int create_type,max_members,writes,invites,accepts,closes,releases,freed_callbacks,leaves,shutdowns,sends,fd;uint64_t accepted,closed; };
enum mock_command { MOCK_INVITE=1,MOCK_REQUEST,MOCK_FAILED,MOCK_COUNT,MOCK_OWNER,MOCK_PEER,MOCK_SEND_BUSY,MOCK_SEND_ERROR,MOCK_BAD_CALLBACK };
void mock_reset(const struct mock_options *);
void mock_command(int,uint64_t);
void mock_message(uint64_t,const void *,int,int);
struct mock_stats mock_stats(void);
#endif
