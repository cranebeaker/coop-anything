// Self-test for the remote body: snapshots in, a smoothed pose out. No game, no DLL, no socket
// traffic - only the Net object is instantiated, to prove the pull path is wired the right way.
//
// Each check states a rule from `remote.h` and fails when the rule stops holding: the midpoint
// of two snapshots is their average, a gap holds and flags instead of extrapolating, yaw takes
// the short way across the wrap, and a lone snapshot is used as it is.
#include <cmath>
#include <cstdio>

#include "net.h"
#include "remote.h"

namespace {

int failures = 0;

void check(const char* name, bool ok, const char* detail = "")
{
    std::printf("%s %s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) {
        ++failures;
    }
}

bool near(float actual, float expected, float tolerance = 1e-4f)
{
    return std::fabs(actual - expected) <= tolerance;
}

// Shortest angular distance in degrees: 180 and -180 are the same direction.
float angle_gap(float a, float b)
{
    float difference = std::fmod(a - b + 180.0f, 360.0f);
    if (difference < 0.0f) {
        difference += 360.0f;
    }
    return std::fabs(difference - 180.0f);
}

coop::Snapshot snap(uint32_t time_ms, float x, float y, float z, float yaw)
{
    coop::Snapshot snapshot;
    snapshot.time_ms = time_ms;
    snapshot.x = x;
    snapshot.y = y;
    snapshot.z = z;
    snapshot.yaw = yaw;
    return snapshot;
}

}  // namespace

int main()
{
    // 1. Two snapshots, one interpolation point. Default delay is 100 ms, so At(1150)
    //    renders the moment 1050, halfway between 1000 and 1100.
    {
        coop::RemoteBody body;
        check("delay defaults to 100 ms", body.DelayMs() == 100);
        body.PushSnapshot(snap(1000, 0.0f, 0.0f, 0.0f, 0.0f));
        body.PushSnapshot(snap(1100, 10.0f, 20.0f, -4.0f, 40.0f));
        coop::RemoteBody::Pose pose = body.At(1150);
        check("midpoint equals the average of the two snapshots",
              near(pose.x, 5.0f) && near(pose.y, 10.0f) && near(pose.z, -2.0f),
              "");
        check("midpoint interpolates yaw", near(pose.yaw, 20.0f));
        check("midpoint is not stale", !pose.stale);

        pose = body.At(1125);   // renders 1025, a quarter of the way
        check("quarter point interpolates", near(pose.x, 2.5f) && near(pose.yaw, 10.0f));

        pose = body.At(1200);   // renders 1100, exactly the newer snapshot
        check("render time at a snapshot returns that snapshot",
              near(pose.x, 10.0f) && near(pose.z, -4.0f) && !pose.stale);
    }

    // 2. One snapshot: use it, and say that it is a hold rather than an interpolation.
    {
        coop::RemoteBody body;
        body.PushSnapshot(snap(1000, 3.0f, 4.0f, 5.0f, 90.0f));
        check("one snapshot is kept", body.Count() == 1);
        coop::RemoteBody::Pose pose = body.At(1050);
        check("one snapshot: the pose equals it",
              near(pose.x, 3.0f) && near(pose.y, 4.0f) && near(pose.z, 5.0f) &&
                  near(pose.yaw, 90.0f));
        check("one snapshot is flagged stale", pose.stale);

        body.Clear();
        check("clear forgets the peer", body.Count() == 0 && body.At(1050).stale);
    }

    // 3. No snapshot at all: the origin, stale. Never a NaN, never a guess.
    {
        coop::RemoteBody body;
        coop::RemoteBody::Pose pose = body.At(5000);
        check("empty body is stale at the origin",
              pose.stale && pose.x == 0.0f && pose.y == 0.0f && pose.z == 0.0f &&
                  pose.yaw == 0.0f);
    }

    // 4. A gap longer than 500 ms: hold the newest and flag it, do not extrapolate.
    {
        coop::RemoteBody body;
        body.PushSnapshot(snap(1000, 0.0f, 0.0f, 0.0f, 0.0f));
        body.PushSnapshot(snap(1100, 10.0f, 20.0f, -4.0f, 40.0f));
        coop::RemoteBody::Pose pose = body.At(1900);   // 800 ms of silence
        check("a stale gap sets the flag", pose.stale);
        check("a stale gap holds the newest snapshot, not an extrapolation",
              near(pose.x, 10.0f) && near(pose.y, 20.0f) && near(pose.z, -4.0f));

        pose = body.At(1550);   // 450 ms of silence: one late packet, still fine
        check("a short silence holds the newest without the flag",
              !pose.stale && near(pose.x, 10.0f));
    }

    // 5. A hole *between* buffered snapshots, with the target inside it.
    {
        coop::RemoteBody body(0);   // no delay, so the target lands inside the history
        body.PushSnapshot(snap(1000, 0.0f, 0.0f, 0.0f, 0.0f));
        body.PushSnapshot(snap(1100, 10.0f, 0.0f, 0.0f, 0.0f));
        body.PushSnapshot(snap(2000, 100.0f, 0.0f, 0.0f, 0.0f));
        coop::RemoteBody::Pose pose = body.At(1600);   // between 1100 and 2000
        check("a hole in the stream is stale", pose.stale);
        check("a hole holds the newer end", near(pose.x, 100.0f));
    }

    // 6. Yaw interpolates the short way across the -180..180 wrap, both directions.
    {
        coop::RemoteBody body;
        body.PushSnapshot(snap(1000, 0.0f, 0.0f, 0.0f, 170.0f));
        body.PushSnapshot(snap(1100, 0.0f, 0.0f, 0.0f, -170.0f));
        coop::RemoteBody::Pose pose = body.At(1150);
        check("yaw 170 -> -170 goes through 180", angle_gap(pose.yaw, 180.0f) < 0.01f);
        std::printf("     yaw 170 -> -170 at the midpoint: %.3f\n", pose.yaw);

        coop::RemoteBody other;
        other.PushSnapshot(snap(1000, 0.0f, 0.0f, 0.0f, -170.0f));
        other.PushSnapshot(snap(1100, 0.0f, 0.0f, 0.0f, 170.0f));
        pose = other.At(1150);
        check("yaw -170 -> 170 goes through -180", angle_gap(pose.yaw, -180.0f) < 0.01f);

        // 350 -> 10 is +20 degrees, not -340, and the result is normalised.
        coop::RemoteBody wrapped;
        wrapped.PushSnapshot(snap(1000, 0.0f, 0.0f, 0.0f, 350.0f));
        wrapped.PushSnapshot(snap(1100, 0.0f, 0.0f, 0.0f, 10.0f));
        pose = wrapped.At(1150);
        check("yaw outside -180..180 is normalised",
              angle_gap(pose.yaw, 0.0f) < 0.01f && pose.yaw >= -180.0f && pose.yaw <= 180.0f);

        // A snapshot pair that does not cross the wrap must not take a detour.
        coop::RemoteBody straight;
        straight.PushSnapshot(snap(1000, 0.0f, 0.0f, 0.0f, -45.0f));
        straight.PushSnapshot(snap(1100, 0.0f, 0.0f, 0.0f, 45.0f));
        pose = straight.At(1150);
        check("yaw without a wrap stays on the short path", near(pose.yaw, 0.0f, 0.01f));
    }

    // 7. The history is bounded, newest-wins, and still interpolates after it wraps.
    {
        coop::RemoteBody body(0);
        for (int i = 0; i < 40; ++i) {
            body.PushSnapshot(snap(1000u + static_cast<uint32_t>(i) * 50u, static_cast<float>(i),
                                   0.0f, 0.0f, 0.0f));
        }
        check("history holds its capacity", body.Count() == coop::RemoteBody::kHistory);
        check("history newest is the last push", near(body.Newest().x, 39.0f));
        check("an older timestamp is rejected", !body.PushSnapshot(snap(1500, 999.0f, 0, 0, 0)));
        const uint32_t last = 1000u + 39u * 50u;
        coop::RemoteBody::Pose pose = body.At(last - 25u);   // halfway between the last two
        check("history still interpolates after it wraps",
              near(pose.x, 38.5f) && !pose.stale);
    }

    // 8. The Net hook-up: polling a Net with no peer must add nothing and not fail.
    {
        coop::Net net;
        coop::RemoteBody body;
        check("pull with no peer adds nothing", !body.Pull(net, 1000) && body.Count() == 0);
    }

    std::printf("%s\n", failures == 0 ? "ALL OK" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
