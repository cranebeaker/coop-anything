# Draft issue for crxhvrd/overgrowth-online (for the owner to post)

**Title:** Join refused with "not using the same mods" when both players have the same mods: the active-mod list is compared in discovery order

**Build:** release 0.40.0 (`Version e2168f3_build-2001 2026-10-21`), Windows, two copies on one PC joined by IP (test bridge `host` / `connect`).

**What happened**

Host and guest had byte-identical `modconfig.xml` files (stock mods + Overhaul). The guest was still rejected:

```
online.cpp:3404: Connection closed by the host: You are not using the same mods the host is! The host has received a full list of expected mods!
```

**Why**

`StateHandshake::OnUpdate` (`Source/Online/online_connection_states.cpp`) compares `actor->active_mods_string` with `OnlineUtility::GetActiveModsString()` as plain strings. That string walks `ModLoading::GetAllMods()` in insertion order, and Steam Workshop mods are inserted in the order their UGC details callbacks arrive, which differs per process. Both logs show the same eight subscribed Workshop items in different orders:

```
host:   Adding steamworks mod: 2, 1, 6, 5, 7, 8, 3, 4
client: Adding steamworks mod: 4, 6, 7, 1, 3, 2, 8, 5
```

Same set, different order, mismatch.

A second contributor to the same symptom: `install.ps1` writes a `modconfig.xml` that lists only the stock mods and Overhaul, so every Workshop subscription the player has is treated as new and activated on first start (the host's rewritten `modconfig.xml` gained five `activated="true"` `modsource="steamworks"` rows). The README says Workshop mods should be disabled for online play, but the installer turns them all on.

**Suggested fixes**

1. Build the mod list from a sorted copy of the ids (both in `GetActiveModsString()` and wherever the client builds its string), or compare as sets.
2. Have `install.ps1` carry over the activation state of Workshop mods from the player's stock `Documents\Wolfire\Overgrowth\modconfig.xml`, or write them as `activated="false"`.

**Workaround that worked:** explicit `activated="false"` rows for every Workshop subscription in both players' `Documents\Wolfire\OvergrowthOnline\modconfig.xml`; after that the same two copies joined and played the first story level together.

**Smaller things seen in the same runs**

- The guest's log prints `online.cpp:2215: Tried to get own players state, but there is no state registered for player id: 0` many times per second during the join, until the host assigns the player id.
- Every start logs `Could not resolve script include: Data/Scripts/campaign_common.as`.
