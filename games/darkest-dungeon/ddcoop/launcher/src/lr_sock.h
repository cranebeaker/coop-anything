/* Non-blocking TCP on 127.0.0.1 only (Winsock / POSIX). The launcher never listens on the network:
 * the game's connection goes to a local port and travels between the PCs over Steam. */
#ifndef LR_SOCK_H
#define LR_SOCK_H

#include <stddef.h>
#include <stdint.h>

typedef intptr_t lr_sock;
#define LR_NOSOCK ((lr_sock)-1)

int lr_sock_startup(void);
void lr_sock_cleanup(void);

/* Listens on 127.0.0.1:port (0 = any free port); *bound gets the port. */
lr_sock lr_listen_local(int port, int *bound);
lr_sock lr_accept_nb(lr_sock listener); /* LR_NOSOCK when nobody is waiting */

lr_sock lr_connect_local_nb(int port);  /* starts a connect to 127.0.0.1:port */
int lr_connect_check(lr_sock s);        /* 1 connected, 0 still pending, -1 failed */

int lr_recv_nb(lr_sock s, uint8_t *buf, size_t n);       /* >0 bytes, 0 nothing yet, -1 closed or error */
int lr_send_nb(lr_sock s, const uint8_t *buf, size_t n); /* bytes taken (0 = buffer full), -1 error */
void lr_shutdown_write(lr_sock s);
void lr_close(lr_sock s);

int lr_port_free(int port); /* 1 if 127.0.0.1:port can be bound right now */

#endif
