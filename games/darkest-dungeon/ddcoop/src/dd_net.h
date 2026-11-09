/* TCP transport: a connection carries the authenticated session (dd_session) and co-op messages
 * (dd_proto) inside its frames. Portable: Winsock on Windows, POSIX sockets elsewhere. Blocking calls
 * with timeouts, meant to run on the mod's own network thread, never on the game's thread. */
#ifndef DD_NET_H
#define DD_NET_H

#include <stddef.h>
#include <stdint.h>

#include "dd_proto.h"
#include "dd_session.h"

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET dd_socket;
#define DD_NO_SOCKET INVALID_SOCKET
#else
typedef int dd_socket;
#define DD_NO_SOCKET (-1)
#endif

#define DD_RX_BUF (DD_FRAME_MAX_PLAIN + DD_FRAME_OVERHEAD)

typedef struct {
    dd_socket sock;
    dd_session sess;
    uint8_t *rx;     /* DD_RX_BUF bytes, allocated by dd_conn_init */
    size_t rx_len;
    int failed;
} dd_conn;

typedef enum { DD_POLL_MSG = 0, DD_POLL_TIMEOUT, DD_POLL_CLOSED } dd_poll_result;

int dd_net_startup(void); /* WSAStartup on Windows; no-op elsewhere */
void dd_net_cleanup(void);

/* Host: listens on port (all interfaces if bind_any, else 127.0.0.1 only). */
dd_socket dd_net_listen(int port, int bind_any);
/* Host: waits up to timeout_ms for a guest; DD_NO_SOCKET on timeout. */
dd_socket dd_net_accept(dd_socket listener, int timeout_ms);
/* Guest: connects to host:port within timeout_ms. */
dd_socket dd_net_connect(const char *host, int port, int timeout_ms);
void dd_net_close(dd_socket s);

int dd_conn_init(dd_conn *c, dd_socket s);
void dd_conn_free(dd_conn *c); /* closes the socket and wipes the session */

/* Runs the handshake (blocking, timeout per step). Returns 1 if the session is open. */
int dd_conn_handshake_host(dd_conn *c, const char *password, int timeout_ms);
int dd_conn_handshake_guest(dd_conn *c, const char *password, int timeout_ms);

/* Sends one co-op message, sealed. Returns 1 on success. */
int dd_conn_send(dd_conn *c, const dd_msg *m);
/* Waits up to timeout_ms for one co-op message. A frame that fails to open or decode closes the
 * connection (DD_POLL_CLOSED): nothing unauthenticated or malformed is ever handed on. */
dd_poll_result dd_conn_poll(dd_conn *c, int timeout_ms, dd_msg *out);

#endif
