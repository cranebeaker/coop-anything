"""Offline replay of recorded flight traces: the replay program behind the cases that from_failure.py writes.

A "cofr-trace-1" fixture carries the raw records of every machine's .cofr recording:
  {"version":1, "format":"cofr-trace-1", "headers":{"0":"<hex of the 64 byte header>", "1":"..."},
   "steps":[{"op":"rec","slot":0,"hex":"<hex of one whole record>", "ty":"TxnApplied", "at_ms":123.4}, ...],
   "schema":"<optional schema JSON text that overrides the one stored in the records>",
   "expect":{"converges":false}, "source":{...}}
Every step is one record and is independent, so the shrinker (shrink.py) can delete any subset (a step with "pin": true stays).
Replaying a fixture rebuilds the traces, aligns them and classifies what the analyzer finds. The result follows the replay
contract of shrink.py / regress.py: one JSON line on stdout, exit 0 converged, 1 diverged, 2 bad fixture.

Usage: python -I tools/flight/trace_replay.py fixture.json

Classes (what the failure looks like, ordered as the analyzer finds it in time):
  digest-mismatch   a digest differs between machines
  divergence        the same txn has different content, was applied twice, or two machines disagree on an actor's owner
  stuck-sync        a txn was applied on one machine and never on another, and nothing healed that machine afterwards
  desync-marker     a machine wrote its own desync marker
  transient-gap     a txn is missing on a machine but a repair, snapshot or baseline restated its state later: NOT a failure by itself
Which apply flags restate state comes from the schema (restate_mask), so a game with other flags keeps working.
Nothing in this file knows a game.
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cofr as C  # noqa: E402
import schema as SC  # noqa: E402
import analyze as A  # noqa: E402

FORMAT = "cofr-trace-1"
KIND_CLASS = {"digest": "digest-mismatch", "txn-hash": "divergence", "txn-double": "divergence", "ownership-conflict": "divergence",
              "ownership-stale": "divergence", "txn-missing": "stuck-sync", "desync-marker": "desync-marker"}
FAILURE_CLASSES = ("digest-mismatch", "divergence", "stuck-sync", "desync-marker")
RECORD_COUNT_OFFSET = 40      # where the header keeps its record count (FORMAT.md section 2)


class BadFixture(ValueError):
    pass


# ---------------------------------------------------------------- raw records

def split_cofr(data):
    """-> (header_bytes, [one bytes object per whole record], parsed_header). A half-written last record is dropped."""
    data = bytes(data)
    h = C.parse_header(data)
    pos = h["header_size"]
    recs = []
    while len(data) - pos >= C.RECORD_HEADER:
        plen = struct.unpack_from("<H", data, pos + 2)[0]
        if len(data) - pos - C.RECORD_HEADER < plen:
            break
        end = pos + C.RECORD_HEADER + plen
        recs.append(data[pos:end])
        pos = end
    return data[:h["header_size"]], recs, h


def traces_from_fixture(fx):
    if not isinstance(fx, dict) or fx.get("format") != FORMAT or not isinstance(fx.get("steps"), list) or not isinstance(fx.get("headers"), dict):
        raise BadFixture("not a %s fixture" % FORMAT)
    parts = {}
    try:
        schema = SC.from_json(fx["schema"]) if fx.get("schema") else None
        for slot, hx in fx["headers"].items():
            parts[int(slot)] = [bytearray(bytes.fromhex(hx)), []]
        for st in fx["steps"]:
            if st.get("op") == "rec":
                parts[int(st["slot"])][1].append(bytes.fromhex(st["hex"]))
        out = []
        for s, (hdr, recs) in sorted(parts.items()):
            if len(hdr) >= RECORD_COUNT_OFFSET + 8:
                struct.pack_into("<Q", hdr, RECORD_COUNT_OFFSET, len(recs))      # the shrinker removed records: keep the header honest
            out.append(C.loads(bytes(hdr) + b"".join(recs), "slot%d" % s, schema=schema))
        return out
    except (ValueError, KeyError, TypeError, C.CofrError, SC.SchemaError) as ex:
        raise BadFixture("bad %s fixture: %s" % (FORMAT, ex))


# ---------------------------------------------------------------- classification

def _restating_applies(a):
    """slot -> sorted aligned times of applies the schema says restate state (snapshot, repair, baseline): they heal a missing txn."""
    mask = a.schema.restate_mask
    out = {}
    for s in a.slots:
        out[s] = sorted(a.t(s, r) for r in a.traces[s]["records"]
                        if r["type"] == C.T_TXN_APPLIED and not r.get("malformed") and r["applied_flags"] & mask)
    return out


def classify(traces):
    """Align the traces and sort what the analyzer finds into classes. -> dict (JSON friendly):
    {"found": bool, "primary": candidate summary or None, "items": [summary, ...], "counts": {class: n}, "slots": [...]}
    A summary is {"class","kind","key","suspects","at_ms","detail","healed"}."""
    a = A.align(traces)
    heal = _restating_applies(a)
    items = []
    for c in A.find_all(a):
        cls = KIND_CLASS[c["kind"]]
        later = any(t >= c["aligned_us"] for s in c.get("suspects", []) for t in heal.get(s, ()))
        if c["kind"] == "txn-missing" and later:
            cls = "transient-gap"
        items.append({"class": cls, "kind": c["kind"], "key": c["key"], "suspects": list(c.get("suspects", [])),
                      "at_ms": round((c["aligned_us"] - a.t0) / 1000.0, 3), "detail": c["detail"], "healed": later})
    real = [i for i in items if i["class"] in FAILURE_CLASSES]
    counts = {}
    for i in items:
        counts[i["class"]] = counts.get(i["class"], 0) + 1
    # the earliest real finding leads; a machine's own desync marker is only its report that something was wrong, so it leads only when nothing else was found
    own = [i for i in real if i["class"] != "desync-marker"]
    return {"found": bool(real), "primary": (own or real or [None])[0], "items": items, "counts": counts, "slots": list(a.slots)}


def result_of(cl):
    """The replay-contract result for a classification."""
    real = [i for i in cl["items"] if i["class"] in FAILURE_CLASSES]
    p = cl["primary"]
    real = ([p] + [i for i in real if i is not p]) if p else real   # violations[0] is what shrink.signature() compares
    return {"diverged": cl["found"], "violations": ["%s %s" % (i["class"], i["key"]) for i in real[:5]],
            "first": {"category": p["class"], "key": p["key"]} if p else None, "counts": cl["counts"]}


def run(fx):
    traces = traces_from_fixture(fx)
    try:
        return result_of(classify(traces))
    except Exception as ex:   # noqa: BLE001  a shrink candidate can be too thin to align (a machine with no records left): that is "no failure"
        return {"diverged": False, "violations": [], "first": None, "counts": {}, "error": "%s: %s" % (type(ex).__name__, ex)}


def oracle(fx):
    """In-process oracle for shrink.shrink_fixture and regress.run_dir (same result shape as shrink.CommandOracle)."""
    return run(fx)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    try:
        with open(argv[0], encoding="utf-8") as f:
            res = run(json.load(f))
    except (OSError, ValueError, IndexError, BadFixture):
        print(json.dumps({"error": "bad fixture"}))
        return 2
    print(json.dumps(res))
    return 1 if res["diverged"] else 0


if __name__ == "__main__":
    sys.exit(main())
