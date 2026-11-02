"""Scenario format (schema 1): loading, game-free validation, jump planning and value comparison.

A scenario is one JSON file: a starting state ("jump to it, do not replay the game") plus steps and checks that run
on 1 to 4 game instances sharing one world. Everything that names a thing in the game goes through the scenario's
`objects` glossary: alias -> {"id": "<whatever your adapter understands>", "kind": "actor|thing|item|base|place|state"}.
Steps use "$alias". The validator rejects unknown aliases, objects of the wrong kind for a field, unknown ops and
check kinds, and scenarios without a check, all without a game. Game specific checks (for example "does this id exist
in the game's data dump") plug in as hooks: see `validate(..., hooks=[...])`.

This module is pure: no game, no network, standard library only. The game-facing part is runner.py plus your own
GameAdapter (see README.md).
"""
import difflib
import json
import os
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCEN_DIR = HERE / "examples"
SCHEMA = 1
MUTANT = os.environ.get("SCEN_MUTANT", "")   # test-only twin switch, see run_tests.py (here: noindex | nonearest | anykind)

NAME_RE = re.compile(r"^[a-z0-9_]+$")
ALIAS_RE = re.compile(r"^\$([A-Za-z][A-Za-z0-9_]*)$")
ALIAS_NAME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
SAVE_RE = re.compile(r"^[A-Za-z0-9_\-]+$")
PLAYER = "player"                                   # built-in object: the player of the instance the step runs on
KINDS = ("actor", "thing", "item", "base", "place", "state")


def K(*kinds, player=False):
    """Field spec: a `$alias` of one of these object kinds (and the literal "player" when player=True)."""
    return ("obj", frozenset(kinds), player)


INT, POS_INT, NUM, STR, ANY, DICT = ("int",), ("pos_int",), ("num",), ("str",), ("any",), ("dict",)

# op -> (required fields, optional fields). Every spec is either K(...) or one of the plain types above.
OPS = {
    "set_state": ({"key": K("state"), "value": ANY}, {}),
    "move": ({"who": K("actor", "thing", player=True), "to": K("thing", "place", "actor")}, {}),
    "spawn": ({"base": K("base", "item"), "at": K("thing", "place", "actor", player=True)}, {"count": POS_INT}),
    "give": ({"who": K("actor", "thing", player=True), "item": K("item"), "count": POS_INT}, {}),
    "take": ({"who": K("actor", "thing", player=True), "item": K("item"), "count": POS_INT}, {}),
    "kill": ({"ref": K("actor")}, {}),
    "revive": ({"ref": K("actor")}, {}),
    "enable": ({"ref": K("actor", "thing")}, {}),
    "disable": ({"ref": K("actor", "thing")}, {}),
    "activate": ({"ref": K("actor", "thing")}, {}),
    "lock": ({"ref": K("thing"), "level": INT}, {}),
    "unlock": ({"ref": K("thing")}, {}),
    "wait": ({"seconds": NUM}, {}),
    "save": ({"name": STR}, {}),
    "load": ({"name": STR}, {}),
    "leave": ({}, {}),
    "join": ({}, {}),
    "action": ({"name": STR}, {"args": DICT}),        # anything game specific: fast travel, dialogue pick, consent answer ...
    "check": ({"kind": STR}, {}),
}
COMMON = {"id", "op", "on", "note", "needs", "parallel", "dirties"}
CHECK_META = {"cmp", "value", "agg", "tol", "timeout"}
CHECK_KINDS = {
    "state": {"key": K("state")},
    "state_reached": {"key": K("state"), "at": ANY},
    "item_count": {"who": K("actor", "thing", player=True), "item": K("item")},
    "dead": {"ref": K("actor")},
    "disabled": {"ref": K("actor", "thing")},
    "locked": {"ref": K("thing")},
    "in_area": {"ref": K("actor", "thing", player=True), "place": K("place")},
    "distance": {"a": K("actor", "thing", "place", player=True), "b": K("actor", "thing", "place", player=True)},
    "stat": {"ref": K("actor", player=True), "stat": STR},
    "follower": {"ref": K("actor")},
    "count": {"base": K("base", "item")},
    "owner": {"ref": K("actor", "thing")},
    "value": {"query": STR, "args": DICT},           # any number your adapter can read by name
}
CMPS = {"==", "!=", "<", "<=", ">", ">=", "same"}
AGGS = {None, "sum", "min", "max"}
SETTLE_OPS = {"move", "spawn", "set_state"}          # the runner pauses briefly after these
OBJECT_FIELDS = {"key", "who", "to", "base", "at", "item", "ref", "place", "a", "b"}


# ---------------------------------------------------------------------------------------------------- loading
def load_scenario(path):
    p = Path(path)
    sc = json.loads(p.read_text(encoding="utf-8"))
    sc.setdefault("_file", str(p))
    return sc


def list_scenarios(directory=SCEN_DIR):
    """All scenario files under `directory` (recursively), sorted by path."""
    return sorted(p for p in Path(directory).rglob("*.json"))


def players_of(sc):
    return int(sc.get("players", 1))


def targets(sc, on):
    """Indices of the players a step runs on: 'host' = 0, 'guest' = every other player, 'all', or one int index."""
    n = players_of(sc)
    on = "host" if on is None else on
    if on == "all":
        return list(range(n))
    if on == "host":
        return [0]
    if on == "guest":
        return list(range(1, n))
    if isinstance(on, int) and not isinstance(on, bool) and 0 <= on < n:
        return [on]
    raise ValueError("bad 'on': %r" % (on,))


def all_steps(sc):
    return list(sc.get("setup") or []) + list(sc.get("steps") or [])


def step_target(step, sc):
    default = "all" if step.get("op") == "check" else "host"
    return targets(sc, step.get("on", default))


# ---------------------------------------------------------------------------------------------------- resolution
class Resolver:
    """`$alias` -> the adapter-facing id string. "player" stays "player"."""

    def __init__(self, sc):
        self.objects = sc.get("objects") or {}

    def obj(self, v):
        if v == PLAYER:
            return PLAYER
        m = ALIAS_RE.match(v) if isinstance(v, str) else None
        if not m or m.group(1) not in self.objects:
            raise KeyError("unknown alias %r" % (v,))
        return str(self.objects[m.group(1)]["id"])

    def kind(self, v):
        if v == PLAYER:
            return PLAYER
        return self.objects[ALIAS_RE.match(v).group(1)]["kind"]

    def base_of(self, v):
        """The `base` id of a placed object (what picking it up gives), or None."""
        if v == PLAYER:
            return None
        o = self.objects[ALIAS_RE.match(v).group(1)]
        return str(o["base"]) if o.get("base") else None


def _spec_of(step):
    op = step["op"]
    if op == "check":
        return dict(CHECK_KINDS.get(step.get("kind"), {}))
    req, opt = OPS[op]
    d = dict(req)
    d.update(opt)
    return d


def resolve_args(step, res):
    """The step's fields as the adapter sees them: object fields resolved to ids, everything else unchanged."""
    out = {}
    for k, spec in _spec_of(step).items():
        if k not in step:
            continue
        if spec[0] == "obj":
            out[k] = res.obj(step[k])
        elif spec[0] == "dict":
            out[k] = {a: (res.obj(b) if isinstance(b, str) and ALIAS_RE.match(b) else b) for a, b in step[k].items()}
        else:
            out[k] = step[k]
    return out


# ---------------------------------------------------------------------------------------------------- validation
def _plain_ok(spec, v):
    t = spec[0]
    if t == "int":
        return isinstance(v, int) and not isinstance(v, bool)
    if t == "pos_int":
        return isinstance(v, int) and not isinstance(v, bool) and v >= 1
    if t == "num":
        return isinstance(v, (int, float)) and not isinstance(v, bool)
    if t == "str":
        return isinstance(v, str) and bool(v.strip())
    if t == "dict":
        return isinstance(v, dict)
    return True


def _strings(v):
    """Every string inside a nested list or dict."""
    if isinstance(v, str):
        yield v
    elif isinstance(v, list):
        for x in v:
            yield from _strings(x)
    elif isinstance(v, dict):
        for x in v.values():
            yield from _strings(x)


def validate(sc, name=None, hooks=()):
    """Return a list of error strings (empty = valid). Game-free. `hooks` are callables f(scenario) -> list of errors."""
    errs = []
    e = errs.append
    if sc.get("schema") != SCHEMA:
        e("schema must be %d" % SCHEMA)
    nm = sc.get("name")
    if not nm or not NAME_RE.match(str(nm)):
        e("name must be lower_snake_case")
    if name and nm != name:
        e("name %r does not match file name %r" % (nm, name))
    for k in ("title", "description"):
        if not sc.get(k):
            e("missing %s" % k)
    n = sc.get("players", 1)
    if not isinstance(n, int) or isinstance(n, bool) or not 1 <= n <= 4:
        e("players must be an int 1..4")
    start = sc.get("start") or {}
    if not start.get("save"):
        e("start.save is required (the name of the save the scenario starts from)")
    elif not SAVE_RE.match(str(start["save"])):
        e("start.save has odd characters")
    if "prepared" in start and not isinstance(start["prepared"], bool):
        e("start.prepared must be true or false (false = always load the plain start.save, never a prepared copy)")

    objs = sc.get("objects") or {}
    if not isinstance(objs, dict):
        e("objects must be an object")
        objs = {}
    for alias, d in objs.items():
        if not ALIAS_NAME_RE.match(alias) or alias == PLAYER:
            e("objects: bad alias %r" % alias)
            continue
        if not isinstance(d, dict) or not isinstance(d.get("id"), str) or not d["id"].strip():
            e("objects.%s: needs a non-empty string id" % alias)
            continue
        if d.get("kind") not in KINDS:
            e("objects.%s: kind must be one of %s" % (alias, "/".join(KINDS)))
        if "base" in d and (not isinstance(d["base"], str) or d["base"].strip() == ""):
            e("objects.%s: base must be an id string" % alias)

    def need_obj(where, v, spec):
        _, kinds, player_ok = spec
        if v == PLAYER:
            if not player_ok:
                e("%s: 'player' is not allowed here" % where)
            return
        m = ALIAS_RE.match(v) if isinstance(v, str) else None
        if not m:
            e("%s: %r is not a $alias" % (where, v))
            return
        o = objs.get(m.group(1))
        if not isinstance(o, dict) or "id" not in o:
            if MUTANT != "noalias":
                e("%s: unknown alias %s" % (where, v))
            return
        if o.get("kind") not in kinds:
            e("%s: %s is a %s, expected one of %s" % (where, v, o.get("kind"), "/".join(sorted(kinds))))

    def check_fields(where, st, spec_map, allowed_extra):
        for k, spec in spec_map.items():
            if k not in st:
                if k in _required(st):
                    e("%s: needs %r" % (where, k))
                continue
            if spec[0] == "obj":
                need_obj("%s.%s" % (where, k), st[k], spec)
            elif not _plain_ok(spec, st[k]):
                e("%s: field %r has the wrong type or value" % (where, k))
            elif spec[0] == "dict":
                for b in st[k].values():
                    if isinstance(b, str) and ALIAS_RE.match(b) and b[1:] not in objs and MUTANT != "noalias":
                        e("%s.%s: unknown alias %s" % (where, k, b))
        for k in st:
            if k not in spec_map and k not in allowed_extra:
                e("%s: unexpected key %r" % (where, k))

    def _required(st):
        if st.get("op") == "check":
            return set(CHECK_KINDS.get(st.get("kind"), {})) - {"args"}
        return set(OPS[st["op"]][0])

    rs = sc.get("reset")
    if rs is not None:
        if not isinstance(rs, dict) or not isinstance(rs.get("keys", []), list):
            e("reset must be an object with a 'keys' list of state aliases")
        else:
            for a in rs.get("keys", []):
                need_obj("reset.keys", a, K("state"))

    ids, checks = set(), 0
    steps = all_steps(sc)
    if not sc.get("steps"):
        e("steps is empty")
    for i, st in enumerate(steps):
        sid = st.get("id")
        w = "step %s" % (sid or i)
        if not sid or not NAME_RE.match(str(sid)):
            e("%s: id must be lower_snake_case" % w)
        elif sid in ids:
            e("%s: duplicate id" % w)
        ids.add(sid)
        op = st.get("op")
        if op not in OPS:
            e("%s: unknown op %r" % (w, op))
            continue
        try:
            targets(sc, st.get("on", "all" if op == "check" else "host"))
        except ValueError:
            e("%s: bad 'on' %r for %s player(s)" % (w, st.get("on"), n))
        if "parallel" in st and not isinstance(st["parallel"], bool):
            e("%s: parallel must be true or false" % w)
        if "needs" in st and (not isinstance(st["needs"], list) or not all(isinstance(x, str) for x in st["needs"])):
            e("%s: needs must be a list of capability names" % w)
        if "dirties" in st:
            if not isinstance(st["dirties"], list):
                e("%s: dirties must be a list of aliases" % w)
            else:
                for a in st["dirties"]:
                    need_obj(w + ".dirties", a, K(*KINDS))
        if op == "check":
            checks += 1
            kind = st.get("kind")
            if kind not in CHECK_KINDS:
                e("%s: unknown check kind %r" % (w, kind))
                continue
            check_fields(w, st, CHECK_KINDS[kind], COMMON | CHECK_META | {"kind"})
            cmp_ = st.get("cmp", "==")
            if cmp_ not in CMPS:
                e("%s: cmp %r not in %s" % (w, cmp_, sorted(CMPS)))
            if cmp_ != "same" and "value" not in st:
                e("%s: check needs a 'value' (or cmp 'same')" % w)
            if "value" in st and (not isinstance(st["value"], (int, float)) or isinstance(st["value"], bool)):
                e("%s: value must be a number (use 1 and 0 for true and false)" % w)
            if st.get("agg") not in AGGS:
                e("%s: agg must be sum, min or max" % w)
            if st.get("agg") and cmp_ == "same":
                e("%s: agg and cmp 'same' cannot be combined" % w)
            if "timeout" in st and (not isinstance(st["timeout"], (int, float)) or not 0 < st["timeout"] <= 120):
                e("%s: timeout must be above 0 and at most 120 seconds" % w)
            continue
        req, opt = OPS[op]
        spec_map = dict(req)
        spec_map.update(opt)
        check_fields(w, st, spec_map, COMMON)
        if op == "wait" and isinstance(st.get("seconds"), (int, float)) and not 0 <= st["seconds"] <= 120:
            e("%s: seconds must be 0..120" % w)
        if op in ("save", "load") and isinstance(st.get("name"), str) and not SAVE_RE.match(st["name"]):
            e("%s: bad save name" % w)
    if checks == 0:
        e("scenario has no check step")

    refs = sc.get("refs")
    if refs is not None:
        if not isinstance(refs, dict):
            e("refs must be an object")
        else:
            for key, v in refs.items():
                for item in _strings(v):
                    if item.startswith("$"):
                        need_obj("refs.%s" % key, item, K(*KINDS))
    for h in hooks:
        errs.extend(h(sc))
    return errs


def validate_file(path, hooks=()):
    p = Path(path)
    try:
        sc = load_scenario(p)
    except Exception as ex:   # noqa: BLE001
        return ["cannot read %s: %s" % (p.name, ex)]
    return validate(sc, name=p.stem, hooks=hooks)


# ---------------------------------------------------------------------------------------------------- game data index
class DataIndex:
    """The ids a game's data knows, so a scenario can be checked without a game.

    entries: {id: kind}. kind is one of KINDS, a list or set of kinds (an id that can be used as two kinds, for example an
    item that is also spawnable), or None for "exists, any kind". kinds: the object kinds this index can speak for (None =
    all of them); objects of other kinds are not checked. case_sensitive=False matches ids the way a game that ignores case does.
    label: how error messages call the data ("the game data", "the item dump")."""

    def __init__(self, entries, kinds=None, case_sensitive=True, label="the game data"):
        self.case_sensitive, self.label = bool(case_sensitive), label
        self.kinds = None if kinds is None else frozenset(kinds)
        self._by_key = {}
        for ident, k in dict(entries).items():
            ks = None if k is None else frozenset([k] if isinstance(k, str) else k)
            self._by_key[self._key(ident)] = (str(ident), ks)

    def _key(self, ident):
        return str(ident) if self.case_sensitive else str(ident).lower()

    def __len__(self):
        return len(self._by_key)

    def lookup(self, ident):
        """-> (spelling in the data, kinds or None), or None when the id is unknown."""
        return self._by_key.get(self._key(ident))

    def nearest(self, ident, n=3):
        """Up to n known ids that look most like `ident`, best first, in the spelling of the data."""
        want = self._key(ident)
        pool = [k for k in self._by_key if abs(len(k) - len(want)) <= max(3, len(want) // 2)]
        scored = []
        for k in difflib.get_close_matches(want, pool, n=max(n, 20), cutoff=0.6):
            prefix = len(os.path.commonprefix([want, k]))
            scored.append((-round(difflib.SequenceMatcher(None, want, k).ratio(), 6), -prefix, k))   # ties: longer shared start, then A to Z
        return [self._by_key[k][0] for _, _, k in sorted(scored)[:n]]


def as_index(obj):
    """What an adapter's data_index() may return -> a DataIndex or None. A dict {id: kind}, a list of ids (any kind) or a DataIndex."""
    if obj is None or isinstance(obj, DataIndex):
        return obj
    if isinstance(obj, dict):
        return DataIndex(obj)
    return DataIndex({i: None for i in obj})


def _uses(sc):
    """alias -> ["setup step <id> (<op>)", "step <id> (<op>)"]: where each glossary object is named, in order."""
    out = {}
    setup_ids = {s.get("id") for s in sc.get("setup") or []}

    def add(v, where):
        m = ALIAS_RE.match(v) if isinstance(v, str) else None
        if m and where not in out.setdefault(m.group(1), []):
            out[m.group(1)].append(where)
    for st in all_steps(sc):
        where = "%s %s (%s)" % ("setup step" if st.get("id") in setup_ids else "step", st.get("id"), st.get("op"))
        for k in OBJECT_FIELDS:
            add(st.get(k), where)
        if isinstance(st.get("args"), dict):
            for b in st["args"].values():
                add(b, where)
    return out


def index_errors(sc, index):
    """Errors for every object of the scenario whose id is not in the game data index, or is there with another kind.
    An unknown id names the nearest known ids. A `base` of an object is checked as an item or spawnable type."""
    if MUTANT == "noindex":
        return []
    index = as_index(index)
    if index is None:
        return []
    errs, uses = [], _uses(sc)
    for alias, d in (sc.get("objects") or {}).items():
        if not isinstance(d, dict) or not isinstance(d.get("id"), str) or d.get("kind") not in KINDS:
            continue   # the format check reports those
        where = "used by %s" % ", ".join(uses[alias][:3]) if uses.get(alias) else "not used by any step"
        checks = [("id", d["id"], {d["kind"]})]
        if isinstance(d.get("base"), str):
            checks.append(("base", d["base"], {"item", "base"}))
        for field, ident, want in checks:
            if index.kinds is not None and not (want & index.kinds):
                continue
            hit = index.lookup(ident)
            if hit is None:
                near = [] if MUTANT == "nonearest" else index.nearest(ident)
                errs.append("objects.%s: %s %r is not in %s (%s)%s" % (
                    alias, field, ident, index.label, where,
                    "; nearest known id: %r%s" % (near[0], " (also close: %s)" % ", ".join(repr(x) for x in near[1:]) if near[1:] else "")
                    if near else ""))
            elif hit[1] is not None and not (want & hit[1]) and MUTANT != "anykind":
                errs.append("objects.%s: %s %r is a %s in %s but the scenario uses it as %s (%s)" % (
                    alias, field, hit[0], "/".join(sorted(hit[1])), index.label, "/".join(sorted(want)), where))
    return errs


def index_hook(index):
    """A validate() hook that checks a scenario against `index` (a DataIndex, a dict, or a list of ids)."""
    idx = as_index(index)
    return lambda sc: index_errors(sc, idx)


# ---------------------------------------------------------------------------------------------------- capabilities
def needs_of(step):
    """Capability names a step needs from the adapter: 'op:<op>', 'check:<kind>', 'action:<name>' plus its own `needs`."""
    out = set(step.get("needs") or [])
    if step.get("op") == "check":
        out.add("check:%s" % step.get("kind"))
    elif step.get("op") == "action":
        out.add("action:%s" % step.get("name"))
    else:
        out.add("op:%s" % step.get("op"))
    return out


def scenario_needs(sc):
    out = set()
    for st in all_steps(sc):
        out |= needs_of(st)
    return out


# ---------------------------------------------------------------------------------------------------- comparison
def compare(vals, cmp_, value, agg, tol):
    """Pure comparison used by the runner. vals = one number per player the check ran on."""
    if cmp_ == "same":
        return max(vals) - min(vals) <= tol
    if agg == "sum":
        vals = [sum(vals)]
    elif agg == "min":
        vals = [min(vals)]
    elif agg == "max":
        vals = [max(vals)]
    ok = True
    for v in vals:
        if cmp_ == "==":
            ok &= abs(v - value) <= tol
        elif cmp_ == "!=":
            ok &= abs(v - value) > tol
        elif cmp_ == "<":
            ok &= v < value
        elif cmp_ == "<=":
            ok &= v <= value
        elif cmp_ == ">":
            ok &= v > value
        elif cmp_ == ">=":
            ok &= v >= value
    return bool(ok)


# ---------------------------------------------------------------------------------------------------- jump planning
def footprint(sc):
    """-> {"dirty": set, "touched": set, "state": set}. Tokens are object ids, 'base:<id>' for spawned bases,
    'inv:<item id>' for player inventories, and '*' for a change that cannot be named (a `load`, an `action` or a
    `set_state` without a `dirties` list). `dirty` = what the scenario changes in the world; `touched` = everything it
    reads or changes; `state` = ids of state keys it sets."""
    res = Resolver(sc)
    dirty, touched, state = set(), set(), set()

    def oid(v):
        return None if v == PLAYER else res.obj(v)

    for st in all_steps(sc):
        op = st["op"]
        for k in OBJECT_FIELDS:
            if k in st and isinstance(st[k], str) and (st[k] == PLAYER or ALIAS_RE.match(st[k])):
                if st[k] != PLAYER:
                    touched.add(res.obj(st[k]))
        if isinstance(st.get("args"), dict):
            for b in st["args"].values():
                if isinstance(b, str) and ALIAS_RE.match(b):
                    touched.add(res.obj(b))
        if op == "check" and st.get("kind") == "item_count":
            touched.add("inv:" + res.obj(st["item"]))
        if op == "check" and st.get("kind") == "count":
            touched.add("base:" + res.obj(st["base"]))
        if op == "spawn":
            dirty.add("base:" + res.obj(st["base"]))
            touched.add("base:" + res.obj(st["base"]))
            dirty.add(oid(st["at"]) or "*")
        elif op == "set_state":
            state.add(res.obj(st["key"]))
            if "dirties" in st:
                dirty |= {res.obj(a) for a in st["dirties"]}
            else:
                dirty.add("*")
        elif op == "move":
            if st["who"] != PLAYER:
                dirty.add(res.obj(st["who"]))
        elif op in ("kill", "revive", "enable", "disable", "lock", "unlock"):
            dirty.add(res.obj(st["ref"]))
        elif op == "activate":
            dirty.add(res.obj(st["ref"]))
            b = res.base_of(st["ref"])
            if b:
                dirty.add("inv:" + b)
                touched.add("inv:" + b)
        elif op in ("give", "take"):
            it = res.obj(st["item"])
            touched.add("inv:" + it)
            dirty.add(oid(st["who"]) or "inv:" + it)
        elif op == "action":
            if "dirties" in st:
                dirty |= {res.obj(a) for a in st["dirties"]}
            else:
                dirty.add("*")
        elif op == "load":
            dirty.add("*")
        if op not in ("set_state", "action") and "dirties" in st:
            dirty |= {res.obj(a) for a in st["dirties"]}
    for a in (sc.get("reset") or {}).get("keys") or []:
        state.add(res.obj(a))
    dirty.discard(None)
    touched.discard(None)
    return {"dirty": dirty, "touched": touched, "state": state}


def reset_ids(sc):
    """Ids of the state keys to reset in place: reset.keys plus every key a step sets. Order kept, no duplicates."""
    res = Resolver(sc)
    ids = [res.obj(a) for a in (sc.get("reset") or {}).get("keys") or []]
    ids += [res.obj(st["key"]) for st in all_steps(sc) if st["op"] == "set_state"]
    seen, out = set(), []
    for i in ids:
        if i not in seen:
            seen.add(i)
            out.append(i)
    return out


def plan_jump(state, sc, fp=None, force_reload=False):
    """Decide whether jumping to `sc` needs a save reload. A reload is required when: nothing is loaded; a different
    start save is loaded; the previous scenario failed or errored; this scenario sets start.reload; the previous scenario
    changed something this one uses; or the previous one changed something unnameable (token '*').
    state = {"loaded": bool, "save": str, "dirty": [...], "failed": bool}. Returns {"reload": bool, "reasons": [...]}."""
    reasons = []
    fp = fp or footprint(sc)
    want = (sc.get("start") or {}).get("save")
    if force_reload:
        reasons.append("reload forced")
    if not state or not state.get("loaded"):
        reasons.append("no save loaded on this instance yet")
    else:
        if state.get("save") != want:
            reasons.append("loaded save %r differs from start.save %r" % (state.get("save"), want))
        if state.get("failed"):
            reasons.append("previous scenario did not pass; the world may be damaged")
        if (sc.get("start") or {}).get("reload"):
            reasons.append("scenario sets start.reload")
        prev = set(state.get("dirty") or [])
        if "*" in prev:
            reasons.append("previous scenario changed the world in ways that cannot be named ('*')")
        else:
            hit = sorted(prev & fp["touched"])
            if hit:
                reasons.append("previous scenario changed things this one uses: %s" % ", ".join(hit[:4]) + (" ..." if len(hit) > 4 else ""))
    return {"reload": bool(reasons), "reasons": reasons}


# ---------------------------------------------------------------------------------------------------- playbook refs
def playbook_refs(sc):
    """The optional `refs` block as plain ids, for the bots tool: {"raiders": ["id1", "id2"], "weapon": "id"}.
    A "$alias" becomes that object's id (also inside lists and nested objects); any other value is passed through."""
    res = Resolver(sc)

    def one(v):
        if isinstance(v, str):
            return res.obj(v) if ALIAS_RE.match(v) else v
        if isinstance(v, list):
            return [one(x) for x in v]
        if isinstance(v, dict):
            return {k: one(x) for k, x in v.items()}
        return v
    return {k: one(v) for k, v in (sc.get("refs") or {}).items()}
