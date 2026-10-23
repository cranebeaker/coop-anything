"""A fake 2 to 4 machine co-op session, so playbooks can be tested without a game (and so you can read what a correct
adapter has to do).

FakeCoop owns the truth plus one lagging view per machine. FakeAdapter is one machine's adapter: it implements the whole
ActionAdapter interface on top of those views. The rules are the usual ones for host-authoritative co-op: the host is
the truth; client actions are intents; the first pickup wins; a barrier or ready check holds a one-way step until every
player has passed or agreed; actor ownership has an epoch and hysteresis.

`bugs` is a set of names. Each name switches on one deliberately wrong behaviour (a mutation twin). The playbook named in
registry.TWINS must FAIL when its bug is on. That is how we know a passing playbook can fail at all. See BUGS below.
"""
import copy
import json
import math
import os
import threading
from typing import Any, Dict, List, Optional, Set, Tuple

from .adapter import ActionAdapter, Result, Why, ensure
from .core import DictComparer, Machine

BUGS = {
    "no_stage_sync": "clients never apply quest stages",
    "no_objective_sync": "clients never apply objective flags",
    "dup_pickup": "the player who lost a pickup race keeps the optimistic copy",
    "no_container_sync": "clients never see container content changes",
    "no_container_lock": "two players can have the same container open at once",
    "door_unsynced": "clients never see door open and close",
    "lock_unsynced": "clients never see lock changes",
    "device_unsynced": "clients never see device (terminal) entries used",
    "dup_kill": "a second shooter's hit on a dead actor counts a second death",
    "stale_echo": "a stale health echo revives a dead actor on clients",
    "double_spawn": "clients create a local duplicate of every spawned actor",
    "spectator_stuck": "the spectator view is never released after dialogue",
    "dialogue_not_bridged": "spectators never get the dialogue view",
    "barrier_early": "the barrier releases on the first pass instead of the last",
    "veto_ignored": "a 'not yet' answer does not stop the one-way step",
    "tether_off": "the tether radius is not enforced",
    "consent_ignored": "fast travel proceeds without party consent",
    "travel_in_combat": "fast travel is allowed in combat",
    "owner_split": "ownership transfer is not delivered to clients",
    "flap_owner": "no hysteresis: ownership flips back and forth near the border",
    "counter_noise": "every commit raises the mismatch and repair counters",
    "no_fight_sync": "clients never see actor health or death",
    "no_retransmit": "a dropped reliable message is never retransmitted",
    "repairs_in_flight": "every dropped message is healed by a repair instead of a retransmit",
    "repair_after_quiet": "a late repair fires after the world has been quiet",
    "no_rebase": "clients keep their old world when the host loads an older save",
    "reward_dup_on_rejoin": "a client that rejoins gets every reward in the ledger again",
    "client_frozen": "clients stop applying host updates without disconnecting (sync makes no progress)",
    "timescale_ignored": "a client accepts the game speed but keeps running at normal speed",
    "speed_not_reset": "a client that was sped up ignores the request to go back to normal speed",
}


def norm(ref: Any) -> str:
    return str(ref).strip().lower()


class FakeCoop:
    def __init__(self, n_players: int = 2, bugs: Optional[Set[str]] = None, lag: int = 1,
                 barriers: Optional[Set[Tuple[str, int]]] = None, readychecks: Optional[Set[Tuple[str, int]]] = None,
                 quest_rules: Optional[dict] = None, tether: Optional[dict] = None, loss_every: int = 0,
                 retransmit_ticks: int = 3):
        unknown = set(bugs or ()) - set(BUGS)
        if unknown:
            raise ValueError("unknown fake bugs %s" % sorted(unknown))
        self.n = n_players
        self.bugs = set(bugs or ())
        self.lag = lag
        self.barriers = {(norm(q), n) for q, n in (barriers or ())}
        self.readychecks = {(norm(q), n) for q, n in (readychecks or ())}
        self.quest_rules = {(norm(q), n): list(v) for (q, n), v in (quest_rules or {}).items()}
        self.tether = tether or {}          # {"anchor": ref, "radius": units}
        self.tether_active = False          # switched on by a successful fast travel
        self.loss_every, self.retransmit_ticks = loss_every, retransmit_ticks
        self._lock = threading.RLock()
        self.n_deliv = 0
        self.vetoed: Set[str] = set()
        self.connected = [True] * n_players
        self.saves: Dict[str, Tuple[dict, int]] = {}
        self.delivered: List[Set[Any]] = [set() for _ in range(n_players)]
        self.reward_log: List[dict] = []
        self.rebases = [0] * n_players
        self.last_commit_tick = 0
        self.endgames: Dict[str, dict] = {}     # ref -> {quest, from, to}: activating it opens a confirm box
        self.boxes: Dict[int, str] = {}          # slot -> endgame ref whose confirm box is open
        self.seq = 0
        self.tick_n = 0
        self.truth = {"quests": {}, "objectives": {}, "refs": {}}
        self.views = [copy.deepcopy(self.truth) for _ in range(n_players)]
        self.pending: List[List[Tuple[int, int, dict]]] = [[] for _ in range(n_players)]
        self.applied = [0] * n_players
        self.players = [{"pos": (0.0, 0.0, 0.0), "cell": "start", "inv": {"WEAPON": 1}, "aim": None, "equipped": None,
                         "dialogue": None, "spectating": False, "consent": {}, "menu": None, "box": "", "drawn": True}
                        for _ in range(n_players)]
        # Conditions a test can switch on to make an action fail for a reason the bot must say (not bugs: the world is correct).
        self.walk_limit: Optional[float] = None     # a walk covers at most this many units, then the player is stuck
        self.aim_min_dist: Optional[float] = None   # aiming at something closer than this is refused
        self.max_pitch: Optional[float] = None      # aiming steeper than this many degrees is refused
        self.speed = [1.0] * n_players              # the game speed of each machine (set_timescale)
        self.speed_log: List[List[float]] = [[] for _ in range(n_players)]
        self.deferred: Dict[Tuple[str, int], dict] = {}     # barrier or ready check waiting for players
        self.early: Dict[str, Set[int]] = {}
        self.stats = {"mismatch": [0] * n_players, "repairs": [0] * n_players}
        self.owner_epochs: Dict[str, int] = {}
        self.dup_n = 0
        self.bots: List[FakeAdapter] = [FakeAdapter(self, i) for i in range(n_players)]

    # ---- building the world ----
    @staticmethod
    def _mkref(ref: str, kind: str, pos, cell: str, **kw) -> dict:
        r = {"kind": kind, "pos": list(pos), "cell": cell, "enabled": True, "dead": False, "hp": 100, "deaths": 0,
             "open": False, "locked": False, "items": {}, "in_use": None, "taken": False, "base": kw.pop("base", ref),
             "owner": 0, "epoch": 1, "hostile": kind == "raider", "entries_used": [], "speaking": None}
        r.update(kw)
        return r

    def add_ref(self, ref: str, kind: str, pos=(0.0, 0.0, 0.0), cell: str = "start", **kw) -> str:
        r = self._mkref(ref, kind, pos, cell, **kw)
        k = norm(ref)
        self.truth["refs"][k] = r
        for v in self.views:
            v["refs"][k] = copy.deepcopy(r)
        return k

    def set_player(self, slot: int, pos=None, cell=None, inv=None) -> None:
        p = self.players[slot]
        if pos is not None:
            p["pos"] = tuple(pos)
        if cell is not None:
            p["cell"] = cell
        if inv is not None:
            p["inv"] = dict(inv)

    def add_quest(self, q: str, stages=()) -> None:
        self.truth["quests"][norm(q)] = {"stages": sorted(set(stages)), "running": bool(stages)}
        for v in self.views:
            v["quests"][norm(q)] = copy.deepcopy(self.truth["quests"][norm(q)])

    # ---- replication ----
    def commit(self, eff: dict) -> None:
        self.seq += 1
        self.last_commit_tick = self.tick_n
        eff = dict(eff, seq=self.seq)
        self._apply(self.truth, eff, client=False, who=0)
        self._apply(self.views[0], eff, client=False, who=0)
        self.applied[0] = self.seq
        for i in range(1, self.n):
            self.pending[i].append((self.tick_n + self.lag, self.seq, eff))
        if "counter_noise" in self.bugs:
            for i in range(self.n):
                self.stats["mismatch"][i] += 1
                self.stats["repairs"][i] += 1

    def tick(self) -> None:
        with self._lock:
            self.tick_n += 1
            for i in range(1, self.n):
                if not self.connected[i] or "client_frozen" in self.bugs:
                    continue
                keep: List[Tuple[int, int, dict]] = []
                blocked = False
                for due, seq, eff in self.pending[i]:
                    if blocked or due > self.tick_n:
                        keep.append((due, seq, eff))
                        blocked = blocked or due > self.tick_n
                        continue
                    if self.loss_every and not eff.get("_retx"):
                        self.n_deliv += 1
                        if self.n_deliv % self.loss_every == 0:         # the datagram was lost
                            self.stats["mismatch"][i] += 1               # a digest taken now would mismatch (transient)
                            if "repairs_in_flight" in self.bugs:
                                self.stats["repairs"][i] += 1
                            if "no_retransmit" not in self.bugs:
                                keep.append((self.tick_n + self.retransmit_ticks, seq, dict(eff, _retx=True)))
                            blocked = True                              # reliable channel: later messages wait behind it
                            continue
                    self._apply(self.views[i], eff, client=True, who=i)
                    self.applied[i] = max(self.applied[i], seq)
                self.pending[i] = keep
            if ("repair_after_quiet" in self.bugs and self.tick_n > self.last_commit_tick
                    and (self.tick_n - self.last_commit_tick) % 6 == 0):
                self.stats["repairs"][0] += 1
                self.stats["mismatch"][0] += 1

    def _apply(self, view: dict, eff: dict, client: bool, who: int) -> None:
        op, b = eff["op"], self.bugs
        if op == "stage":
            if client and "no_stage_sync" in b:
                return
            q = view["quests"].setdefault(eff["q"], {"stages": [], "running": True})
            if eff["n"] not in q["stages"]:
                q["stages"] = sorted(q["stages"] + [eff["n"]])
            q["running"] = True
        elif op == "reward":
            if eff["id"] not in self.delivered[who]:
                self.delivered[who].add(eff["id"])
                inv = self.players[who]["inv"]
                inv[eff["item"]] = inv.get(eff["item"], 0) + eff["count"]
            if not client and all(e["id"] != eff["id"] for e in self.reward_log):
                self.reward_log.append(eff)
        elif op == "quest":
            view["quests"].setdefault(eff["q"], {"stages": [], "running": False})["running"] = eff["running"]
        elif op == "obj":
            if client and "no_objective_sync" in b:
                return
            view["objectives"].setdefault(eff["q"], {}).setdefault(str(eff["o"]), {}).update(eff["f"])
        elif op == "ref":
            f, k = dict(eff["f"]), eff["ref"]
            if client:
                if "no_container_sync" in b:
                    f.pop("items", None)
                if "door_unsynced" in b:
                    f.pop("open", None)
                if "lock_unsynced" in b:
                    f.pop("locked", None)
                if "device_unsynced" in b:
                    f.pop("entries_used", None)
                if "owner_split" in b:
                    f.pop("owner", None)
                    f.pop("epoch", None)
                if "no_fight_sync" in b and ("hp" in f or "dead" in f):
                    f.pop("hp", None)
                    f.pop("dead", None)
                if "stale_echo" in b and f.get("dead") is True and k in view["refs"]:
                    f["dead"] = False
                    f["hp"] = 30
            view["refs"].setdefault(k, {}).update(copy.deepcopy(f))
            if client and "double_spawn" in b and eff.get("spawn"):
                self.dup_n += 1
                view["refs"]["%s#dup%d" % (k, self.dup_n)] = copy.deepcopy(view["refs"][k])
        elif op == "dialogue":
            slot = eff["slot"]
            if who != slot and not (client and "dialogue_not_bridged" in b):
                on = eff["on"]
                self.players[who]["spectating"] = True if (client and "spectator_stuck" in b and not on) else on

    # ---- helpers (authority = host) ----
    def t(self, ref: str) -> Optional[dict]:
        return self.truth["refs"].get(norm(ref))

    def set_ref(self, ref: str, **f) -> None:
        k = norm(ref)
        self.truth["refs"][k].update(f)
        self.commit({"op": "ref", "ref": k, "f": f})

    @staticmethod
    def dist(a, b) -> float:
        return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))

    # ---- stage, barrier, ready check ----
    def request_stage(self, q: str, n: int) -> Result:
        key = (norm(q), n)
        if key in self.barriers:
            d = self.deferred.setdefault(key, {"kind": "barrier", "passed": set()})
            if "barrier_early" in self.bugs and d["passed"]:
                self._release(key)
            return Result(True, "deferred by barrier")
        if key in self.readychecks:
            self.deferred.setdefault(key, {"kind": "ready", "yes": set(), "no": set()})
            return Result(True, "deferred by ready check")
        self._do_stage(*key)
        return Result(True)

    def _do_stage(self, q: str, n: int) -> None:
        if n in self.truth["quests"].get(q, {}).get("stages", []):
            return
        self.commit({"op": "stage", "q": q, "n": n})
        for eff in self.quest_rules.get((q, n), []):
            self.commit(dict(eff))

    def _release(self, key) -> None:
        self.deferred.pop(key, None)
        self._do_stage(*key)

    def pass_scene(self, slot: int, q: str) -> Result:
        q = norm(q)
        for key, d in list(self.deferred.items()):
            if key[0] == q and d["kind"] == "barrier":
                d["passed"].add(slot)
                if "barrier_early" in self.bugs or len(d["passed"]) >= self.n:
                    self._release(key)
        self.early.setdefault(q, set()).add(slot)          # an early pass (before the host asked) is remembered
        for key, d in list(self.deferred.items()):
            if key[0] == q and d["kind"] == "barrier" and len(self.early[q] | d["passed"]) >= self.n:
                self._release(key)
        return Result(True)

    def vote(self, slot: int, q: str, yes: bool) -> Result:
        q = norm(q)
        hit = False
        for key, d in list(self.deferred.items()):
            if key[0] == q and d["kind"] == "ready":
                self.vetoed.discard(q)
                hit = True
                (d["yes"] if yes else d["no"]).add(slot)
                ignored = "veto_ignored" in self.bugs
                if d["no"] and not ignored:
                    self.deferred.pop(key)
                    self.vetoed.add(key[0])
                    self.commit({"op": "ref", "ref": "message:veto", "f": {"text": "%s:%s vetoed by %s" % (q, key[1], sorted(d["no"]))}})
                elif len(d["yes"]) >= self.n or (ignored and len(d["yes"] | d["no"]) >= self.n):
                    self._release(key)
        if not hit and q in self.vetoed:
            return Result(True, "late answer to a vetoed check is ignored")
        return Result(hit, "" if hit else "no ready check is pending for that quest")

    # ---- ownership ----
    def rebalance_owners(self) -> None:
        for k, r in self.truth["refs"].items():
            if r["kind"] not in ("raider", "actor") or r["dead"]:
                continue
            cur = r["owner"]
            best = cur
            bd = self.dist(self.players[cur]["pos"], r["pos"]) if self.players[cur]["cell"] == r["cell"] else 1e9
            for s in range(self.n):
                if self.players[s]["cell"] != r["cell"]:
                    continue
                d = self.dist(self.players[s]["pos"], r["pos"])
                margin = 0 if "flap_owner" in self.bugs else 1500
                if d + margin < bd:
                    best, bd = s, d
            if bd >= 1e9:
                continue
            if best != cur:
                r["epoch"] += 1
                self.set_ref(k, owner=best, epoch=r["epoch"])
                self.owner_epochs[k] = r["epoch"]


class FakeAdapter(ActionAdapter):
    """One machine of the fake session. Implements the full ActionAdapter interface."""

    def __init__(self, world: FakeCoop, idx: int):
        self.world = world
        self.idx = idx
        self.name = "host" if idx == 0 else "client%d" % idx
        self.actions: List[Tuple[str, tuple]] = []

    # --- plumbing ---
    @property
    def view(self) -> dict:
        return self.world.views[self.idx]

    @property
    def me(self) -> dict:
        return self.world.players[self.idx]

    def _log(self, name, *a):
        self.actions.append((name, a))

    def _ref(self, ref) -> Optional[dict]:
        return self.view["refs"].get(norm(ref))

    # --- engine surface ---
    def status(self) -> dict:
        w = self.world
        w.tick()
        counters = {"bad_messages": 0, "apply_failures": 0, "repaired_effects": 0}
        if self.idx == 0:
            counters.update(mismatches=sum(w.stats["mismatch"]), repairs=sum(w.stats["repairs"]), rebases=0)
            return {"seq": w.seq, "synced": all(not p for i, p in enumerate(w.pending) if w.connected[i]), "counters": counters,
                    "acks": {w.bots[i].name: w.applied[i] for i in range(1, w.n)}}
        counters.update(mismatches=0, repairs=0, rebases=w.rebases[self.idx])
        return {"seq": w.applied[self.idx], "counters": counters}

    def snapshot(self) -> dict:
        v = copy.deepcopy(self.view)
        for r in v["refs"].values():       # per-player and cosmetic fields are not shared state
            r.pop("in_use", None)
        return {"shared": v, "per_player": {"inv": dict(self.me["inv"])}}

    def capture(self, directory) -> dict:
        os.makedirs(directory, exist_ok=True)
        p = os.path.join(directory, "actions.json")
        with open(p, "w", encoding="utf-8") as f:
            json.dump([[n, [str(x) for x in a]] for n, a in self.actions], f, indent=1)
        return {"actions": p, "count": len(self.actions)}

    # --- say why: what blocks input right now (the base class adds this text to every failed action) ---
    def blockers(self):
        out = []
        if self.me["menu"]:
            out.append(("menu", "the %s menu is open" % self.me["menu"]))
        if self.me["box"]:
            out.append(("message_box", "a message box is up (%s)" % self.me["box"]))
        elif self.idx in self.world.boxes:
            out.append(("message_box", "a confirm box is up"))
        if self.me["equipped"] and not self.me["drawn"]:
            out.append(("holstered", "the weapon %s is holstered" % self.me["equipped"]))
        return out

    # --- movement ---
    def goto_ref(self, ref, dist=150):
        self._log("goto_ref", ref)
        r = self._ref(ref)
        if r is None:
            return Result(False, "%s is not loaded on %s" % (ref, self.name))
        off = dist * (0.5 + 0.1 * self.idx)     # players stop short of the object, each a bit differently
        return self.act(lambda: self._move((r["pos"][0] + off, r["pos"][1], r["pos"][2]), r["cell"]), free=True, ignore=("holstered",),
                        post=[lambda d: self._arrived(r["pos"], r["cell"], dist)])

    def goto_pos(self, x, y, z, cell=None):
        return self.act(lambda: self._move((x, y, z), cell or self.me["cell"]), free=True, ignore=("holstered",),
                        post=[lambda d: self._arrived((x, y, z), cell or self.me["cell"], 50.0)])

    def _arrived(self, target, cell, wanted) -> None:
        """Postcondition of a walk: really within `wanted` of the target (the world may have stopped the player halfway)."""
        left = self.world.dist(self.me["pos"], target)
        if self.me["cell"] == cell and left > wanted * 1.5 + 100:
            raise Why.stuck(left, wanted, "the player stopped %.0f units short" % (left - wanted))

    def _move(self, pos, cell) -> Result:
        w = self.world
        if w.tether and w.tether_active and "tether_off" not in w.bugs:
            a = w.t(w.tether["anchor"])
            if a and w.dist(pos, a["pos"]) > w.tether["radius"]:
                return Result(False, "tethered: %s units around the anchor" % w.tether["radius"])
        if w.walk_limit is not None and cell == self.me["cell"]:      # a walk that cannot cover the whole way
            d = w.dist(self.me["pos"], pos)
            if d > w.walk_limit:
                f = w.walk_limit / d
                pos = tuple(a + (b - a) * f for a, b in zip(self.me["pos"], pos))
        self.me["pos"], self.me["cell"] = tuple(pos), cell
        w.rebalance_owners()
        return Result(True)

    # --- combat ---
    def equip(self, item):
        if self.me["inv"].get(item, 0) <= 0:
            return Result(False, "%s is not in the inventory" % item)
        self.me["equipped"] = item
        self.me["drawn"] = True
        return Result(True)

    def aim_at(self, ref):
        r = self._ref(ref)
        if r is None:
            return Result(False, "%s is not loaded on %s" % (ref, self.name))
        w = self.world
        if r["cell"] != self.me["cell"] or w.dist(r["pos"], self.me["pos"]) > 4000:
            return Result(False, "target is out of range")

        def check_geometry():
            d = w.dist(r["pos"], self.me["pos"])
            if w.aim_min_dist is not None and d < w.aim_min_dist:
                raise Why.too_close(d, w.aim_min_dist)
            if w.max_pitch is not None:
                flat = math.hypot(r["pos"][0] - self.me["pos"][0], r["pos"][1] - self.me["pos"][1])
                pitch = math.degrees(math.atan2(abs(r["pos"][2] - self.me["pos"][2]), max(flat, 1e-6)))
                if pitch > w.max_pitch:
                    raise Why.too_steep(pitch, w.max_pitch, d)

        def do():
            self.me["aim"] = norm(ref)

        def aimed(data):
            ensure(self.me["aim"] == norm(ref), "postcondition", "the aim did not change to %s" % ref)
        return self.act(do, free=True, ignore=("holstered",), pre=[check_geometry], post=[aimed])

    def fire(self, n=1):
        w, k = self.world, self.me["aim"]
        if not k:
            return Result(False, "nothing aimed at")
        if not self.me["equipped"]:
            return Result(False, "no weapon equipped")
        self.require_free()           # a menu, a message box or a holstered weapon: fail with that reason, not with a silent miss
        hits = 0
        for _ in range(n):
            r = w.t(k)
            if r is None or not r["enabled"]:
                return Result(False, "target is gone", {"hits": hits})
            if r["dead"]:
                if "dup_kill" in w.bugs:
                    w.set_ref(k, deaths=r["deaths"] + 1)
                return Result(False, "target is already dead", {"hits": hits})
            hp = r["hp"] - 25
            f: Dict[str, Any] = {"hp": max(hp, 0)}
            if hp <= 0:
                f.update(dead=True, deaths=r["deaths"] + 1, killer=self.idx)
            w.set_ref(k, **f)
            hits += 1
        return Result(True, "", {"hits": hits})

    # --- interaction ---
    def activate(self, ref):
        r = self._ref(ref)
        if r is None:
            return Result(False, "%s is not loaded on %s" % (ref, self.name))
        if r["kind"] == "door":
            return self.close_door(ref) if r["open"] else self.open_door(ref)
        if r["kind"] == "container":
            return self.open_container(ref)
        if norm(ref) in self.world.endgames:
            self.world.boxes[self.idx] = norm(ref)           # the confirm box opens on this player
        return Result(True)

    def open_door(self, ref):
        r = self.world.t(ref)
        if r is None or r["kind"] != "door":
            return Result(False, "not a door")
        if r["locked"]:
            return Result(False, "door is locked")
        if not r["open"]:
            self.world.set_ref(ref, open=True)
        return Result(True)

    def close_door(self, ref):
        r = self.world.t(ref)
        if r is None or r["kind"] != "door":
            return Result(False, "not a door")
        if r["open"]:
            self.world.set_ref(ref, open=False)
        return Result(True)

    def open_container(self, ref):
        r = self.world.t(ref)
        if r is None or r["kind"] != "container":
            return Result(False, "not a container")
        if r["locked"]:
            return Result(False, "container is locked")
        if r["in_use"] not in (None, self.idx) and "no_container_lock" not in self.world.bugs:
            return Result(False, "container is in use by player %s" % r["in_use"])
        r["in_use"] = self.idx
        return Result(True)

    def close_container(self, ref):
        r = self.world.t(ref)
        if r and r["in_use"] == self.idx:
            r["in_use"] = None
        return Result(True)

    def take(self, item, count, from_ref):
        w = self.world
        r = w.t(from_ref)
        if r is None:
            return Result(False, "no such container")
        if r["in_use"] != self.idx:
            return Result(False, "container is not open")
        have = r["items"].get(item, 0)
        if have < count:
            return Result(False, "only %d x %s left" % (have, item))
        items = dict(r["items"])
        items[item] = have - count
        if items[item] == 0:
            del items[item]
        w.set_ref(from_ref, items=items)
        self.me["inv"][item] = self.me["inv"].get(item, 0) + count
        return Result(True)

    def put(self, item, count, to_ref):
        w = self.world
        r = w.t(to_ref)
        if r is None or r["in_use"] != self.idx:
            return Result(False, "container is not open")
        if self.me["inv"].get(item, 0) < count:
            return Result(False, "only %d x %s in the inventory" % (self.me["inv"].get(item, 0), item))
        self.me["inv"][item] -= count
        items = dict(r["items"])
        items[item] = items.get(item, 0) + count
        w.set_ref(to_ref, items=items)
        return Result(True)

    def pickup(self, ref):
        w, k = self.world, norm(ref)
        v = self._ref(ref)
        if v is None or v["taken"]:
            return Result(False, "already taken")
        item = v["base"]
        self.me["inv"][item] = self.me["inv"].get(item, 0) + 1     # optimistic local apply
        r = w.t(k)
        if r["taken"]:
            if "dup_pickup" not in w.bugs:
                self.me["inv"][item] -= 1                           # rollback
            return Result(False, "already taken (lost the race)")
        w.set_ref(k, taken=True, enabled=False, taken_by=self.idx)
        return Result(True)

    def drop(self, item, count=1):
        if self.me["inv"].get(item, 0) < count:
            return Result(False, "only %d x %s" % (self.me["inv"].get(item, 0), item))
        self.me["inv"][item] -= count
        w = self.world
        w.dup_n += 1
        k = "dropped:%d" % w.dup_n
        w.add_ref(k, "item", self.me["pos"], self.me["cell"], base=item)
        w.commit({"op": "ref", "ref": k, "f": copy.deepcopy(w.truth["refs"][k]), "spawn": False})
        return Result(True, "", {"ref": k})

    def talk(self, npc):
        w = self.world
        r = w.t(npc)
        if r is None:
            return Result(False, "no such NPC")
        if self.me["dialogue"]:
            return Result(False, "already in dialogue")
        if r["speaking"] not in (None, self.idx):
            return Result(False, "NPC is talking to another player")
        self.me["dialogue"] = norm(npc)
        w.set_ref(npc, speaking=self.idx)
        w.commit({"op": "dialogue", "slot": self.idx, "on": True})
        return Result(True)

    def pick_topic(self, topic):
        w = self.world
        if not self.me["dialogue"]:
            return Result(False, "not in dialogue")
        k = self.me["dialogue"]
        w.set_ref(k, last_topic=norm(topic))
        eff = w.truth["refs"][k].get("topic_effects", {}).get(norm(topic))
        if isinstance(eff, tuple):
            w.request_stage(*eff)
        return Result(True)

    def end_dialogue(self):
        w, k = self.world, self.me["dialogue"]
        if k:
            self.me["dialogue"] = None
            w.set_ref(k, speaking=None)
            w.commit({"op": "dialogue", "slot": self.idx, "on": False})
        return Result(True)

    def use_device(self, ref, entry):
        w = self.world
        r = w.t(ref)
        if r is None or r["kind"] != "terminal":
            return Result(False, "not a terminal")
        if r["locked"]:
            return Result(False, "terminal is locked")
        w.set_ref(ref, entries_used=list(r["entries_used"]) + [entry])
        if r.get("unlocks") and entry == "unlock":
            w.set_ref(r["unlocks"], locked=False)
        return Result(True)

    def unlock(self, ref, key=None):
        w = self.world
        r = w.t(ref)
        if r is None:
            return Result(False, "no such object")
        need = r.get("key")
        if need and key != need and self.me["inv"].get(need, 0) <= 0:
            return Result(False, "need key %s" % need)
        if r["locked"]:
            w.set_ref(ref, locked=False)
        return Result(True)

    # --- story ---
    def set_stage(self, quest, stage):
        if self.idx != 0:
            return Result(False, "guest scripts do not run: only the host advances stages")
        return self.world.request_stage(quest, stage)

    def start_quest(self, quest):
        if self.idx != 0:
            return Result(False, "only the host starts quests")
        self.world.commit({"op": "quest", "q": norm(quest), "running": True})
        return Result(True)

    def barrier_pass(self, quest):
        return self.world.pass_scene(self.idx, quest)

    def answer_prompt(self, button):
        w = self.world
        k = w.boxes.pop(self.idx, None)
        if k is None:
            return Result(False, "no message box is open")
        if button == 0:
            return Result(True, "", {"declined": True})
        g = w.endgames[k]
        w.deferred[(norm(g["quest"]), g["to"])] = {"kind": "ready", "yes": {self.idx}, "no": set()}   # everybody else gets a prompt
        return Result(True)

    def consent_pending(self):
        return any(d["kind"] == "ready" and self.idx not in d["yes"] | d["no"] for d in self.world.deferred.values())

    def consent(self, kind, quest="", yes=True):
        w = self.world
        if kind == "ready":
            if not quest:
                quest = next((q for (q, n), d in w.deferred.items() if d["kind"] == "ready"), "")
            return w.vote(self.idx, quest, yes)
        self.me["consent"][kind] = bool(yes)
        return Result(True)

    def fast_travel(self, marker):
        w = self.world
        m = w.t(marker)
        if m is None:
            return Result(False, "no such marker")
        if "travel_in_combat" not in w.bugs and any(
                r["hostile"] and not r["dead"] and r["enabled"] and r["cell"] == self.me["cell"] for r in w.truth["refs"].values()):
            return Result(False, "fast travel refused: in combat")
        others = [i for i in range(w.n) if i != self.idx]
        if "consent_ignored" not in w.bugs and not all(w.players[i]["consent"].get("fasttravel") for i in others):
            return Result(False, "fast travel refused: party consent missing")
        for i in range(w.n):
            w.players[i]["pos"], w.players[i]["cell"] = tuple(m["pos"]), m["cell"]
            w.players[i]["consent"].pop("fasttravel", None)
        w.tether_active = True
        w.rebalance_owners()
        return Result(True)

    def spawn(self, base, at):
        w = self.world
        if self.idx != 0:
            return Result(False, "only the host spawns")
        a = w.t(at)
        if a is None:
            return Result(False, "no such spawn point")
        w.dup_n += 1
        k = "spawned:%d" % w.dup_n
        w.commit({"op": "ref", "ref": k, "f": w._mkref(k, "raider", a["pos"], a["cell"], base=base), "spawn": True})
        return Result(True, "", {"ref": k})

    def save_game(self, name):
        w = self.world
        if self.idx != 0:
            return Result(False, "only the host saves the world")
        w.saves[name] = (copy.deepcopy(w.truth), w.seq)
        return Result(True)

    def load_game(self, name):
        w = self.world
        if self.idx != 0:
            return Result(False, "only the host loads the world")
        if name not in w.saves:
            return Result(False, "no such save")
        truth, _ = w.saves[name]
        w.truth = copy.deepcopy(truth)
        w.views[0] = copy.deepcopy(truth)
        w.seq += 1
        w.applied[0] = w.seq
        if "no_rebase" not in w.bugs:
            for i in range(1, w.n):
                w.views[i] = copy.deepcopy(truth)
                w.pending[i] = []
                w.applied[i] = w.seq
                w.rebases[i] += 1
        return Result(True)

    def session_leave(self):
        if self.idx == 0:
            return Result(False, "the host cannot leave")
        self.world.connected[self.idx] = False
        return Result(True)

    def session_join(self):
        w = self.world
        w.connected[self.idx] = True
        if "reward_dup_on_rejoin" in w.bugs:
            for eff in w.reward_log:
                inv = self.me["inv"]
                inv[eff["item"]] = inv.get(eff["item"], 0) + eff["count"]
        return Result(True)

    def set_timescale(self, x):
        w = self.world
        if x <= 0:
            return Result(False, "refused: the game speed must be above 0, got %g" % x)
        w.speed_log[self.idx].append(x)
        if self.idx and "timescale_ignored" in w.bugs:
            return Result(True)                                      # accepted, but nothing changed
        if self.idx and "speed_not_reset" in w.bugs and x == 1.0 and w.speed[self.idx] != 1.0:
            return Result(True)
        w.speed[self.idx] = float(x)
        return Result(True)

    # --- queries ---
    def timescale(self):
        return self.world.speed[self.idx]

    def cell(self):
        return self.me["cell"]

    def distance_to(self, ref):
        r = self._ref(ref)
        if r is None:
            raise RuntimeError("%s is not loaded" % ref)
        return self.world.dist(self.me["pos"], r["pos"])

    def inventory_count(self, item, holder=None):
        if holder is None:
            return int(self.me["inv"].get(item, 0))
        return int(self._ref(holder)["items"].get(item, 0))

    def death_count(self, ref):
        return int(self._ref(ref)["deaths"])

    def target_alive(self, ref):
        r = self._ref(ref)
        return bool(r and r["enabled"] and not r["dead"])

    def ref_enabled(self, ref):
        r = self._ref(ref)
        return bool(r and r["enabled"])

    def in_dialogue(self):
        return bool(self.me["dialogue"])

    def spectating(self):
        return bool(self.me["spectating"])

    def is_locked(self, ref):
        return bool(self._ref(ref)["locked"])

    def door_open(self, ref):
        return bool(self._ref(ref)["open"])

    def device_entry_used(self, ref, entry):
        return entry in self._ref(ref)["entries_used"]

    def quest_stage(self, quest):
        st = self.view["quests"].get(norm(quest), {}).get("stages", [])
        return max(st) if st else 0

    def stage_done(self, quest, stage):
        return stage in self.view["quests"].get(norm(quest), {}).get("stages", [])

    def quest_running(self, quest):
        return bool(self.view["quests"].get(norm(quest), {}).get("running"))

    def objective_displayed(self, quest, n):
        return bool(self.view["objectives"].get(norm(quest), {}).get(str(n), {}).get("displayed"))

    def objective_completed(self, quest, n):
        return bool(self.view["objectives"].get(norm(quest), {}).get(str(n), {}).get("completed"))

    def owner_of(self, ref):
        r = self._ref(ref)
        if r is None:
            raise RuntimeError("%s is not loaded on %s" % (ref, self.name))
        return (r["owner"], r["epoch"])

    def ref_count(self, base, cell=""):
        return sum(1 for r in self.view["refs"].values()
                   if r.get("base") and norm(r["base"]) == norm(base) and r["enabled"] and not r["dead"] and (not cell or r["cell"] == cell))

    def rebases(self):
        return self.world.rebases[self.idx]


class FakeComparer(DictComparer):
    """The default dict comparer works on the fake's snapshots as they are."""


def make_machines(world: FakeCoop) -> List[Machine]:
    return [Machine(i, b.name, b, instance=i) for i, b in enumerate(world.bots)]
