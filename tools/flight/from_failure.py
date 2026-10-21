"""Turn an in-game sync failure into an offline repro that runs in seconds.

  python -I tools/flight/from_failure.py [PATH ...] [options]

PATH is any of: a failure.json, a FAIL-* directory (the bots runner, tools/bots, writes one per failed step), a run directory
(every failure.json below it), two or more recordings, or nothing (the newest failure.json under ./coop-bots-out). For every failure it
  1. finds the host and client recordings named by failure.json (see "Where the recordings are looked for"),
  2. converts each one to .cofr (a no-op for files that already are .cofr, see "Converters"),
  3. replays them offline through the flight analyzer and says whether the failure reproduces: REPRODUCED (digest mismatch,
     divergence, stuck sync or a machine's own desync marker) or NOT REPRODUCED (nothing in the recordings),
  4. cross-checks that with what the bots saw in the live status (counters, repairs, host and client update numbers),
  5. shrinks the recordings to the few records that still show the same failure (delta debugging, a few seconds),
  6. writes a regression case, tools/flight/regress/<name>.json, that `python -I tools/flight/regress.py tools/flight/regress`
     replays in a second without a game or a replay program.
Exit code: 0 every failure reproduced, 1 at least one did not reproduce in its recordings, 2 recordings missing or unreadable.

The case pins the recorded evidence: it passes ("expect": converges false) while the recordings still show the failure. A recording
cannot change when the mod is fixed, so the proof that a fix works is a fresh run: run this tool on the new failure dir and expect
"not reproduced". Use --expect-converges to write the case the other way round (fails until the analyzer stops seeing the failure).

Where the recordings are looked for. failure.json lists a machine per entry ({"name", "role", "instance", "status", "capture", ...}).
A machine's recording is named by its "flight_dump" entry (a path, or {"path": ...}), or by a "flight", "recording", "cofr" or "trace"
entry, at the top of the machine or inside its "capture" dict (the dict an adapter's capture() returns; any value there whose
file name matches --pattern counts too). A relative path is tried, in this order, under <root>/<instance>/, <root>/, the failure
folder's <machine name>/ and the failure folder itself, then as written. <root> is --instances DIR or $COOP_INSTANCES_ROOT: the folder
that holds one sub folder per game copy. With no recorded path the newest file that matches --pattern (default *.cofr) in the machine's
failure sub folder or game copy, written up to 10 minutes before the failure, is used.

Converters. Recordings that are not .cofr need a converter: a Python function you write for your game,
    convert(path: str, info: dict) -> bytes
where `path` is the recording as found, `info` is {"name": ..., "role": ..., "instance": ...} (any value may be None) and the
return value is the bytes of a version 1 .cofr file (FORMAT.md). Any exception it raises is reported as "cannot convert". Select it with
--converter module:function (module found on --converter-path DIR, or a path to a .py file: file.py:function). The default converter
passes .cofr files through unchanged. The written case keeps the converted records, so it replays without your converter.

Options: --instances DIR, --out DIR (default tools/flight/regress), --name NAME, --no-shrink, --no-write, --max-seconds S (shrink budget,
default 30), --max-runs N, --expect-converges, --pattern GLOB, --converter SPEC, --converter-path DIR, --schema FILE (overrides the schema stored
in the recordings), --json, --report FILE.html.
"""
import argparse
import glob
import hashlib
import importlib
import importlib.util
import json
import os
import sys
import time
from fnmatch import fnmatch

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import cofr as C  # noqa: E402
import schema as SC  # noqa: E402
import analyze as A  # noqa: E402
import shrink as S  # noqa: E402
import gentest as G  # noqa: E402
import report as R  # noqa: E402
import trace_replay as TR  # noqa: E402

DEFAULT_OUT = os.path.join(HERE, "regress")
ENV_ROOT = "COOP_INSTANCES_ROOT"
DEFAULT_PATTERN = "*.cofr"
FAILURE_DIR = "coop-bots-out"      # where tools/bots/run_coop.py writes its runs, relative to where it was started
REF_KEYS = ("flight_dump", "flight", "recording", "cofr", "trace")
ERROR_KEYS = ("flight_dump_error", "flight_error", "recording_error", "capture_error")
GUESS_WINDOW_S = 600           # a recording with no recorded path is guessed from the newest one written this long before the failure
DROP_ORDER = ("NetIn", "NetOut", "Marker", "Ownership", "TxnCaptured", "Digest", "TxnApplied", "Meta")
MARGINS_MS = (1000, 5000, 20000, 60000)
COUNTERS = (("mismatches", "mismatch(es)"), ("repairs", "repair(s)"), ("repaired_effects", "repaired effect(s)"),
            ("bad_messages", "bad message(s)"), ("apply_failures", "failed apply(s)"), ("rebases", "rebase(s)"))


def instances_root(arg=None):
    return arg or os.environ.get(ENV_ROOT) or None


def _scrub(text):
    """Machine paths (the home folder, the working folder) never go into a case that gets shared."""
    text = str(text or "")
    for p in {os.path.expanduser("~"), os.getcwd()}:
        for q in (p, p.replace("\\", "/"), p.replace("/", "\\")):
            if len(q) > 3:
                text = text.replace(q, "<path>")
    return text


# ---------------------------------------------------------------- converters

class ConvertError(Exception):
    pass


def identity_convert(path, info):
    """The default converter: the file already is .cofr."""
    with open(path, "rb") as f:
        return f.read()


def load_converter(spec, path=None):
    """A converter function from "module:function" (module found on `path`) or "file.py:function". No spec: the identity converter."""
    if not spec:
        return identity_convert
    mod, sep, fn = spec.rpartition(":")
    if not sep or not mod or not fn:
        raise ConvertError("a converter is written module:function (or file.py:function)")
    try:
        if mod.lower().endswith(".py"):
            sp = importlib.util.spec_from_file_location("coop_flight_converter", mod)
            module = importlib.util.module_from_spec(sp)
            sp.loader.exec_module(module)
        else:
            if path and os.path.abspath(path) not in sys.path:
                sys.path.insert(0, os.path.abspath(path))
            module = importlib.import_module(mod)
        func = getattr(module, fn)
    except (ImportError, AttributeError, OSError, SyntaxError, ValueError) as ex:
        raise ConvertError("cannot load converter %s: %s" % (spec, ex))
    if not callable(func):
        raise ConvertError("converter %s is not a function" % spec)
    return func


def convert_file(convert, path, info):
    """Run a converter on one recording. -> .cofr bytes. Raises ConvertError (with the reason) when it cannot."""
    try:
        data = convert(path, dict(info))
    except OSError as ex:
        raise ConvertError("cannot read %s: %s" % (os.path.basename(path), ex))
    except Exception as ex:   # noqa: BLE001  a game's converter can fail in any way
        raise ConvertError("cannot convert %s: %s: %s" % (os.path.basename(path), type(ex).__name__, ex))
    if not isinstance(data, (bytes, bytearray)):
        raise ConvertError("cannot convert %s: the converter returned %s, not bytes" % (os.path.basename(path), type(data).__name__))
    return bytes(data)


# ---------------------------------------------------------------- finding failures and recordings

def find_failures(paths):
    """-> list of failure.json paths, newest first. No paths: the single newest one under ./coop-bots-out."""
    found = []
    for p in paths:
        if os.path.isfile(p):
            found.append(p)
        elif os.path.isfile(os.path.join(p, "failure.json")):
            found.append(os.path.join(p, "failure.json"))
        elif os.path.isdir(p):
            for dp, _, fns in os.walk(p):
                if "failure.json" in fns:
                    found.append(os.path.join(dp, "failure.json"))
    if not paths:
        found = glob.glob(os.path.join(os.getcwd(), FAILURE_DIR, "**", "failure.json"), recursive=True)
    found.sort(key=lambda f: (-os.path.getmtime(f), f))
    return found[:1] if not paths else found


def _ref_path(v):
    if isinstance(v, str) and v:
        return v
    if isinstance(v, dict):
        for k in ("path", "file"):
            if isinstance(v.get(k), str) and v[k]:
                return v[k]
    return None


def recording_ref(m, pattern=DEFAULT_PATTERN):
    """The recording a machine entry of failure.json names: (path or None, error text or None)."""
    cap = m.get("capture") if isinstance(m.get("capture"), dict) else {}
    for holder in (m, cap):
        for k in REF_KEYS:
            p = _ref_path(holder.get(k))
            if p:
                return p, None
    for v in cap.values():
        p = _ref_path(v)
        if p and fnmatch(os.path.basename(p.replace("\\", "/")).lower(), pattern.lower()):
            return p, None
    for holder in (m, cap):
        for k in ERROR_KEYS:
            if holder.get(k):
                return None, str(holder[k])
    return None, None


def resolve_dump(rel, m, fail_dir, root):
    """Where a recorded recording path lives on disk, or None. Bots record it relative to the game copy's folder."""
    if not rel:
        return None
    rel = str(rel).replace("\\", "/")
    base = os.path.basename(rel)
    name, instance = m.get("name"), m.get("instance")
    cands = []
    if root and instance is not None:
        cands.append(os.path.join(root, str(instance), rel))
    if root:
        cands.append(os.path.join(root, rel))
    if name:
        cands += [os.path.join(fail_dir, str(name), rel), os.path.join(fail_dir, str(name), base)]
    cands += [os.path.join(fail_dir, rel), os.path.join(fail_dir, base), rel]
    for c in cands:
        if os.path.isfile(c):
            return c
    if root:
        hits = glob.glob(os.path.join(root, "*", os.path.dirname(rel), base))
        if hits:
            return max(hits, key=os.path.getmtime)
    return None


def guess_dump(m, failure_time, root, fail_dir, pattern=DEFAULT_PATTERN):
    """The newest matching file of this machine written shortly before the failure was saved, or None."""
    files = []
    if m.get("name"):
        files += glob.glob(os.path.join(fail_dir, str(m["name"]), pattern))
    if root and m.get("instance") is not None:
        inst = os.path.join(root, str(m["instance"]))
        files += glob.glob(os.path.join(inst, pattern)) + glob.glob(os.path.join(inst, "*", pattern))
    near = [(os.path.getmtime(f), f) for f in files if 0 <= failure_time - os.path.getmtime(f) <= GUESS_WINDOW_S or abs(failure_time - os.path.getmtime(f)) < 5]
    return max(near)[1] if near else None


def collect(failure_json, root, pattern=DEFAULT_PATTERN):
    """Read failure.json and locate each machine's recording. -> dict with the manifest and a 'machines' list of
    {name, role, instance, path, how, error}."""
    fail_dir = os.path.dirname(failure_json)
    with open(failure_json, encoding="utf-8") as f:
        fj = json.load(f)
    t_fail = os.path.getmtime(failure_json)
    machines = []
    for m in fj.get("machines", []):
        info = {"name": m.get("name"), "role": m.get("role"), "instance": m.get("instance"), "path": None, "how": None, "error": None}
        rel, err = recording_ref(m, pattern)
        p = resolve_dump(rel, m, fail_dir, root)
        if p:
            info["path"], info["how"] = p, "recorded"
        else:
            g = guess_dump(m, t_fail, root, fail_dir, pattern)
            if g:
                info["path"], info["how"] = g, "guessed from file time"
            else:
                info["error"] = err or ("recording not found: %s" % rel if rel else "no recording named in failure.json")
        machines.append(info)
    return {"manifest": fj, "machines": machines}


def load_machines(machines, convert=identity_convert, schema=None):
    """Convert and read the located recordings. -> (traces, raw) where raw is [(slot, header_bytes, [record bytes])]. Unreadable ones get info['error']."""
    traces, raw, slots = [], [], set()
    for m in machines:
        if not m["path"]:
            continue
        try:
            data = convert_file(convert, m["path"], {k: m.get(k) for k in ("name", "role", "instance")})
            hdr, recs, h = TR.split_cofr(data)
            tr = C.loads(data, os.path.basename(m["path"]), schema=schema)
        except ConvertError as ex:
            m["error"] = str(ex)
            continue
        except C.CofrError as ex:
            m["error"] = "cannot read %s: %s%s" % (os.path.basename(m["path"]), ex, " (a recording in another format needs --converter module:function)" if "magic" in str(ex) else "")
            continue
        if h["slot"] in slots:
            m["error"] = "slot %d is recorded twice (two files from the same machine?)" % h["slot"]
            continue
        slots.add(h["slot"])
        m["slot"], m["records"], m["dropped"] = h["slot"], len(tr["records"]), h["records_dropped"]
        traces.append(tr)
        raw.append((h["slot"], hdr, recs))
    return traces, raw


# ---------------------------------------------------------------- what the live status said

def _int(v):
    try:
        return int(v or 0)
    except (TypeError, ValueError):
        return 0


def status_evidence(manifest):
    """Pull the sync symptoms out of the bots' status dumps (the status contract of tools/bots: "seq", "synced", "counters"):
    mismatch, repair and error counters, client update numbers behind the host's, a host that is not synced, lost links."""
    ev = {k: 0 for k, _ in COUNTERS}
    ev.update({"lag": [], "unsynced": [], "disconnected": [], "symptoms": []})
    seqs = {}
    host = None
    for m in manifest.get("machines", []):
        st = m.get("status") if isinstance(m.get("status"), dict) else {}
        name = m.get("name")
        counters = st.get("counters") if isinstance(st.get("counters"), dict) else {}
        for k, _ in COUNTERS:
            ev[k] += _int(counters.get(k))
        if st.get("seq") is not None:
            seqs[name] = _int(st.get("seq"))
        if m.get("role") == "host":
            host = name
            if st.get("synced") is False:
                ev["unsynced"].append(name)
        elif st.get("connected") in (0, False):
            ev["disconnected"].append(name)
    if host in seqs:
        for name, seq in seqs.items():
            if name != host and seq < seqs[host]:
                ev["lag"].append("%s at update %d of %d" % (name, seq, seqs[host]))
    for k, label in COUNTERS:
        if ev[k]:
            ev["symptoms"].append("%d %s" % (ev[k], label))
    if ev["lag"]:
        ev["symptoms"].append("sync lag: " + "; ".join(ev["lag"]))
    if ev["unsynced"]:
        ev["symptoms"].append("host reports synced=false")
    if ev["disconnected"]:
        ev["symptoms"].append("not connected: " + ", ".join(ev["disconnected"]))
    return ev


def agreement(ev, cl):
    """One line on whether the recordings explain the live symptoms."""
    classes = {i["class"] for i in cl["items"] if i["class"] in TR.FAILURE_CLASSES}
    if not ev["symptoms"]:
        return "the live status showed no sync symptoms" + ("" if cl["found"] else " and neither do the recordings")
    if not cl["found"]:
        return "live symptoms (%s) are NOT visible in the recordings (ring too short, or digests not recorded)" % "; ".join(ev["symptoms"])
    bits = []
    if ev["mismatches"]:
        bits.append("mismatches in status %s in recordings" % ("and" if classes & {"digest-mismatch", "divergence"} else "but not"))
    if ev["lag"] or ev["unsynced"] or ev["disconnected"]:
        bits.append("sync lag in status %s in recordings" % ("and" if "stuck-sync" in classes else "but not"))
    return "; ".join(bits) or "the recordings show a failure of a different kind than the status counters"


# ---------------------------------------------------------------- fixture and shrink

def build_fixture(raw, traces, source, schema_text=None):
    """One independent step per record, machine by machine in file order. at_ms is the aligned time (reference clock, from the first record).
    Schema records are pinned: the shrink never removes the schema the records are read with."""
    a = A.align(traces)
    by_slot = {t["header"]["slot"]: t for t in traces}
    headers = {}
    steps = []
    for slot, hdr, recs in raw:
        headers[str(slot)] = hdr.hex()
        for r, b in zip(by_slot[slot]["records"], recs):
            st = {"op": "rec", "slot": slot, "ty": r["name"], "at_ms": round((a.t(slot, r) - a.t0) / 1000.0, 3), "hex": b.hex()}
            if r["type"] == C.T_SCHEMA:
                st["pin"] = True
            steps.append(st)
    fx = {"version": 1, "format": TR.FORMAT, "config": {"clients": max(1, len(raw) - 1)}, "headers": headers, "steps": steps, "source": source}
    if schema_text:
        fx["schema"] = schema_text
    return fx


def _with_steps(fx, steps):
    f = dict(fx)
    f["steps"] = steps
    return f


def reduce_fixture(fx, primary, max_runs, max_seconds):
    """Shrink in three cheap-to-expensive stages, each kept only if the SAME failure signature remains:
    a time window around the first failure, whole record types, then delta debugging over the rest."""
    t0 = time.time()
    base = TR.oracle(fx)
    sig = S.signature(base)
    if sig is None:
        raise S.OracleError("the fixture does not reproduce the failure")
    stages = []

    def same(cand):
        r = TR.oracle(cand)
        return bool(r.get("diverged")) and S.signature(r) == sig

    cur = fx
    at = primary["at_ms"]
    for m in MARGINS_MS:
        keep = [s for s in cur["steps"] if abs(s["at_ms"] - at) <= m or s.get("pin") or s["ty"] == "Meta"]
        if len(keep) < len(cur["steps"]) and same(_with_steps(cur, keep)):
            stages.append("window +-%dms: %d -> %d records" % (m, len(cur["steps"]), len(keep)))
            cur = _with_steps(cur, keep)
            break
        if len(keep) >= len(cur["steps"]):
            break
    extra = sorted({s["ty"] for s in cur["steps"]} - set(C.TYPE_NAMES.values()))       # game-defined (and unknown) record kinds
    for ty in list(DROP_ORDER[:-1]) + extra + list(DROP_ORDER[-1:]):
        keep = [s for s in cur["steps"] if s["ty"] != ty]
        if len(keep) < len(cur["steps"]) and same(_with_steps(cur, keep)):
            stages.append("dropped all %s (%d records)" % (ty, len(cur["steps"]) - len(keep)))
            cur = _with_steps(cur, keep)
    left = max(1.0, max_seconds - (time.time() - t0)) if max_seconds else None
    out = S.shrink_fixture(cur, TR.oracle, max_runs=max_runs, max_seconds=left)
    out["shrunk"]["original_steps"] = len(fx["steps"])
    out["shrunk"]["stages"] = stages
    out["shrunk"]["seconds"] = round(time.time() - t0, 2)
    return out


# ---------------------------------------------------------------- one failure

def case_name(run, fail, explicit=None):
    if explicit:
        return G.safe_name(explicit)
    tag = hashlib.sha1(("%s/%s" % (run, fail)).encode()).hexdigest()[:8]
    return "%s_%s" % (G.safe_name("%s_%s" % (run, fail))[:38].rstrip("_"), tag)


def _source(manifest, machines, run, fail):
    return {"run": run, "failure": fail, "playbook": manifest.get("playbook"), "step": manifest.get("step"), "kind": manifest.get("kind"),
            "reason": _scrub(manifest.get("reason"))[:300], "recordings": [{"name": m["name"], "role": m["role"], "slot": m.get("slot"),
                                                                           "file": os.path.basename(m["path"]), "records": m.get("records"),
                                                                           "dropped": m.get("dropped")} for m in machines if m.get("slot") is not None]}


def process(label, run, fail, manifest, machines, opts):
    """Reproduce one failure. -> report dict. machines: located recordings (see collect)."""
    rep = {"label": label, "playbook": manifest.get("playbook"), "step": manifest.get("step"), "kind": manifest.get("kind"), "reason": manifest.get("reason"),
           "machines": [{k: m.get(k) for k in ("name", "role", "instance", "how", "error", "slot", "records", "dropped")} for m in machines]}
    for r, m in zip(rep["machines"], machines):
        r["file"] = os.path.basename(m["path"]) if m.get("path") else None
    ev = status_evidence(manifest)
    rep["status"] = ev
    traces, raw = load_machines(machines, opts.get("convert") or identity_convert, opts.get("schema"))
    for r, m in zip(rep["machines"], machines):
        r.update({k: m.get(k) for k in ("error", "slot", "records", "dropped")})
    if len(traces) < 2:
        rep["verdict"] = "no-recordings"
        rep["message"] = "need recordings from at least 2 machines; got %d (%s)" % (len(traces), "; ".join(m["error"] for m in machines if m.get("error")) or "none located")
        return rep
    t0 = time.time()
    cl = TR.classify(traces)
    rep["replay_seconds"] = round(time.time() - t0, 2)
    rep["classes"] = cl["counts"]
    rep["items"] = cl["items"][:8]
    rep["agreement"] = agreement(ev, cl)
    rep["notes"] = [w for t in traces for w in t["warnings"]] + ["%s: ring dropped %d record(s) before the dump" % (m["name"], m["dropped"]) for m in machines if m.get("dropped")]
    rep["_traces"] = traces
    if not cl["found"]:
        rep["verdict"] = "not-reproduced"
        rep["message"] = "the recordings show no digest mismatch, divergence or stuck sync" + (" (%d transient gap(s) healed by a repair)" % cl["counts"]["transient-gap"] if cl["counts"].get("transient-gap") else "")
        return rep
    rep["verdict"] = "reproduced"
    rep["primary"] = cl["primary"]
    src = _source(manifest, machines, run, fail)
    fx = build_fixture(raw, traces, src, opts.get("schema_text"))
    fx["evidence"] = {"symptoms": [_scrub(x) for x in ev["symptoms"]], "found": cl["primary"]}
    if opts.get("shrink", True):
        try:
            fx = reduce_fixture(fx, cl["primary"], opts.get("max_runs", 3000), opts.get("max_seconds", 30))
            rep["shrunk"] = fx["shrunk"]
        except S.OracleError as ex:
            rep["shrink_error"] = str(ex)
    else:
        fx["shrunk"] = {"signature": S.signature(TR.oracle(fx)), "original_steps": len(fx["steps"]), "reduced_steps": len(fx["steps"]), "complete": False, "stages": []}
    sig = (fx.get("shrunk") or {}).get("signature")
    expect = {"converges": bool(opts.get("expect_converges")), "bug_signature": sig, "meaning": "recorded evidence of the failure"}
    rep["records_in_case"] = len(fx["steps"])
    if opts.get("write", True):
        name = case_name(run, fail, opts.get("name"))
        rep["case"] = G.generate(fx, name, opts.get("out") or DEFAULT_OUT, expect=expect)[0]
    rep["_fixture"] = fx
    return rep


def _run_and_fail(fj):
    fail_dir = os.path.dirname(os.path.abspath(fj))
    fail = os.path.basename(fail_dir)
    parent = os.path.dirname(fail_dir)
    run = os.path.basename(os.path.dirname(parent)) if os.path.basename(parent) else ""
    return run, fail


def from_failure_json(fj, opts):
    root = instances_root(opts.get("instances"))
    c = collect(fj, root, opts.get("pattern") or DEFAULT_PATTERN)
    run, fail = _run_and_fail(fj)
    return process("%s/%s" % (run, fail), run, fail, c["manifest"], c["machines"], opts)


def from_recordings(files, opts):
    machines = [{"name": os.path.basename(f), "role": None, "instance": None, "path": f, "how": "given", "error": None} for f in files]
    manifest = {"playbook": "recordings", "step": os.path.basename(files[0]), "kind": "manual", "reason": "recordings given on the command line", "machines": []}
    fail = os.path.splitext(os.path.basename(files[0]))[0]
    return process(fail, "manual", fail, manifest, machines, opts)


# ---------------------------------------------------------------- output

def format_report(rep):
    out = ["== %s" % rep["label"]]
    if rep.get("step"):
        out.append("  step: %s [%s]   %s" % (rep["step"], rep.get("kind"), (rep.get("reason") or "")[:200]))
    for m in rep["machines"]:
        out.append("  %-8s %s" % (m.get("name") or "?", ("%s (%s, slot %s, %s records)" % (m["file"], m.get("how"), m.get("slot"), m.get("records"))) if m.get("file") and m.get("records") is not None
                                  else (m.get("error") or "no recording")))
    st = rep["status"]
    out.append("  live status: " + ("; ".join(st["symptoms"]) if st["symptoms"] else "no sync symptoms"))
    if rep["verdict"] == "no-recordings":
        out.append("  NO REPRO: " + rep["message"])
        return "\n".join(out)
    out.append("  replayed offline in %.2fs: %s" % (rep["replay_seconds"], ", ".join("%s x%d" % kv for kv in sorted(rep["classes"].items())) or "nothing found"))
    if rep["verdict"] == "reproduced":
        p = rep["primary"]
        out.append("  REPRODUCED: %s (%s) at %.1f ms, suspect slot(s) %s" % (p["class"], p["key"], p["at_ms"], p["suspects"]))
        out.append("    " + p["detail"][:300])
    else:
        out.append("  NOT REPRODUCED: " + rep["message"])
    out.append("  check: " + rep["agreement"])
    for n in rep.get("notes", [])[:3]:
        out.append("  note: " + n)
    sh = rep.get("shrunk")
    if sh:
        out.append("  shrunk %d -> %d records in %d replays, %.1fs%s" % (sh["original_steps"], sh["reduced_steps"], sh["oracle_runs"], sh.get("seconds", 0),
                                                                        "" if sh["complete"] else " (budget hit, may not be minimal)"))
        for s in sh.get("stages", []):
            out.append("    " + s)
    if rep.get("shrink_error"):
        out.append("  shrink skipped: " + rep["shrink_error"])
    if rep.get("case"):
        out.append("  wrote %s" % _shown(rep["case"]))
    return "\n".join(out)


def _shown(p):
    """A path relative to the repo when it is inside it."""
    try:
        r = os.path.relpath(p, ROOT)
        return p if r.startswith("..") else r
    except ValueError:
        return p


def clean(rep):
    return {k: v for k, v in rep.items() if not k.startswith("_")}


def _is_recording(p):
    return not os.path.isdir(p) and not p.lower().endswith(".json")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="*")
    ap.add_argument("--instances")
    ap.add_argument("--out")
    ap.add_argument("--name")
    ap.add_argument("--no-shrink", action="store_true")
    ap.add_argument("--no-write", action="store_true")
    ap.add_argument("--expect-converges", action="store_true")
    ap.add_argument("--max-seconds", type=float, default=30)
    ap.add_argument("--max-runs", type=int, default=3000)
    ap.add_argument("--pattern", default=DEFAULT_PATTERN)
    ap.add_argument("--converter")
    ap.add_argument("--converter-path")
    ap.add_argument("--schema")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--report")
    a = ap.parse_args(argv)
    try:
        convert = load_converter(a.converter, a.converter_path)
        schema, schema_text = None, None
        if a.schema:
            with open(a.schema, encoding="utf-8") as f:
                schema_text = f.read()
            schema = SC.from_json(schema_text)
    except (ConvertError, OSError, SC.SchemaError) as ex:
        print("cannot start: %s" % ex)
        return 2
    opts = {"instances": a.instances, "out": a.out or DEFAULT_OUT, "name": a.name, "shrink": not a.no_shrink, "write": not a.no_write,
            "expect_converges": a.expect_converges, "max_seconds": a.max_seconds, "max_runs": a.max_runs, "pattern": a.pattern,
            "convert": convert, "schema": schema, "schema_text": schema_text}
    reps = []
    recs = [p for p in a.paths if _is_recording(p)]
    if recs:
        if len(recs) != len(a.paths):
            print("give either recordings (two or more) or failure paths, not both")
            return 2
        if len(recs) < 2:
            print("give two or more recordings (one per machine), or a failure.json or a failure folder")
            return 2
        reps.append(from_recordings(recs, opts))
    else:
        fjs = find_failures(a.paths)
        if not fjs:
            print("no failure.json found" + (" under " + ", ".join(a.paths) if a.paths else " under ./" + FAILURE_DIR))
            return 2
        if a.name and len(fjs) > 1:
            opts["name"] = None
        for fj in fjs:
            try:
                reps.append(from_failure_json(fj, opts))
            except (OSError, ValueError) as ex:
                reps.append({"label": fj, "verdict": "no-recordings", "message": "cannot read failure.json: %s" % ex, "machines": [],
                             "status": status_evidence({})})
    if a.json:
        print(json.dumps([clean(r) for r in reps], indent=1, default=str))
    else:
        for r in reps:
            print(format_report(r))
        if len(reps) > 1:
            tally = {}
            for r in reps:
                tally[r["verdict"]] = tally.get(r["verdict"], 0) + 1
            print("\n%d failure(s): %s" % (len(reps), ", ".join("%d %s" % (n, v) for v, n in sorted(tally.items()))))
        if any(r.get("case") for r in reps):
            print("replay the written case(s): python -I tools/flight/regress.py %s" % _shown(opts["out"]))
    if a.report and reps and reps[0].get("_traces"):
        with open(a.report, "w", encoding="utf-8") as f:
            f.write(R.render(reps[0]["_traces"], reps[0].get("_fixture")))
        print("wrote " + a.report)
    if any(r["verdict"] == "no-recordings" for r in reps):
        return 2
    return 1 if any(r["verdict"] == "not-reproduced" for r in reps) else 0


if __name__ == "__main__":
    sys.exit(main())
