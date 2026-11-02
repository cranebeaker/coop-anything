"""In-memory stand-in for 1..N game instances sharing one world (tests and dry runs only; never launches a game).

It models just enough for the shipped example scenarios: state keys, kills, locks, item counts, spawns, positions,
stats, loot pickup, followers, an owner of the world's actors, leave and join, and a few named actions. Flags switch on
deliberate bugs that the scenario checks must catch:

    desync_guest  the guest instance reads zeros for everything (the guest never sees the host's changes)
    double_spawn  every spawn creates two
    dup_loot      a picked up world item can be picked up again
    no_handoff    walking away from the actors does not hand them to the guest
    popups_persist  a prepared save raises its popups again (the reload check must catch it)
    name_prefix   save_game only accepts names that start with this text (an engine with a naming rule)

The optional hooks of gameadapter.py are switched on per world, so a plain world behaves like a game without them:

    w.sim = SyncSim(...)             sync_status(): a host and clients that join and catch up as the virtual clock advances
    w.popup_offsets = [2, 3, ...]    popup_count() and dismiss_popup(): popups that appear that many seconds after each load
    w.index = {"id": "kind"}         data_index()
    w.refuse_loads = {"name"}        load_save(name) raises ScenarioError for these names

A FakeGame is a GameAdapter, so the same Runner code that drives a real game drives it.
"""
import threading

import scenlib
from gameadapter import GameAdapter, ScenarioError

ALL_CAPS = ({"op:" + o for o in scenlib.OPS if o != "check"} | {"check:" + k for k in scenlib.CHECK_KINDS}
            | {"action:fast_travel", "action:say"})


class Clock:
    """Virtual time: sleeping just advances a number, so tests finish in milliseconds."""

    def __init__(self):
        self.t = 0.0

    def sleep(self, s):
        self.t += s

    def now(self):
        return self.t


class World:
    def __init__(self, **flags):
        self.flags = flags
        self.nplayers = 2
        self.pickups = {}            # id of a world item -> id of the item type it gives
        self.coords = {}             # id -> (x, y) for distance checks; unknown ids get a stable made-up spot
        self.values = {}             # answers for the free `value` check
        self.loads, self.saves, self.calls = [], {}, []
        self.sim = None                  # SyncSim: answers sync_status() when set (else the fake has no sync readings)
        self.index = None                # {id: kind}: answers data_index() when set
        self.popup_offsets = None        # seconds after a load at which each popup appears; None = popup_count() cannot tell
        self.popup_loaded_at, self.popup_pressed = {}, {}
        self.clean_saves = set()         # saves written after every popup was pressed: loading them raises none
        self.save_files, self.save_log = {}, []   # (instance, name) -> version of the save file; (instance, name) in write order
        self.refuse_loads = set()
        self.lock = threading.RLock()    # parallel steps run in threads; one action at a time keeps the fake deterministic
        self.reset_world()

    def reset_world(self):
        self.state, self.reached = {}, {}
        self.dead, self.disabled, self.locked, self.taken = set(), set(), {}, set()
        self.items, self.stats, self.team, self.spawned = {}, {}, set(), {}
        self.pos, self.area, self.owner, self.left = {}, {}, 0, set()

    def load_scenario(self, sc):
        """Learn which world objects hand out an item when activated (objects with a `base`)."""
        for o in (sc.get("objects") or {}).values():
            if o.get("base"):
                self.pickups[str(o["id"])] = str(o["base"])
        far = (scenlib.playbook_refs(sc).get("far_pos") or {}).get("ref")   # convention: walking the host there hands the actors to the guest
        if far:
            self.flags.setdefault("handoff_place", far)

    def touch_save(self, i, name):
        """Change the save file `name` of instance i, as the game or the owner replacing it would."""
        self.save_files[(i, name)] = self.save_files.get((i, name), 0) + 1

    def where(self, key):
        if key not in self.pos:
            self.pos[key] = self.coords.get(key, (sum(map(ord, key)) * 37 % 9000 + 2000, 0))
        return self.pos[key]


class FakeGame(GameAdapter):
    ROLLING_SAVES = ("autosave", "quicksave")

    def __init__(self, world, i, clock, lacks=()):
        self.w, self.i, self.clock = world, i, clock
        self.instance = i                  # the id a factory would have given this copy; tests change it to model a second pair
        self.OPS = ALL_CAPS - set(lacks)

    # -- helpers
    def _log(self, kind, text):
        self.w.calls.append((self.i, kind, text))

    def _key(self, who):
        return "player%d" % self.i if who == "player" else who

    def _blind(self):
        return bool(self.w.flags.get("desync_guest")) and self.i == 1

    # -- adapter surface
    def do(self, op, args):
        with self.w.lock:
            self._do(op, args)

    def _do(self, op, args):
        w, i = self.w, self.i
        self._log("do", "%s %r" % (op, args))
        if w.sim is not None and op in scenlib.SETTLE_OPS:
            w.sim.on_op(self.clock.now())
        if op == "set_state":
            w.state[args["key"]] = args["value"]
            w.reached.setdefault(args["key"], set()).add(args["value"])
        elif op == "move":
            who, to = self._key(args["who"]), args["to"]
            w.pos[who] = w.where(to)
            w.area[who] = to
            if args["who"] == "player" and i == 0 and w.flags.get("handoff_place") == to and not w.flags.get("no_handoff"):
                w.owner = 1
        elif op == "spawn":
            n = args.get("count", 1) * (2 if w.flags.get("double_spawn") else 1)
            k = (args["at"], args["base"])
            w.spawned[k] = w.spawned.get(k, 0) + n
        elif op in ("give", "take"):
            k = (self._key(args["who"]), args["item"])
            delta = args["count"] if op == "give" else -args["count"]
            w.items[k] = max(0, w.items.get(k, 0) + delta)
        elif op == "kill":
            w.dead.add(args["ref"])
        elif op == "revive":
            w.dead.discard(args["ref"])
        elif op == "enable":
            w.disabled.discard(args["ref"])
        elif op == "disable":
            w.disabled.add(args["ref"])
        elif op == "lock":
            w.locked[args["ref"]] = args["level"]
        elif op == "unlock":
            w.locked.pop(args["ref"], None)
        elif op == "activate":
            ref = args["ref"]
            if ref in w.pickups and (ref not in w.taken or w.flags.get("dup_loot")):
                w.taken.add(ref)
                k = ("player%d" % i, w.pickups[ref])
                w.items[k] = w.items.get(k, 0) + 1
        elif op == "leave":
            w.left.add(i)
        elif op == "join":
            w.left.discard(i)
        elif op == "action":
            name, a = args["name"], args.get("args") or {}
            if name == "fast_travel":
                for p in range(w.nplayers):
                    w.pos["player%d" % p] = w.where(a["marker"])
                    w.area["player%d" % p] = a["marker"]
            elif name == "say":
                w.state[a["quest"]] = a.get("sets", 25)
                w.reached.setdefault(a["quest"], set()).add(a.get("sets", 25))
            else:
                raise ScenarioError("fake game has no action %r" % name)
        else:
            raise ScenarioError("fake game cannot do op %r" % op)

    def read(self, kind, args):
        w = self.w
        self._log("read", kind)
        if self._blind():
            return 0.0
        if kind == "state":
            return float(w.state.get(args["key"], 0))
        if kind == "state_reached":
            return float(args["at"] in w.reached.get(args["key"], set()))
        if kind == "item_count":
            return float(w.items.get((self._key(args["who"]), args["item"]), 0))
        if kind == "dead":
            return float(args["ref"] in w.dead)
        if kind == "disabled":
            return float(args["ref"] in w.disabled)
        if kind == "locked":
            return float(w.locked.get(args["ref"], 0) > 0)
        if kind == "in_area":
            return float(w.area.get(self._key(args["ref"])) == args["place"])
        if kind == "distance":
            a, b = w.where(self._key(args["a"])), w.where(self._key(args["b"]))
            return float(((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5)
        if kind == "stat":
            return float(w.stats.get((args["ref"], args["stat"]), 200.0))
        if kind == "follower":
            return float(args["ref"] in w.team)
        if kind == "count":
            return float(sum(n for (_, b), n in w.spawned.items() if b == args["base"]))
        if kind == "owner":
            return float(w.owner)
        if kind == "value":
            return float(w.values.get(args["query"], 0))
        raise ScenarioError("fake game cannot read %r" % kind)

    def reset_state(self, ids):
        self._log("reset", ",".join(ids))
        for q in ids:
            self.w.state.pop(q, None)
            self.w.reached.pop(q, None)

    def load_save(self, name):
        self._log("load", name)
        if name in self.w.refuse_loads:
            raise ScenarioError("load failed: the game refused save %s" % name)
        self.w.loads.append((self.i, name))
        self.w.reset_world()
        snap = self.w.saves.get(name)
        if snap:
            self.w.state, self.w.reached = dict(snap[0]), {k: set(v) for k, v in snap[1].items()}
        if self.w.popup_offsets is not None:   # a clean save raises no popups; every other save raises all of them again
            self.w.popup_loaded_at[self.i] = self.clock.now()
            self.w.popup_pressed[self.i] = 0
            if name in self.w.clean_saves and not self.w.flags.get("popups_persist"):
                self.w.popup_loaded_at.pop(self.i)
        if self.w.sim is not None:
            self.w.sim.on_load(self.clock.now())

    def save_game(self, name):
        self._log("save", name)
        prefix = self.w.flags.get("name_prefix")
        if prefix and not name.startswith(prefix):
            raise ScenarioError("the engine refused the save name %s" % name)
        self.w.saves[name] = (dict(self.w.state), {k: set(v) for k, v in self.w.reached.items()})
        if self.w.popup_offsets is not None:
            # clean = every popup of this copy has appeared and was pressed (one that is still to come would be missing from the save)
            done = self.w.popup_loaded_at.get(self.i) is None or (
                self.w.popup_pressed.get(self.i, 0) >= len(self.w.popup_offsets) and self._open_popups() == 0)
            (self.w.clean_saves.add if done else self.w.clean_saves.discard)(name)
        self.w.save_log.append((self.i, name))
        self.w.touch_save(self.i, name)

    # -- optional hooks
    def sync_status(self):
        return self.w.sim.status(self.i, self.clock.now()) if self.w.sim is not None else None

    def _open_popups(self):
        """Popups that have appeared since this copy loaded and are not pressed yet (a popup stays up until pressed)."""
        w = self.w
        if self.i not in w.popup_loaded_at:
            return 0
        shown = sum(1 for off in w.popup_offsets if off <= self.clock.now() - w.popup_loaded_at[self.i])
        return max(0, shown - w.popup_pressed.get(self.i, 0))

    def popup_count(self):
        return None if self.w.popup_offsets is None else self._open_popups()

    def dismiss_popup(self):
        if self.w.popup_offsets is None or self._open_popups() == 0:
            return False
        self.w.popup_pressed[self.i] = self.w.popup_pressed.get(self.i, 0) + 1
        return True

    def prepared_names(self, save):
        return ["coop_%s_ready" % save, "autosave_coop_%s_ready" % save]    # the second one is for an engine with a naming rule

    def save_exists(self, name):
        return (self.i, name) in self.w.save_files

    def save_stamp(self, name):
        return self.w.save_files.get((self.i, name))

    def data_index(self):
        return self.w.index


def make(n=2, lacks=(), **flags):
    w = World(**flags)
    clock = Clock()
    return w, clock, [FakeGame(w, i, clock, lacks) for i in range(n)]


class SyncSim:
    """sync_status() of a host (instance 0) and clients (the others), as a function of the time since the last load.

    join_at: s until the clients have joined; catch_up_s: s until they applied the host head; digests start at digest_from and then
    one arrives every `every` s. digests picks the host's judgement of them:
      "ok" every digest ok | "mismatch_same" every digest mismatches on the same keys and the repair never fixes it |
      "mismatch_once" the first digest mismatches (a repair), the rest are ok | "stale" every digest is stale | "silent" none arrives |
      "mismatch_moving" every digest mismatches on other keys (the repairs make progress, so it is not given up on)
    stuck_at: client applied stops at this count (None = catches up). rejected: the clients refuse the join. disconnected: their
    link is down. joined=False: they never join. op_lag_s: after a move, spawn or state change the clients are one step behind the
    host for this many seconds (a very large value: they never catch up).
"""

    def __init__(self, join_at=2.0, catch_up_s=4.0, head=100, digest_from=6.0, every=2.0, digests="ok", stuck_at=None,
                 rejected=False, disconnected=False, joined=True, op_lag_s=0.0):
        self.join_at, self.catch_up_s, self.head, self.digest_from, self.every = join_at, catch_up_s, head, digest_from, every
        self.digests, self.stuck_at, self.rejected, self.disconnected, self.joined = digests, stuck_at, rejected, disconnected, joined
        self.op_lag_s, self.busy_until = op_lag_s, 0.0
        self.t0 = 0.0

    def on_load(self, t):
        self.t0, self.busy_until = t, 0.0

    def on_op(self, t):
        self.busy_until = t + self.op_lag_s

    def _digests(self, t):
        n = max(0, int((t - self.t0 - self.digest_from) / self.every) + 1) if t - self.t0 >= self.digest_from else 0
        d = dict(received=0, ok=0, mismatch=0, rateLimited=0, stale=0, repairs=0)
        entries = []
        if self.digests == "silent":
            return d, entries
        d["received"] = n
        if self.digests == "ok":
            d["ok"] = n
        elif self.digests in ("mismatch_same", "mismatch_moving"):
            d["mismatch"] = d["repairs"] = n
            for k in range(1, n + 1):
                key = "actor:raider1.dead" if self.digests == "mismatch_same" else "actor:raider%d.dead" % k
                entries.append({"id": k, "repaired": False, "key": key, "host": "1", "client": "0"})
        elif self.digests == "mismatch_once":
            d["mismatch"] = d["repairs"] = min(n, 1)
            d["ok"] = max(0, n - 1)
            if n:
                entries.append({"id": 1, "repaired": True, "key": "state:quest1", "host": "25", "client": "0"})
        elif self.digests == "stale":
            d["stale"] = n
        return d, entries

    def status(self, i, t):
        dt = t - self.t0
        if i == 0:
            d, entries = self._digests(t)
            return {"role": "host", "head": self.head, "digests": d, "desyncs": entries}
        applied = -1
        if dt >= self.join_at:
            applied = min(self.head, int((dt - self.join_at) / self.catch_up_s * self.head))
            if self.stuck_at is not None:
                applied = min(applied, self.stuck_at)
            if t < self.busy_until:
                applied = min(applied, self.head - 1)
        return {"role": "client", "connected": not self.disconnected, "joined": self.joined and dt >= self.join_at,
                "syncing": False, "applied": applied, "rejected": "version mismatch" if self.rejected else None}
