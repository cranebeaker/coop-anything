#!/usr/bin/env python3
"""Run multi-bot co-op playbooks.

Game-free (the fake co-op world; this is what the tests use):
    python -I tools/bots/run_coop.py --list
    python -I tools/bots/run_coop.py --fake --playbook all
    python -I tools/bots/run_coop.py --fake --playbook loot_race --players 3 --bug dup_pickup      # must FAIL
    python -I tools/bots/run_coop.py --fake --playbook loot_race --link bad_wifi                   # lossy link, quiet windows
    python -I tools/bots/run_coop.py --fake --playbook fight --from-step 4 --timescale 3           # resume at step 4, game at 3x

On your game (you start the game instances yourself and join them into one session; this tool never launches a game):
    python -I tools/bots/run_coop.py --adapter my_game:make_adapter --players 2 \\
        --scenario my_scenarios/fight.json --playbook fight

`make_adapter(i)` returns the ActionAdapter of instance i (0 is the host). With `--instances 2,3` the factory gets those instance
numbers instead, the first one is the host (this is how tools/bots/run_parallel.py starts one runner per pair). `--scenario` is a scenario file whose `refs`
block names what the playbook needs; `--refs file.json` gives the same as plain JSON.

After every step the runner checks that all machines converged, that the shared snapshots are equal and that the desync
counters did not rise. A failing step writes FAIL-<n>-<step>/ (failure.json, snapshots, adapter capture) under the output
directory. Exit code: 0 at least one pass and no failure, 1 a playbook failed or nothing passed, 2 usage or setup error.
A SKIP means the adapter lacks an action the playbook needs; a skip is never a pass.

Fast rounds. A failed step prints its number. `--from-step N` runs step N and later, `--only-step N` just step N; the first step
that runs arranges the world it expects (see `ctx.run_step` in coopbots/core.py), so no reload is needed. Only playbooks whose
steps all have an arrange part support this (`--list` marks them). `--timescale X` calls the adapter's set_timescale(X) before the
first step and set_timescale(1) at the end, also after a failure. `--settle-stall S` is how long a wait for sync may see no progress
before it fails with the reason (0 turns the early stop off; the default is max(6, budget / 4) seconds).
"""
import argparse
import importlib
import json
import os
import sys
import time
from typing import Any, Dict, List

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from coopbots import core, fakecoop, links, refs as refs_mod, registry  # noqa: E402


def log(s: str) -> None:
    print(s, flush=True)


def run_fake(names: List[str], players: int, bugs: set, outroot: str, opts: dict, loss: int, ids: List[int]) -> List[core.RunResult]:
    results = []
    for name in names:
        e = registry.REGISTRY[name]
        world, refs = e.fake(len(ids) or players or e.players, bugs, loss_every=loss)
        o = {"fast": True, "ui_s": 0.3, "barrier_s": 0.3, "stage_s": 0.3, "handoff_s": 0.3, "spawn_s": 0.3, "settle_s": 1.0, "settle_poll_s": 0.0}
        o.update(opts)
        machines = fakecoop.make_machines(world)
        for m, i in zip(machines, ids):
            m.instance = i
        results.append(core.run_playbook(registry.make(name), machines, fakecoop.FakeComparer(),
                                         os.path.join(outroot, name), refs, o, log))
    return results


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", action="store_true", help="list playbooks and exit")
    ap.add_argument("--playbook", default="", help="name, comma list, or 'all'")
    ap.add_argument("--players", type=int, default=0, help="number of bots (default: the playbook's own default)")
    ap.add_argument("--fake", action="store_true", help="game-free run on the fake co-op world")
    ap.add_argument("--bug", action="append", default=[], help="(fake) switch on a deliberate bug; the run must then fail")
    ap.add_argument("--adapter", default="", help="pkg.module:factory, factory(i) -> ActionAdapter of instance i")
    ap.add_argument("--scenario", default="", help="scenario JSON whose `refs` block names the playbook's things")
    ap.add_argument("--refs", default="", help="JSON file with the refs, instead of --scenario")
    ap.add_argument("--link", default="", help="link profile: " + ", ".join(links.PROFILES))
    ap.add_argument("--profiles", default="", help="profiles file for repair budgets (default: tools/chaos/profiles.json)")
    ap.add_argument("--no-compare", action="store_true", help="skip the snapshot comparison after each step")
    ap.add_argument("--instances", default="", help="instance numbers for the adapter factory, e.g. 2,3; the first is the host (default 0..N-1)")
    ap.add_argument("--from-step", type=int, default=0, help="run step N and later (the first one arranges the world); resumable playbooks only")
    ap.add_argument("--only-step", type=int, default=0, help="run only step N (arranged first); resumable playbooks only")
    ap.add_argument("--timescale", type=float, default=1.0, help="game speed multiplier set through the adapter (set_timescale), reset at the end")
    ap.add_argument("--settle-stall", type=float, default=None, help="seconds without progress before a sync wait fails early (0 = never)")
    ap.add_argument("--outdir", default="")
    ap.add_argument("--keep-going", action="store_true", help="run the next playbook after a failure")
    a = ap.parse_args(argv)

    if a.list:
        for n, e in registry.REGISTRY.items():
            pb = registry.make(n)
            print("%-28s %dp  %s needs refs: %-40s %s" % (n, e.players, "R" if pb.resumable else "-", ",".join(pb.needs_refs) or "-", pb.summary))
        print("R = steps can be resumed with --from-step / --only-step")
        return 0
    if not a.playbook:
        ap.error("--playbook is required (or --list)")
    names = list(registry.REGISTRY) if a.playbook == "all" else [x for x in a.playbook.split(",") if x]
    unknown = [n for n in names if n not in registry.REGISTRY]
    if unknown:
        print("unknown playbook(s) %s; use --list" % unknown, file=sys.stderr)
        return 2
    if not a.fake and not a.adapter:
        print("give --fake or --adapter pkg.module:factory", file=sys.stderr)
        return 2
    try:
        opts: Dict[str, Any] = links.link_options(a.link, a.profiles or links.default_profiles_path())
    except ValueError as ex:
        print(str(ex), file=sys.stderr)
        return 2
    outroot = a.outdir or os.path.join("coop-bots-out", time.strftime("%Y%m%dT%H%M%S"))
    if a.no_compare:
        opts["compare"] = False
    if a.from_step < 0 or a.only_step < 0 or a.timescale <= 0:
        print("--from-step and --only-step are 1 or more, --timescale is above 0", file=sys.stderr)
        return 2
    if a.from_step and a.only_step:
        print("give --from-step or --only-step, not both", file=sys.stderr)
        return 2
    if a.from_step > 1 or a.only_step:
        not_ok = [n for n in names if not registry.make(n).resumable]
        if not_ok:
            print("%s cannot run from a later step: no arrange part yet. Resumable: %s" % (", ".join(not_ok), ", ".join(sorted(
                n for n in registry.REGISTRY if registry.make(n).resumable))), file=sys.stderr)
            return 2
    opts.update(from_step=a.from_step, only_step=a.only_step, timescale=a.timescale)
    if a.settle_stall is not None:
        opts["settle_stall_s"] = a.settle_stall
    try:
        ids = [int(x) for x in a.instances.split(",") if x.strip() != ""]
    except ValueError:
        print("--instances needs numbers like 2,3", file=sys.stderr)
        return 2
    if ids and a.players and a.players != len(ids):
        print("--players %d does not match --instances %s" % (a.players, a.instances), file=sys.stderr)
        return 2

    os.makedirs(outroot, exist_ok=True)
    results: List[core.RunResult] = []
    if a.fake:
        results = run_fake(names, a.players, set(a.bug), outroot, opts, links.FAKE_LOSS_EVERY.get(a.link or "lan", 0), ids)
    else:
        sys.path.insert(0, os.getcwd())
        mod, _, fn = a.adapter.partition(":")
        factory = getattr(importlib.import_module(mod), fn)
        try:
            if a.scenario:
                refs = refs_mod.refs_from_scenario(a.scenario)
            elif a.refs:
                with open(a.refs, encoding="utf-8") as f:
                    refs = json.load(f)
            else:
                refs = {}
        except (OSError, ValueError, KeyError) as ex:
            print("cannot read the refs: %s" % ex, file=sys.stderr)
            return 2
        for name in names:
            e = registry.REGISTRY[name]
            n = len(ids) or a.players or e.players
            machines = [core.Machine(k, "host" if k == 0 else "client%d" % k, factory(ids[k] if ids else k), ids[k] if ids else k)
                        for k in range(n)]
            r = core.run_playbook(registry.make(name), machines, core.DictComparer(), os.path.join(outroot, name), refs, opts, log)
            results.append(r)
            if r.status == "fail" and not a.keep_going:
                break

    print()
    width = max([len(r.playbook) for r in results] + [8])
    for r in results:
        print("%-4s  %-*s  %d steps  %s" % (r.status.upper(), width, r.playbook, len(r.steps), r.reason))
        bad = next((s for s in r.steps if not s.ok), None)
        if bad is not None and bad.number:
            print("      rerun that step: --playbook %s --from-step %d   (or --only-step %d)" % (r.playbook, bad.number, bad.number)
                  if registry.make(r.playbook).resumable else "      failed at step %d (%s); this playbook runs whole" % (bad.number, bad.name))
    ok = any(r.status == "pass" for r in results) and not any(r.status == "fail" for r in results)
    with open(os.path.join(outroot, "summary.json"), "w", encoding="utf-8") as f:
        json.dump({"ok": ok, "link": a.link or None, "bugs": a.bug, "results": [r.to_json() for r in results]}, f, indent=1, default=str)
    print("summary: %s" % os.path.join(outroot, "summary.json"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
