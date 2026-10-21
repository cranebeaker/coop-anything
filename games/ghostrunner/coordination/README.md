# Agent space: Ghostrunner

A plain-text place where agents (and their humans) working on co-op for Ghostrunner
coordinate, so that two agents never edit the same file at the same time and nobody
repeats work that already failed.

This folder is **for coordination, not for findings**. Findings live in
[`../NOTES.md`](../NOTES.md) and the write-up in [`../README.md`](../README.md). If you
learned something, append it to [`LOG.md`](LOG.md) with evidence; the work-item owner merges
it into `NOTES.md`.

## Files

| File | What it is | How to edit |
|---|---|---|
| `WORKBOARD.md` | Who is doing what right now, and what is blocked | Edit only **your own** row |
| `LOG.md` | Append-only journal: decisions, dead ends, handoffs | **Append only**, newest at the bottom, never rewrite old entries |
| `README.md` | This protocol | Change only with the owner's agreement |

## Protocol

1. **Claim before you start.** Add a row to `WORKBOARD.md` with `Owner`, `Status:
   in_progress`, `Write scope` (the paths you will modify) and `Updated`. A row without a
   write scope is not a claim.
2. **Write scope is a promise.** Do not edit paths outside your row. If you need a file
   that another row owns, write the request in `LOG.md` and pick different work this turn.
3. **One owner per file.** `NOTES.md` and `README.md` are permanent single-writer
   documents owned by whoever currently holds the merge row on the workboard. Everyone else
   goes through `LOG.md`.
4. **Append, never reorder, `LOG.md`.** Newest entries at the bottom. A git merge of two
   appends at the end is trivial; an edit in the middle is a conflict for everyone.
5. **Evidence or it did not happen.** Every claim gets a file plus offset/key/command, or a
   URL. Keep observed, derived and unknown separate — the hub's `AGENTS.md` rule 4 applies
   inside this folder too.
6. **Isolate your rig.** Your own test copy, your own network port, your own log paths, and
   only kill processes you started. The machine has 16 GB of RAM; two UE4 clients is
   already most of it.
7. **Stop conditions are shared.** If you hit one of the dead ends listed below, do not
   retry it from scratch: read the log entry, then either solve it or report.
8. **Nothing private here.** No game files, no real names, no paths with a username, no
   tokens. The upstream repo is public.

## Log entry format

```text
### YYYY-MM-DD — <handle> — <one line summary>
- Did: what you actually ran or changed
- Result: what happened, with evidence (file:offset, log line, command, URL)
- Next: the smallest next step for whoever picks this up
- Claim: work items you opened/closed (W-n ids)
```

## Current state (2026-10-21)

- The engine is **Unreal Engine 4.26**; the release is GOG, and the copy under test is a
  third-party distribution (see the "platform services" decision below).
- **UE4SS v3.0.1 injects and runs Lua in this build with no custom config.** This is proven
  at runtime and is the thing to build on.
- **Blocker: no gameplay level has been entered yet.** `open <level>` crashes the build;
  the level has to be reached through the game's own flow. Until that works, the real
  player pawn and its movement component are unknown, and no listen-server test is valid.
- Native UE4 replication is present but unproven: `IpNetDriver`, `SteamNetDriver` and
  `EOSNetDriver` are loadable classes, `NetDriverDefinitions` is honoured from the loose
  user config, but `listen` creates no NetDriver.

## Decisions that constrain everyone

- **Platform services are out of scope.** The copy under test is a pirated distribution, so
  EOS / GOG Galaxy / Steamworks auth cannot be assumed to work, and using platform services
  to paper over that would be a licensing and fairness problem. **Transport is our own UDP
  relay** (direct IP, LAN, or an overlay VPN such as Tailscale/ZeroTier). The PoppyCoop
  shape — file seam first, then UDP — is the reference.
- **No DRM, no pak signing, no anti-tamper work.** The pak index is encrypted and the `.sig`
  files are byte-identical dummies; nothing in this project may try to work around either.
  Store-agnostic runtime-only changes is the hard rule.
- **`open <level>` is a known crash.** Do not re-run it as a "quick test"; it costs a UE4
  crash report and teaches nothing new.
- **`listen` on an already-loaded standalone world does not create a NetDriver.** A map has
  to be loaded with `?listen`, through a real load path.

## Where the conversation happens

GitHub PR #107 (this game's PR) and issue #108 (the claim). Use PR comments for discussion
that needs a human, `LOG.md` for discussion between agents. One game per PR stays the rule —
do not add other games to this branch.

## If you cannot push to this branch

A `git push` returning **403** means the pushing side authenticated as a different GitHub
identity than the one that owns the fork. It is not a repository permission problem and
nothing needs to be granted by the owner. Three ways to contribute anyway, in order of
least friction:

1. **Open a pull request into the game branch** from your own fork:
   `gh pr create --repo karkqm/coop-anything --base game/ghostrunner --draft`.
   Put your `LOG.md` entry and `WORKBOARD.md` claim in that PR. The branch owner merges it,
   which keeps `LOG.md` append-only.
2. **Comment on PR #107** or issue #108 with your log entry, claim, or findings. Say clearly
   which work item id you want, so it can be transcribed without ambiguity.
3. **Send your entry to the branch owner** for transcription. Label it as a handoff, name the
   snapshot you read (`git rev-parse HEAD`), and separate observed from derived facts.

Never paste credentials, tokens, private paths or game files into any of these channels.

