#!/usr/bin/env python3
"""Prepared saves: the scenario start save with its load popups already pressed away, so a jump skips them.

Some games raise a pile of message boxes after a load (a save older than the game's add-ons, notices, tutorials), over many
seconds. An open box can keep the game in menu mode, so test input is lost. Press them away once, save the game under a
"ready" name on each copy, and let the runner load that save instead. The jump keeps its popup clearing as a safety net, but
waits only a few seconds when every copy loaded a prepared save.

    python -I tools/scenarios/prepare_save.py NAME [NAME ...] --adapter pkg.mod:factory [--instances 0,1]
    python -I tools/scenarios/prepare_save.py --save autosave --adapter pkg.mod:factory
    python -I tools/scenarios/prepare_save.py --status [NAME ...] [--adapter pkg.mod:factory] [--instances 0,1]

NAME is a scenario name (its start.save is prepared). --adapter is the same `pkg.mod:factory` that scen.py takes: factory(i)
returns the GameAdapter of instance i. --instances lists the instance ids handed to the factory (default 0,1), for example 2,3
for a second pair of copies. The tool never starts a game. It prints one JSON line per save with `ok` true or false and exits 1 when
the popups never go quiet, a copy refused to save, or the reload check still shows a popup. Exit 2 is a usage problem (no adapter,
an adapter without popup hooks).

The adapter supplies: popup_count(), dismiss_popup(), prepared_names(save), save_game(name), and optionally save_exists(name)
and save_stamp(name) (see gameadapter.py). Without popup_count() nothing can be prepared, and nothing is saved.

Where the facts live: one JSON file (default ./scen_prepared.json, or the path in $SCEN_PREPARED_FILE, or --prepared FILE),
keyed by instance id and start save. An entry says which save name was written, whether the reload check passed, and the stamp
of the source save. A prepared save is used only when the adapter says its file is there, the reload check passed, and the
source save has not changed since (saves in ROLLING_SAVES are exempt: the game rewrites those itself).

Opt out: set SCEN_NO_PREPARED=1 in the environment, or "start": {"prepared": false} in a scenario file.
"""
import argparse
import importlib
import json
import os
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

NO_PREPARED_ENV = "SCEN_NO_PREPARED"
MANIFEST_ENV = "SCEN_PREPARED_FILE"
DEFAULT_MANIFEST = "scen_prepared.json"
# test-only (SCEN_MUTANT): "noverify" | "earlysave" | "nostale" | "unverified" | "posids"
MUTANT = os.environ.get("SCEN_MUTANT", "")
# After a load, the quiet window of the popup clearing: no popup on any instance for this long. A plain save may raise its popups
# in waves with long gaps, so it waits longer. A prepared save raises none, so a few seconds only guard against a stray notice.
PLAIN_QUIET_S = 20.0
PREPARED_QUIET_S = 4.0


def opted_out():
    return bool(os.environ.get(NO_PREPARED_ENV))


class PreparedSaves:
    """Which instance holds a usable prepared copy of which start save. The facts are in one JSON file."""

    def __init__(self, path=None):
        self.path = Path(path or os.environ.get(MANIFEST_ENV) or DEFAULT_MANIFEST)

    @classmethod
    def auto(cls):
        """The default store when its file exists and the opt-out variable is not set, else None."""
        if opted_out():
            return None
        s = cls()
        return s if s.path.is_file() else None

    def _read(self):
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {"instances": {}}
        if not isinstance(data, dict) or not isinstance(data.get("instances"), dict):
            return {"instances": {}}
        return data

    def record(self, inst, save, name, verified, popups, stamp):
        """Write the entry of one start save on one instance. `stamp` is the source save's stamp taken now, so a replaced
        source shows up as stale later."""
        data = self._read()
        data["instances"].setdefault(str(inst), {})[save] = {
            "name": name, "verified": bool(verified), "popups": popups, "at": round(time.time()), "source": stamp}
        self.path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self.path.with_name(self.path.name + ".tmp")
        tmp.write_text(json.dumps(data, indent=1, sort_keys=True), encoding="utf-8")
        os.replace(tmp, self.path)

    def pick(self, inst, save, game=None):
        """-> (name, why). name is the prepared save to load on this instance, or None with the reason it is not used.
        `game` is that instance's adapter: it answers save_exists and save_stamp. Without it those two checks are skipped."""
        e = (self._read()["instances"].get(str(inst)) or {}).get(save)
        if not e:
            return None, "no prepared save of %r on instance %s" % (save, inst)
        name = e.get("name")
        if not name:
            return None, "the manifest entry of %r has no save name" % save
        if game is not None and _try(game.save_exists, name) is False:
            return None, "prepared save %s is gone" % name
        if not e.get("verified") and MUTANT != "unverified":
            return None, "prepared save %s did not pass the reload check" % name
        src = e.get("source")
        rolling = [r.lower() for r in getattr(game, "ROLLING_SAVES", ())] if game is not None else []
        if src is not None and game is not None and save.lower() not in rolling and MUTANT != "nostale":
            now = _try(game.save_stamp, save)
            if now is not None and now != src:
                return None, "source save %r changed after %s was prepared" % (save, name)
        return name, "prepared save %s" % name

    def status(self, inst, save, game=None):
        name, why = self.pick(inst, save, game)
        return {"instance": inst, "save": save, "prepared": name, "why": why, "checked": game is not None}


class NoPopupHooks(RuntimeError):
    """The adapters cannot report popups, so there is nothing to prepare."""


def _try(fn, *args):
    """A hook that may be missing or unable to answer: NotImplementedError and ScenarioError both mean "cannot tell" (None)."""
    from gameadapter import ScenarioError
    try:
        return fn(*args)
    except (NotImplementedError, ScenarioError):
        return None


def prepare(games, save, store, instances=None, log=print, verify=True, quiet_s=PLAIN_QUIET_S, max_s=150.0, **runner_kw):
    """Make the prepared copy of `save` on every adapter: load it, press the popups away until quiet, save the game under the ready
    name, then (verify) load the ready save and check that no popup comes up. `store` is a PreparedSaves. `instances` are the
    instance ids of the adapters (default 0..n-1). Returns {"save", "ok", "popupsPressed", "perInstance": [...]}.
    Raises NoPopupHooks when the adapters cannot report popups, and RuntimeError when the popups did not go quiet (nothing is saved).
    runner_kw goes to runner.Runner (tests pass a fake clock)."""
    import runner
    ids = list(instances) if instances is not None else list(range(len(games)))
    rn = runner.Runner(games, runner.StateStore(), prepared=False, **runner_kw)
    for g in games:
        g.load_save(save)
    rn.sleep(rn.load_wait_s)
    boxes = {"pressed": 0, "quiet": True} if MUTANT == "earlysave" else rn.clear_popups(quiet_s=quiet_s, max_s=max_s)
    if boxes is None:
        raise NoPopupHooks("these instances cannot report popups (popup_count() gives None): nothing was saved. "
                           "Implement popup_count() and dismiss_popup() in the adapter")
    if not boxes.get("quiet"):
        raise RuntimeError("popups were still coming after %.0f s (%s pressed): nothing was saved" % (max_s, boxes.get("pressed")))
    out = []
    for k, g in enumerate(games):
        names = list(g.prepared_names(save))
        saved, err = None, None
        for name in names:
            try:
                g.save_game(name)
                saved = name
                break
            except Exception as e:   # noqa: BLE001 - the next name may be the one the engine takes
                err = "%s: %s" % (type(e).__name__, str(e)[:160])
        if saved is None:
            out.append({"instance": ids[k], "ok": False, "error": "save_game failed for %s (%s)" % (", ".join(names), err)})
        else:
            out.append({"instance": ids[k], "ok": True, "name": saved})
    if verify and MUTANT != "noverify":
        for k, g in enumerate(games):
            if out[k].get("ok"):
                g.load_save(out[k]["name"])
        rn.sleep(rn.load_wait_s)
        again = rn.clear_popups(quiet_s=PREPARED_QUIET_S, max_s=max_s) or {}
        clean = bool(again.get("quiet")) and not again.get("pressed")
        for k in range(len(games)):
            if out[k].get("ok"):
                out[k]["verified"] = clean
                if not clean:
                    out[k]["error"] = "a load of %s still raised %s popup(s)" % (out[k]["name"], again.get("pressed"))
    else:
        for r in out:
            r["verified"] = bool(r.get("ok"))
    for k, r in enumerate(out):
        if r.get("ok"):
            inst = k if MUTANT == "posids" else r["instance"]
            store.record(inst, save, r["name"], r["verified"], boxes.get("pressed"), _try(games[k].save_stamp, save))
    ok = all(r.get("ok") and r.get("verified") for r in out)
    log("prepared %r: %s" % (save, "ok" if ok else "FAILED " + json.dumps(out)))
    return {"save": save, "ok": ok, "popupsPressed": boxes.get("pressed"), "perInstance": out}


def _factory(spec):
    sys.path.insert(0, str(Path.cwd()))
    mod, _, fn = spec.partition(":")
    return getattr(importlib.import_module(mod), fn)


def main(argv=None, factory=None, store=None, **kw):
    """`factory` and `store` let tests pass an adapter factory and a PreparedSaves directly. kw goes to prepare()."""
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="*", help="scenario names (their start saves are prepared)")
    ap.add_argument("--save", action="append", default=[], help="a save name to prepare (repeatable)")
    ap.add_argument("--status", action="store_true", help="show what is prepared, change nothing (a game is not needed)")
    ap.add_argument("--no-verify", action="store_true", help="skip the reload check")
    ap.add_argument("--adapter", help="pkg.mod:factory, factory(i) returns the GameAdapter of instance i (as for scen.py run)")
    ap.add_argument("--instances", default="0,1", metavar="IDS",
                    help="instance ids handed to the factory, e.g. 0,1 (default) or 2,3 for a second pair of copies")
    ap.add_argument("--dir", help="folder with scenario files (default: the shipped examples)")
    ap.add_argument("--prepared", help="the manifest file (default $%s or ./%s)" % (MANIFEST_ENV, DEFAULT_MANIFEST))
    a = ap.parse_args(argv)
    import scenlib
    directory = a.dir or str(scenlib.SCEN_DIR)
    saves = list(a.save)
    for n in a.names:
        p = Path(n)
        hits = [p] if p.suffix == ".json" else sorted(Path(directory).rglob(n + ".json"))
        if not hits:
            print("prepare_save: no scenario named %r under %s" % (n, directory))
            return 2
        s = scenlib.load_scenario(hits[0])["start"]["save"]
        if s not in saves:
            saves.append(s)
    if not a.names and not a.save:
        for p in scenlib.list_scenarios(directory):
            s = scenlib.load_scenario(p)["start"]["save"]
            if s not in saves:
                saves.append(s)
    try:
        ids = [int(x) for x in a.instances.split(",") if x.strip()]
    except ValueError:
        print("prepare_save: --instances wants a comma list of numbers, got %r" % a.instances)
        return 2
    store = store or PreparedSaves(a.prepared)
    make = factory or (_factory(a.adapter) if a.adapter else None)
    if a.status:
        games = {i: make(i) for i in ids} if make else {}
        for s in saves:
            for i in ids:
                print(json.dumps(store.status(i, s, games.get(i))))
        return 0
    if make is None:
        print("prepare_save: needs --adapter pkg.mod:factory (it is the same one scen.py run takes). --status works without it.")
        return 2
    try:
        games = [make(i) for i in ids]
        bad = 0
        for s in saves:
            r = prepare(games, s, store, instances=ids, verify=not a.no_verify, **kw)
            print(json.dumps(r))
            bad += not r["ok"]
        return 1 if bad else 0
    except NoPopupHooks as e:
        print("prepare_save: %s" % e)
        return 2
    except RuntimeError as e:   # popups never went quiet
        print("prepare_save: %s" % e)
        return 1


if __name__ == "__main__":
    sys.exit(main())
