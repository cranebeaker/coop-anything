"""Co-op flight record tools. Usage: python -I tools/flight/flight.py <command> ...

  print FILE [--type NetIn,Digest] [--limit N]     pretty-print one .cofr trace
  align FILE FILE [FILE ...]                       line up 2 to 4 machines (clock offsets, lost/duplicated messages)
  diverge FILE FILE [...] [--json] [--check]       first divergence plus causal chain (--check: exit 1 if one is found)
  report FILE FILE [...] -o report.html [--shrink shrunk.json]
  fixture FILE FILE [...] -o fixture.json          build a replay fixture from the traces
  shrink fixture.json (--replay-cmd "exe {fixture}" | --replay-py module:function) -o shrunk.json
  gentest shrunk.json --name NAME [--out DIR]      emit a regression test (JSON fixture + cmake add_test)
  regress DIR (--replay-cmd "exe {fixture}" | --replay-py module:function)   run every generated regression fixture
  schema [FILE]                                    print the default schema, or check a schema file and print it
  from-failure [PATH ...] [options]                in-game failure -> offline repro, shrink, regression case (see from_failure.py)

print, align, diverge, report and fixture take --schema FILE to use that schema instead of the one stored in the traces.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cofr  # noqa: E402
import schema as sch  # noqa: E402
import analyze  # noqa: E402
import shrink  # noqa: E402
import gentest  # noqa: E402
import report  # noqa: E402
import regress  # noqa: E402

DEFAULT_REGRESS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "regress")


def _schema(path):
    if not path:
        return None
    try:
        return sch.load(path)
    except (OSError, sch.SchemaError) as ex:
        raise SystemExit("cannot use schema %s: %s" % (path, ex))


def _load_all(paths, schema_path=None):
    s = _schema(schema_path)
    out = []
    for p in paths:
        try:
            out.append(cofr.load(p, schema=s))
        except (OSError, cofr.CofrError) as ex:
            raise SystemExit("cannot read %s: %s" % (p, ex))
    if len(out) < 2:
        raise SystemExit("need at least 2 traces (one per machine)")
    return out


def _json_div(d, schema):
    if d is None:
        return None
    d = dict(d)
    chain = d.pop("chain")
    d["chain"] = {"window_us": chain["window_us"], "notes": chain["notes"],
                  "records": [{"slot": e["slot"], "aligned_us": e["aligned_us"], "role": e["role"], "name": e["rec"]["name"],
                               "what": cofr.describe(e["rec"], schema)} for e in chain["records"]]}
    return d


def _oracle(a):
    try:
        if a.replay_py:
            return shrink.PyOracle(a.replay_py, a.replay_path)
        if a.replay_cmd:
            return shrink.CommandOracle(a.replay_cmd, tmpdir=a.tmpdir)
    except shrink.OracleError as ex:
        raise SystemExit("cannot set up the replay hook: %s" % ex)
    raise SystemExit("give --replay-cmd or --replay-py")


def main(argv=None):
    argv = sys.argv[1:] if argv is None else list(argv)
    if argv and argv[0] == "from-failure":
        import from_failure  # noqa: E402
        return from_failure.main(argv[1:])
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("print"); p.add_argument("file"); p.add_argument("--type"); p.add_argument("--limit", type=int); p.add_argument("--schema")
    p = sub.add_parser("align"); p.add_argument("files", nargs="+"); p.add_argument("--schema")
    p = sub.add_parser("diverge"); p.add_argument("files", nargs="+"); p.add_argument("--json", action="store_true"); p.add_argument("--check", action="store_true"); p.add_argument("--schema")
    p = sub.add_parser("report"); p.add_argument("files", nargs="+"); p.add_argument("-o", required=True); p.add_argument("--shrink"); p.add_argument("--schema")
    p = sub.add_parser("fixture"); p.add_argument("files", nargs="+"); p.add_argument("-o", required=True); p.add_argument("--schema")
    p = sub.add_parser("shrink"); p.add_argument("fixture"); p.add_argument("-o", required=True)
    p.add_argument("--replay-cmd"); p.add_argument("--replay-py"); p.add_argument("--replay-path"); p.add_argument("--list-key")
    p.add_argument("--max-runs", type=int, default=2000); p.add_argument("--max-seconds", type=float); p.add_argument("--tmpdir")
    p = sub.add_parser("gentest"); p.add_argument("shrunk"); p.add_argument("--name", required=True); p.add_argument("--out", default=DEFAULT_REGRESS)
    p.add_argument("--target", default="replay_tool"); p.add_argument("--flag", default="")
    p = sub.add_parser("regress"); p.add_argument("dir"); p.add_argument("--replay-cmd"); p.add_argument("--replay-py"); p.add_argument("--replay-path"); p.add_argument("--tmpdir")
    p = sub.add_parser("schema"); p.add_argument("file", nargs="?")
    a = ap.parse_args(argv)

    if a.cmd == "print":
        try:
            tr = cofr.load(a.file, schema=_schema(a.schema))
        except (OSError, cofr.CofrError) as ex:
            raise SystemExit("cannot read %s: %s" % (a.file, ex))
        types = {t.strip().lower() for t in a.type.split(",")} if a.type else None
        print(cofr.pretty(tr, types, a.limit))
        return 0
    if a.cmd == "align":
        print(analyze.format_alignment(analyze.align(_load_all(a.files, a.schema))))
        return 0
    if a.cmd == "diverge":
        al = analyze.align(_load_all(a.files, a.schema))
        d = analyze.find_divergence(al)
        if a.json:
            print(json.dumps(_json_div(d, al.schema), indent=1, sort_keys=True, default=str))
        else:
            print(analyze.format_alignment(al))
            print(analyze.format_divergence(al, d))
        return 1 if (a.check and d) else 0
    if a.cmd == "report":
        sh = None
        if a.shrink:
            with open(a.shrink) as f:
                sh = json.load(f)
        html = report.render(_load_all(a.files, a.schema), sh)
        with open(a.o, "w", encoding="utf-8") as f:
            f.write(html)
        print("wrote " + a.o)
        return 0
    if a.cmd == "fixture":
        fx = shrink.fixture_from_traces(_load_all(a.files, a.schema))
        with open(a.o, "w") as f:
            json.dump(fx, f, indent=1)
        print("wrote %s (%d steps, %d skipped)" % (a.o, len(fx["steps"]), fx["source"]["skipped_steps"]))
        return 0
    if a.cmd == "shrink":
        with open(a.fixture) as f:
            fx = json.load(f)
        try:
            out = shrink.shrink_fixture(fx, _oracle(a), a.max_runs, a.max_seconds, a.list_key)
        except shrink.OracleError as ex:
            print("shrink failed: %s" % ex)
            return 2
        with open(a.o, "w") as f:
            json.dump(out, f, indent=1)
        s = out["shrunk"]
        print("shrunk %d -> %d steps in %d replays%s; failure: %s" % (
            s["original_steps"], s["reduced_steps"], s["oracle_runs"], "" if s["complete"] else " (budget hit, may not be minimal)", s["signature"]))
        return 0
    if a.cmd == "gentest":
        with open(a.shrunk) as f:
            fx = json.load(f)
        for path in gentest.generate(fx, a.name, a.out, a.target, a.flag):
            print("wrote " + path)
        return 0
    if a.cmd == "regress":
        rest = [a.dir]
        for flag, val in (("--replay-cmd", a.replay_cmd), ("--replay-py", a.replay_py), ("--replay-path", a.replay_path), ("--tmpdir", a.tmpdir)):
            if val:
                rest += [flag, val]
        return regress.main(rest)
    if a.cmd == "schema":
        s = _schema(a.file) if a.file else sch.default()
        print(s.to_json())
        return 0
    return 2


if __name__ == "__main__":
    sys.exit(main())
