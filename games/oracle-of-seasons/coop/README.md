# Original co-op helpers and native tests

These components are not a playable multiplayer mod. They contain original
layout/art/test code, with no game assets or decompiled game implementation.
The [approved design](../../../docs/superpowers/specs/2026-10-16-oracle-of-seasons-coop-design.md)
describes the intended game behavior; native integration remains in progress.

## Partner marker

Build and run the standalone tests with a C11 compiler and Make:

```sh
make test
```

Recorded result on 2026-10-16: 149 layout checks and 105 renderer checks passed
with `-Wall -Wextra -Werror -pedantic`. Negative controls detected missing
clamping, visible/hidden-state mistakes, HUD corruption, a fabricated area arrow,
and incorrect direction quantization. Native game screenshots and live partner
tracking are separate integration requirements, not proved by these tests.

`coop_marker_layout` places an 18x20 badge inside a padded viewport, below its
16-pixel HUD. Inputs explicitly distinguish visibility, known location and shared
coordinate space. `coop_marker_draw` draws original pixel art into a caller-owned
RGB555 buffer: blue P2 or green P1, a player label, and an outward chevron. Area
mode omits the arrow and adds a separate clipped caption below or above the badge.
It hides if there is insufficient room for a readable caption. The bounded
uppercase font substitutes `?` for unsupported characters.

Generate an abstract 20-panel art preview without game data:

```sh
make preview
# Output: build/marker-contact-sheet.ppm
```

The APIs and allocation/bounds requirements are documented in
[marker.h](marker.h) and [marker_draw.h](marker_draw.h). They have no SDL or network
dependency. A game renderer must supply trustworthy view/location data and invoke
the draw helper on the appropriate final player view.

## Native runtime test

[native-tests/README.md](native-tests/README.md) gives the pinned source/fixture
identities and standalone compile/run commands for the original runtime isolation
harness. It uses a separately supplied ROM and native-port checkout. Its bounded
isolation result does not prove two players share one world.

## Loopback transport helper

[transport/README.md](transport/README.md) documents a standalone C11/POSIX
framing and compatibility-handshake helper. Run `make -C transport test` for its
two real-TCP test executables. Independent raw peers and six compile-success
mutations check framing, compatibility, sequence replay and short-I/O handling.

The helper binds and connects only to literal `127.0.0.1`. Gameplay integration,
session security, reconnect resync, fault conditions and friend connectivity are
still pending. It does not implement host/join for the game.
