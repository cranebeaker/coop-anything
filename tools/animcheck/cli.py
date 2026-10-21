#!/usr/bin/env python3
"""animcheck command line.

  python -I tools/animcheck/cli.py analyze host.jsonl client.jsonl [--profile P.json] [--html out.html] [--json out.json]
        [--avatar "A=host:14>client:FF000A"] [--assert "pose_err_p95<=12" --assert glitches==0]
        [--expect-moving 0x10A2B3:1000-5000] [--suppress rubberband:0x10A2B3:1500-2600]
  python -I tools/animcheck/cli.py collect --connect host=127.0.0.1:9100 --connect client=127.0.0.1:9101 --seconds 10 --out DIR
  python -I tools/animcheck/cli.py demo --out DIR          write two synthetic streams with known defects, then analyze them
  python -I tools/animcheck/cli.py profile P.json          check a profile and print what it contains
  python -I tools/animcheck/cli.py selftest                same as: python -I tools/animcheck/run_tests.py

Exit code 0 = no failed assertions (default assertions: glitches==0, wrong_weapon==0, wrong_facing==0 and
pose_err_p95 <= --pose-tol when there is a pair to compare), 1 = failed, 2 = bad arguments.
Times in --expect-moving and --suppress are in the stream's own clock (rx_ms when every probe has it, else t_ms), in ms.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from animcheck.analyze import analyze  # noqa: E402
from animcheck.counters import check  # noqa: E402
from animcheck.model import load_stream  # noqa: E402
from animcheck.profile import DEFAULT_PROFILE, Profile  # noqa: E402


def _ref(s: str) -> int:
    return int(s, 16)


def _window(s: str):
    m = re.match(r"^(-?\d+(?:\.\d+)?)-(-?\d+(?:\.\d+)?)$", s)
    if not m:
        raise ValueError(f"bad time window {s!r} (want T0-T1 in ms)")
    return float(m.group(1)), float(m.group(2))


def _load_profile(path):
    if not path:
        return DEFAULT_PROFILE
    return Profile.load(path)


def cmd_analyze(a) -> int:
    try:
        profile = _load_profile(a.profile)
    except (OSError, ValueError, json.JSONDecodeError) as e:
        print(f"bad --profile: {e}", file=sys.stderr)
        return 2
    streams = {}
    for p in a.streams:
        name, path = (p.split("=", 1) if "=" in p else (None, p))
        st = load_stream(path, machine=name, profile=profile)
        streams[name or st.machine] = st
    th = profile.thresholds()
    if a.pose_tol is not None:
        th = th.with_(pose_tol_units=a.pose_tol)
    if a.delay_ms is not None:
        th = th.with_(interp_delay_ms=a.delay_ms)
    if a.latency_ms is not None:
        th = th.with_(latency_allow_ms=a.latency_ms)
    avatars = []
    for spec in a.avatar:   # NAME=OWNER_MACHINE:OWNER_REF>OBSERVER_MACHINE:OBSERVER_REF  (refs hex)
        m = re.match(r"^(\w+)=(\w+):(\w+)>(\w+):(\w+)$", spec)
        if not m:
            print(f"bad --avatar {spec!r}", file=sys.stderr)
            return 2
        avatars.append({"name": m.group(1), "owner": m.group(2), "owner_ref": int(m.group(3), 16),
                        "observer": m.group(4), "observer_ref": int(m.group(5), 16)})
    expect = {}
    suppress = []
    try:
        for spec in a.expect_moving:   # REF:T0-T1
            ref, _, win = spec.partition(":")
            expect.setdefault(_ref(ref), []).append(_window(win))
        for spec in a.suppress:        # KIND:REF:T0-T1   (KIND or REF may be *)
            kind, ref, win = spec.split(":", 2)
            t0, t1 = _window(win)
            suppress.append({"kind": None if kind == "*" else kind, "ref": None if ref == "*" else _ref(ref), "t0": t0, "t1": t1})
    except ValueError as e:
        print(f"bad --expect-moving / --suppress: {e}", file=sys.stderr)
        return 2
    rep = analyze(streams, th, owner=a.owner, auto_offset=a.auto_offset, label=a.label, avatars=avatars,
                  expect_moving=expect or None, suppress=suppress, profile=profile)
    spec = {}
    for s in a.assert_:
        m = re.match(r"^([\w.@]+)\s*(<=|>=|==|!=|<|>)\s*(.+)$", s)
        if not m:
            print(f"bad --assert {s!r}", file=sys.stderr)
            return 2
        spec[m.group(1)] = m.group(2) + m.group(3)
    if spec:
        rep["failures"] = check(rep, spec)
        rep["ok"] = not rep["failures"]
    if a.json:
        Path(a.json).write_text(json.dumps(rep, indent=1), encoding="utf-8")
    if a.html:
        from animcheck.report_html import render
        Path(a.html).write_text(render(rep), encoding="utf-8")
    c = rep["counters"]
    print("animcheck:", " ".join(f"{k}={c[k]}" for k in ("samples", "actors", "pose_err_p50", "pose_err_p95", "group_mismatch_pct",
                                                      "wrong_weapon", "wrong_facing", "glitches")))
    for k in sorted(c):
        if (k.startswith("glitch.") or k.startswith("avatar.glitch.")) and c[k]:
            print(f"  {k} = {c[k]}")
    if rep["unknown_groups"]:
        shown = ", ".join(f"{g} (x{n})" for g, n in sorted(rep["unknown_groups"].items(), key=lambda kv: -kv[1])[:8])
        print(f"  note: groups the profile does not know: {shown}")
    for f in rep["failures"]:
        print("  FAIL", f)
    print("PASS" if rep["ok"] else "FAIL")
    return 0 if rep["ok"] else 1


def cmd_collect(a) -> int:
    from animcheck.collect import Collector
    addrs = {}
    for s in a.connect:
        try:
            name, _, hp = s.partition("=")
            host, _, port = hp.rpartition(":")
            addrs[name] = (host, int(port))
        except ValueError:
            print(f"bad --connect {s!r} (want name=host:port)", file=sys.stderr)
            return 2
    try:
        params = json.loads(a.params)
    except json.JSONDecodeError as e:
        print(f"bad --params: {e}", file=sys.stderr)
        return 2
    try:
        with Collector.from_addresses(addrs, out_dir=a.out, method=a.method, stats_method=a.stats_method or None) as c:
            c.run(a.seconds, a.hz, params)
            print("probes:", {m: len(p) for m, p in c.probes.items()}, "errors:", c.errors)
            costs = c.costs()
            if costs:
                print("cost:", json.dumps(costs))
    except OSError as e:
        print(f"cannot connect: {e}", file=sys.stderr)
        return 2
    return 0


def cmd_demo(a) -> int:
    from animcheck import synth
    from animcheck.model import write_probes
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for name, probes in synth.demo_streams().items():
        write_probes(out / f"{name}.probe.jsonl", probes)
    print(f"wrote {out / 'host.probe.jsonl'} and {out / 'client.probe.jsonl'}")
    argv = ["analyze", f"host={out / 'host.probe.jsonl'}", f"client={out / 'client.probe.jsonl'}",
            "--avatar", f"player=host:{synth.HOST_REF:X}>client:00FF000A", "--html", str(out / "report.html"),
            "--json", str(out / "report.json"), "--label", "demo", "--pose-tol", "8"]
    print("running: cli.py " + " ".join(argv[:3]) + " ...")
    main(argv)   # the demo streams contain defects on purpose, so this analysis reports FAIL
    print(f"report: {out / 'report.html'}")
    return 0


def cmd_profile(a) -> int:
    try:
        p = Profile.load(a.path)
    except (OSError, ValueError, json.JSONDecodeError) as e:
        print(f"bad profile: {e}", file=sys.stderr)
        return 2
    print(p.summary())
    p.thresholds()
    return 0


def cmd_selftest(a) -> int:
    from animcheck import run_tests
    return run_tests.main()


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="animcheck")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("analyze", help="check probe streams (JSONL)")
    p.add_argument("streams", nargs="+", help="probe JSONL files, optionally name=path; the first is the owner")
    p.add_argument("--profile", help="game profile JSON (bone names, animation groups); default: generic")
    p.add_argument("--owner")
    p.add_argument("--html")
    p.add_argument("--json")
    p.add_argument("--label", default="")
    p.add_argument("--assert", dest="assert_", action="append", default=[])
    p.add_argument("--pose-tol", type=float, default=None, help="pose error bound in world units (default 12 scaled by the profile)")
    p.add_argument("--delay-ms", type=float, default=None, help="interpolation buffer of the receiver (default 100)")
    p.add_argument("--latency-ms", type=float, default=None, help="extra latency window searched on each side (default 150)")
    p.add_argument("--avatar", action="append", default=[], help="NAME=host:14>client:FF000A, player ref on its machine > puppet ref on the other")
    p.add_argument("--expect-moving", action="append", default=[], help="REF:T0-T1, the actor must be walking then (enables the stuck check)")
    p.add_argument("--suppress", action="append", default=[], help="KIND:REF:T0-T1, ignore matching findings (KIND or REF may be *)")
    p.add_argument("--auto-offset", action="store_true", help="estimate the clock offset between streams that use different clocks")
    p.set_defaults(fn=cmd_analyze)
    p = sub.add_parser("collect", help="poll running game instances over TCP and save JSONL streams")
    p.add_argument("--connect", action="append", required=True, help="name=host:port, e.g. host=127.0.0.1:9100")
    p.add_argument("--method", default="anim.probe")
    p.add_argument("--stats-method", default="anim.stats", help="optional method returning the probe's own cost; '' to skip")
    p.add_argument("--params", default="{}", help="JSON object sent as the request params")
    p.add_argument("--seconds", type=float, default=10)
    p.add_argument("--hz", type=float, default=20)
    p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_collect)
    p = sub.add_parser("demo", help="write synthetic streams with known defects and analyze them")
    p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_demo)
    p = sub.add_parser("profile", help="validate a profile and print a summary")
    p.add_argument("path")
    p.set_defaults(fn=cmd_profile)
    p = sub.add_parser("selftest", help="run the full test command")
    p.set_defaults(fn=cmd_selftest)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    raise SystemExit(main())
