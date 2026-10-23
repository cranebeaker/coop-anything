"""Ranching Together (SR2MP) two-copy smoke/sync test on one PC.

usage: python -I run_coop_test.py --host-ip <LAN IPv4> [--keep]
Prints `RESULT: PASS` or `RESULT: FAIL` (exit code 0/1).
Uses tools/gamepool (slots, pid-scoped kills) and the CoopQA.Bot mod inside each copy.
Owner's save folder and install are hashed before and after; any change is a FAIL.
"""
import argparse, hashlib, json, os, shutil, subprocess, sys, time
from pathlib import Path

REPO = Path(__file__).resolve().parents[3] if (Path(__file__).resolve().parents[1].name == "slime-rancher-2") else Path(__file__).resolve().parents[2]
# Private, per-PC values live in local/sr2-qa.json (git-ignored):
# {"steamid": "<steamid64>", "saves_dir": "...", "host_save": "x.sav", "client_save": "y.sav",
#  "owner_paths": ["%USERPROFILE%\AppData\LocalLow\MonomiPark\SlimeRancher2", "<game dir>"]}
LOCAL = json.loads((REPO / "local" / "sr2-qa.json").read_text(encoding="utf-8"))
CFG = REPO / "local" / "sr2.json"
INST = Path(json.loads(CFG.read_text(encoding="utf-8"))["instances_root"])
SAVES = Path(LOCAL["saves_dir"])
STEAMID = LOCAL["steamid"]
OWNER = [Path(os.path.expandvars(p)) for p in LOCAL["owner_paths"]]
PORT = 1919
HOST_SAVE, CLIENT_SAVE = LOCAL["host_save"], LOCAL["client_save"]

checks = []


def check(name, ok, detail=""):
    checks.append((name, bool(ok), detail))
    print(f"[{'PASS' if ok else 'FAIL'}] {name} {detail}", flush=True)
    return ok


def tree_hash(root: Path):
    h = {}
    for p in sorted(root.rglob("*")):
        if p.is_file():
            h[str(p.relative_to(root))] = hashlib.sha256(p.read_bytes()).hexdigest()
    return h


def gp(*a, timeout=900):
    r = subprocess.run([sys.executable, "-I", str(REPO / "tools/gamepool/gamepool.py"), "--config", str(CFG), *a],
                       capture_output=True, text=True, timeout=timeout)
    print(r.stdout.strip(), r.stderr.strip(), flush=True)
    return r.returncode


def prepare(i, role, save, pid):
    d = INST / str(i)
    savedir = d / "LocalLow/MonomiPark/SlimeRancher2/Steam" / STEAMID
    if savedir.exists():
        shutil.rmtree(savedir)          # fresh state every run: only the one test save
    savedir.mkdir(parents=True)
    shutil.copy2(SAVES / save, savedir / save)
    (d / "UserData/CoopQA").mkdir(parents=True, exist_ok=True)
    for f in ("cmd.txt", "state.json"):
        (d / "UserData/CoopQA" / f).unlink(missing_ok=True)
    (d / "UserData/CoopQA.Bot.ini").write_text(
        f"role={role}\nplayer_id={pid}\nsave={(savedir / save).as_posix()}\n", encoding="utf-8")


def state(i):
    try:
        return json.loads((INST / str(i) / "UserData/CoopQA/state.json").read_text(encoding="utf-8"))
    except Exception:
        return {}


_seq = {0: 0, 1: 0}


def cmd(i, text):
    _seq[i] += 1
    with open(INST / str(i) / "UserData/CoopQA/cmd.txt", "a", encoding="utf-8") as f:
        f.write(f"{_seq[i]} {text}\n")
    return _seq[i]


def wait(desc, fn, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        v = fn()
        if v:
            return v
        time.sleep(1)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host-ip", required=True, help="this PC's LAN IPv4 (SR2MP refuses 127.0.0.1)")
    ap.add_argument("--keep", action="store_true", help="leave the copies running")
    a = ap.parse_args()

    before = {str(p): tree_hash(p) for p in OWNER}
    prepare(0, "host", HOST_SAVE, "PLAYER_QAHOST001")
    prepare(1, "client", CLIENT_SAVE, "PLAYER_QACLNT002")
    try:
        if gp("up", "--n", "2") != 0:
            check("both copies started", False); return
        check("both copies started", True)

        h = wait("host in game", lambda: state(0).get("in_game") and state(0), 300)
        c = wait("client in game", lambda: state(1).get("in_game") and state(1), 300)
        check("host loaded its save via CliOptions", h, (h or {}).get("save_name", ""))
        check("client loaded its save via CliOptions", c, (c or {}).get("save_name", ""))
        if not (h and c):
            return
        time.sleep(5)
        h0, c0 = state(0), state(1)
        print(f"before connect: host currency={h0.get('currency')} client currency={c0.get('currency')}")
        check("saves differ before connect (test can see a sync)", h0.get("currency") != c0.get("currency"),
              f"{h0.get('currency')} vs {c0.get('currency')}")

        cmd(0, f"host {PORT}")
        check("server running", wait("server", lambda: state(0).get("server_running"), 30))
        cmd(1, f"connect {a.host_ip} {PORT}")
        check("client connected", wait("connect", lambda: state(1).get("client_connected"), 90))
        check("host sees 1 client", wait("clients", lambda: state(0).get("server_clients") == 1, 60))
        hs = wait("host sees remote", lambda: any(r["id"] == "PLAYER_QACLNT002" for r in state(0).get("remote_players", [])) and state(0), 60)
        cs = wait("client sees remote", lambda: any(r["id"] == "PLAYER_QAHOST001" for r in state(1).get("remote_players", [])) and state(1), 60)
        check("host sees client's player", hs)
        check("client sees host's player", cs)

        # initial world sync: client's money becomes the host's
        hc = state(0).get("currency")
        check("client currency == host currency after join",
              wait("cur", lambda: state(1).get("currency") == state(0).get("currency"), 60),
              f"host={hc} client={state(1).get('currency')}")

        # live sync: host earns 777, client must follow; then client earns 111, host must follow
        base = state(0).get("currency") or 0
        cmd(0, "addcurrency 777")
        check("host +777 reaches client", wait("c+", lambda: state(1).get("currency") == base + 777, 30),
              f"expected {base + 777} got {state(1).get('currency')}")
        cmd(1, "addcurrency 111")
        check("client +111 reaches host", wait("h+", lambda: state(0).get("currency") == base + 888, 30),
              f"expected {base + 888} got {state(0).get('currency')}")

        # position sync: each side's view of the other player is near that player's real position
        time.sleep(3)
        h, c = state(0), state(1)
        def dist(p, q): return sum((x - y) ** 2 for x, y in zip(p, q)) ** 0.5
        rh = next(r for r in c["remote_players"] if r["id"] == "PLAYER_QAHOST001")
        rc = next(r for r in h["remote_players"] if r["id"] == "PLAYER_QACLNT002")
        check("client's view of host position within 2 m", dist(rh["pos"], h["pos"]) < 2.0, f"d={dist(rh['pos'], h['pos']):.2f}")
        check("host's view of client position within 2 m", dist(rc["pos"], c["pos"]) < 2.0, f"d={dist(rc['pos'], c['pos']):.2f}")
        for i in (0, 1):
            s = state(i)
            print(f"copy {i} events:", *s.get("events", [])[-12:], sep="\n   ")
    finally:
        if not a.keep:
            gp("down")
            time.sleep(3)
        after = {str(p): tree_hash(p) for p in OWNER}
        for p in OWNER:
            changed = [k for k in set(before[str(p)]) | set(after[str(p)]) if before[str(p)].get(k) != after[str(p)].get(k)]
            check(f"owner folder unchanged: {p.name}", not changed, ", ".join(changed[:5]))
        ok = all(c[1] for c in checks)
        print(f"\nRESULT: {'PASS' if ok else 'FAIL'}  ({sum(c[1] for c in checks)}/{len(checks)} checks)")
        (REPO / "local/qa/last_result.json").write_text(json.dumps(checks, indent=1))
        sys.exit(0 if ok else 1)


main()
