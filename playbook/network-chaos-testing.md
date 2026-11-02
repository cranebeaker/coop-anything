# Network chaos testing: make the link bad on purpose

Co-op sync that works on a LAN often breaks on the internet. This page shows how to test
for that with repeatable bad networks, in a simulator and between real game copies. The tool
is in [tools/chaos/](../tools/chaos/README.md). It works with any UDP game or mod.

Read [networking.md](networking.md) first for sync models and transports, and
[automated-qa.md](automated-qa.md) for the overall test shape. This page covers the bad-link part.

## 1. What a bad link does to a co-op mod

| Link problem | What breaks in a sync layer | What a good layer does |
|---|---|---|
| Packet loss | An update never arrives, so the copies differ forever | Acknowledge and resend, or detect the gap and repair |
| Bursts of loss | Several updates in a row vanish, so a single resend is not enough | Resend with backoff, repair by comparing state |
| Reordering | Update 7 arrives before update 6. Applying both in arrival order gives a wrong end state | Number every update. Buffer gaps. Apply in order |
| Duplicates | The same "give 100 caps" applies twice | Make every update idempotent, or de-duplicate by id |
| Corruption | Garbage is applied as real state | Checksum every message. Drop what fails |
| Jitter | Remote players and actors stutter, timeouts fire early | Interpolation buffer, timeouts longer than the worst jitter |
| Small pipe | A big catch-up (late join, rejoin) floods the link and starves live updates | Chunk it, pace it, keep live traffic first |
| One-way outage | A client can hear the host but not talk, or the reverse | Retry requests, do not run an action twice when the retry finally lands |
| Full disconnect | A client leaves for a few seconds and comes back with old state | Resume from the last acknowledged update, or send a fresh baseline |

Most of these bugs do not show up with two copies on one PC, because loopback is perfect.
Turning the link bad is the only way to see them before your friends do.

## 2. Three layers of tooling

| Layer | Tool | Good for | Not good for |
|---|---|---|---|
| In the engine or library | Unreal `NetEmulation`, LiteNetLib simulation, netem on Linux (see [networking.md](networking.md#5-testing-netcode)) | Quick checks when the engine has the knob | Repeatable seeds, a packet log, scripted outages at exact test steps |
| System wide | [clumsy](https://github.com/jagt/clumsy) on Windows | A first look by hand, no code changes | Repeating the exact same bad luck, driving it from a test script |
| This repo's chaos box | `tools/chaos` | Seeded, scripted, logged impairment between two real copies, and the same model inside a simulator | TCP games (UDP only) |

Use the chaos box when a test must run unattended and fail the same way twice.

## 3. The recommended order

1. **Simulator first.** If you have a game-free sync core, run it over the C++ mirror
   (`tools/chaos/cpp/impair.h`) with many seeds. This is fast, because there are no sockets and no real time, and no game
   is needed. A fuzzer on top of it is described in [fuzzing-coop-sync.md](fuzzing-coop-sync.md).
2. **Real copies through the relay.** Once the layer survives the simulator, put the relay between
   two real game copies and run your normal scenario with `bad_wifi`, then `hell`.
3. **Outages at chosen steps.** Cut one direction or both in the middle of a scenario, then heal, and
   assert that the copies converge.
4. **Keep the failing seeds.** Every failure becomes a regression test with the profile, the seed and the
   scenario step written down.

Because both the relay and the C++ mirror share one random stream, "profile X, seed N" means the same
drops and delays in both places. A failure found in the simulator can be replayed on real copies and the
other way round.

## 4. Choosing profiles

The box ships six, all in `profiles.json`. Delays are one way.

| Profile | In plain words | Run it to learn |
|---|---|---|
| `lan` | Nearly perfect | Your baseline. Any failure here is not a network bug |
| `good_wifi` | 12 ms, 0.5 % loss | The normal case still works |
| `bad_wifi` | 35 ms +/- 25, 2 % loss with bursts, a little reorder and duplication | The main bug finder. Run every scenario with this |
| `mobile_tether` | 70 ms, 2 Mbit/s cap | Catch-up traffic and queue behaviour |
| `transatlantic` | 90 ms, clean | Timeouts and anything that assumed low latency |
| `hell` | 200 ms +/- 150, 8 % loss, 10 % reorder, 5 % duplicates, 512 kbit/s | Stress. If it passes here, it is solid |

Add your own by editing `profiles.json` and running `python -I tools/chaos/gen_header.py --write`.
The tests then check the C++ mirror still agrees.

## 5. A scenario template

1. Start the host, then the relay in front of it (`chaos.py relay --target ... --info-file info.json`).
2. Start the client against the relay's listen port.
3. Assert the mode first: two players are connected, and the relay `stats` show packets in both directions.
   A scenario that silently fell back to single player proves nothing.
4. Set a profile: `client.set_profile("info.json", "bad_wifi")`.
5. Do the thing under test through the same path a player uses: complete a quest step, pick up loot,
   open a door, change an object.
6. Optional outage: `client.partition("info.json", "s2c", seconds=3)` while the client acts, then wait for it to heal.
7. **End the chaos, then wait for quiet.** Switch to `lan` (or call `reconnect`), wait until the layer has
   had time to resend and repair (a fixed window plus a check that the host's last update number equals the
   client's acknowledged number is better than a bare sleep).
8. Compare state on both copies (snapshot and diff, see [desync-hunting.md](desync-hunting.md)).
9. Assert that the chaos really happened: relay `stats` show drops and, for the profiles that have them,
   duplicates. A "pass" under a profile that dropped nothing is a hollow pass.

## 6. What to assert

- **Convergence.** After the chaos ends and the system is quiet, both copies hold the same shared state.
- **No double apply.** Rewards, spawns and counters changed once, not twice.
- **No stale win.** An old update that arrives late does not overwrite a newer one.
- **Exactly once for requests.** A client action sent twice because of a retry runs once on the host.
- **Bounded repair.** The number of repairs per minute under `bad_wifi` stays small, and is zero on `lan`.
  Repairs mean the first line of defence missed something. Log them and treat a rising count as a bug.
- **Recovery time.** After a 5 s blackout the copies are back in sync within a number of seconds you wrote down.

Do not assert on exact timing of single packets. Assert on end state and on counts, with tolerances.

## 7. Making failures repeatable

- Fix the profile seed in the test (`--seed N` or `set_profile` with `seed`). Print it at the top of the log.
- Use `--log` to get a packet log. It records every datagram with the verdict (forwarded or dropped, and why).
  Match its first bytes and order against your own traces to find which datagram a desync began with.
- If a run is flaky only under chaos, rerun it with the same seed. If the failure repeats, it is a sync bug.
  If it does not, something else (wall-clock timing, game load) is in play, and you should say so in the report.
- Wall-clock jitter on the relay is small but real. The simulator uses virtual time and is exactly repeatable. Prefer
  the simulator when you need a bit-exact replay.

## 8. Pitfalls

- **Latency stacks.** The relay delay is per direction and adds to the real delay. 35 ms in each direction is about
  70 ms round trip.
- **Game timeouts are part of the test.** Under `hell` a game's own connection timeout may fire. Decide whether
  that is an expected result ("client is kicked after 10 s of blackout") and assert it, rather than loosening the profile.
- **Encrypted or signed packets.** `corrupt` then makes the receiver drop the packet, which is a fine test of
  the drop path. It does not test your own checksum unless the checksum is yours.
- **Steam relay or tunnelled games.** Put the box between the two ends of your local tunnel, on loopback, not on the
  Steam leg. See [networking-steam-spacewar.md](networking-steam-spacewar.md).
- **One client port.** A client that changes its source port looks like a new peer.
- **Ports.** Use one relay per host, with its own ports, per parallel agent, so agents do not collide.
- **Do not ship the box inside your game.** It is a test tool.

## 9. Test the box itself

A chaos tool that silently does nothing would make every test pass. `tools/chaos/run_tests.py` checks this:
model statistics per profile, real UDP round trips, and a set of mutation twins (each one breaks a behaviour
on purpose, such as "no loss" or "partition ignored", and a test must fail). Run it after any change to the box:

```
python -I tools/chaos/run_tests.py
```

It prints `ALL OK` only when everything passes and every mutant is caught.

## 10. What to put in your game's QA section

- Which profiles you ran, with seeds.
- The outage tests (which direction, how long, how long to recover).
- The assertions from section 6, and the command that runs them.
- Open items (for example, a game that could not be pointed at the relay) go in a GitHub issue, not the write-up.

## Sources and credits

- [Gaffer on Games](https://gafferongames.com/): the networking articles on snapshots, state sync and reliability are the best
  introduction to why these problems exist.
- [clumsy](https://github.com/jagt/clumsy) (Windows) and Linux `tc netem` for system level impairment.
- The Gilbert-Elliott two-state model is the textbook way to make loss come in bursts.
