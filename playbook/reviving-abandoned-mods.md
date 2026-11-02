# Reviving a half-finished co-op mod

Most loved games already have a co-op attempt somewhere: a stalled alpha, a closed
beta, a fork with one contributor. Finishing one of those beats starting from zero.
This is the route that worked for Deus Ex (HX, last build 2019) and STALKER Shadow of
Chernobyl (Shadow of COOP fix patch).

## 1. Find every fragment

- Search GitHub (code and forks), ModDB, Nexus, Reddit, Steam forums, archive.org
  copies of dead sites, and the game's community Discord. Read
  [research.md](research.md).
- Look for the **last build**, not the last news post. Authors often keep a builds or
  testing folder that is newer than the release page.
- Check the author's other projects. HX's author also made a large overhaul of the
  same game, which explained the code style and some design choices.
- Note the license or lack of one. Without a license you can study and patch, but ask
  before redistributing the original files. Ship your fixes as a separate package and
  point players to the original download.

## 2. Get readable code

- **Script layer:** many engines keep game logic in script packages (UnrealScript in
  `.u` files, Lua in archives, Papyrus). Dump them to text. For HX, a small Python
  script dumped 960 classes (about 120k lines) from the package.
- **Native layer:** decompile with Ghidra headless (a `DecompileAll` script over every
  function, written to one `.c` file plus a function index) or an agent tool such as
  REA. Keep a grep helper that extracts one function by name. See
  [reverse-engineering.md](reverse-engineering.md).
- **Stock reference:** get the original game's script source if it was ever released
  (Deus Ex shipped its UnrealScript), so you can diff the mod against the base game.

## 3. Audit before fixing

Split the code by subsystem (player, devices, conversations, weapons, missions, travel)
and give each to an agent with read-only instructions: list bugs with file and line,
how a player would hit it, and confidence. Then **check every claim yourself** before
fixing. On HX about one claim in six was wrong; one "host can't use computers" claim was
refuted by a live test.

Also **diff every copy of the protocol**. A mod's server side often exists twice (a standalone
server and a copy compiled into the game, or a relay and a client with hand-copied packet ids), and
they drift. OpenOL (Outlast) ships a relay copy that still parses the pre-channel packet format, so
clients silently reconnect every 30 s, and its relay and client disagree on the enemy packet ids. A
10-minute check with a logging proxy between client and server finds this faster than reading code.

## 4. Fix without touching the original package

Rebuilding the original package is often impossible (missing source, native classes
whose layout lives in a closed DLL, compiler assertions). Instead:

- Ship a **new package** that subclasses the broken classes and swaps them in at
  runtime: a game type subclass, a mutator that replaces spawned actors, player class
  subclasses picked at login.
- Group fixes into **modules** (one per subsystem) that the game type forwards hooks to,
  so parallel agents can work without editing the same file.
- Keep a `BUGS.md`: fixed, open, refuted, and "out of reach" (in native code you can't
  change).

## 5. Test it live, then automate

Get a host and a real client running on one PC first, then turn every fix into a test
in the suite. See [automated-qa.md](automated-qa.md). Live testing found bugs no audit
did: a client crash on every third map change, a crash when the inventory screen was
open during travel, a forced conversation silently dropped.

## 6. Make it easy to play

- An installer that finds the game, copies files and edits the INI: one file per OS, see
  [shipping-to-players.md](shipping-to-players.md).
- Host and Join launchers. Better: Steam relay so nobody types an IP, see
  [networking-steam-spacewar.md](networking-steam-spacewar.md).
- A README with the three steps, and the known issues in plain words.

## 7. Give it back

Post the fixes where the original community lives (their Discord, forum, repo), credit
the original author, and add the project to the [directory](../DIRECTORY.md) with its
QA level.
