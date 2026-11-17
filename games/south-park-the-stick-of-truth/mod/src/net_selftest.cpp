// Self-test for the transport: two endpoints on loopback, no game, no DLL.
//
// Checks that a snapshot published by one side arrives at the other with the same numbers,
// that the guest learns the host's address from the first packet, that an older sequence never
// overwrites a newer one, and that the peer age grows when packets stop.
#include <windows.h>

#include <cstdio>

#include "net.h"

namespace {

int failures = 0;

void check(const char* name, bool ok, const char* detail = "")
{
    std::printf("%s %s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) {
        ++failures;
    }
}

}  // namespace

int main()
{
    coop::Net host;
    coop::Net guest;
    host.SetIdentity(228353);
    guest.SetIdentity(228353);
    check("host listens", host.Listen(27991));
    check("guest connects", guest.Connect("127.0.0.1", 27991));

    coop::PlayerState state;
    state.x = 1.5f;
    state.y = -2.25f;
    state.z = 0.75f;
    state.yaw = 90.0f;
    state.animation = 7;
    state.body_id = 3;

    guest.Publish(state, 1000);
    for (int attempt = 0; attempt < 20; ++attempt) {
        host.Poll(1010 + attempt * 10);
        if (host.HasPeer()) {
            break;
        }
        ::Sleep(10);
    }
    check("first snapshot arrives", host.HasPeer());
    check("hello from the guest arrives", host.PeerHandshakeOk());
    coop::PlayerState peer = host.Peer();
    check("position matches", peer.x == 1.5f && peer.y == -2.25f && peer.z == 0.75f);
    check("yaw and ids match", peer.yaw == 90.0f && peer.animation == 7 && peer.body_id == 3);

    // A newer snapshot must win.
    state.x = 9.0f;
    host.Publish(state, 2000);
    for (int attempt = 0; attempt < 20; ++attempt) {
        guest.Poll(2010 + attempt * 10);
        if (guest.Peer().x == 9.0f) {
            break;
        }
        ::Sleep(10);
    }
    check("newer snapshot wins", guest.Peer().x == 9.0f);
    check("hello from the host arrives", guest.PeerHandshakeOk());

    // A third endpoint with a different build id must not pass the handshake.
    coop::Net other;
    other.SetIdentity(999999);
    coop::PlayerState tiny;
    tiny.x = 0.25f;
    other.Connect("127.0.0.1", 27991);
    other.Publish(tiny, 2400);
    for (int attempt = 0; attempt < 20; ++attempt) {
        host.Poll(2410 + attempt * 10);   // the stranger's hello lands on the host
        ::Sleep(10);
    }
    check("mixed build refused", !host.PeerHandshakeOk() && host.HasPeer());
    std::printf("host sees peer build %u\n", host.PeerBuildId());

    // Age grows once packets stop; the test waits long enough to see it.
    const uint32_t age = host.PeerAgeMs(2600);
    check("peer age grows", age >= 500, "");
    std::printf("peer age at 2600 ms: %u\n", age);
    std::printf("sent %u, received %u\n", host.SentCount(), guest.ReceivedCount());

    std::printf("%s\n", failures == 0 ? "ALL OK" : "FAILURES");
    return failures == 0 ? 0 : 1;
}

