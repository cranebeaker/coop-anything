# Sekiro Online co-op test kit

Records both sides of a real Sekiro Online session and says whether a co-op
connection was actually established. No screenshots for a human to squint at:
the agent OCRs the mod's own HUD and the report turns that into a verdict.

## Why it works this way

The mod draws its state on screen:

```
[Sekiro Online v2.05 - By Luke Yui] [Steam: Ready] [Server: Unavailable] [Connection: Offline]
```

`Connection:` is the field that matters. While nobody is hosting or joined it reads
`Offline`; a session changes it. That makes the HUD the cheapest trustworthy oracle we
have, because the mod keeps no session log of its own.

Input injection was tried and dropped: Sekiro reads raw input, so synthetic keyboard
events are ignored, and virtual-pad automation was abandoned by request. The kit therefore
records and asserts; the humans do the two button presses.

## Two PCs (the supported path)

Both players need the mod installed (see `../install.ps1`) and the same game build.

1. **Host PC** and **friend PC**, in PowerShell in this folder:
   ```powershell
   .\run.ps1 -Label host  -Minutes 15
   .\run.ps1 -Label guest -Minutes 15
   ```
   `run.ps1` launches Sekiro through Steam if it is not running, then records.
2. Host: load a save, use the **Cursed Emblem**, invite the friend from the Steam friends list.
3. Friend: accept the invite.
4. Leave both agents running until the session is clearly up (or the timer ends).
5. Copy the friend's `guest.jsonl` to the host PC and run:
   ```powershell
   python report.py host.jsonl guest.jsonl
   ```

Exit codes: `0` both sides reported a non-offline connection, `1` everyone stayed
offline, `2` partial/unreadable (nothing claimed).

### What it asserts

- Both clients loaded the mod (HUD version line was read).
- The HUD was readable often enough to judge (`Connection:` parsed).
- At least one non-`Offline` connection state on **both** sides in the same session.
- It also records `Steam:` and `Server:` states, so a dead matchmaking server shows up in
  the evidence instead of being guessed at.

It does **not** assert enemy/story sync. That needs memory inspection of both clients.

## One PC (not supported for a real join)

A second local client is possible to launch but not to join:
- `sekiro.exe` enforces `Global\SekiroMutex`; a second process exits immediately
  (no error dialog, exit code 51).
- Patching the mutex string in a copy trips Arxan and the copy exits with code 51.
- Loading the old `sekiro_mutex_escape.dll` through me3 (`-e <copy> -n <dll>`) also ends
  in the guest exiting; me3 attaches, then the process disappears.
- A lobby join needs a second Steam identity anyway, and this PC only has one account
  signed in. Steam's multi-client support is unsupported and fragile (unique
  `-master_ipc_name_override` plus a separate Windows user is the only community route).

Use the two-PC path, or sign a second Steam account in under a second Windows user and
launch the guest by hand.

## Files

- `agent.py`: per-PC recorder (Pillow + Windows OCR; `pip install pillow winsdk`).
- `report.py`: merges logs into a verdict.
- `run.ps1`: launch + record + report for one side.

Verified 2026-10-20 on Steam build 5794815: single-client recording read all four HUD
fields in 9/9 samples (`version`, `Steam`, `Server`, `Connection`) and the report
correctly returned `NO CO-OP` for an offline client; the verdict logic was checked
against synthetic pass/partial/none/unreadable logs.
