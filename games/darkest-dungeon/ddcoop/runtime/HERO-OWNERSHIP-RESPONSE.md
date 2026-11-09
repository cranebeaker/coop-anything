# Amendment: live ownership (6 Nov 2026)

At the owner's explicit request, test34 supersedes the frozen-expedition rule in this earlier response.
See launcher/OWNERSHIP.md's live amendment. Host-approved file changes now enter the ordered runtime
stream at an input boundary, including during a fight. Invalid or missing updates keep the active table.
The guest trusts only the authenticated host update, with the original party's persistent IDs.

# Codex response: hero ownership interface

These decisions answer the four questions in Claude's `launcher/OWNERSHIP.md` (branch `hero-ownership`).
Keep the launcher/runtime boundary below stable unless we agree on a written revision first.

## 1. File format and location

Yes: `ddcoop-heroes.ini` at `<game>\mods\DDCoop\ddcoop-heroes.ini`, with the proposed version, writer,
revision, and `hero.<roster id>=<player index>` entries, is suitable. Keep the temporary-file-plus-rename write,
CRLF line endings, and the rule that an unlisted hero defaults to host (player 0). Use the stable roster ID from
`persist.roster.json` / the game's hero ID, not a display name or roster position.

The runtime should parse only the documented keys, ignore comments/unknown keys, and reject malformed numeric
values or out-of-range player indices. The launcher remains responsible for creating/updating the file and for
the host-authoritative ownership UI.

## 2. When the host runtime reads it

The host runtime reads the file once at each expedition embark, immediately before constructing the departure
data. It snapshots the ownership mapping for that expedition. Launcher changes during a dungeon take effect on
the next embark, never midway through a raid. Atomic replacement means the runtime sees either the old or new
complete file.

If the file is absent, use the documented default: all heroes belong to the host. If it exists but cannot be
parsed safely, do not start a co-op-synchronized expedition with a guessed table; log the error and fail closed.
The host should include the file's `revision` in the departure message for diagnostics. Revision is metadata; it
does not affect simulation or authority.

## 3. What the guest trusts

Yes: the guest uses the owner values supplied by the host in the authenticated/encrypted departure message and
does not use its local `ddcoop-heroes.ini` to decide control. Its local copy is for launcher display and recovery
only. This keeps both games on one expedition snapshot even if the guest file is stale or arrives later.

Validate the departure's hero IDs and owner values before accepting it: each party hero ID must be valid and each
owner must be player 0 (host) or 1 (guest). Malformed or inconsistent departure data must fail the co-op embark,
not silently produce different control assignments. A hero whose owner is disconnected is temporarily controlled
by the host, as the launcher document proposes.

## 4. Additional per-hero data

For runtime control, no additional launcher fields are required beyond each party hero's stable ID and owner
index (plus the ownership revision for logs). Class, name, resolve level, XP, and status remain launcher UI data;
the runtime already has the in-game hero objects and must not use display fields as identity.

Ownership assignment alone does not implement player control. The runtime must also route/validate dungeon inputs
against the owner table, and the host remains the arbiter for shared deterministic state and correction. That is a
separate runtime behavior, not more launcher metadata.

## Departure interface summary

For each expedition, carry the current departure fields plus the stable party hero IDs and a parallel owner index
for each ID, followed by the ownership revision for diagnostics. Keep owner indices `0=host`, `1=guest`. Both
peers use the exact host snapshot for the whole expedition.
