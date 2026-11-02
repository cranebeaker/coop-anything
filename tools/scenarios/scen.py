#!/usr/bin/env python3
"""Scenario command line.

    python -I tools/scenarios/scen.py list [DIR]
    python -I tools/scenarios/scen.py validate [FILE_OR_NAME ...]        game-free; exit 1 on any error
    python -I tools/scenarios/scen.py validate ... --index ids.json      also check every id against a game data index file
                                                                         ({"id": "kind"}, kind one of the object kinds or null)
    python -I tools/scenarios/scen.py validate ... --adapter pkg.mod:factory   same, with the adapter's data_index()
    python -I tools/scenarios/scen.py plan NAME [--prev NAME]            show the jump decision (reload or in place), no game
    python -I tools/scenarios/scen.py run NAME ... --fake                dry run on the in-memory fake game (no game)
    python -I tools/scenarios/scen.py run NAME ... --adapter pkg.mod:factory [--players N] [--instances 2,3] [--prepared FILE]
                                                                         run on your game; factory(i) returns the GameAdapter
                                                                         of instance i. The tool never launches a game itself.
                                                                         --instances gives the ids handed to the factory (default
                                                                         0..N-1), --prepared the prepared-save manifest of
                                                                         prepare_save.py (see that tool).

A NAME is the file name without .json, looked up under the examples folder (or the folder given with --dir).
"""
import argparse
import importlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import scenlib  # noqa: E402
import runner  # noqa: E402


def _paths(names, directory):
    if not names:
        return scenlib.list_scenarios(directory)
    out = []
    for n in names:
        p = Path(n)
        if p.suffix == ".json":
            out.append(p)
        else:
            hits = sorted(Path(directory).rglob(n + ".json"))
            out.append(hits[0] if hits else Path(directory) / (n + ".json"))
    return out


def cmd_list(a):
    for p in scenlib.list_scenarios(a.dir):
        sc = scenlib.load_scenario(p)
        print("%-24s players=%d  %s" % (sc["name"], scenlib.players_of(sc), sc["title"]))
    return 0


def _factory(spec):
    sys.path.insert(0, str(Path.cwd()))
    mod, _, fn = spec.partition(":")
    return getattr(importlib.import_module(mod), fn)


def _ids(text, n):
    return [int(x) for x in text.split(",") if x.strip()] if text else list(range(n))


def cmd_validate(a):
    paths = _paths(a.names, a.dir)
    hooks = []
    if a.index:
        hooks.append(scenlib.index_hook(json.loads(Path(a.index).read_text(encoding="utf-8"))))
    elif a.adapter:
        idx = _factory(a.adapter)(0).data_index()
        if idx is None:
            print("note: ids were not checked against the game data: the adapter provides no data_index()")
        else:
            hooks.append(scenlib.index_hook(idx))
    bad = 0
    for p in paths:
        errs = scenlib.validate_file(p, hooks=hooks)
        print("%s %s" % ("FAIL" if errs else "ok  ", p.name))
        for e in errs:
            print("    " + e)
        bad += bool(errs)
    print("%d scenario file(s) checked, %d invalid" % (len(paths), bad))
    return 1 if bad else 0


def cmd_plan(a):
    sc = scenlib.load_scenario(_paths([a.name], a.dir)[0])
    state = {"loaded": False}
    if a.prev:
        pv = scenlib.load_scenario(_paths([a.prev], a.dir)[0])
        state = {"loaded": True, "save": pv["start"]["save"], "dirty": sorted(scenlib.footprint(pv)["dirty"]), "failed": False}
    print(json.dumps(scenlib.plan_jump(state, sc), indent=1))
    print("state keys reset in place:", " ".join(scenlib.reset_ids(sc)) or "(none)")
    return 0


def cmd_run(a):
    scs = [scenlib.load_scenario(p) for p in _paths(a.names, a.dir)]
    ids = _ids(a.instances, a.players or max(scenlib.players_of(s) for s in scs))
    n = len(ids)
    if a.fake:
        import fakegame
        w, clock, games = fakegame.make(n)
        rn = runner.Runner(games, runner.StateStore(), sleep=clock.sleep, now=clock.now)
        for s in scs:
            w.load_scenario(s)
    elif a.adapter:
        factory = _factory(a.adapter)
        import prepare_save
        rn = runner.Runner([factory(i) for i in ids], runner.StateStore(a.state), instances=ids,
                           prepared=prepare_save.PreparedSaves(a.prepared) if a.prepared else None)
    else:
        print("run needs --fake or --adapter pkg.mod:factory")
        return 2
    results = [rn.run(s, a.force_reload) for s in scs if scenlib.players_of(s) <= n]
    print(runner.summarize(results))
    if a.json:
        Path(a.json).parent.mkdir(parents=True, exist_ok=True)
        Path(a.json).write_text(json.dumps(results, indent=1), encoding="utf-8")
    return 0 if results and all(r["status"] == "pass" for r in results) else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default=str(scenlib.SCEN_DIR), help="folder with scenario files (default: the shipped examples)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list").set_defaults(fn=cmd_list)
    p = sub.add_parser("validate")
    p.add_argument("names", nargs="*")
    p.add_argument("--index", help="JSON file {id: kind} of the ids the game data knows")
    p.add_argument("--adapter", help="pkg.mod:factory; its data_index() is used (the adapter is made for instance 0)")
    p.set_defaults(fn=cmd_validate)
    p = sub.add_parser("plan")
    p.add_argument("name")
    p.add_argument("--prev")
    p.set_defaults(fn=cmd_plan)
    p = sub.add_parser("run")
    p.add_argument("names", nargs="*")
    p.add_argument("--fake", action="store_true")
    p.add_argument("--adapter")
    p.add_argument("--players", type=int, default=0)
    p.add_argument("--instances", default="", help="instance ids handed to the adapter factory, e.g. 2,3 (default 0..players-1)")
    p.add_argument("--prepared", help="prepared-save manifest file (default $SCEN_PREPARED_FILE or ./scen_prepared.json when it exists)")
    p.add_argument("--state", help="file that remembers what is loaded between runs")
    p.add_argument("--json")
    p.add_argument("--force-reload", action="store_true")
    p.set_defaults(fn=cmd_run)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
