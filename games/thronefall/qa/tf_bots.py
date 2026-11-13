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


def panel_set(bot, key, target, tries=12):
    """Press > on a Co-op Settings row until it shows `target`."""
    for _ in range(tries):
        rows = bot.call("coop_panel", action="state")["rows"]
        if rows.get(key, "").split(" (")[0] == target:
            return Result(True, "%s = %s" % (key, target))
        bot.call("coop_panel", action="click", key=key, dir=1)
    return Result(False, "%s never reached %s (now %s)" % (key, target, bot.call("coop_panel", action="state")["rows"].get(key)))


def boss_max_hp(bot):
    """maxHp of every Hp of every active boss, sorted."""
    out = []
    for line in bot.call("boss_hp")["bosses"]:
        if "active=True" in line:
            out.append(float(line.rsplit("maxHp=", 1)[1]))
    return sorted(out)


class CoopSettings(core.Playbook):
    """The map menu Co-op Settings (host changes, client read only), the night vote, and the boss difficulty changed
    between two levels without a new lobby. Level must contain a boss (Frostsee)."""
    name = "coop_settings"
    summary = "Settings menu on the map, night vote, boss difficulty applied from the next level without a new lobby."
    min_players = 2
    max_players = 2

    def __init__(self, level="Frostsee"):
        self.level = level

    def open_menu(self, ctx, bot):
        bot.call("open_pause")
        ctx.wait_until("pause menu shows Co-op Settings",
                       lambda: any(e.startswith("Co-op Settings") for e in bot.call("ui")["elements"]), 8)
        bot.call("ui_apply", name="Co-op Settings")
        ctx.wait_until("Co-op Settings open", lambda: bot.call("coop_panel", action="state")["open"], 5)

    def boss_round(self, ctx, host, client, label):
        r = host.call("force_boss_spawn")
        ctx.log("%s: boss spawn %s" % (label, r))
        ctx.wait_until("boss active on both", lambda: boss_max_hp(host) and boss_max_hp(client), 20)
        ctx.pause(2)
        h, c = boss_max_hp(host), boss_max_hp(client)
        ctx.log("%s: boss maxHp host %s client %s" % (label, h, c))
        ctx.expect(h == c, "boss HP differs: host %s, client %s" % (h, c))
        return h

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot

        with ctx.step("host opens Co-op Settings from the map's pause menu and turns the night vote on", compare=False):
            self.open_menu(ctx, host)
            ctx.must(panel_set(host, "vote", "On"), "night vote on")
            ctx.must(panel_set(host, "boss", "Normal"), "bosses normal")
            ctx.wait_until("client sees the host's values in the lobby",
                           lambda: "vote=On" in (client.call("lobbydata", key="tfmp_coop")["value"] or ""), 10)
            self.open_menu(ctx, client)
            rows = client.call("coop_panel", action="state")["rows"]
            ctx.log("client panel: %s" % rows)
            ctx.expect(rows.get("vote") == "On (read only)", "client shows night vote %r" % rows.get("vote"))
            ctx.expect(all(v.endswith("(read only)") for v in rows.values()), "client can change values: %s" % rows)

        enter_level(ctx, host, client, self.level)

        with ctx.step("host builds the castle"):
            castle = next(s for s in host.slots() if s["name"] == "Castle Center")
            ctx.must(host.build(castle["id"]), "host builds the castle")

        with ctx.step("night vote: the host alone cannot start the night", compare=False):
            host.call("hold_night", seconds=6)
            ctx.must(host.wait_task(20), "host holds the night button")
            ctx.pause(2)
            ctx.expect(host.time_of_day() == "Day" and client.time_of_day() == "Day",
                       "night started with only the host ready: host %s client %s" % (host.time_of_day(), client.time_of_day()))

        with ctx.step("night vote: when the client holds it too, night falls on both"):
            ctx.must(client.call_night(), "client holds the night button")
            ctx.wait_until("night on both", lambda: host.time_of_day() == "Night" and client.time_of_day() == "Night", 15)

        with ctx.step("boss with boss difficulty Normal", compare=False):
            self.normal = self.boss_round(ctx, host, client, "Normal")

        with ctx.step("back to the map; host sets bosses to Tough in the menu (same lobby)", compare=False):
            ctx.must(host.back_to_map(), "host: back to map")
            ctx.wait_until("both in the map", lambda: host.scene() == "_LevelSelect" and client.scene() == "_LevelSelect", 60)
            ctx.pause(5)
            self.open_menu(ctx, host)
            ctx.must(panel_set(host, "boss", "Tough"), "bosses tough")
            ctx.wait_until("client sees boss=Tough", lambda: "boss=Tough" in (client.call("lobbydata", key="tfmp_coop")["value"] or ""), 10)

        enter_level(ctx, host, client, self.level)

        with ctx.step("boss with boss difficulty Tough: twice the HP on both", compare=False):
            tough = self.boss_round(ctx, host, client, "Tough")
            ctx.expect(len(tough) == len(self.normal) and all(abs(t - 2 * n) < 1 for t, n in zip(tough, self.normal)),
                       "Tough boss HP %s is not twice Normal %s" % (tough, self.normal))


def keys_set(bot, row, key):
    """What the player does in "Your keys": click the key button (capture starts), then press `key`."""
    r = bot.call("keys", action="click", row=row)
    if not r.get("capturing"):
        return Result(False, "%s: clicking the key button did not start the capture: %s" % (row, r))
    r = bot.call("keys", action="set", row=row, key=key)
    return Result(not r["capturing"], "%s -> %s, shown as %r" % (row, key, r["rows"].get(row)))


def same_point(a, b, tol=0.05):
    return a is not None and b is not None and all(abs(x - y) <= tol for x, y in zip(a, b))


class MapPing(core.Playbook):
    """Map ping: off by default (nothing is sent), the host turns it on in Co-op Settings, every player rebinds their
    own keys there, and a ping from either player shows on both at the same spot with the pinging player's name."""
    name = "map_ping"
    summary = "Map ping off by default, on via the host's menu, keys rebindable per player, pings shown on both at the same spot."
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level

    def ping(self, ctx, sender, receiver, label, **at):
        before = receiver.call("map_ping")["shown"]
        r = sender.call("map_ping", action="now", **at)
        ctx.expect(r["sent"] is True, "%s: ping not sent: %s" % (label, r))
        ctx.wait_until("%s: the ping arrives" % label, lambda: receiver.call("map_ping")["shown"] > before, 10)
        s, d = sender.call("map_ping"), receiver.call("map_ping")
        ctx.log("%s: sender player at %s, ping at %s; receiver shows %s, marker visible %s" % (
            label, s["player"], s["last"], d["last"], d["markerVisible"]))
        ctx.expect(same_point(s["last"], d["last"]), "%s: ping at %s on the sender, %s on the receiver" % (label, s["last"], d["last"]))
        ctx.expect(d["markerVisible"] and d["marker"] and not d["marker"].startswith("Player "),
                   "%s: no named marker on the receiver: %r" % (label, d["marker"]))
        return s

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot

        with ctx.step("map ping is off by default: pressing the key sends nothing", compare=False):
            h, c = host.call("map_ping"), client.call("map_ping")
            ctx.expect(not h["active"] and not c["active"], "map ping active by default: host %s client %s" % (h["active"], c["active"]))
            r = client.call("map_ping", action="now")
            ctx.pause(2)
            ctx.expect(r["sent"] is False, "client could ping while off: %s" % r)
            ctx.expect(host.call("map_ping")["shown"] == h["shown"], "host showed a ping while off")

        with ctx.step("host turns Map ping on in Co-op Settings; the client sees it read only", compare=False):
            CoopSettings.open_menu(None, ctx, host)
            ctx.must(panel_set(host, "ping", "On"), "map ping on")
            ctx.wait_until("client sees ping=On in the lobby",
                           lambda: "ping=On" in (client.call("lobbydata", key="tfmp_coop")["value"] or ""), 10)
            CoopSettings.open_menu(None, ctx, client)
            rows = client.call("coop_panel", action="state")["rows"]
            ctx.expect(rows.get("ping") == "On (read only)", "client shows map ping %r" % rows.get("ping"))
            ctx.wait_until("map ping active on both", lambda: host.call("map_ping")["active"] and client.call("map_ping")["active"], 10)

        with ctx.step("every player rebinds their own keys (client too, although the host rows are read only)", compare=False):
            ctx.must(keys_set(host, "kping", "Mouse3"), "host: ping key")
            ctx.must(keys_set(host, "kchat", "T"), "host: chat key")
            ctx.must(keys_set(host, "klist", "F2"), "host: player list key")
            ctx.must(keys_set(client, "kping", "F"), "client: ping key")
            r = host.call("keys", action="click", row="kpingat", dir=1)
            ctx.log("host keys: %s" % r)
            ctx.expect((r["ping"], r["chat"], r["list"], r["pingAt"]) == ("Mouse3", "T", "F2", "Player"),
                       "host keys not applied: %s" % r)
            ctx.expect(r["rows"].get("kping") == "Mouse 4", "ping key shown as %r" % r["rows"].get("kping"))
            c = client.call("keys")
            ctx.log("client keys: %s" % c)
            ctx.expect(c["ping"] == "F" and c["chat"] == "Return", "client keys wrong: %s" % c)
            ctx.must(keys_set(client, "kping", "Mouse2"), "client: ping key back to the default")
            host.call("coop_panel", action="close")
            client.call("coop_panel", action="close")

        enter_level(ctx, host, client, self.level)

        with ctx.step("host pings at its own position (Ping at: Player): shown on both at the same spot", compare=False):
            s = self.ping(ctx, host, client, "host")
            ctx.expect(same_point(s["player"], s["last"], 0.5), "ping %s is not at the host %s" % (s["last"], s["player"]))

        with ctx.step("client pings at two cursor points (Ping at: Cursor): on the ground under the cursor, same spot on both",
                      compare=False):
            spots = []
            for x, y in ((0.3, 0.65), (0.7, 0.35)):
                ctx.pause(1)
                s = self.ping(ctx, client, host, "client at screen %.2f,%.2f" % (x, y), x=x, y=y)
                ctx.log("client screen %s, ping drawn at %s of its screen" % (s["screen"], s["lastOnScreen"]))
                ctx.expect(s["lastOnScreen"] and abs(s["lastOnScreen"][0] - x) < 0.03 and abs(s["lastOnScreen"][1] - y) < 0.03,
                           "ping at %s is not under the cursor %s" % (s["lastOnScreen"], (x, y)))
                ctx.expect(abs(s["last"][1] - s["player"][1]) < 8, "ping height %s far from the player %s" % (s["last"], s["player"]))
                spots.append(s["last"])
            ctx.expect(not same_point(spots[0], spots[1], 1.0), "two cursor points gave the same spot %s" % spots)


def keys_step(bot, row, target, tries=12):
    """Press > on a "Your settings" row until it shows `target`."""
    for _ in range(tries):
        if bot.call("keys")["rows"].get(row) == target:
            return Result(True, "%s = %s" % (row, target))
        bot.call("keys", action="click", row=row, dir=1)
    return Result(False, "%s never reached %s (now %s)" % (row, target, bot.call("keys")["rows"].get(row)))


def ids(bot):
    """(own player id, other player id) as this machine sees them."""
    players = bot.call("colors")["players"]
    own = next(p["id"] for p in players if p["local"])
    other = next(p["id"] for p in players if not p["local"])
    return own, other


class CoopExtras(core.Playbook):
    """1.2.7.7: player colours (cape too), friend arrow, the start-screen bar (active settings, ready check), ping kinds
    and the ping wheel, and the match stats after playing the level to its end."""
    name = "coop_extras"
    summary = ("Colours on capes and pings, friend arrow, active settings and ready check on the start screen, ping "
               "kinds, match stats equal on both after the match.")
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level

    def colors_agree(self, ctx, host, client, where):
        h = {p["id"]: p for p in host.call("colors")["players"]}
        c = {p["id"]: p for p in client.call("colors")["players"]}
        ctx.log("%s colours: host sees %s; client sees %s" % (where, list(h.values()), list(c.values())))
        for pid in h:
            ctx.expect(h[pid]["color"] == c[pid]["color"], "%s: player %s has colour %s on the host, %s on the client"
                       % (where, pid, h[pid]["color"], c[pid]["color"]))
            for view, name in ((h, "host"), (c, "client")):
                ctx.expect(view[pid]["cape"] == view[pid]["color"], "%s: player %s cape %s on the %s, colour %s"
                           % (where, pid, view[pid]["cape"], name, view[pid]["color"]))
                ctx.expect(view[pid]["body"] == view[pid]["color"], "%s: player %s body %s on the %s, colour %s"
                           % (where, pid, view[pid]["body"], name, view[pid]["color"]))

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        host_id, client_id = ids(host)

        with ctx.step("defaults: no colours chosen, no arrow, nothing extra", compare=False):
            for b in (host, client):
                players = b.call("colors")["players"]
                ctx.expect(all(p["choice"] == 0 for p in players), "a colour is chosen by default: %s" % players)
                ctx.expect(all(p["body"] == "5879DB" for p in players), "body not the game's blue by default: %s" % players)
                ctx.expect(not b.call("arrows")["on"], "friend arrow on by default")

        with ctx.step("host: ready check, match stats, map ping on, Count & HP Tough; colours Red and Cyan; arrows on",
                      compare=False):
            CoopSettings.open_menu(None, ctx, host)
            for key, value in (("ready", "On"), ("stats", "On"), ("ping", "On"), ("count", "Tough")):
                ctx.must(panel_set(host, key, value), "host: %s %s" % (key, value))
            ctx.must(keys_step(host, "kcolor", "Red"), "host colour Red")
            ctx.must(keys_step(host, "karrow", "On"), "host arrow on")
            hd = host.call("coop_panel", action="state")["details"]
            ctx.log("host explanations: %s" % hd)
            ctx.expect(hd.get("count") == "enemy HP 150%", "Count & HP Tough explained as %r" % hd.get("count"))
            CoopSettings.open_menu(None, ctx, client)
            ctx.wait_until("client shows the host's explanations",
                           lambda: client.call("coop_panel", action="state")["details"].get("count") == "enemy HP 150%", 10)
            ht, ct = host.call("coop_panel", action="state")["total"], client.call("coop_panel", action="state")["total"]
            ctx.log("total line: host %r, client %r" % (ht, ct))
            ctx.expect(ht == ct and ht.startswith("In total: enemy HP 150%."), "total line: host %r client %r" % (ht, ct))
            ctx.must(keys_step(client, "kcolor", "Cyan"), "client colour Cyan")
            ctx.must(keys_step(client, "karrow", "On"), "client arrow on")
            host.call("coop_panel", action="close")
            client.call("coop_panel", action="close")

        with ctx.step("colours reach the other player; capes painted on both machines (map)", compare=False):
            ctx.wait_until("both see both choices", lambda: all(
                {p["id"]: p["choice"] for p in b.call("colors")["players"]} == {host_id: 7, client_id: 2}
                for b in (host, client)), 15)
            ctx.pause(1)
            self.colors_agree(ctx, host, client, "map")

        with ctx.step("friend arrow: client walks far off, the host's arrow points at it; back near, it goes", compare=False):
            hp = host.call("map_ping")["player"]
            # The overworld camera is zoomed out: 60 m is still on screen, 300 m is outside the map (the game puts
            # the player back). 150 m east is off screen.
            client.call("teleport", x=hp[0] + 150, z=hp[2])
            ctx.wait_until("host shows an arrow to the client", lambda: str(client_id) in host.call("arrows")["visible"], 15)
            d = host.call("arrows")["visible"][str(client_id)]
            ctx.log("host arrow to the client (client 150 m east): direction %s" % d)
            ctx.expect(d[0] > 0.5, "arrow does not point east: %s" % d)
            client.call("teleport", x=hp[0] + 2, z=hp[2])
            ctx.wait_until("arrow gone once the client is near", lambda: str(client_id) not in host.call("arrows")["visible"], 15)

        with ctx.step("host opens %s: the bar shows the active settings and the ready check on both" % self.level, compare=False):
            ctx.must(host.open_level(self.level), "host opens the level flag")
            ctx.wait_until("loadout open on both", lambda: host.popup_open() and client.popup_open(), 15)
            ctx.pause(1)
            h, c = host.call("loadout_info"), client.call("loadout_info")
            ctx.log("host bar: %s" % h)
            ctx.log("client bar: %s" % c)
            for name, info in (("host", h), ("client", c)):
                ctx.expect(info["bar"] and info["settings"] and "Count & HP Tough" in info["settings"]
                           and "Ready check On" in info["settings"], "%s bar wrong: %s" % (name, info))
                ctx.expect(info["readyText"] and "waiting for" in info["readyText"], "%s ready line: %s" % (name, info))
            ctx.expect(c["button"] and not h["button"], "Ready button: host %s client %s" % (h["button"], c["button"]))

        with ctx.step("both pick a weapon; the host's Play waits for the client's Ready", compare=False):
            ctx.must(host.first_page("new"), "host: new run, weapon grid")
            ctx.must(host.pick_weapon(), "host picks a weapon")
            ctx.must(client.pick_weapon(), "client picks a weapon")
            ctx.wait_until("host knows both picks",
                           lambda: len([v for v in host.weapon_picks().values() if v != "Invalid"]) == 2, 10)
            host.press_play()
            ctx.pause(4)
            ctx.expect(host.scene() == "_LevelSelect" and client.scene() == "_LevelSelect",
                       "the level started without the client's Ready")
            ctx.expect(host.call("loadout_info")["dialog"], "no 'Waiting for ... to press Ready' dialog on the host")

        with ctx.step("client presses Ready; the host sees it; Play starts the level", compare=False):
            ctx.must(Result(client.call("loadout_info", action="ready").get("ok", True), "client Ready"), "client presses Ready")
            ctx.wait_until("host sees the client ready", lambda: host.call("loadout_info")["ready"].get(str(client_id)), 10)
            ctx.wait_until("the waiting dialog closes by itself", lambda: not host.call("loadout_info")["dialog"], 5)
            ctx.pause(0.5)
            ctx.log("host bar after Ready: %s" % host.call("loadout_info")["readyText"])
            ctx.must(host.press_play(), "host presses Play")
            ctx.wait_until("both in the level", lambda: host.scene() == self.level and client.scene() == self.level, 60)
            ctx.pause(3)

        with ctx.step("capes keep their colours in the level", compare=False):
            self.colors_agree(ctx, host, client, "level")

        with ctx.step("ping wheel: mouse direction picks the kind", compare=False):
            cases = {(0, 100): "Attack", (100, 0): "Build", (0, -100): "Upgrade4", (-100, 0): "Upgrade2",
                     (10, 10): "Look", (70, 70): "Defend", (-70, 70): "Upgrade1"}
            got = {k: host.call("wheel_pick", dx=k[0], dy=k[1])["kind"] for k in cases}
            ctx.log("wheel picks: %s" % got)
            ctx.expect(got == cases, "wheel picks %s, expected %s" % (got, cases))

        with ctx.step("ping kinds arrive with their sign and label, in the sender's colour", compare=False):
            for sender, receiver, kind, sign, label in (
                    (client, host, "Attack", None, "Attack here"),
                    (host, client, "Upgrade4", "4", "Upgrade to 4"),
                    (client, host, "Danger", "!", "Danger!"),
                    (host, client, "Wait", None, "Wait")):
                before = receiver.call("map_ping")["shown"]
                ctx.must(Result(sender.call("map_ping", action="now", x=0.55, y=0.55, kind=kind)["sent"], kind), "ping %s" % kind)
                ctx.wait_until("%s arrives" % kind, lambda: receiver.call("map_ping")["shown"] > before, 10)
                r = receiver.call("map_ping")
                ctx.log("%s ping on the receiver: sign %r, label %r" % (kind, r["sign"], r["marker"]))
                ctx.expect(r["kind"] == kind and (sign is None or r["sign"] == sign) and r["sign"]
                           and r["marker"] and r["marker"].endswith(label), "%s ping wrong on the receiver: %s" % (kind, r))
                ctx.pause(1)

        with ctx.step("day 1: host builds the castle, the client one more building if affordable", compare=False):
            castle = next(s for s in host.slots() if s["name"] == "Castle Center")
            ctx.must(host.build(castle["id"]), "host builds the castle")
            ctx.settle()
            other = [s for s in affordable(host.slots(), host.gold()) if s["name"] != "Castle Center"]
            self.client_built = 0
            if other:
                ctx.must(client.build(other[0]["id"]), "client builds %s" % other[0]["name"])
                self.client_built = 1
                ctx.settle()
            ctx.log("host stats so far: %s" % host.call("match_stats")["current"])

        # Play to the end with only the castle (and its upgrades), so the match ends within a few nights.
        DayNight(self.level, 0, build_filter=lambda s: s["name"] == "Castle Center", enter=False).run(ctx)

        with ctx.step("match stats: the same table on both, matching the host's counts", compare=False):
            ctx.wait_until("stats shown on both", lambda: host.call("match_stats")["panel"] and client.call("match_stats")["panel"], 20)
            h, c = host.call("match_stats"), client.call("match_stats")
            ctx.log("host stats panel %s, host counts %s" % (h["rows"], h["current"]))
            ctx.log("client stats panel %s" % c["rows"])
            ctx.expect(h["rows"] == c["rows"], "stats differ: host %s client %s" % (h["rows"], c["rows"]))
            rows = {int(r.split("|")[0]): [int(x) for x in r.split("|")[1:]] for r in h["rows"]}
            ctx.expect(rows.get(host_id, [0, 0, 0, 0])[2] >= 1, "host built nothing: %s" % rows)
            ctx.expect(rows.get(client_id, [0, 0, 0, 0])[2] == self.client_built, "client built %s, expected %s" % (rows, self.client_built))
            ctx.expect(sum(r[1] for r in rows.values()) > 0, "no player damage counted: %s" % rows)


def wait_dawn(ctx, host, client, timeout_s=420, sampler=None):
    """Play the running night until it is day on both (or the match ends)."""
    end = time.monotonic() + timeout_s
    while time.monotonic() < end:
        if sampler:
            sampler()
        if host.gamestate() in ENDED or client.gamestate() in ENDED:
            return host.gamestate()
        if host.time_of_day() == "Day" and client.time_of_day() == "Day":
            return "Day"
        time.sleep(2)
    return None


class CoopMore(core.Playbook):
    """1.2.7.9: profiles, extra-enemy gold, separate damage scaling, ping spreading, live kill counter, respawn wait
    growing per death, teammate revive and watching a teammate while down."""
    name = "coop_more"
    summary = ("Profiles save/load; pings replace and spread; kill counter equal on both; extra enemies carry gold; "
               "respawn wait 20 s then 30 s; a teammate next to the fallen player revives them; camera watches while down.")
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        host_id, client_id = ids(host)

        with ctx.step("profiles: save Count & HP Tough in slot 2, change it, load slot 2", compare=False):
            CoopSettings.open_menu(None, ctx, host)
            ctx.must(panel_set(host, "count", "Tough"), "count Tough")
            p = host.call("profile", action="save", slot=2)
            ctx.log("slot 2 after save: %s" % p)
            ctx.must(panel_set(host, "count", "Normal"), "count back to Normal")
            p = host.call("profile", action="load", slot=2)
            ctx.log("slot 2 after load: %s" % p)
            ctx.expect(host.call("coop_panel", action="state")["rows"]["count"] == "Tough", "load did not bring back Tough")
            ctx.expect(p["stored"] and "count=Tough" in p["stored"], "slot 2 does not hold count=Tough: %s" % p)

        with ctx.step("host: more enemies, 100% gold, revive, respawn 20 s +10 s, scaled as 2 with enemy damage Medium",
                      compare=False):
            for key, value in (("count", "MoreEnemies"), ("gold", "100%"), ("revive", "On"), ("respawn", "20 s"),
                               ("rinc", "10 s"), ("scale", "Players2"), ("dstep", "Medium")):
                ctx.must(panel_set(host, key, value, tries=24), "host: %s %s" % (key, value))
            ping = panel_set(host, "ping", "On")
            ctx.must(ping, "map ping on")
            total = host.call("coop_panel", action="state")["total"]
            ctx.log("total line: %s" % total)
            ctx.expect("damage +20% (0% + 20% scaling)" in total and "extra enemies drop 100% gold" in total,
                       "total line: %s" % total)
            ctx.must(keys_step(host, "kkills", "On"), "host kill counter on")
            host.call("coop_panel", action="close")
            CoopSettings.open_menu(None, ctx, client)
            ctx.must(keys_step(client, "kkills", "On"), "client kill counter on")
            ctx.must(keys_step(client, "kwatch", "On"), "client watches when down")
            client.call("coop_panel", action="close")
            ctx.wait_until("client sees the host's rules", lambda: "respawn=20 s" in (client.call("lobbydata", key="tfmp_coop")["value"] or ""), 10)

        enter_level(ctx, host, client, self.level)

        with ctx.step("pings: same spot replaces, nearby plates move apart", compare=False):
            client.call("map_ping", action="now", x=0.5, y=0.5)
            ctx.pause(0.5)
            client.call("map_ping", action="now", x=0.5, y=0.5, kind="Attack")
            ctx.pause(0.8)
            one = host.call("hud")
            ctx.log("after two pings on one spot: host shows %s ping(s)" % one["pings"])
            ctx.expect(one["pings"] == 1, "same-spot ping not replaced: %s markers" % one["pings"])
            client.call("map_ping", action="now", x=0.58, y=0.5, kind="Defend")
            ctx.pause(0.8)
            two = host.call("hud")
            ctx.log("after a ping next to it: %s markers, %s plate(s) moved" % (two["pings"], two["platesMoved"]))
            ctx.expect(two["pings"] == 2 and two["platesMoved"] >= 1, "plates not spread: %s" % two)

        with ctx.step("day 1: host builds the castle", compare=False):
            castle = next(s for s in host.slots() if s["name"] == "Castle Center")
            ctx.must(host.build(castle["id"]), "host builds the castle")
            ctx.settle()

        with ctx.step("night 1 starts", compare=False):
            ctx.must(host.call_night(), "host calls the night")
            ctx.wait_until("night on both", lambda: host.time_of_day() == "Night" and client.time_of_day() == "Night", 15)

        def down_on_both():
            return (host.call("revive_state")["players"][str(client_id)]["down"]
                    and client.call("revive_state")["players"][str(client_id)]["down"])

        def up_on_both():
            return (not host.call("revive_state")["players"][str(client_id)]["down"]
                    and not client.call("revive_state")["players"][str(client_id)]["down"])

        for death, expect_wait in ((1, 20.0), (2, 30.0)):
            with ctx.step("death %d: client goes down; respawn wait %.0f s on both; camera watches the host" % (death, expect_wait),
                          compare=False):
                host.call("teleport", x=host.call("revive_state")["players"][str(host_id)]["pos"][0] + 20,
                          z=host.call("revive_state")["players"][str(host_id)]["pos"][2])
                ctx.pause(1)
                ctx.log(host.call("knock_out", id=client_id))
                ctx.wait_until("client down on both", down_on_both, 10)
                ctx.pause(1)
                h = host.call("revive_state")
                c = client.call("revive_state")
                ctx.log("host view %s; client view %s, watching %s" % (h["players"][str(client_id)], c["players"][str(client_id)], c["watching"]))
                for name, view in (("host", h), ("client", c)):
                    st = view["players"][str(client_id)]
                    ctx.expect(st["deaths"] == death, "%s counts %s deaths, expected %d" % (name, st["deaths"], death))
                    ctx.expect(abs(st["wait"] - expect_wait) < 0.1, "%s respawn wait %s, expected %s" % (name, st["wait"], expect_wait))
                ctx.expect(c["watching"], "client camera is not watching the host")
                ctx.expect(c["watchedId"] == host_id and c["watchText"] and "to switch" in c["watchText"],
                           "client watch hint wrong: %s / %r" % (c["watchedId"], c["watchText"]))
                # With one teammate up, next / previous stay on that teammate.
                client.call("watch_cycle", dir=1)
                ctx.pause(0.3)
                client.call("watch_cycle", dir=-1)
                ctx.pause(0.3)
                ctx.expect(client.call("revive_state")["watchedId"] == host_id, "switching lost the only teammate")

            with ctx.step("death %d: the host stands next to the client; it gets up within the revive time" % death,
                          compare=False):
                pos = host.call("revive_state")["players"][str(client_id)]["pos"]
                before = host.call("revive_state")["revived"]
                host.call("teleport", x=pos[0] + 1.0, z=pos[2])
                seen = []
                t0 = time.monotonic()
                while time.monotonic() - t0 < 10 and not up_on_both():
                    seen.append(round(client.call("revive_state")["players"][str(client_id)]["progress"], 2))
                    time.sleep(0.3)
                took = time.monotonic() - t0
                ctx.log("revived after %.1f s; progress the client saw: %s" % (took, sorted(set(seen))))
                ctx.expect(up_on_both(), "client still down after 10 s next to the host")
                ctx.expect(host.call("revive_state")["revived"] == before + 1, "host did not revive")
                ctx.expect(any(0 < s < 1 for s in seen), "no revive bar progress reached the client")
                ctx.pause(1)
                ctx.expect(not client.call("revive_state")["watching"], "camera still watching after the revive")

        samples = []

        def sample():
            hk, ck = host.call("hud")["kills"], client.call("hud")["kills"]
            samples.append((hk, ck))

        with ctx.step("night 1 to dawn: kill counter equal on both and to the host's counts; extra enemies carry gold",
                      compare=False):
            result = wait_dawn(ctx, host, client, sampler=sample)
            ctx.log("night 1 ended: %s" % result)
            ctx.pause(3)
            h, c, stats = host.call("hud"), client.call("hud"), host.call("match_stats")["current"]
            ctx.log("kill counter host %s client %s, host counts %s; text on client: %r" % (h["kills"], c["kills"], stats, c["text"]))
            ctx.log("extra enemies spawned %s carrying %s coins" % (h["extraSpawned"], h["extraCoins"]))
            ctx.expect(h["kills"] == c["kills"], "kill counter differs: host %s client %s" % (h["kills"], c["kills"]))
            ctx.expect(all(int(stats.get(k, "0|").split("|")[0]) == v for k, v in h["kills"].items()),
                       "kill counter %s does not match the host's counts %s" % (h["kills"], stats))
            ctx.expect(c["text"], "no kill counter text on the client")
            ctx.expect(h["extraSpawned"] > 0 and h["extraCoins"] > 0, "extra enemies without gold: %s" % h)


class LateJoin(core.Playbook):
    """1.2.7.9: with "Join during a level" the client crashes after dawn, the host builds on, the client restarts and
    joins straight into the running level with the host's current day; the host's own dawn save stays as it was."""
    name = "late_join"
    summary = "Client crash after dawn, host builds, client rejoins into the running level with the current day; host save unchanged."
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        cm = ctx.clients[0]

        with ctx.step("host turns on Join during a level", compare=False):
            CoopSettings.open_menu(None, ctx, host)
            ctx.must(panel_set(host, "join", "On"), "join during a level on")
            host.call("coop_panel", action="close")

        DayNight(self.level, 1).run(ctx)     # into the level, day 1, night 1, dawn 1 (the host saves at dawn)
        dawn_hash = host.call("save_hash", level=self.level)
        ctx.log("host dawn save: %s" % dawn_hash)

        with ctx.step("client crashes after dawn; host builds alone", compare=False):
            ctx.log(client_restart("-Kill"))
            ctx.absent.add(cm.name)
            ctx.wait_until("host sees 1 player", lambda: host.players() == 1, 60)
            options = affordable(host.slots(), host.gold())
            if options:
                ctx.must(host.build(options[0]["id"]), "host builds %s" % options[0]["name"])
            else:
                ctx.log("nothing affordable; skipped")
            self.after = host.shared()
            ctx.log("host now: gold %s, wave %s" % (self.after["gold"], self.after["wave"]))

        with ctx.step("client restarts, joins the lobby and is brought into the running level", settle=False, compare=False):
            ctx.log(client_restart("-Start", "-Unlock"))
            ctx.must(client.reconnect(), "client control port")
            ctx.wait_until("both see 2 players", lambda: client.players() >= 2 and host.players() >= 2, 90)
            ctx.wait_until("client in %s" % self.level, lambda: client.scene() == self.level, 120)
            ctx.absent.discard(cm.name)
            ctx.log("host admitted %s late joiner(s)" % host.call("late_join")["admitted"])
            ctx.pause(6)
            ctx.log("client applied %s replayed build(s)" % client.call("late_join")["replayed"])

        with ctx.step("client has the host's current day (buildings, gold, wave)"):
            ctx.pause(3)

        with ctx.step("the host's own dawn save file is unchanged", compare=False):
            now = host.call("save_hash", level=self.level)
            ctx.expect(now["hash"] == dawn_hash["hash"], "host save changed: %s -> %s" % (dawn_hash, now))

        DayNight(self.level, 1, enter=False).run(ctx)   # play on together: day 2, night 2, dawn 2


def palette_hex(name):
    rgb = {"Red": (1.00, 0.25, 0.25), "Green": (0.45, 1.00, 0.40), "Cyan": (0.30, 0.85, 1.00)}[name]
    return "".join("%02X" % int(round(c * 255)) for c in rgb)


def slot(bot, sid):
    return next(s for s in bot.slots() if s["id"] == sid)


class CoopBig(core.Playbook):
    """1.2.8.0: tabs, individual part colours and shared wardrobes, chat colours, team HP, planning, build vote
    (no, then yes), destroyed buildings with gold back, rebuild, night summary, down alerts, endless."""
    name = "coop_big"
    summary = ("Part colours and wardrobes seen by the other player; chat names coloured; plans and a build vote; "
               "destroyed tower: summary, gold back, rebuild; down alert; endless keeps the level going.")
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels"):
        self.level = level

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        host_id, client_id = ids(host)

        with ctx.step("host rules: plan first, vote AnyOne, summary, destroyed stay destroyed 50%, endless", compare=False):
            CoopSettings.open_menu(None, ctx, host)
            for key, value in (("plan", "On"), ("vote2", "AnyOne"), ("summary", "On"), ("keep", "On"),
                               ("refund", "50%"), ("endless", "On")):
                ctx.must(panel_set(host, key, value, tries=24), "host: %s %s" % (key, value))
            for key, value in (("kwardrobe", "On"), ("kteamhp", "On"), ("kdown", "On")):
                ctx.must(keys_step(host, key, value), "host: %s %s" % (key, value))
            ctx.must(keys_step(host, "gsprintmode", "Toggle"), "host: sprint toggle")
            ctx.must(keys_set(host, "ginteract", "F"), "host: extra interact key F")
            ctx.log("host sprint: %s" % host.call("sprint"))
            host.call("coop_panel", action="close")

        with ctx.step("client: individual colours (clothes red, horse green), wardrobes on; host changes its wardrobe",
                      compare=False):
            CoopSettings.open_menu(None, ctx, client)
            ctx.must(keys_step(client, "kmode", "Individual"), "client: individual colours")
            ctx.must(keys_step(client, "kpart0", "Red"), "client: clothes red")
            ctx.must(keys_step(client, "kpart5", "Green"), "client: horse green")
            ctx.must(keys_step(client, "kwardrobe", "On"), "client: show wardrobes")
            client.call("coop_panel", action="close")
            dressed = host.call("set_wardrobe", crown=1, capeMat=2)
            ctx.log("host wardrobe now %s" % dressed)

        with ctx.step("each sees the other's colours and wardrobe", compare=False):
            ctx.wait_until("host sees the client's red clothes and green horse", lambda: (
                "body=%s" % palette_hex("Red") in host.call("part_colors")["players"][str(client_id)]
                and "horse=%s" % palette_hex("Green") in host.call("part_colors")["players"][str(client_id)]), 15)
            ctx.wait_until("client sees the host's outfit", lambda: (
                client.call("wardrobe")["wearing"].get(str(host_id)) == host.call("wardrobe")["own"]), 15)
            ctx.log("host sees client %s; client sees host wearing %s (host own %s)" % (
                host.call("part_colors")["players"][str(client_id)], client.call("wardrobe")["wearing"].get(str(host_id)),
                host.call("wardrobe")["own"]))

        enter_level(ctx, host, client, self.level)

        with ctx.step("chat names in the sender's colour; team HP bars", compare=False):
            client.call("chat", say="hello from the client")
            ctx.wait_until("host got the message", lambda: "hello from the client" in (host.call("chat")["last"] or ""), 10)
            last = host.call("chat")["last"]
            ctx.log("host chat line: %r" % last)
            ctx.expect(last.startswith("<color=#"), "client name not coloured: %r" % last)
            hp = host.call("night_report")["teamHp"]
            ctx.log("host team HP: %s" % hp)
            ctx.expect(hp and str(host_id) in hp and str(client_id) in hp, "team HP bars missing: %r" % hp)

        castle = next(s for s in host.slots() if s["name"] == "Castle Center")
        with ctx.step("plan first: the host's castle becomes a plan, gold stays, both see it", compare=False):
            gold = host.gold()
            host.build(castle["id"])
            ctx.wait_until("castle planned on both", lambda: any(p.split("|")[0] == str(castle["id"]) for p in client.call("plans")["plans"]), 10)
            ctx.pause(1)
            ctx.log("plans on the client: %s; plates %s blueprints %s" % (
                client.call("plans")["plans"], client.call("plans")["plates"], client.call("plans")["blueprints"]))
            ctx.expect(slot(host, castle["id"])["level"] == 0, "castle was built instead of planned")
            ctx.expect(host.gold() == gold, "gold changed by planning: %s -> %s" % (gold, host.gold()))
            ctx.expect(client.call("plans")["plates"] >= 1, "no plan plate on the client")

        with ctx.step("confirm the castle: vote opens on the client, client says yes, castle built", compare=False):
            host.call("plans", action="confirm", near=False)
            ctx.wait_until("vote open on the client", lambda: (client.call("plans")["vote"] or "").split("|")[1:2] == ["Open"], 10)
            ctx.log("client vote: %s" % client.call("plans")["voteText"])
            client.call("plans", action="vote", yes=True)
            ctx.wait_until("castle built", lambda: slot(host, castle["id"])["level"] == 1 and slot(client, castle["id"])["level"] == 1, 15)

        tower_id = None
        with ctx.step("client plans a building; vote NO keeps it planned; vote YES builds it", compare=False):
            options = [s for s in affordable(host.slots(), host.gold()) if s["name"] != "Castle Center"]
            ctx.expect(options, "nothing affordable to plan")
            if options:
                tower_id = options[0]["id"]
                client.build(tower_id)
                ctx.wait_until("planned", lambda: any(p.split("|")[0] == str(tower_id) for p in host.call("plans")["plans"]), 10)
                host.call("plans", action="confirm", near=False)
                ctx.wait_until("vote open", lambda: (client.call("plans")["vote"] or "").split("|")[1:2] == ["Open"], 10)
                client.call("plans", action="vote", yes=False)
                ctx.wait_until("vote failed", lambda: "Failed" in (host.call("plans")["vote"] or ""), 10)
                ctx.pause(1)
                ctx.expect(slot(host, tower_id)["level"] == 0, "built although the vote failed")
                ctx.expect(any(p.split("|")[0] == str(tower_id) for p in host.call("plans")["plans"]), "plan lost after the failed vote")
                host.call("plans", action="confirm", near=False)
                ctx.wait_until("second vote open", lambda: (client.call("plans")["vote"] or "").split("|")[1:2] == ["Open"], 10)
                client.call("plans", action="vote", yes=True)
                ctx.wait_until("built", lambda: slot(host, tower_id)["level"] == 1 and slot(client, tower_id)["level"] == 1, 15)
                ctx.log("built %s" % options[0]["name"])

        with ctx.step("night: the client goes down (alert on the host), the tower is destroyed", compare=False):
            ctx.must(host.call_night(), "host calls the night")
            ctx.wait_until("night on both", lambda: host.time_of_day() == "Night" and client.time_of_day() == "Night", 20)
            alerts = host.call("night_report")["alerts"]
            host.call("knock_out", id=client_id)
            ctx.wait_until("down alert on the host", lambda: host.call("night_report")["alerts"] > alerts, 10)
            ctx.log("host team HP while down: %s" % host.call("night_report")["teamHp"])
            if tower_id is not None:
                ctx.log("destroy: %s" % host.call("destroy_building", id=tower_id))

        with ctx.step("dawn: night summary on both, gold back 50%, the tower listed as a rebuild", compare=False):
            ctx.expect(wait_dawn(ctx, host, client) == "Day", "no dawn")
            ctx.pause(3)
            h, c = host.call("night_report"), client.call("night_report")
            ctx.log("summary on the client: %s / %s" % (c["rows"], c["lostText"]))
            ctx.expect(h["rows"] == c["rows"] and c["rows"], "summary differs or missing: host %s client %s" % (h["rows"], c["rows"]))
            if tower_id is not None:
                ctx.expect(c["lostText"] and "gold back" in c["lostText"], "no lost building / gold back: %r" % c["lostText"])
                ctx.wait_until("rebuild listed on the client", lambda: any(p.split("|")[4] == "rebuild" for p in client.call("plans")["plans"]), 10)
                ctx.log("plans now: %s" % client.call("plans")["plans"])

        if tower_id is not None:
            with ctx.step("rebuild: confirm, client votes yes, the tower stands again", compare=False):
                host.call("plans", action="confirm", near=False)
                ctx.wait_until("vote open", lambda: (client.call("plans")["vote"] or "").split("|")[1:2] == ["Open"], 10)
                client.call("plans", action="vote", yes=True)
                ctx.wait_until("rebuild gone from the list", lambda: not any(p.split("|")[4] == "rebuild" for p in host.call("plans")["plans"]), 15)
                ctx.log("host message: %s" % host.call("plans")["message"])
                ctx.expect("Rebuild" in (host.call("plans")["message"] or ""), "no rebuild message")

        with ctx.step("endless: at the level's last wave the level does not end; the next nights get harder", compare=False):
            e = host.call("endless")
            last = e["original"] - 1
            h = host.call("set_wave", wave=last)
            c = client.call("set_wave", wave=last)
            ctx.log("host at the last wave: %s; client %s" % (h, c))
            ctx.expect(not h["matchOver"] and h["count"] > e["original"], "level would end: %s" % h)
            h2 = host.call("set_wave", wave=e["original"])
            client.call("set_wave", wave=e["original"])
            ctx.log("first extra night: %s" % host.call("endless"))
            ctx.expect(abs(host.call("endless")["bonusNext"] - 0.10) < 1e-3, "no +10% on the first extra night")
            ctx.expect(client.call("endless")["count"] >= h2["count"] - 1, "client wave list shorter than the host's")


class DayNight(core.Playbook):
    name = "day_night"
    summary = ("Open a level from the map, both pick weapons, start; host and client build in turns; nights alternate "
               "between host and client calling them; state equal on both after every step, no game errors.")
    min_players = 2
    max_players = 2

    def __init__(self, level="Nordfels", nights=2, night_timeout_s=420, build_filter=None, night_sampler=None, enter=True):
        self.level, self.nights, self.night_timeout_s = level, nights, night_timeout_s
        self.build_filter = build_filter        # slot -> bool: which buildings the bots may build (None: any)
        self.night_sampler = night_sampler      # (host, client) -> None, called every 2 s during a night
        self.enter = enter                      # False: both are already in the level
        self.ended = None

    def run(self, ctx):
        host, client = ctx.host.bot, ctx.clients[0].bot
        if self.enter:
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
    elif a.playbook == "coop_settings":
        pb = CoopSettings(a.level)
    elif a.playbook == "map_ping":
        pb = MapPing(a.level)
    elif a.playbook == "coop_extras":
        pb = CoopExtras(a.level)
    elif a.playbook == "coop_more":
        pb = CoopMore(a.level)
    elif a.playbook == "late_join":
        pb = LateJoin(a.level)
    elif a.playbook == "coop_big":
        pb = CoopBig(a.level)
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
