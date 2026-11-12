#include "win_min.h"
#include "coop_net.h"

// ================= Minimal winsock declarations (no CRT/SDK headers) =================

typedef unsigned long long COOP_SOCKET;
typedef const char* COOP_SOCKADDR_PTR;

#define COOP_AF_INET      2
#define COOP_SOCK_DGRAM   2
#define COOP_IPPROTO_UDP  17
#define COOP_SOL_SOCKET   0xFFFF
#define COOP_SO_RCVTIMEO  0x1006
#define COOP_INVALID_SOCK ((COOP_SOCKET)(~0ULL))
#define COOP_WSA_VERSION  0x0202 // MAKEWORD(2, 2)

struct coop_sockaddr_in {
    short          sin_family;
    unsigned short sin_port;   // network order
    unsigned int   sin_addr;   // network order
    char           sin_zero[8];
};

__declspec(dllimport) int WSAStartup(unsigned short wVersionRequested, void* lpWSAData);
__declspec(dllimport) int WSACleanup(void);
__declspec(dllimport) COOP_SOCKET socket(int af, int type, int protocol);
__declspec(dllimport) int bind(COOP_SOCKET s, const void* name, int namelen);
__declspec(dllimport) int getsockname(COOP_SOCKET s, void* name, int* namelen);
__declspec(dllimport) int sendto(COOP_SOCKET s, const char* buf, int len, int flags, const void* to, int tolen);
__declspec(dllimport) int recvfrom(COOP_SOCKET s, char* buf, int len, int flags, void* from, int* fromlen);
__declspec(dllimport) int closesocket(COOP_SOCKET s);
__declspec(dllimport) int setsockopt(COOP_SOCKET s, int level, int optname, const char* optval, int optlen);
__declspec(dllimport) unsigned short htons(unsigned short hostshort);
__declspec(dllimport) unsigned short ntohs(unsigned short netshort);

// WSADATA is ~400 bytes; a 512-byte scratch buffer is safe.
static unsigned char g_wsaData[512];

static struct coop_sockaddr_in make_addr(const CoopNetAddress* a)
{
    struct coop_sockaddr_in sa;
    sa.sin_family = COOP_AF_INET;
    sa.sin_port = htons(a->port);
    sa.sin_addr = a->ip;
    for (int i = 0; i < 8; i++) sa.sin_zero[i] = 0;
    return sa;
}

int coop_net_init(void)
{
    return WSAStartup(COOP_WSA_VERSION, g_wsaData) == 0 ? 0 : -1;
}

void coop_net_shutdown(void)
{
    WSACleanup();
}

unsigned long long coop_net_open(unsigned short bind_port)
{
    COOP_SOCKET s = socket(COOP_AF_INET, COOP_SOCK_DGRAM, COOP_IPPROTO_UDP);
    if (s == COOP_INVALID_SOCK)
        return COOP_NET_INVALID_SOCKET;

    struct coop_sockaddr_in sa;
    sa.sin_family = COOP_AF_INET;
    sa.sin_port = htons(bind_port);
    sa.sin_addr = 0; // INADDR_ANY
    for (int i = 0; i < 8; i++) sa.sin_zero[i] = 0;

    if (bind(s, &sa, (int)sizeof(sa)) != 0) {
        closesocket(s);
        return COOP_NET_INVALID_SOCKET;
    }
    return (unsigned long long)s;
}

void coop_net_close(unsigned long long sock)
{
    if (sock != COOP_NET_INVALID_SOCKET)
        closesocket((COOP_SOCKET)sock);
}

int coop_net_send(unsigned long long sock, const void* data, int len, const CoopNetAddress* to)
{
    if (!data || len <= 0 || !to)
        return -1;
    struct coop_sockaddr_in sa = make_addr(to);
    return sendto((COOP_SOCKET)sock, (const char*)data, len, 0, &sa, (int)sizeof(sa));
}

int coop_net_recv(unsigned long long sock, void* buf, int cap, CoopNetAddress* from, int timeout_ms)
{
    if (!buf || cap <= 0)
        return -1;

    if (timeout_ms > 0) {
        // Windows expects a DWORD timeout in milliseconds for SO_RCVTIMEO
        // (unlike POSIX struct timeval).
        unsigned long timeout = (unsigned long)timeout_ms;
        setsockopt((COOP_SOCKET)sock, COOP_SOL_SOCKET, COOP_SO_RCVTIMEO,
                   (const char*)&timeout, (int)sizeof(timeout));
    }

    struct coop_sockaddr_in sa;
    int sa_len = (int)sizeof(sa);
    int n = recvfrom((COOP_SOCKET)sock, (char*)buf, cap, 0, &sa, &sa_len);
    if (n < 0)
        return -1;

    if (from) {
        from->ip = sa.sin_addr;
        from->port = ntohs(sa.sin_port);
    }
    return n;
}

int coop_net_local_port(unsigned long long sock, unsigned short* out_port)
{
    if (!out_port)
        return -1;
    struct coop_sockaddr_in sa;
    int sa_len = (int)sizeof(sa);
    if (getsockname((COOP_SOCKET)sock, &sa, &sa_len) != 0)
        return -1;
    *out_port = ntohs(sa.sin_port);
    return 0;
}
