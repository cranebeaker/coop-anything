# Example: the inspector on a Fallout: New Vegas co-op project

[fnv-coop](https://github.com/cranebeaker/fnv-coop) (commit e85460f) showed these rows for each running copy. [fnv_fields.json](fnv_fields.json) is that table as a field list. Nothing in the inspector depends on it;
it shows how a game's own methods and expressions plug in.

## Endpoints

Each copy served the bridge on one pipe, `pipe://nvcoop-<instance>`:

```
python -I tools/inspect/inspect_live.py --fields tools/inspect/examples/fnv/fnv_fields.json \
    --endpoint host=pipe://nvcoop-0 --endpoint client=pipe://nvcoop-1
```

## Where the rows come from

| Row | Source |
|---|---|
| role, lastSeq, acks, allSynced, digest, mismatches | the plugin's `coop.status` method: `role`, `lastSeq`, `host.peers[{slot, acked, joined}]`, `host.allSynced`, `desyncs.digests{ok, mismatch, repairs}` |
| menu, msg queue | the bot module's `bot.menu` method: `kind`, `msgCount` |
| boxes | `ui.status`, `recentBoxes[{seq, script, text}]` |
| position, cell | `ref.info` for the player reference `FalloutNV.esm:14` (`x`, `y`, `z`, `cell`) |
| weapon drawn, equipped, health | console expressions `player.IsWeaponOut`, `player.GetEquippedObject 5` and `player.GetAV Health` through the bridge's `eval` |

The original also showed a block for a chosen reference (distance, health, who owns it) when you passed `--ref`. In a field list that is a `call` field with `params` set to that reference, for example `{"label": "target owner", "source": "call", "method": "coop.owner", "params": {"ref": "FalloutNV.esm:104C70"}, "path": "owner"}`.
`eval_allow` holds the full first word of each expression (`player.isweaponout`, ...). Every method named in `read_methods` only reads. The `msg queue` row here is the raw `msgCount`; the original subtracted the index of the box on screen.

## Problems it names

An open menu, queued message boxes, nothing equipped, the host reporting that not all clients are synced, a digest mismatch, and copies at different `lastSeq` values.
