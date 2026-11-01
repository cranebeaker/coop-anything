"""Two real Tale of Immortal clients on one PC: host, join, shared world, sync, disconnect.

    py -I games/tale-of-immortal/qa/run_coop_test.py --config local/toi-qa.json [--mutant drop_npc_death] [--until STAGE]

Local config (private paths stay in local/):
    {"gamepool": "local/toi.json", "saves": "<backup>/saves/Steam", "host_slot": "...", "client_slot": "...",
     "client_folder": "3_qaclnt", "bot": "local/lab/build/CoopQA.Bot.dll", "runs": "local/lab/runs"}

Use a world that loads with no exception on both sides, because a world that lost a mod stalls saving and loading.
host_slot and client_slot can be the same world. client_folder then renames the client's copy, because the mod reports
a shared world as soon as both folder names match, and the bot renames the client's character before it ships it.

Prints one CHECK line per step and one RESULT line. A run where the owner's real data changed is a FAIL, whatever the
game checks said. With --mutant drop_npc_death the client drops the host's NPC death messages, and the NPC death check
must fail; `--suite` runs the normal run and that negative control and scores both.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
QA = Path(__file__).resolve().parent
PY = sys.executable
HOST, CLIENT = 0, 1
STAGES = ["boot", "session", "world", "merge", "sync", "move", "death", "month", "disconnect"]


class Stop(Exception):
    pass


class Run:
    def __init__(self, cfg, mutant, until, run_id, solo=False):
        self.cfg = cfg
        self.mutant = mutant
        self.solo = solo
        self.until = until
        self.dir = REPO / cfg["runs"] / run_id
        self.dir.mkdir(parents=True, exist_ok=True)
        self.pool = REPO / cfg["gamepool"]
        pool = json.loads(self.pool.read_text(encoding="utf-8"))
        self.inst = Path(pool["instances_root"])
        self.checks = []
        self.n = {HOST: 0, CLIENT: 0}

    # ---- helpers ----
    def log(self, s):
        print(s, flush=True)
        with open(self.dir / "run.log", "a", encoding="utf-8") as f:
            f.write(s + "\n")

    def check(self, name, ok, detail="", soft=False):
        """A soft check still counts as a failure in the result. It only lets later stages run."""
        self.checks.append((name, bool(ok), detail))
        self.log(f"CHECK {name} {'PASS' if ok else 'FAIL'} {detail}"[:240])
        if not ok and not soft:
            raise Stop(name)

    def qa_dir(self, i):
        return self.inst / str(i) / "UserData" / "CoopQA"

    def state(self, i):
        try:
            return json.loads((self.qa_dir(i) / "state.json").read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}

    def wait(self, what, fn, timeout):
        """Poll until fn() is truthy. Returns (True, value) or (False, None)."""
        end = time.time() + timeout
        while time.time() < end:
            for i in (HOST, CLIENT):
                self.crash_check(i)
            v = fn()
            if v:
                return True, v
            time.sleep(2)
        self.log(f"TIMEOUT waiting for {what} ({timeout}s)")
        return False, None

    def crash_check(self, i):
        if getattr(self, "client_gone", False) and i == CLIENT:
            return
        s = self.state(i)
        if s and time.time() - self.parse_utc(s.get("utc", "")) > 90:
            raise Stop(f"instance {i} state is stale, step={s.get('step')} scene={s.get('scene')}")

    @staticmethod
    def parse_utc(u):
        m = re.match(r"(\d+)-(\d+)-(\d+)T(\d+):(\d+):(\d+)", u)
        if not m:
            return 0
        return time.mktime(time.strptime(m.group(0), "%Y-%m-%dT%H:%M:%S")) - time.timezone

    def cmd(self, i, text, timeout=60):
        self.n[i] += 1
        n = self.n[i]
        with open(self.qa_dir(i) / "cmd.txt", "a", encoding="utf-8") as f:
            f.write(f"{n} {text}\n")
        def acked():
            try:
                for line in (self.qa_dir(i) / "ack.txt").read_text(encoding="utf-8").splitlines():
                    if line.startswith(f"{n} "):
                        return line[len(str(n)) + 1:]
            except OSError:
                return None
            return None
        ok, v = self.wait(f"ack of '{text}' on {i}", acked, timeout)
        return v if ok else "no-ack"

    def dump(self, i, name):
        r = self.cmd(i, f"dump {name}", 120)
        out = {}
        try:
            for line in (self.qa_dir(i) / name).read_text(encoding="utf-8").splitlines():
                p = line.split("\t")
                out[p[0]] = (p[1] == "1", int(p[2]), int(p[3]))
        except OSError:
            pass
        shutil.copy2(self.qa_dir(i) / name, self.dir / f"{i}-{name}") if (self.qa_dir(i) / name).exists() else None
        self.log(f"dump {i}: {r} parsed={len(out)}")
        return out

    def gp(self, *args, timeout=420):
        r = subprocess.run([PY, "-I", str(REPO / "tools/gamepool/gamepool.py"), "--config", str(self.pool), *args],
                           cwd=REPO, capture_output=True, text=True, timeout=timeout)
        return r.returncode, r.stdout + r.stderr

    # ---- setup and teardown ----
    def real(self, name):
        out = self.dir / f"real-{name}.json"
        r = subprocess.run([PY, "-I", str(QA / "isolation_check.py"), "snapshot", "--out", str(out)],
                           capture_output=True, text=True, timeout=400)
        self.log(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "snapshot: no output")
        return r.returncode == 0

    def reset_saves(self):
        low = Path.home() / "AppData" / "LocalLow"
        src = Path(self.cfg["saves"]) / "CacheData"
        for i in (HOST, CLIENT):
            company = f"toicoop0{i}"
            root = low / company / "guigubahuang"
            if not re.fullmatch(r"toicoop0\d", company):
                raise Stop("refusing to reset a folder that is not a test company")
            target = root / "Steam"
            if target.exists():
                shutil.rmtree(target)
            shutil.copytree(src, target / "CacheData")
            new = self.cfg.get("client_folder")
            if i == CLIENT and new:      # same world, own folder: the mod calls two equal folder names a shared world
                (target / "CacheData" / self.cfg["client_slot"]).rename(target / "CacheData" / new)

    def install_bot(self):
        for i, role in ((HOST, "host"), (CLIENT, "client")):
            d = self.inst / str(i)
            shutil.copy2(REPO / self.cfg["bot"], d / "Mods" / "CoopQA.Bot.dll")
            q = self.qa_dir(i)
            if q.exists():
                shutil.rmtree(q)
            q.mkdir(parents=True)
            lines = [f"role={'solo' if self.solo else role}", "ip=127.0.0.1",
                     "slot=" + (self.cfg["host_slot"] if i == HOST else self.cfg.get("client_folder") or self.cfg["client_slot"])]
            if self.mutant and i == CLIENT:
                lines.append("mutant=" + self.mutant)
            (q / "config.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def collect(self):
        for i in (HOST, CLIENT):
            d = self.dir / f"inst{i}"
            d.mkdir(exist_ok=True)
            for rel in ("MelonLoader/Latest.log", "Player.log", "UserData/CoopQA/events.log", "UserData/CoopQA/state.json"):
                p = self.inst / str(i) / rel
                if p.exists():
                    shutil.copy2(p, d / p.name)

    # ---- the test ----
    def stage(self, name):
        if self.until and STAGES.index(name) > STAGES.index(self.until):
            raise Stop("__until__")

    def body(self):
        self.stage("boot")
        rc, out = self.gp("up", "--n", "2")
        self.log(out.strip().splitlines()[-1][:200] if out.strip() else "no output")
        self.check("boot_two_clients", rc == 0, f"gamepool up rc={rc}")
        ok, _ = self.wait("bots and patches", lambda: self.state(HOST).get("patched") and self.state(CLIENT).get("patched"), 150)
        self.check("bot_and_mod_loaded_in_both", ok)
        h, c = self.state(HOST), self.state(CLIENT)
        self.check("roles_are_what_we_set", h.get("role") == "host" and c.get("role") == "client", f"{h.get('role')}/{c.get('role')}")

        self.stage("session")
        ok, _ = self.wait("join", lambda: self.state(HOST).get("peers", 0) >= 1 and self.state(CLIENT).get("connected"), 120)
        self.check("host_session_and_client_joined", ok,
                   f"host peers={self.state(HOST).get('peers')} client connected={self.state(CLIENT).get('connected')}")

        self.stage("world")
        ok, _ = self.wait("both in a world", lambda: self.state(HOST).get("player") and self.state(CLIENT).get("player"), 240)
        self.check("both_clients_loaded_a_world", ok, f"host={self.state(HOST).get('player')} client={self.state(CLIENT).get('player')}")

        self.stage("merge")
        ok, _ = self.wait("shared world", lambda: self.state(HOST).get("shared") and self.state(CLIENT).get("shared"), 420)
        h, c = self.state(HOST), self.state(CLIENT)
        self.check("shared_world_active_on_both", ok,
                   f"host step={h.get('step')} shared={h.get('shared')} client step={c.get('step')} shared={c.get('shared')}")
        ok, _ = self.wait("players bound", lambda: self.state(HOST).get("player") and self.state(CLIENT).get("player"), 60)
        h, c = self.state(HOST), self.state(CLIENT)
        self.check("same_shared_folder", h.get("my_folder") and h.get("my_folder") == c.get("my_folder"),
                   f"{h.get('my_folder')} / {c.get('my_folder')}")
        self.check("different_characters_in_shared_world", h.get("player") != c.get("player"), f"{h.get('player')} / {c.get('player')}")

        self.stage("sync")
        time.sleep(15)
        hd, cd = self.dump(HOST, "a.tsv"), self.dump(CLIENT, "a.tsv")
        self.check("world_dumps_not_empty", len(hd) > 50 and len(cd) > 50, f"host={len(hd)} client={len(cd)}")
        h, c = self.state(HOST), self.state(CLIENT)
        self.check("each_side_has_both_players", c["player"] in hd and h["player"] in cd and h["player"] in hd and c["player"] in cd)
        alive_h = {k for k, v in hd.items() if not v[0]}
        alive_c = {k for k, v in cd.items() if not v[0]}
        diff = alive_h ^ alive_c
        self.log(f"alive host={len(alive_h)} client={len(alive_c)} only-one-side={len(diff)}")
        (self.dir / "alive-diff.txt").write_text("\n".join(sorted(diff)), encoding="utf-8")
        self.check("same_living_npcs_after_merge", not diff, f"differ={len(diff)}")
        both = sorted(alive_h & alive_c)
        far = sum(1 for k in both if hd[k][1:] != cd[k][1:])
        self.log(f"npc positions differing={far} of {len(both)}")

        self.stage("move")
        self.cmd(HOST, f"watch {c['player']}")
        self.cmd(CLIENT, f"watch {h['player']}")
        before = (c.get("px"), c.get("py"))
        moved = False
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            self.log("move: " + str(self.cmd(CLIENT, f"move {dx} {dy}")))
            ok, _ = self.wait("client moved", lambda: (self.state(CLIENT).get("px"), self.state(CLIENT).get("py")) != before, 25)
            if ok:
                moved = True
                break
        self.check("client_really_moved", moved, f"from {before} to {(self.state(CLIENT).get('px'), self.state(CLIENT).get('py'))}")
        def seen():
            s, m = self.state(CLIENT), self.state(HOST).get("watch", "")
            return f"{c['player']}=alive,{s.get('px')},{s.get('py')}" in m
        ok, _ = self.wait("host sees the client's new tile", seen, 45)
        self.check("host_sees_client_position", ok, self.state(HOST).get("watch", "")[:120])

        self.stage("death")
        players = {h["player"], c["player"]}
        victim = next(k for k in both[len(both) // 2:] if k not in players)
        self.cmd(CLIENT, f"watch {victim}")
        self.cmd(HOST, f"watch {victim}")
        ok, _ = self.wait("victim alive on client", lambda: f"{victim}=alive" in self.state(CLIENT).get("watch", ""), 20)
        self.check("victim_alive_on_client_first", ok, victim)
        r = self.cmd(HOST, f"kill {victim}")
        self.log("kill: " + str(r))
        ok, _ = self.wait("victim dead on host", lambda: re.search(re.escape(victim) + "=(dead|gone)", self.state(HOST).get("watch", "")), 30)
        self.check("victim_died_on_host", ok, str(r))
        ok, _ = self.wait("victim dead on client", lambda: re.search(re.escape(victim) + "=(dead|gone)", self.state(CLIENT).get("watch", "")), 60)
        c2 = self.state(CLIENT)
        self.check("npc_death_reaches_client", ok, f"client watch={c2.get('watch', '')[:100]} dropped={c2.get('deaths_dropped')}")

        self.stage("month")
        seen_diff = set()
        for rnd in (1, 2):
            m0 = self.state(HOST).get("month")
            self.check(f"same_month_before_{rnd}", m0 == self.state(CLIENT).get("month"), f"{m0}/{self.state(CLIENT).get('month')}")
            self.log("skip host: " + str(self.cmd(HOST, "skipmonth")))
            self.log("skip client: " + str(self.cmd(CLIENT, "skipmonth")))
            def advanced():
                x, y = self.state(HOST), self.state(CLIENT)
                return x.get("month", 0) > m0 and y.get("month", 0) > m0 and not x.get("month_running") and not y.get("month_running")
            ok, _ = self.wait("month advance on both", advanced, 150)
            a, b = self.state(HOST), self.state(CLIENT)
            self.check(f"month_advanced_on_both_{rnd}", ok and a.get("month") == b.get("month"), f"{m0} -> {a.get('month')}/{b.get('month')}")
            diff = None
            for k in range(3):       # poll: the mod applies some results a moment after the month
                time.sleep(15)
                hd2, cd2 = self.dump(HOST, f"m{rnd}_{k}.tsv"), self.dump(CLIENT, f"m{rnd}_{k}.tsv")
                alive_h = {x for x, v in hd2.items() if not v[0]}
                alive_c = {x for x, v in cd2.items() if not v[0]}
                diff = alive_h ^ alive_c
                both2 = alive_h & alive_c
                moved = sum(1 for x in both2 if hd2[x][1:] != cd2[x][1:])
                self.log(f"month {rnd} sample {k}: alive host={len(alive_h)} client={len(alive_c)} only-one-side={len(diff)} positions-differ={moved}/{len(both2)}")
                if not diff:
                    break
            (self.dir / f"alive-diff-month{rnd}.txt").write_text(chr(10).join(sorted(diff)), encoding="utf-8")
            kept = len(diff & seen_diff)
            self.log(f"month {rnd}: {kept} of the {len(seen_diff)} NPCs that differed last month still differ")
            seen_diff = set(diff)
            self.check(f"same_living_npcs_after_month_{rnd}", not diff, f"differ={len(diff)} of {len(alive_h | alive_c)}", soft=True)

        self.stage("disconnect")
        rc, out = self.gp("status")
        rows = json.loads(out[out.index("["):])
        pid = next(r["pid"] for r in rows if int(r.get("instance", -1)) == CLIENT)
        f0 = self.state(HOST).get("frames", 0)
        self.client_gone = True
        k = subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, text=True)
        self.log("client killed: " + k.stdout.strip()[:90])
        ok, _ = self.wait("host sees the peer leave", lambda: self.state(HOST).get("peers", 1) == 0, 90)
        self.check("host_notices_disconnect", ok, f"peers={self.state(HOST).get('peers')}")
        time.sleep(10)
        f1 = self.state(HOST).get("frames", 0)
        self.check("host_survives_disconnect", f1 > f0 and self.state(HOST).get("scene") == "Map", f"frames {f0} -> {f1}")

    def save_control(self):
        """No session at all: how long does a plain game save take on each world?"""
        rc, out = self.gp("up", "--n", "2")
        self.check("boot_two_clients", rc == 0, f"gamepool up rc={rc}")
        ok, _ = self.wait("both in a world", lambda: self.state(HOST).get("step") == "solo" and self.state(CLIENT).get("step") == "solo", 240)
        self.check("both_clients_loaded_a_world", ok, f"{self.state(HOST).get('player')}/{self.state(CLIENT).get('player')}")
        for i in (HOST, CLIENT):
            self.log(f"save {i}: " + str(self.cmd(i, "save")))
            ok, _ = self.wait(f"save on {i}", lambda: self.state(i).get("save_ok") in ("true", "false"), 200)
            s = self.state(i)
            self.log(f"plain save instance {i} slot={s.get('my_folder') or self.cfg['host_slot' if i == HOST else 'client_slot']} ok={s.get('save_ok')} ms={s.get('save_ms')}")
            self.check(f"plain_save_finished_{i}", ok, f"ok={s.get('save_ok')} ms={s.get('save_ms')}")

    def go(self):
        code = 1
        try:
            if not self.real("before"):
                raise Stop("real-folder snapshot failed")
            self.reset_saves()
            self.install_bot()
            self.save_control() if self.solo else self.body()
            self.log("all stages finished")
        except Stop as e:
            if str(e) != "__until__":
                self.log(f"STOPPED at: {e}")
        except Exception as e:  # startup is a gate: a crash in the runner must still print a RESULT
            self.checks.append(("runner", False, repr(e)[:200]))
            self.log(f"RUNNER ERROR {e!r}")
        finally:
            try:
                self.collect()
            except Exception as e:
                self.log(f"collect error {e!r}")
            rc, out = self.gp("down")
            self.log("down: " + (out.strip().splitlines()[-1] if out.strip() else "no output"))
            ok = self.real("after")
            r = subprocess.run([PY, "-I", str(QA / "isolation_check.py"), "compare", "--before",
                                str(self.dir / "real-before.json"), "--after", str(self.dir / "real-after.json")],
                               capture_output=True, text=True)
            self.log(r.stdout.strip())
            self.checks.append(("real_data_unchanged", ok and r.returncode == 0, "isolation_check"))
        passed = sum(1 for c in self.checks if c[1])
        failed = [c[0] for c in self.checks if not c[1]]
        (self.dir / "result.json").write_text(json.dumps({"checks": self.checks, "failed": failed, "mutant": self.mutant}, indent=1), encoding="utf-8")
        verdict = "PASS" if self.checks and not failed else "FAIL"
        self.log(f"RESULT: {verdict}  ({passed}/{len(self.checks)} checks)" + (f" failed={failed}" if failed else ""))
        return 0 if verdict == "PASS" else 1, failed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--mutant", default="")
    ap.add_argument("--until", default="", choices=[""] + STAGES)
    ap.add_argument("--suite", action="store_true")
    ap.add_argument("--save-control", action="store_true")
    ap.add_argument("--id", default=time.strftime("run-%H%M%S"))
    a = ap.parse_args()
    cfg = json.loads((REPO / a.config).read_text(encoding="utf-8"))
    if not a.suite:
        return Run(cfg, a.mutant, a.until, a.id, a.save_control).go()[0]
    rc1, _ = Run(cfg, "", "", a.id + "-normal").go()
    rc2, failed = Run(cfg, "drop_npc_death", "", a.id + "-control").go()
    control_ok = rc2 != 0 and failed == ["npc_death_reaches_client"]
    print(f"NEGATIVE CONTROL: {'OK' if control_ok else 'NOT OK'} (failed={failed})")
    print(f"SUITE: {'PASS' if rc1 == 0 and control_ok else 'FAIL'}")
    return 0 if rc1 == 0 and control_ok else 1


if __name__ == "__main__":
    sys.exit(main())
