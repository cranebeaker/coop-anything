"""Turn recorder logs from one or two PCs into a co-op session verdict.

Usage:
  python report.py host.jsonl guest.jsonl

Exit codes: 0 = session state seen on both sides, 1 = everyone stayed offline,
2 = not enough readable evidence (inconclusive).
"""
import json
import sys
from collections import Counter

KNOWN = {"offline", "online", "hosting", "connected", "joining"}


def load(path):
    rows = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
    return rows


def summarise(path):
    rows = load(path)
    huds = [r["hud"] for r in rows if r.get("hud")]
    labels = {r.get("label", "?") for r in rows}
    if not huds:
        return {"path": path, "labels": labels, "samples": 0, "unreadable": True,
                "note": "no HUD samples (game not running, or the mod was not loaded)"}

    versions = sorted({h.get("version") for h in huds if h.get("version")})
    steam = Counter(h.get("steam") for h in huds if h.get("steam"))
    server = Counter(h.get("server") for h in huds if h.get("server"))
    conn = Counter((h.get("connection") or "").strip().lower() for h in huds if h.get("connection"))
    unreadable = sum(1 for h in huds if not h.get("connection"))
    return {
        "path": path, "labels": labels, "samples": len(huds),
        "versions": versions, "steam": dict(steam), "server": dict(server),
        "connection": dict(conn), "unreadable": unreadable,
        "last_hud": (huds[-1].get("raw") or "")[:160],
    }


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    summaries = [summarise(p) for p in sys.argv[1:]]
    print("Sekiro Online co-op session report")
    print("=" * 62)
    recognized = []
    for s in summaries:
        print(f"\n{s['path']}  (samples: {s.get('samples')})")
        if s.get("note"):
            print(f"  {s['note']}")
            recognized.append(False)
            continue
        print(f"  mod version(s): {', '.join(v or '?' for v in s['versions']) or 'not read'}")
        print(f"  Steam states : {s['steam'] or 'not read'}")
        print(f"  Server states: {s['server'] or 'not read'}")
        print(f"  Connection   : {s['connection'] or 'not read'} "
              f"({s['unreadable']} unreadable samples)")
        print(f"  last HUD     : {s['last_hud']}")
        states = {k for k in s["connection"] if k in KNOWN}
        recognized.append(bool(states))

    print("\n" + "=" * 62)
    if any(s.get("note") for s in summaries):
        print("VERDICT: NO EVIDENCE - one side never produced a HUD sample.")
        return 2
    if not all(recognized):
        print("VERDICT: INCONCLUSIVE - the HUD connection state was not readable often enough.")
        print("         Keep the game in the foreground and re-run; the HUD must be visible.")
        return 2

    left_offline = []
    for s in summaries:
        states = {k for k in s["connection"] if k in KNOWN}
        left_offline.append(bool(states - {"offline"}))
    if all(left_offline):
        print("VERDICT: PASS - both clients reported a non-offline connection state.")
        return 0
    if any(left_offline):
        print("VERDICT: PARTIAL - only one side left 'Connection: Offline'.")
        return 2
    print("VERDICT: NO CO-OP - every readable sample stayed 'Connection: Offline'.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
