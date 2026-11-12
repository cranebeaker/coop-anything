#ifndef COOP_NET_H
#define COOP_NET_H

// UDP transport for the Outlast 2 co-op relay layer (Phase 3 prep).
// Freestanding-friendly: implemented against ws2_32 imports declared in
// coop_net.c (no CRT headers needed).

typedef struct CoopNetAddress {
    unsigned int   ip;   // network byte order (127.0.0.1 = 0x0100007F)
    unsigned short port; // host byte order
} CoopNetAddress;

#define COOP_NET_INVALID_SOCKET (~(unsigned long long)0)

// Returns 0 on success, negative on failure.
int coop_net_init(void);
void coop_net_shutdown(void);

// Opens a UDP socket bound to bind_port (0 = ephemeral).
// Returns socket handle or COOP_NET_INVALID_SOCKET.
unsigned long long coop_net_open(unsigned short bind_port);
void coop_net_close(unsigned long long sock);

// Sends len bytes to address. Returns bytes sent or -1.
int coop_net_send(unsigned long long sock, const void* data, int len, const CoopNetAddress* to);

// Receives up to cap bytes. Waits up to timeout_ms (0 = block forever).
// Fills from (if non-null). Returns bytes received or -1 on error/timeout.
int coop_net_recv(unsigned long long sock, void* buf, int cap, CoopNetAddress* from, int timeout_ms);

// Reports the bound local port (host byte order). Returns 0 on success.
int coop_net_local_port(unsigned long long sock, unsigned short* out_port);

#endif // COOP_NET_H
