#include "lr_sock.h"

#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define CLOSESOCK closesocket
static int would_block(void) {
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY;
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSESOCK close
#define INVALID_SOCKET (-1)
static int would_block(void) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS; }
#endif

int lr_sock_startup(void) {
#ifdef _WIN32
    WSADATA w;
    return WSAStartup(MAKEWORD(2, 2), &w) == 0;
#else
    signal(SIGPIPE, SIG_IGN); /* a closed game socket must not kill the launcher */
    return 1;
#endif
}

void lr_sock_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static void set_nonblocking(lr_sock s) {
#ifdef _WIN32
    u_long on = 1;
    ioctlsocket((SOCKET)s, FIONBIO, &on);
#else
    fcntl((int)s, F_SETFL, fcntl((int)s, F_GETFL) | O_NONBLOCK);
#endif
    int one = 1;
    setsockopt((int)s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
}

static struct sockaddr_in local_addr(int port) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}

lr_sock lr_listen_local(int port, int *bound) {
    lr_sock s = (lr_sock)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == (lr_sock)INVALID_SOCKET) return LR_NOSOCK;
#ifdef _WIN32
    int excl = 1; /* nobody else may share the port */
    setsockopt((SOCKET)s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&excl, sizeof excl);
#endif
    struct sockaddr_in a = local_addr(port);
    if (bind((int)s, (struct sockaddr *)&a, sizeof a) != 0 || listen((int)s, 4) != 0) {
        CLOSESOCK(s);
        return LR_NOSOCK;
    }
    socklen_t len = sizeof a;
    getsockname((int)s, (struct sockaddr *)&a, &len);
    if (bound) *bound = ntohs(a.sin_port);
    set_nonblocking(s);
    return s;
}

lr_sock lr_accept_nb(lr_sock listener) {
    lr_sock c = (lr_sock)accept((int)listener, NULL, NULL);
    if (c == (lr_sock)INVALID_SOCKET) return LR_NOSOCK;
    set_nonblocking(c);
    return c;
}

lr_sock lr_connect_local_nb(int port) {
    lr_sock s = (lr_sock)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == (lr_sock)INVALID_SOCKET) return LR_NOSOCK;
    set_nonblocking(s);
    struct sockaddr_in a = local_addr(port);
    if (connect((int)s, (struct sockaddr *)&a, sizeof a) != 0 && !would_block()) {
        CLOSESOCK(s);
        return LR_NOSOCK;
    }
    return s;
}

int lr_connect_check(lr_sock s) {
    fd_set w, e;
    FD_ZERO(&w);
    FD_ZERO(&e);
    FD_SET(s, &w);
    FD_SET(s, &e);
    struct timeval tv = {0, 0};
    int r = select((int)s + 1, NULL, &w, &e, &tv);
    if (r == 0) return 0;
    if (r < 0) return -1;
    int err = 0;
    socklen_t len = sizeof err;
    getsockopt((int)s, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
    return (err == 0 && FD_ISSET(s, &w)) ? 1 : -1;
}

int lr_recv_nb(lr_sock s, uint8_t *buf, size_t n) {
    int r = (int)recv((int)s, (char *)buf, (int)n, 0);
    if (r > 0) return r;
    if (r < 0 && would_block()) return 0;
    return -1;
}

int lr_send_nb(lr_sock s, const uint8_t *buf, size_t n) {
#ifdef MSG_NOSIGNAL
    int r = (int)send((int)s, (const char *)buf, (int)n, MSG_NOSIGNAL);
#else
    int r = (int)send((int)s, (const char *)buf, (int)n, 0);
#endif
    if (r >= 0) return r;
    return would_block() ? 0 : -1;
}

void lr_shutdown_write(lr_sock s) {
#ifdef _WIN32
    shutdown((SOCKET)s, SD_SEND);
#else
    shutdown((int)s, SHUT_WR);
#endif
}

void lr_close(lr_sock s) {
    if (s != LR_NOSOCK) CLOSESOCK(s);
}

int lr_port_free(int port) {
    int bound = 0;
    lr_sock s = lr_listen_local(port, &bound);
    if (s == LR_NOSOCK) return 0;
    lr_close(s);
    return 1;
}
