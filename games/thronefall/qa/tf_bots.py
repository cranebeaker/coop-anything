"""Thronefall co-op playbooks on the hub bot runner (tools/bots).

The game copies must already be running and connected (run-connect.ps1 -Unlock). Instance 0 is the host on port 47801,
instance 1 the client on 47802. The runner settles and compares the `shared` snapshot after every step; the game's
error count is the `apply_failures` counter, which must never rise.

    python -I tf_bots.py --playbook day_night --level Nordfels --nights 2
"""
import argparse
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HUB = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(HUB, "tools", "bots"))
sys.path.insert(0, HERE)

from coopbots import core  # noqa: E402
from coopbots.adapter import ActionAdapter, Result  # noqa: E402
from tfqa import Conn  # noqa: E402

PORTS = [47801, 47802]


class Thronefall(ActionAdapter):
    def __init__(self, port, name):
        self.conn = Conn(port)
        self.name = name

    def call(self, cmd, **args):
        return self.conn.call(cmd, **args)

    # ---- engine surface -------------------------------------------------------------------------------------------
    def status(self):
        s = self.call("status")
        return {"seq": int(s["seq"]), "counters": s["counters"], "scene": s["scene"], "players": s["players"],
                "task": s["task"]}

    def snapshot(self):
        s = self.call("snapshot")
        return {"shared": s["shared"], "per_player": s["per_player"]}

    def capture(self, directory):
        os.makedirs(directory, exist_ok=True)
        info = {"errors": self.call("errors"), "slots": self.call("slots"), "loadout": self.call("loadout"),
                "scenestate": self.call("scenestate"), "status": self.call("status")}
        with open(os.path.join(directory, "%s-game.json" % self.name), "w", encoding="utf-8") as f:
            json.dump(info, f, indent=1)
        return {"file": "%s-game.json" % self.name}

    # ---- actions --------------------------------------------------------------------------------------------------
    def wait_task(self, timeout_s=90):
        end = time.monotonic() + timeout_s
        while True:
            t = self.call("task")
            if t.get("state") != "running":
                return Result(t.get("state") == "done", "%s: %s" % (t.get("task"), t.get("reason")))
            if time.monotonic() > end:
                self.call("stop")
                return Result(False, "%s still running after %ds" % (t.get("task"), timeout_s))
            time.sleep(0.5)

    def open_level(self, scene):
        self.call("open_level", scene=scene)
        return self.wait_task()

    def popup_open(self):
        return self.call("loadout")["popup_open"]

    def first_page(self, choice="new"):
        """The level's first page: "Play" (no saved run), or "New Run" / "Continue Run" when the host has a saved run.
        choice "new" presses Play or New Run, "continue" presses Continue Run. Returns the frame that follows."""
        want = ["Continue Run"] if choice == "continue" else ["Play Button", "New Run"]
        end = time.monotonic() + 8
        while True:
            ui = self.call("ui")
            if ui["active_frame"] != "Level Select Frame":
                return Result(choice == "new", "first page already left: %s" % ui["active_frame"])
            names = [e.split(" [")[0] for e in ui["elements"]]
            hit = next((w for w in want if w in names), None)
            if hit:   # the frame animates in; its buttons appear a moment after it opens
                r = self.call("ui_apply", name=hit)
                return Result(True, "pressed %s, frame now %s" % (hit, r["active_frame"]), {"pressed": hit, "frame": r["active_frame"]})
            if time.monotonic() > end:
                return Result(False, "first page never showed %s: %s" % (" or ".join(want), names))
            time.sleep(0.3)

    def saved_run_offered(self):
        names = [e.split(" [")[0] for e in self.call("ui")["elements"]]
        return "Continue Run" in names

    def pick_weapon(self, name=None):
        args = {"name": name} if name else {}
        r = self.call("pick_weapon", **args)
        return Result(True, "picked %s" % r.get("applied"))

    def weapon_picks(self):
        return self.call("loadout")["weapon_picks"]

    def press_play(self):
        r = self.call("press_play")
        return Result(True, "play applied, frame now %s" % r.get("active_frame"))

    def scene(self):
        return self.call("status")["scene"]

    def slots(self):
        return self.call("slots")["slots"]

    def gold(self):
        return int(self.call("snapshot")["shared"]["gold"])

    def build(self, slot_id):
        before = {s["id"]: s["level"] for s in self.slots()}.get(slot_id)
        self.call("build", id=slot_id)
        r = self.wait_task()
        after = {s["id"]: s["level"] for s in self.slots()}.get(slot_id)
        if r.ok and not (after is not None and before is not None and after > before):
            return Result(False, "task said done but slot %s level %s -> %s" % (slot_id, before, after))
        return r

    def call_night(self):
        self.call("call_night")
        return self.wait_task(30)

    def time_of_day(self):
        return self.call("snapshot")["shared"]["time"]

    def gamestate(self):
        return self.call("snapshot")["shared"]["gamestate"]

    def shared(self):
        return self.call("snapshot")["shared"]

    def enemies(self):
        return self.call("snapshot")["per_player"].get("enemies", 0)

    def building_hp(self):
        return self.call("snapshot")["per_player"]["building_hp"]


    def reconnect(self, timeout_s=120):
        """After the game copy restarted: connect to its control port again."""
        self.conn.close()
        end = time.monotonic() + timeout_s
        while True:
            try:
                self.conn = Conn(self.conn.port)
                self.call("ping")
                return Result(True, "control port back")
            except (OSError, ConnectionError, RuntimeError) as ex:
                if time.monotonic() > end:
                    return Result(False, "control port %d not back after %ds: %s" % (self.conn.port, timeout_s, ex))
                time.sleep(1)

    def lobby(self):
        return self.call("status")["lobby"]

    def players(self):
        return self.call("status")["players"]

    def join_lobby(self, lobby):
        self.call("join_lobby", lobby=lobby)
        return Result(True, "join requested")

    def back_to_map(self):
        self.call("back_to_map")
        return Result(True, "back to map pressed")


def client_restart(*flags):
    import subprocess
    script = os.path.join(HERE, "client-restart.ps1")
    # Output goes to a file, not a pipe: -Start launches a long-lived helper (keep-minimized.ps1) that would inherit the
    # pipe, and subprocess would wait for it as long as the games run.
    log = os.path.join(os.environ.get("TEMP", HERE), "tf-client-restart.log")
    with open(log, "w", encoding="utf-8") as f:
        subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script] + list(flags),
                       stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, timeout=120)
    with open(log, encoding="utf-8", errors="replace") as f:
        return f.read().strip()


def make_adapter(i):
    return Thronefall(PORTS[i], "host" if i == 0 else "client%d" % i)


# ---- playbook -----------------------------------------------------------------------------------------------------
def affordable(slots, gold):
    """Buildable level-0 slots this much gold can pay for, without an upgrade choice dialog: towers first (to survive
    the later nights), then the cheapest (ties: lowest id)."""
    c = [s for s in slots if s["interactable"] and s["level"] == 0 and 0 < s["next_cost"] <= gold and not s.get("choice")]
    return sorted(c, key=lambda s: (0 if "Tower" in s["name"] else 1, s["next_cost"], s["id"]))


ENDED = ("AfterMatchVictory", "AfterMatchDefeat")


def enter_level(ctx, host, client, level):
    """Steps 1-4: both in the map, host opens the level flag, both pick a weapon, host presses Play, both load."""
    with ctx.step("connected in the level select map"):
        ctx.expect(all(m.bot.status()["players"] == 2 for m in ctx.present), "both machines see 2 players")
        for m in ctx.present:
            lv = next((l for l in m.bot.call("levels")["levels"] if l["scene"] == level), None)
            ctx.log("%s: %s %s" % (m.name, level, "unlocked" if lv and lv["playable"] else "LOCKED on this machine"))

    with ctx.step("host opens %s, the loadout opens on both" % level):
        ctx.must(host.open_level(level), "host opens the level flag")
        ctx.wait_until("loadout open on both", lambda: host.popup_open() and client.popup_open(), 15)

    with ctx.step("both pick a weapon"):
        ctx.must(host.first_page("new"), "host starts a new run and goes to the weapon grid")
        ctx.must(host.pick_weapon(), "host picks a weapon")
        ctx.must(client.pick_weapon(), "client picks a weapon")
        ctx.wait_until("host knows both picks",
                       lambda: len([v for v in host.weapon_picks().values() if v != "Invalid"]) == 2, 10)

    with ctx.step("host presses Play, both load %s" % level):
        ctx.must(host.press_play(), "host presses Play")
        ctx.wait_until("both in the level", lambda: host.scene() == level and client.scene() == level, 60)
        ctx.pause(3)


END_FRAMES = ("Level Up Frame", "After Match Frame", "After Match Frame ")


class LockedEndscreen(core.Playbook):
    """Fix 4: the level is locked on the client, the host opens it, the client follows. Fix 5: after the match ends
    the client clicks through its end screens and presses Back first; the host, still on its end screens, is pulled to
    the map and must not keep an end screen open there."""
    name = "locked_endscreen"
    summary = "Client follows the host into a level it has not unlocked; Back from one player's end screen closes the other's."
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        # Only the castle: the bots lose within a few nights, which ends the match.
        DayNight(self.level, 0, build_filter=lambda s: s["name"] == "Castle Center").run(ctx)

        with ctx.step("client clicks through its end screens and presses Back first", compare=False):
            pressed = []
            for _ in range(10):
                if client.scene() == "_LevelSelect":
                    break
                ui = client.call("ui")
                buttons = [e.split(" [")[0] for e in ui["elements"] if e.endswith("[TFUITextButton]")]
                choice = next((b for b in buttons if b in ("Continue Reward Button", "Back Button")), None)
                if choice:
                    client.call("ui_apply", name=choice)
                    pressed.append("%s on %s" % (choice, ui["active_frame"]))
                time.sleep(3)
            ctx.log("client pressed: %s" % pressed)
            ctx.wait_until("both in the map", lambda: host.scene() == "_LevelSelect" and client.scene() == "_LevelSelect", 60)
            ctx.pause(3)

        with ctx.step("no end screen left open on the host"):
            ui = host.call("ui")
            ctx.log("host after being pulled to the map: active %s, open %s" % (ui["active_frame"], ui["open_frames"]))
            left = [f for f in ui["open_frames"] if f in END_FRAMES] + ([ui["active_frame"]] if ui["active_frame"] in END_FRAMES else [])
            ctx.expect(not left, "end screen still open on the host in the map: %s" % left)


class Difficulty(core.Playbook):
    """Co-op difficulty: during a night, both machines must have the same enemies with the same max HP."""
    name = "difficulty"
    summary = "Enemy count and per-enemy max HP equal on host and client during a night; logs the peak count and HP."
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level
        self.peak, self.seen, self.mismatch = 0, {}, []

    def sample(self, host, client):
        h = host.call("snapshot")["per_player"]
        c = client.call("snapshot")["per_player"]
        self.peak = max(self.peak, h.get("enemies", 0))
        hm, cm = h.get("enemy_maxhp", {}), c.get("enemy_maxhp", {})
        self.seen.update(hm)   # enemy id -> "maxHp|speed|elite|damage" on the host
        for k in set(hm) & set(cm):
            if hm[k] != cm[k]:
                self.mismatch.append((k, hm[k], cm[k]))

    def run(self, ctx):
        DayNight(self.level, 1, night_sampler=self.sample).run(ctx)
        with ctx.step("enemies had the same HP, speed, elite flag and damage on both"):
            parts = [v.split("|") for v in self.seen.values()]
            ctx.log("night 1: peak enemy count %d, enemies seen %d, elites %d, max HP %s, speed %s, damage %s" % (
                self.peak, len(parts), sum(1 for p in parts if p[2] == "1"),
                sorted({p[0] for p in parts}), sorted({p[1] for p in parts}), sorted({p[3] for p in parts})))
            ctx.expect(not self.mismatch, "enemy differs (id, host, client): %s" % self.mismatch[:8])


class DayNight(core.Playbook):
    name = "day_night"
    summary = ("Open a level from the map, both pick weapons, start; host and client build in turns; nights alternate "
               "between host and client calling them; state equal on both after every step, no game errors.")
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels", nights=2, night_timeout_s=420, build_filter=None, night_sampler=None):
        self.level, self.nights, self.night_timeout_s = level, nights, night_timeout_s
        self.build_filter = build_filter        # slot -> bool: which buildings the bots may build (None: any)
        self.night_sampler = night_sampler      # (host, client) -> None, called every 2 s during a night
        self.ended = None

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        enter_level(ctx, host, client, self.level)
        builders = [host, client]
        night, ended = 0, None
        while self.nights == 0 or night < self.nights:
            night += 1
            with ctx.step("day %d: host and client spend the gold in turns" % night):
                turn, built = 0, []
                while True:
                    gold = host.gold()
                    options = [s for s in affordable(host.slots(), gold) if self.build_filter is None or self.build_filter(s)]
                    if not options:
                        break
                    who = builders[turn % 2]
                    slot = options[0]
                    r = who.build(slot["id"])
                    ctx.must(r, "%s builds %s (id %s, cost %s)" % (who.name, slot["name"], slot["id"], slot["next_cost"]))
                    built.append((who.name, slot["name"], slot["id"]))
                    ctx.settle()
                    turn += 1
                    if len(built) >= 8:
                        break
                s = host.shared()
                ctx.log("day %d (wave %s of %s, gold left %s) built: %s" % (night, s["wave"], s["wave_count"], s["gold"], built))

            caller = builders[(night - 1) % 2]
            with ctx.step("night %d: %s calls the night, both play it to dawn or to the end" % (night, caller.name)):
                ctx.must(caller.call_night(), "%s calls the night" % caller.name)
                ctx.wait_until("night on both", lambda: host.time_of_day() == "Night" and client.time_of_day() == "Night", 15)
                worst, end = 0, time.monotonic() + self.night_timeout_s
                while time.monotonic() < end:
                    eh, ec = host.enemies(), client.enemies()
                    worst = max(worst, abs(eh - ec))
                    if self.night_sampler:
                        self.night_sampler(host, client)
                    gh, gc = host.gamestate(), client.gamestate()
                    if gh in ENDED or gc in ENDED:
                        ctx.wait_until("match end on both", lambda: host.gamestate() in ENDED and client.gamestate() in ENDED, 30)
                        ended = host.gamestate()
                        break
                    if host.time_of_day() == "Day" and client.time_of_day() == "Day":
                        break
                    time.sleep(2)
                ctx.expect(ended or (host.time_of_day() == "Day" and client.time_of_day() == "Day"),
                           "dawn on both within %ds" % self.night_timeout_s)
                ctx.log("night %d: largest enemy count difference while fighting: %d%s"
                        % (night, worst, ", match ended: %s" % ended if ended else ""))
                ctx.pause(3)

            if ended:
                break
            with ctx.step("dawn %d: building HP equal on both" % night):
                hh, ch = host.building_hp(), client.building_hp()
                bad = {k: (hh[k], ch.get(k)) for k in hh if ch.get(k) is None or abs(hh[k] - ch[k]) > 0.5}
                ctx.expect(not bad, "building HP differs (host, client): %s" % dict(list(bad.items())[:8]))

        self.ended = ended
        if self.nights == 0:
            ctx.expect(ended, "the level ended (victory or defeat)")
            with ctx.step("match end: same result on both (%s)" % ended):
                ctx.expect(client.gamestate() == ended, "client shows %s, host %s" % (client.gamestate(), ended))
                for m in ctx.present:
                    ctx.log("%s end screen: %s" % (m.name, m.bot.call("ui")))


class SaveContinue(core.Playbook):
    """Run after day_night and a full restart of both games: Continue Run must bring both players back to the saved dawn."""
    name = "save_continue"
    summary = "Host continues the saved run after both games restarted; both see the buildings, gold and wave of the saved dawn."
    min_players = 2
    max_players = 2

    def __init__(self, level, expected_shared):
        self.level, self.expected = level, expected_shared

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        with ctx.step("connected in the level select map"):
            ctx.expect(all(m.bot.status()["players"] == 2 for m in ctx.present), "both machines see 2 players")

        with ctx.step("host opens %s and is offered Continue Run" % self.level):
            ctx.must(host.open_level(self.level), "host opens the level flag")
            ctx.wait_until("loadout open on both", lambda: host.popup_open() and client.popup_open(), 15)
            ctx.wait_until("Continue Run offered", host.saved_run_offered, 8)

        with ctx.step("host presses Continue Run, both load the saved run"):
            r = ctx.must(host.first_page("continue"), "host presses Continue Run")
            ctx.log("after Continue Run: %s" % r.reason)
            if host.call("ui")["active_frame"] and "Perk Select" in (host.call("ui")["active_frame"] or ""):
                ctx.must(host.pick_weapon(), "host picks a weapon")
                ctx.must(client.pick_weapon(), "client picks a weapon")
                ctx.must(host.press_play(), "host presses Play")
            ctx.wait_until("both in the level", lambda: host.scene() == self.level and client.scene() == self.level, 60)
            ctx.pause(5)

        with ctx.step("both match the saved dawn"):
            # Gold is not compared with the reference: the game saves at dawn before the morning coins are collected,
            # while the reference snapshot was taken after. Host and client gold must still be equal (runner compare).
            golds = {m.name: m.bot.snapshot()["shared"]["gold"] for m in ctx.present}
            ctx.log("gold after continue: %s (reference snapshot after collecting: %s)" % (golds, self.expected.get("gold")))
            ctx.expect(len(set(golds.values())) == 1, "gold differs between machines: %s" % golds)
            for m in ctx.present:
                got = m.bot.snapshot()["shared"]
                diffs = {k: (self.expected.get(k), got.get(k)) for k in ("scene", "time", "wave")
                         if self.expected.get(k) != got.get(k)}
                eb, gb = self.expected["buildings"], got["buildings"]
                bdiff = {k: (eb.get(k), gb.get(k)) for k in set(eb) | set(gb) if eb.get(k) != gb.get(k)}
                ctx.expect(not diffs and not bdiff, "%s differs from the saved dawn: %s; buildings %s"
                           % (m.name, diffs, dict(list(bdiff.items())[:8])))


class Rejoin(core.Playbook):
    """The client crashes mid-match; the host plays on; the client comes back; the run continues from the last dawn."""
    name = "rejoin"
    summary = ("Client crash mid-match, host plays on alone, client restarts and rejoins (first while the host is still in "
               "the level, then after the host went back to the map), host continues the run: both at the saved dawn.")
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        cm = ctx.clients[0]
        day_night = DayNight(self.level, 1)
        day_night.run(ctx)     # steps 1-7: into the level, day 1, night 1, dawn 1 (the host saves the run at dawn)
        saved = host.shared()
        ctx.log("saved dawn: wave %s, built %s" % (saved["wave"], sorted(k for k, v in saved["buildings"].items() if ":L0:" not in v)))
        lobby = host.lobby()

        with ctx.step("client crashes mid-match; host stays in the level with 1 player", compare=False):
            ctx.log(client_restart("-Kill"))
            ctx.absent.add(cm.name)
            ctx.wait_until("host sees 1 player", lambda: host.players() == 1, 60)
            ctx.expect(host.gamestate() == "InMatch", "host still in the match: %s" % host.gamestate())

        with ctx.step("host builds alone", compare=False):
            options = affordable(host.slots(), host.gold())
            if options:
                ctx.must(host.build(options[0]["id"]), "host builds %s" % options[0]["name"])
            else:
                ctx.log("nothing affordable; skipped")

        with ctx.step("client restarts and tries to rejoin while the host is in the level", settle=False, compare=False):
            ctx.log(client_restart("-Start", "-Unlock"))
            ctx.must(client.reconnect(), "client control port")
            end = time.monotonic() + 45
            while time.monotonic() < end and not (client.players() >= 2 and host.players() >= 2):
                time.sleep(2)
            joined_in_level = client.players() >= 2 and host.players() >= 2
            ctx.log("rejoin while the host is in the level: %s (client step %s, client scene %s)"
                    % ("JOINED" if joined_in_level else "not possible", client.call("status")["step"], client.scene()))

        if not joined_in_level:
            with ctx.step("host goes back to the map; client joins there", compare=False):
                ctx.must(host.back_to_map(), "host: pause menu, back to map")
                ctx.wait_until("host in the map", lambda: host.scene() == "_LevelSelect", 60)
                # Like a player: press join, and press it again if it did not work (right after the host's map loads
                # Steam can still refuse the lobby: "Failed to join lobby with code 3").
                attempts = 0
                while attempts < 5 and not (host.players() == 2 and client.players() == 2):
                    attempts += 1
                    ctx.must(client.join_lobby(lobby), "client joins the lobby (attempt %d)" % attempts)
                    end = time.monotonic() + 15
                    while time.monotonic() < end and not (host.players() == 2 and client.players() == 2):
                        time.sleep(1)
                ctx.log("client joined after %d attempt(s)" % attempts)
                ctx.pause(5)   # a person would not open the level flag in the same second the friend appears
                ctx.expect(host.players() == 2 and client.players() == 2,
                           "both see 2 players after %d join attempts (host %s, client %s)" % (attempts, host.players(), client.players()))
            ctx.absent.discard(cm.name)

            with ctx.step("host continues the run; both are at the saved dawn"):
                ctx.must(host.open_level(self.level), "host opens the level flag")
                ctx.wait_until("loadout open on both", lambda: host.popup_open() and client.popup_open(), 15)
                ctx.wait_until("Continue Run offered", host.saved_run_offered, 8)
                ctx.must(host.first_page("continue"), "host presses Continue Run")
                ctx.wait_until("both in the level", lambda: host.scene() == self.level and client.scene() == self.level, 60)
                ctx.pause(5)
                for m in ctx.present:
                    got = m.bot.shared()
                    eb, gb = saved["buildings"], got["buildings"]
                    bdiff = {k: (eb.get(k), gb.get(k)) for k in set(eb) | set(gb) if eb.get(k) != gb.get(k)}
                    ctx.expect(not bdiff and got["wave"] == saved["wave"],
                               "%s is not at the saved dawn: wave %s (saved %s), buildings %s"
                               % (m.name, got["wave"], saved["wave"], dict(list(bdiff.items())[:8])))
        else:
            ctx.absent.discard(cm.name)
            with ctx.step("client rejoined in the level: state equal"):
                ctx.pause(5)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--playbook", default="day_night")
    ap.add_argument("--level", default="Nordfels")
    ap.add_argument("--nights", type=int, default=2)
    ap.add_argument("--expect", default="", help="save_continue: a host.json snapshot from the saved dawn of an earlier run")
    ap.add_argument("--outdir", default=os.path.join("D:\\coop-test", "thronefall", "runs", time.strftime("bots-%Y%m%d-%H%M%S")))
    a = ap.parse_args(argv)
    machines = [core.Machine(i, "host" if i == 0 else "client%d" % i, make_adapter(i), i) for i in range(2)]
    if a.playbook == "save_continue":
        with open(a.expect, encoding="utf-8") as f:
            snap = json.load(f)
        pb = SaveContinue(a.level, snap.get("shared", snap))
    elif a.playbook == "rejoin":
        pb = Rejoin(a.level)
    elif a.playbook == "locked_endscreen":
        pb = LockedEndscreen(a.level)
    elif a.playbook == "difficulty":
        pb = Difficulty(a.level)
    else:
        pb = DayNight(a.level, a.nights)
    opts = {"settle_s": 30.0, "settle_poll_s": 0.5}
    r = core.run_playbook(pb, machines, core.DictComparer(), a.outdir, {}, opts, print)
    print()
    print("%s  %s  %d steps  %s" % (r.status.upper(), r.playbook, len(r.steps), r.reason))
    for s in r.steps:
        print("  %s %02d %s" % ("ok  " if s.ok else "FAIL", s.number, s.name))
    print("evidence: %s" % a.outdir)
    return 0 if r.status == "pass" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
