"""Prove a test run left the owner's real game data alone.

    py -I isolation_check.py snapshot --out before.json
    py -I isolation_check.py compare  --before before.json --after after.json

Hashes the real save tree, the real PlayerPrefs registry key and the Steam per-game
userdata folders, plus a manifest of the real install. Reads only. Exit code 0 means
identical, 1 means something differed, 2 means the check itself could not run.
"""
import argparse
import hashlib
import json
import os
import sys
import winreg
from pathlib import Path

APP = "1468810"
COMPANY = "guigugame"
PRODUCT = "guigubahuang"
STEAM = Path(r"C:\Program Files (x86)\Steam")
INSTALL = STEAM / "steamapps" / "common" / "TOI"
BIG = 64 * 1024 * 1024


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def tree(root, full=True):
    out = {}
    root = Path(root)
    if not root.exists():
        return out
    for d, _, files in os.walk(root):
        for n in files:
            p = Path(d) / n
            rel = str(p.relative_to(root))
            st = p.stat()
            if full or st.st_size < BIG:
                out[rel] = sha(p)
            else:
                out[rel] = f"size={st.st_size};mtime={st.st_mtime_ns}"
    return out


def prefs():
    """Every value of the real PlayerPrefs key: name, type and a hash of the data."""
    out = {}
    key = "Software\\" + COMPANY + "\\" + PRODUCT
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as k:
        n = winreg.QueryInfoKey(k)[1]
        for i in range(n):
            name, data, typ = winreg.EnumValue(k, i)
            out[name] = [typ, hashlib.sha256(repr(data).encode("utf-8", "replace")).hexdigest()]
    return out


def snapshot():
    low = Path(os.environ["USERPROFILE"]) / "AppData" / "LocalLow" / COMPANY
    userdata = {}
    for u in (STEAM / "userdata").glob("*"):
        p = u / APP
        if p.exists():
            userdata[u.name] = tree(p)
    return {
        "saves": tree(low),
        "playerprefs": prefs(),
        "steam_userdata": userdata,
        "install": tree(INSTALL, full=False),
    }


def diff(a, b):
    return [k for k in sorted(set(a) | set(b)) if a.get(k) != b.get(k)]


def compare(before, after):
    bad = {
        "saves": diff(before["saves"], after["saves"]),
        "playerprefs": diff(before["playerprefs"], after["playerprefs"]),
        "install": diff(before["install"], after["install"]),
    }
    u = []
    for acc in sorted(set(before["steam_userdata"]) | set(after["steam_userdata"])):
        u += [acc + "/" + x for x in diff(before["steam_userdata"].get(acc, {}), after["steam_userdata"].get(acc, {}))]
    bad["steam_userdata"] = u
    return bad


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("snapshot")
    s.add_argument("--out", required=True)
    c = sub.add_parser("compare")
    c.add_argument("--before", required=True)
    c.add_argument("--after", required=True)
    a = ap.parse_args()
    try:
        if a.cmd == "snapshot":
            snap = snapshot()
            Path(a.out).write_text(json.dumps(snap), encoding="utf-8")
            print(f"snapshot: saves={len(snap['saves'])} playerprefs={len(snap['playerprefs'])} "
                  f"install={len(snap['install'])} userdata_accounts={len(snap['steam_userdata'])}")
            if not snap["saves"] or not snap["playerprefs"]:
                print("ISOLATION: ERROR empty snapshot")
                return 2
            return 0
        b = json.loads(Path(a.before).read_text(encoding="utf-8"))
        f = json.loads(Path(a.after).read_text(encoding="utf-8"))
        bad = compare(b, f)
        for k, v in bad.items():
            print(f"{k}: compared={len(b.get(k, []))} changed={len(v)}" + (f" e.g. {v[:5]}" if v else ""))
        ok = not any(bad[k] for k in ("saves", "playerprefs", "install"))
        print("ISOLATION: PASS" if ok else "ISOLATION: FAIL")
        return 0 if ok else 1
    except Exception as e:  # a check that cannot run must never read as a pass
        print(f"ISOLATION: ERROR {type(e).__name__}: {e}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
