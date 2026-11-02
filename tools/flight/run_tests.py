"""Tests and mutation twins for the co-op flight record tools. One command from the repo root:

    python -I tools/flight/run_tests.py          # everything; prints ALL OK

  (default)      Python tests, the C++ build and tests (with their mutation twins), the C++ write / Python read
                 cross tests, and the Python mutation twin
  --quick        only the fast Python tests (a few seconds); prints QUICK OK, never ALL OK
  --no-cpp       skip the C++ phase (the cross tests are skipped too); prints PYTHON OK, never ALL OK
  --no-twin      skip the Python mutation twin; prints TESTS OK, never ALL OK
  --only A,B     run just the Python test classes A and B (used by the twin for the failure-to-case tools, handy while working on one tool)

The twin copies the Python sources to a temp folder, breaks one thing at a time and requires the suite to fail.
Environment: COFR_SRC=<dir> tests the Python sources in that folder (the twin uses it), COFR_BUILD_DIR=<dir> is the C++
build folder (default: a folder in the system temp dir, so nothing is written into the repo), COFR_CROSS_DIR=<dir> holds
files written by the C++ tests (set by this script).
"""
import contextlib
import io
import json
import glob
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get("COFR_SRC") or HERE
sys.path.insert(0, SRC)
import cofr as C  # noqa: E402
import schema as S  # noqa: E402
import analyze as A  # noqa: E402
import shrink as SH  # noqa: E402
import gentest as G  # noqa: E402
import report as R  # noqa: E402
import regress as RG  # noqa: E402
import trace_replay as TR  # noqa: E402
import from_failure as FF  # noqa: E402

QUICK = "--quick" in sys.argv
slow = unittest.skipIf(QUICK, "slow: skipped by --quick")
GOLDEN = os.path.join(HERE, "golden")
SCHEMAS = os.path.join(HERE, "schemas")
CPP_DIR = os.path.join(HERE, "cpp")
CROSS = os.environ.get("COFR_CROSS_DIR")
TOY = os.path.join(SRC, "toy_replay.py")
REPLAY = [sys.executable, "-I", TOY, "{fixture}"]


# ---------------------------------------------------------------- synthetic traces

class TB:
    """Builds the trace of one machine from records given in milliseconds."""

    def __init__(self, slot, wall_skew_ms=0, instance=None, reason=1, schema=None, meta=None):
        self.h = {"slot": slot, "role": 0 if slot == 0 else 1, "reason": reason, "session": 9, "epoch": 1, "proto_version": 3,
                  "wall_unix_ms": 1_000_000 + wall_skew_ms, "records_dropped": 0}
        self.schema = schema
        self.recs = []
        m = {"label": instance or ("inst%d" % slot)}
        m.update(meta or {})
        self.add(C.T_META, 0, meta=m)
        if schema is not None:
            self.add(C.T_SCHEMA, 0, schema=schema)

    def add(self, ty, t_ms, **f):
        r = {"type": ty, "mono_us": int(t_ms * 1000), "frame": int(t_ms / 16)}
        r.update(f)
        self.recs.append(r)

    def net(self, out, t_ms, peer, mtype, seq, raw, epoch=1, flags=0):
        self.add(C.T_NET_OUT if out else C.T_NET_IN, t_ms, peer=peer, channel=0 if out else 0xFF, mux=0xC0, msg_type=mtype, decode=0,
                 session=9, epoch=epoch, seq=seq, seq_hi=seq, raw=raw, truncated=bool(flags))

    def txn(self, ty, t_ms, seq, h, flags=0, origin=0):
        """flags are the applied flags of a TxnApplied record (1 snapshot, 2 repair, 4 baseline, 8 catch-up in the default schema)."""
        if ty == C.T_TXN_CAPTURED:
            self.add(ty, t_ms, kind=0, intent_id=0, seq=seq, effect_count=1, hash=h, cause=0, epoch=1)
        else:
            self.add(ty, t_ms, seq=seq, epoch=1, origin=origin, index=0, applied_flags=flags, cause=0, effect_tag=0, hash=h)

    def digest(self, t_ms, cat, seq, hash_, keys=None, shards=None, bucketed=False):
        self.add(C.T_DIGEST, t_ms, category=cat, epoch=1, seq=seq, hash=hash_, item_count=0, shards=shards or [], keys=keys or {}, bucketed=bucketed)

    def own(self, t_ms, op, actor, epoch, frm, to):
        self.add(C.T_OWNERSHIP, t_ms, op=op, actor=actor, epoch=epoch, from_slot=frm, to_slot=to, reason=0, txn_id=0)

    def data(self):
        return C.dumps(self.h, sorted(self.recs, key=lambda r: r["mono_us"]), self.schema)

    def trace(self):
        return C.loads(self.data(), "slot%d" % self.h["slot"])


def good_hash(seq):
    return 0x1000 + seq


def scenario(bad_seq=None, lost=(), digest2="differ", dup_seq=None, clean_extras=False, instance1=None, late_tail=0):
    """3 machines, host slot 0, clients 1 and 2; slot 2's wall clock is 500 ms fast. 5 ms latency each way."""
    tb = [TB(0), TB(1, instance=instance1), TB(2, wall_skew_ms=500)]
    for seq, t in ((1, 100), (2, 200), (3, 300), (4, 400)):
        tb[0].txn(C.T_TXN_CAPTURED, t - 1, seq, good_hash(seq))
        tb[0].txn(C.T_TXN_APPLIED, t, seq, good_hash(seq))
        for c in (1, 2):
            raw = bytes([4, seq, c, 0xAB])
            tb[0].net(True, t, c, 4, seq, raw)
            if (seq, c) in lost:
                continue
            tb[c].net(False, t + 5, 0, 4, seq, raw)
            h = 0xBAD if (bad_seq == seq and c == 1) else good_hash(seq)
            tb[c].txn(C.T_TXN_APPLIED, t + 6, seq, h)
            ack = bytes([5, seq, c])
            tb[c].net(True, t + 7, 0, 5, seq, ack)
            tb[0].net(False, t + 12, c, 5, seq, ack)
            if dup_seq == seq and c == 1:
                tb[c].net(False, t + 9, 0, 4, seq, raw)
    for seq, c in lost:  # retransmit 30 ms later
        raw = bytes([4, seq, c, 0xAB])
        t = seq * 100 + 30
        tb[0].net(True, t, c, 4, seq, raw)
        tb[c].net(False, t + 5, 0, 4, seq, raw)
        tb[c].txn(C.T_TXN_APPLIED, t + 6, seq, good_hash(seq))
        tb[c].net(True, t + 7, 0, 5, seq, bytes([5, seq, c]))
        tb[0].net(False, t + 12, c, 5, seq, bytes([5, seq, c]))
    for t in (250,):
        for s in range(3):
            tb[s].digest(t, 0, 1, 0xAAAA, keys={0x10: 5, 0x20: 6})
    if digest2 == "differ":
        tb[0].digest(700, 0, 2, 0x1111, keys={0x10: 5, 0x20: 6})
        tb[1].digest(700, 0, 2, 0x2222, keys={0x10: 5, 0x20: 7})
        tb[2].digest(700, 0, 2, 0x1111, keys={0x10: 5, 0x20: 6})
    elif digest2 == "same":
        for s in range(3):
            tb[s].digest(700, 0, 2, 0x1111, keys={0x10: 5, 0x20: 6})
    if clean_extras:
        tb[1].txn(C.T_TXN_APPLIED, 215, 2, 0, flags=2)   # a repair restating state, with a different hash: not a divergence
        for s in range(3):
            tb[s].own(500, 0, 5, 3, 255, 1)
            tb[s].own(510, 0, 5, 3, 255, 1)                          # idempotent regrant, same epoch
    if late_tail:
        for s in range(3):
            tb[s].add(C.T_MARKER, late_tail, kind=0, text="tick")
    return [t.trace() for t in tb]


def read_golden(name):
    return C.load(os.path.join(GOLDEN, name))


def read_bytes(path):
    with open(path, "rb") as f:
        return f.read()


# ---------------------------------------------------------------- tests

class FormatTests(unittest.TestCase):
    def test_roundtrip_all_types(self):
        sch = S.Schema({"game": "t", "custom_records": {"40": {"name": "Spawn", "fields": [{"name": "id", "type": "u32"}, {"name": "label", "type": "str16"}]}}})
        recs = [
            {"type": 1, "mono_us": 0, "frame": 0, "meta": {"label": "A B", "build": "7"}},
            {"type": 9, "mono_us": 0, "frame": 0, "schema": sch},
            {"type": 3, "mono_us": 5, "frame": 1, "peer": 2, "channel": 0xFF, "mux": 0xC0, "msg_type": 4, "decode": 0, "session": 300,
             "epoch": 70000, "seq": 1 << 40, "seq_hi": (1 << 40) + 2, "raw_len": 99, "raw": b"\x00\x01\xfe", "truncated": True},
            {"type": 4, "mono_us": 9, "frame": 2, "kind": 1, "intent_id": 129, "seq": 300, "effect_count": 3, "hash": 1 << 63, "cause": 2, "epoch": 4},
            {"type": 5, "mono_us": 10, "frame": 2, "seq": 300, "epoch": 4, "origin": 1, "index": 2, "applied_flags": 9, "cause": 3, "effect_tag": 40,
             "hash": (1 << 64) - 1},
            {"type": 6, "mono_us": 11, "frame": 3, "op": 3, "actor": 0xFF00000000001001, "epoch": 2, "from_slot": 1, "to_slot": 255, "reason": 9, "txn_id": 77},
            {"type": 7, "mono_us": 12, "frame": 3, "category": 2, "epoch": 3, "seq": 8, "hash": 0xFEEDFACE, "item_count": 5,
             "shards": [1, 2, 3], "keys": {1 << 63: 5}, "derived": True, "bucketed": True},
            {"type": 8, "mono_us": 13, "frame": 3, "kind": 4, "text": "desync é"},
            {"type": 40, "mono_us": 14, "frame": 3, "fields": {"id": 7, "label": "orc"}},
            {"type": 99, "mono_us": 14, "frame": 4, "payload": b"\x09\x09"},
        ]
        h = {"slot": 3, "role": 1, "reason": 2, "proto_version": 5, "wall_unix_ms": 1234567890123, "dump_t_us": 99, "records_dropped": 8,
             "session": 4, "epoch": 6}
        tr = C.loads(C.dumps(h, recs, sch))
        self.assertFalse(tr["truncated"])
        self.assertEqual(tr["header"]["reason_name"], "crash")
        self.assertEqual(tr["header"]["role_name"], "client")
        self.assertEqual((tr["header"]["session"], tr["header"]["epoch"], tr["header"]["record_count"]), (4, 6, 10))
        self.assertEqual(tr["header"]["records_dropped"], 8)
        self.assertEqual(tr["header"]["proto_version"], 5)
        self.assertEqual(tr["header"]["instance"], "A B")
        got = tr["records"]
        self.assertEqual(len(got), 10)
        for want, g in zip(recs, got):
            for k, v in want.items():
                if k == "schema":
                    continue
                self.assertEqual(g[k], v, (want["type"], k))
        self.assertEqual(got[5]["ref"], "actor:FF00000000001001")
        self.assertEqual(got[9]["name"], "Unknown99")
        self.assertEqual(got[8]["name"], "Spawn")
        self.assertEqual(got[2]["flags"], 1)
        self.assertEqual(got[6]["flags"], 2 | 8)
        self.assertEqual(tr["schema"].game, "t")
        self.assertIn("seq=300", C.describe(got[3]))
        self.assertIn("peer 2", C.describe(got[2]))
        self.assertIn("[test-step]", C.describe(got[7]))
        self.assertIn("rejectedStale", C.describe(got[5]))
        self.assertIn("label=orc", C.describe(got[8], tr["schema"]))

    def test_datagram_hash_differs_for_different_datagrams(self):
        recs = [{"type": 2, "mono_us": i, "frame": 0, "peer": 1, "raw": bytes([i, 7, 7, 7])} for i in range(3)]
        got = C.loads(C.dumps({"slot": 0}, recs))["records"]
        self.assertEqual(len({r["dhash"] for r in got}), 3)

    def test_hash_helpers_match_known_values(self):
        # FNV-1a 64 of "a" and of the empty string are published constants
        self.assertEqual(C.hash64(b""), 0xCBF29CE484222325)
        self.assertEqual(C.hash64(b"a"), 0xAF63DC4C8601EC8C)
        self.assertEqual(C.bucket_mix(1, 2), C.bucket_mix(1, 2))
        self.assertNotEqual(C.bucket_mix(1, 2), C.bucket_mix(1, 3))
        self.assertNotEqual(C.bucket_mix(1, 2), C.bucket_mix(2, 2))
        a, ha = C.bucketed_digest([(5, 1), (6, 2)])
        b, hb = C.bucketed_digest([(6, 2), (5, 1)])
        self.assertEqual((a, ha), (b, hb))
        self.assertEqual(len(a), 256)
        c, hc = C.bucketed_digest([(5, 1), (6, 3)])
        self.assertEqual([i for i in range(256) if a[i] != c[i]], [6])
        self.assertEqual(len(C.bucketed_digest([(5, 1)], 64)[0]), 64)

    def test_golden_files_from_the_recorder(self):
        names = ("sample_host.cofr", "sample_client.cofr", "desync_host.cofr", "desync_client.cofr")
        trs = [read_golden(n) for n in names]
        self.assertEqual([t["header"]["role_name"] for t in trs], ["host", "client"] * 2)
        self.assertTrue(all(not t["truncated"] and not t["warnings"] for t in trs))
        self.assertEqual({r["name"] for t in trs[:2] for r in t["records"]} >= {"Meta", "Schema", "NetIn", "NetOut", "TxnApplied", "TxnCaptured", "Digest", "Marker", "Ownership"}, True)
        self.assertEqual([t["schema"].game for t in trs], ["example", "example", "toy", "toy"])
        self.assertEqual(trs[0]["meta"]["peer.1"], "1")
        a = A.align(trs[:2])
        self.assertEqual(len(a.lost), 0)
        self.assertEqual(len(a.orphans), 0)
        self.assertGreaterEqual(len(a.messages), 10)
        self.assertIsNone(A.find_divergence(a))     # the clean pair agrees on everything

    def test_golden_files_roundtrip_byte_for_byte(self):
        for n in ("sample_host.cofr", "sample_client.cofr", "desync_host.cofr", "desync_client.cofr"):
            data = read_bytes(os.path.join(GOLDEN, n))
            tr = C.loads(data)
            self.assertEqual(C.dumps(tr["header"], C.to_records(tr), tr["schema"]), data, n)

    def test_python_recomputes_the_digests_the_cpp_recorder_wrote(self):
        # the bucket hashes and the digest hash in the golden files were computed by the C++ recorder from the entries
        # that the detail records list; Python must get the same numbers
        for n in ("sample_host.cofr", "sample_client.cofr", "desync_host.cofr", "desync_client.cofr"):
            tr = read_golden(n)
            digests = [r for r in tr["records"] if r["type"] == C.T_DIGEST]
            main = [r for r in digests if r["bucketed"] and not r["detail"]][-1]
            entries = {}
            for r in digests:
                if r["detail"]:
                    entries.update(r["keys"])
            self.assertEqual(len(entries), main["item_count"], n)
            self.assertEqual(C.bucketed_digest(entries.items()), (main["shards"], main["hash"]), n)

    def test_golden_desync_pair_is_narrowed_to_bucket_key_and_value(self):
        trs = [read_golden("desync_host.cofr"), read_golden("desync_client.cofr")]
        a = A.align(trs)
        d = A.find_divergence(a)
        key = C.hash64(b"stage.q1")     # the same hash the C++ test used to name the key
        self.assertEqual(d["kind"], "digest")
        self.assertEqual(d["diff_keys"], ["0x%016X" % key])
        self.assertEqual(d["key"], "world:0x%016X" % key)             # the toy schema calls category 0 "world"
        nw = d["narrowing"]
        self.assertEqual(nw["buckets"], [key % 256])
        k = nw["keys"][0]
        self.assertEqual((k["bucket"], k["values"], k["texts"]), (key % 256, {0: 10, 1: 99}, {0: "stage.q1 = 10", 1: "stage.q1 = 99"}))
        names = [e["rec"]["name"] for e in d["chain"]["records"]]
        self.assertIn("ScriptWrite", names)                           # a game-defined record in the causal chain
        self.assertIn("TxnApplied", names)
        text = A.format_divergence(a, d)
        self.assertIn("stage.q1 = 99", text)
        self.assertIn("ScriptWrite", text)

    def test_truncated_tail_is_tolerated(self):
        tr = scenario()[0]
        data = C.dumps(tr["header"], [dict(r) for r in tr["records"]])
        for cut in (1, 3, 7):
            t2 = C.loads(data[:-cut])
            self.assertTrue(t2["truncated"])
            self.assertEqual(len(t2["records"]), len(tr["records"]) - 1)
            self.assertTrue(any("middle of a record" in w for w in t2["warnings"]))

    def test_bad_files(self):
        with self.assertRaises(C.CofrError):
            C.loads(b"XXXX" + bytes(80))
        with self.assertRaises(C.CofrError):
            C.loads(b"ABCD" + bytes([1, 0, 64, 0]) + bytes(80))        # another format's magic
        good = C.dumps({"slot": 1}, [])
        bad = bytearray(good)
        bad[4] = 2
        with self.assertRaises(C.CofrError):
            C.loads(bytes(bad))
        with self.assertRaises(C.CofrError):
            C.loads(good[:10])
        with self.assertRaises(C.CofrError):
            C.loads(good[:40])
        self.assertEqual(C.loads(good)["records"], [])
        self.assertEqual(len(good), C.HEADER_SIZE)

    def test_malformed_body_is_kept(self):
        h = {"slot": 1}
        marker = C.dumps(h, [{"type": 8, "mono_us": 5, "frame": 0, "kind": 0, "text": "ok"}])[C.HEADER_SIZE:]
        broken = struct.pack("<BBHIQ", 3, 0, 5, 0, 0) + b"\x00" * 5     # a NetIn whose body is too short for its fields
        tr = C.loads(C.dumps(h, []) + broken + marker)
        self.assertEqual(len(tr["records"]), 2)
        self.assertTrue(tr["records"][0].get("malformed"))
        self.assertEqual(tr["records"][1]["name"], "Marker")
        self.assertTrue(any("malformed" in w for w in tr["warnings"]))

    def test_pretty(self):
        text = C.pretty(scenario()[0], types={"digest"})
        self.assertIn("Digest", text)
        self.assertNotIn("NetIn", text)
        self.assertIn("role=host", text)
        self.assertEqual(C.pretty(scenario()[0], limit=2).count("\n"), 4)


class SchemaTests(unittest.TestCase):
    CUSTOM = {"game": "toy", "digest_categories": {"0": "world"}, "message_types": {"4": "batch"}, "txn_messages": [4],
              "custom_records": {"40": {"name": "Write", "fields": [
                  {"name": "a", "type": "u8"}, {"name": "b", "type": "u16"}, {"name": "c", "type": "u32"}, {"name": "d", "type": "u64"},
                  {"name": "e", "type": "i8"}, {"name": "f", "type": "i16"}, {"name": "g", "type": "i32"}, {"name": "h", "type": "i64"},
                  {"name": "i", "type": "f32"}, {"name": "j", "type": "f64"}, {"name": "k", "type": "str16"}, {"name": "l", "type": "bytes16"},
                  {"name": "m", "type": "bytes"}]}}}

    def test_default_schema_file_matches_the_code(self):
        with open(os.path.join(SCHEMAS, "default.json"), encoding="utf-8") as f:
            self.assertEqual(json.load(f), S.default().to_dict())

    def test_default_schema_matches_the_one_the_recorder_embeds(self):
        tr = read_golden("sample_host.cofr")
        recorded = [r for r in tr["records"] if r["type"] == C.T_SCHEMA][0]
        self.assertEqual(json.loads(recorded["json"]), S.default().to_dict())

    def test_example_game_schema_is_valid(self):
        s = S.load(os.path.join(SCHEMAS, "example_game.json"))
        self.assertTrue(s.custom_records)
        self.assertEqual(S.from_json(s.to_json()).to_dict(), s.to_dict())

    def test_present_keys_replace_missing_keys_keep_the_default(self):
        s = S.Schema({"digest_categories": {"0": "world"}})
        self.assertEqual(s.category(0), "world")
        self.assertEqual(s.category(2), "cat2")                  # the table was replaced, not merged
        self.assertEqual(s.marker_kind(1), "desync")             # not mentioned: default kept
        self.assertEqual(s.own_op(0), "grant")
        self.assertEqual(s.flag_names(2 | 8), ["repair", "catch-up"])
        self.assertTrue(s.is_txn_message(99))                    # txn_messages missing: every message counts
        self.assertFalse(S.Schema({"txn_messages": [4]}).is_txn_message(5))

    def test_bad_schemas_are_refused(self):
        for bad in ({"digest_categories": [1]}, {"digest_categories": {"x": "a"}}, {"ownership_owner_ops": "0"}, {"restate_mask": 300},
                    {"custom_records": {"3": {"name": "x"}}}, {"custom_records": {"40": {"fields": []}}},
                    {"custom_records": {"40": {"name": "x", "fields": [{"name": "a", "type": "u128"}]}}},
                    {"custom_records": {"40": {"name": "x", "fields": [{"name": "a", "type": "u8"}, {"name": "a", "type": "u8"}]}}},
                    {"custom_records": {"40": {"name": "x", "fields": [{"name": "r", "type": "bytes"}, {"name": "a", "type": "u8"}]}}},
                    [1, 2]):
            with self.assertRaises(S.SchemaError, msg=str(bad)):
                S.Schema(bad)
        with self.assertRaises(S.SchemaError):
            S.from_json("{not json")

    def test_every_field_type_round_trips(self):
        s = S.Schema(self.CUSTOM)
        vals = {"a": 255, "b": 65535, "c": 4294967295, "d": (1 << 64) - 1, "e": -128, "f": -32768, "g": -2, "h": -(1 << 63), "i": 0.5, "j": -2.25,
                "k": "héllo", "l": b"\x00\xff", "m": b"tail"}
        body = S.encode_custom(s.custom_records[40], vals)
        self.assertEqual(S.decode_custom(s.custom_records[40], body), vals)
        with self.assertRaises(S.ShortBody):
            S.decode_custom(s.custom_records[40], body[:5])

    def test_custom_records_are_decoded_and_named_through_the_schema(self):
        s = S.Schema(self.CUSTOM)
        body = S.encode_custom(s.custom_records[40], {"a": 1, "k": "x"})
        data = C.dumps({"slot": 0}, [{"type": 40, "mono_us": 1, "frame": 0, "payload": body}], s)
        tr = C.loads(data)                                         # no schema record in the file: the default knows nothing of type 40
        self.assertEqual(tr["records"][0]["name"], "Unknown40")
        tr = C.loads(data, schema=s)                               # an external schema overrides
        self.assertEqual((tr["records"][0]["name"], tr["records"][0]["fields"]["k"]), ("Write", "x"))
        short = C.dumps({"slot": 0}, [{"type": 40, "mono_us": 1, "frame": 0, "payload": b"\x01"}], s)
        tr = C.loads(short, schema=s)
        self.assertTrue(tr["records"][0].get("malformed"))
        self.assertTrue(any("shorter than its schema" in w for w in tr["warnings"]))

    def test_schema_in_the_file_is_used_and_an_unusable_one_falls_back(self):
        tb = TB(0, schema=S.Schema(self.CUSTOM))
        tb.digest(5, 0, 1, 1, shards=[1])
        tr = tb.trace()
        self.assertEqual(tr["schema"].game, "toy")
        self.assertIn("world epoch=1", C.describe(tr["records"][-1], tr["schema"]))
        raw = C.dumps(tb.h, [{"type": 9, "mono_us": 0, "frame": 0, "json": "{not json"}])
        tr = C.loads(raw)
        self.assertEqual(tr["schema"].game, "example")
        self.assertTrue(any("schema record" in w for w in tr["warnings"]))

    def test_names_come_from_the_schema_in_the_analysis(self):
        sch = S.Schema({"digest_categories": {"0": "world"}, "message_types": {"4": "batch"}})
        tb = [TB(0, schema=sch), TB(1, schema=sch)]
        tb[0].digest(10, 0, 3, 1, keys={7: 1})
        tb[1].digest(10, 0, 3, 2, keys={7: 2})
        a = A.align([t.trace() for t in tb])
        d = A.find_divergence(a)
        self.assertEqual(d["key"], "world:0x0000000000000007")
        self.assertIn("world digest", d["detail"])


class AlignTests(unittest.TestCase):
    def test_clock_offset_and_matching(self):
        a = A.align(scenario())
        self.assertEqual(a.slots, [0, 1, 2])
        self.assertEqual(a.ref_slot, 0)
        self.assertAlmostEqual(a.offset_us[2], 500000, delta=1000)
        self.assertLess(abs(a.offset_us[1]), 1000)
        self.assertEqual(len(a.lost), 0)
        self.assertEqual(len(a.orphans), 0)
        # 4 txn batches + 4 acks, per client
        self.assertEqual(len(a.messages), 16)

    def test_lost_and_duplicate(self):
        a = A.align(scenario(lost={(2, 2)}, dup_seq=4))
        self.assertEqual(len(a.lost), 1)
        self.assertEqual(a.lost[0][:2], (0, 2))
        self.assertEqual(len(a.duplicates), 1)
        self.assertEqual(a.duplicates[0][:2], (0, 1))
        self.assertIn("lost: 1", A.format_alignment(a))

    def test_transport_peer_ids_are_mapped_to_slots_by_meta(self):
        m1 = bytes([0xC0, 2, 5, 2, 0, 9, 2, 0, 0, 0, 0, 0])
        m2 = bytes([0xC0, 4, 5, 4, 0, 9, 1, 1, 1, 1, 1, 1])
        h, c = TB(0, meta={"peer.7": "2"}), TB(2)
        h.net(True, 10, 7, 2, 0, m1)            # the host sent to transport peer 7, which is machine slot 2
        h.net(True, 20, 7, 4, 1, m2)
        c.net(False, 15, 0, 2, 0, m1)           # the client sees the host as peer 0 (the same number as its slot)
        c.net(False, 25, 0, 4, 1, m2)
        c.net(True, 30, 0, 5, 1, m2[::-1])
        h.net(False, 35, 7, 5, 1, m2[::-1])
        a = A.align([h.trace(), c.trace()])
        self.assertEqual((len(a.messages), len(a.lost), len(a.orphans)), (3, 0, 0))
        self.assertEqual({(m["from"], m["to"]) for m in a.messages}, {(0, 2), (2, 0)})
        # without the Meta entry the host's peer 7 means slot 7: nothing matches
        h2 = TB(0)
        h2.net(True, 10, 7, 2, 0, m1)
        a = A.align([h2.trace(), c.trace()])
        self.assertEqual(len(a.messages), 0)

    def test_orphan_input(self):
        t0, t1 = TB(0), TB(1)
        t0.add(C.T_MARKER, 1, kind=0, text="x")
        t1.net(False, 50, 0, 4, 99, b"\x04\x63\x00")  # arrived from slot 0, slot 0 never sent it
        a = A.align([t0.trace(), t1.trace()])
        self.assertEqual(len(a.orphans), 1)

    def test_duplicate_slot_and_dropped_notes(self):
        t = scenario()
        t[1]["header"]["slot"] = 2
        t[0]["header"]["records_dropped"] = 5
        a = A.align(t)
        self.assertEqual(len(a.slots), 3)
        self.assertTrue(any("two traces claim slot 2" in x for x in a.anomalies))
        self.assertTrue(any("lost 5 old records" in x for x in a.anomalies))

    def test_four_machines(self):
        tb = [TB(s) for s in range(4)]
        for s in range(4):
            tb[s].txn(C.T_TXN_APPLIED, 100, 1, 7)
        tb[3].txn(C.T_TXN_APPLIED, 200, 2, 9)
        tb[0].txn(C.T_TXN_APPLIED, 200, 2, 8)
        tb[1].txn(C.T_TXN_APPLIED, 200, 2, 8)
        tb[2].txn(C.T_TXN_APPLIED, 200, 2, 8)
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual((d["kind"], d["suspects"], d["slots"]), ("txn-hash", [3], [0, 1, 2, 3]))


class DivergenceTests(unittest.TestCase):
    def test_clean_run_has_no_divergence(self):
        a = A.align(scenario(digest2="same", clean_extras=True, dup_seq=4))
        self.assertIsNone(A.find_divergence(a))
        self.assertIn("No divergence", A.format_divergence(a, None))

    def test_bad_txn_is_first_and_names_the_culprit(self):
        a = A.align(scenario(bad_seq=3, lost={(2, 2)}))
        d = A.find_divergence(a)
        self.assertEqual(d["kind"], "txn-hash")
        self.assertEqual(d["key"], "txn:3[0]")
        self.assertEqual(d["suspects"], [1])
        self.assertEqual(d["slots"], [0, 1, 2])
        direct = [e for e in d["chain"]["records"] if e["role"] == "direct"]
        self.assertTrue({0, 1, 2} <= {e["slot"] for e in direct})
        self.assertTrue(all(e["rec"].get("seq") == 3 for e in direct if e["rec"]["name"].startswith("Txn")))
        self.assertTrue(any("lost in transit" in n for n in d["chain"]["notes"]))
        self.assertTrue(any(c["kind"] == "digest" for c in d["also"]))
        ts = [e["aligned_us"] for e in d["chain"]["records"]]
        self.assertEqual(ts, sorted(ts))
        text = A.format_divergence(a, d)
        self.assertIn("FIRST DIVERGENCE (txn-hash)", text)
        self.assertIn("suspect slot(s): [1]", text)

    def test_digest_divergence_names_the_key_and_window(self):
        a = A.align(scenario())
        d = A.find_divergence(a)
        self.assertEqual(d["kind"], "digest")
        self.assertEqual(d["suspects"], [1])
        self.assertEqual(d["diff_keys"], ["0x0000000000000020"])
        self.assertEqual(d["key"], "story:0x0000000000000020")
        lo, hi = d["chain"]["window_us"]
        agree = max(a.t(s, r) for s in a.slots for r in a.traces[s]["records"] if r["type"] == C.T_DIGEST and r["seq"] == 1)
        self.assertEqual(lo, agree)
        self.assertEqual(hi, d["aligned_us"])
        for e in d["chain"]["records"]:
            self.assertGreaterEqual(e["aligned_us"], lo)
            self.assertLessEqual(e["aligned_us"], hi)

    def test_shard_level_digest(self):
        tb = [TB(0), TB(1)]
        tb[0].digest(10, 1, 3, 1, shards=[5, 6, 7])
        tb[1].digest(10, 1, 3, 2, shards=[5, 9, 7])
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["diff_shards"], [1])
        self.assertEqual(d["key"], "actors:shard:1")
        self.assertEqual(d["narrowing"]["buckets"], [])           # not key buckets: shard 1 is not a bucket of keys

    def test_per_key_detail_records_and_entry_markers(self):
        key = 0xAA650F118B7CB748
        tb = [TB(0), TB(1)]
        for t, h in zip(tb, (11, 22)):
            t.add(C.T_DIGEST, 900, category=4, epoch=1, seq=0, hash=h, item_count=3, shards=[0] * 255 + [h], keys={}, bucketed=True)
        # detail records (flag bit2): the entries arrive split over two records on each machine
        for i, (t, val) in enumerate(zip(tb, (5, 77))):
            t.add(C.T_DIGEST, 901, category=4, epoch=1, seq=0, hash=0, item_count=3, shards=[], keys={0x10: 1, key: val}, detail=True)
            t.add(C.T_DIGEST, 902, category=4, epoch=1, seq=0, hash=0, item_count=3, shards=[], keys={0x20: 2}, detail=True)
            t.add(C.T_MARKER, 903, kind=0, text="entry %016x %d quest_stage intro = %d" % (key, val, val))
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["kind"], "digest")
        self.assertEqual(d["diff_keys"], ["0x%016X" % key])
        self.assertEqual(d["diff_shards"], [255])
        self.assertIn("quest_stage intro = 5", d["detail"])
        self.assertIn("quest_stage intro = 77", d["detail"])
        self.assertEqual(C.loads(tb[0].data())["records"][2]["detail"], True)
        nw = d["narrowing"]
        self.assertEqual(nw["buckets"], [255])
        self.assertEqual([k["key"] for k in nw["keys"]], ["0x%016X" % key])
        k = nw["keys"][0]
        self.assertEqual(k["bucket"], key % 256)
        self.assertEqual(k["values"], {0: 5, 1: 77})
        self.assertIn("= 77", k["texts"][1])
        text = A.format_divergence(A.align([t.trace() for t in tb]), d)
        self.assertIn("narrowing: bucket(s) 255 of 256", text)
        self.assertIn("slot 1: value hash 77", text)
        self.assertIn("NOT in a differing bucket", text)    # the test's bucket 255 does not match key mod 256 (0x48)

    def test_story_bucket_is_narrowed_to_a_key_by_a_later_desync_dump(self):
        key = 0xAA650F118B7CB748
        tb = [TB(0), TB(1)]
        for t, h in zip(tb, (11, 22)):   # a story digest: buckets only, bucket 0x48 differs
            t.add(C.T_DIGEST, 100, category=0, epoch=1, seq=7, hash=h, item_count=3, shards=[0] * 0x48 + [h] + [0] * (255 - 0x48), keys={}, bucketed=True)
        for t, val in zip(tb, (0, 77)):   # later, the desync dump of the real world with key detail (and an extra identical key)
            t.add(C.T_DIGEST, 900, category=4, epoch=1, seq=0, hash=val + 1, item_count=2, shards=[0] * 0x48 + [val] + [0] * (255 - 0x48), keys={}, bucketed=True)
            t.add(C.T_DIGEST, 901, category=4, epoch=1, seq=0, hash=0, item_count=2, shards=[], keys={0x10: 1, 0x21: val + 1, key: val}, detail=True)
            t.add(C.T_MARKER, 903, kind=0, text="entry %016x %d quest_stage intro = %d" % (key, val, val))
        a = A.align([t.trace() for t in tb])
        cands = [c for c in A.find_all(a) if c["kind"] == "digest" and c["category"] == 0]
        self.assertEqual(len(cands), 1)
        nw = cands[0]["narrowing"]
        self.assertEqual(nw["buckets"], [0x48])
        self.assertEqual([k["key"] for k in nw["keys"]], ["0x%016X" % key])
        self.assertTrue(nw["keys"][0]["in_diff_bucket"])
        self.assertTrue(nw["key_source"].startswith("other digest"))   # category 4 (the desync dump) supplied the keys
        self.assertIn("= 77", nw["keys"][0]["texts"][1])

    def test_bucket_count_is_not_fixed_at_256(self):
        entries = [(k, k * 3) for k in range(40)]
        changed = [(k, v + (1 if k == 37 else 0)) for k, v in entries]
        tb = [TB(0), TB(1)]
        for t, ent in zip(tb, (entries, changed)):
            shards, h = C.bucketed_digest(ent, 16)
            t.add(C.T_DIGEST, 10, category=0, epoch=1, seq=1, hash=h, item_count=40, shards=shards, keys={}, bucketed=True)
            t.add(C.T_DIGEST, 11, category=0, epoch=1, seq=1, hash=0, item_count=40, shards=[], keys=dict(ent), detail=True)
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["narrowing"]["buckets"], [37 % 16])
        self.assertEqual(d["diff_keys"], ["0x%016X" % 37])
        self.assertEqual(d["narrowing"]["keys"][0]["bucket"], 37 % 16)
        self.assertTrue(d["narrowing"]["keys"][0]["in_diff_bucket"])
        self.assertEqual(d["narrowing"]["bucket_count"], 16)

    def test_narrowing_without_key_detail_says_so(self):
        tb = [TB(0), TB(1)]
        tb[0].digest(10, 1, 3, 1, shards=[0] * 256, bucketed=True)
        tb[1].digest(10, 1, 3, 2, shards=[0] * 9 + [4] + [0] * 246, bucketed=True)
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["narrowing"]["buckets"], [9])
        self.assertEqual(d["narrowing"]["keys"], [])
        self.assertIn("no per-key detail recorded", A.format_divergence(A.align([t.trace() for t in tb]), d))

    def test_host_commit_is_not_reported_as_a_missing_apply(self):
        tb = [TB(0), TB(1)]
        tb[0].txn(C.T_TXN_CAPTURED, 100, 1, 5)
        tb[1].txn(C.T_TXN_APPLIED, 150, 1, 5)
        tb[0].add(C.T_MARKER, 5_000_000, kind=0, text="late")   # the host's trace goes on for seconds
        a = A.align([t.trace() for t in tb])
        self.assertEqual([c for c in A.find_all(a) if c["kind"] == "txn-missing"], [])

    def test_earliest_wins_over_later(self):
        a = A.align(scenario(bad_seq=2))
        d = A.find_divergence(a)
        self.assertEqual(d["key"], "txn:2[0]")
        a = A.align(scenario(bad_seq=4))
        self.assertEqual(A.find_divergence(a)["key"], "txn:4[0]")  # digest at 700 is later than seq 4 at 400

    def test_ownership_conflict_and_stale(self):
        tb = [TB(0), TB(1), TB(2)]
        tb[0].own(100, 0, 5, 3, 255, 1)
        tb[2].own(110, 0, 5, 3, 255, 2)
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["kind"], "ownership-conflict")
        self.assertEqual(d["suspects"], [1, 2])
        tb = [TB(0), TB(1)]
        tb[1].own(100, 0, 6, 4, 255, 1)
        tb[1].own(120, 1, 6, 3, 1, 0)
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["kind"], "ownership-stale")
        self.assertEqual(d["suspects"], [1])
        # rejected stale records are the system working, not a divergence
        tb = [TB(0), TB(1)]
        tb[1].own(100, 0, 6, 4, 255, 1)
        tb[1].own(120, 3, 6, 3, 1, 0)
        tb[1].own(130, 4, 6, 3, 1, 0)
        tb[1].own(140, 5, 6, 2, 1, 0)
        tb[0].own(150, 5, 6, 1, 0, 2)
        self.assertIn("rejectedNonOwner", C.describe(tb[1].trace()["records"][-1]))
        self.assertIsNone(A.find_divergence(A.align([t.trace() for t in tb])))

    def test_which_ownership_ops_count_comes_from_the_schema(self):
        sch = S.Schema({"ownership_owner_ops": [1]})          # only a transfer sets an owner in this game
        tb = [TB(0, schema=sch), TB(1, schema=sch)]
        tb[0].own(100, 0, 5, 3, 255, 0)
        tb[1].own(110, 0, 5, 3, 255, 1)                       # two grants at one epoch: not an owner op here
        self.assertIsNone(A.find_divergence(A.align([t.trace() for t in tb])))
        tb[0].own(120, 1, 5, 4, 0, 0)
        tb[1].own(130, 1, 5, 4, 0, 1)
        self.assertEqual(A.find_divergence(A.align([t.trace() for t in tb]))["kind"], "ownership-conflict")

    def test_which_applies_restate_state_comes_from_the_schema(self):
        def pair(schema):
            tb = [TB(0, schema=schema), TB(1, schema=schema)]
            tb[0].txn(C.T_TXN_APPLIED, 100, 1, 5)
            tb[1].txn(C.T_TXN_APPLIED, 100, 1, 6, flags=2)    # flag bit 2 is "repair" in the default schema
            return A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertIsNone(pair(None))
        self.assertEqual(pair(S.Schema({"restate_mask": 0}))["kind"], "txn-hash")

    def test_desync_marker_is_reported_with_its_actor_history(self):
        tb = [TB(0), TB(1)]
        tb[0].own(100, 1, 0x100A, 16, 1, 0)
        tb[0].own(200, 1, 0x100A, 22, 0, 1)
        tb[0].own(210, 1, 0x200B, 3, 0, 1)
        tb[1].add(C.T_MARKER, 900, kind=1, text="stale_applied: update at epoch 11 accepted: ref=#100a epoch=11")
        tb[1].add(C.T_MARKER, 910, kind=1, text="stale_applied: again: ref=#100a")
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual((d["kind"], d["suspects"], d["key"]), ("desync-marker", [1], "marker:stale_applied"))
        self.assertEqual(d["total_candidates"], 1)   # the repeat is folded
        direct = [e["rec"]["ref"] for e in d["chain"]["records"] if e["role"] == "direct" and e["rec"]["type"] == C.T_OWNERSHIP]
        self.assertEqual(direct, ["actor:100A", "actor:100A"])
        self.assertIn("slot 1 reported", d["detail"])

    def test_game_defined_record_naming_the_seq_is_part_of_the_chain(self):
        sch = S.Schema({"custom_records": {"40": {"name": "Script", "fields": [{"name": "seq", "type": "u64"}]}}})
        tb = [TB(0, schema=sch), TB(1, schema=sch)]
        tb[0].txn(C.T_TXN_APPLIED, 100, 5, 1)
        tb[1].txn(C.T_TXN_APPLIED, 100, 5, 2)
        tb[1].add(40, 101, fields={"seq": 5})
        tb[1].add(40, 102, fields={"seq": 99})
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        scripts = [(e["role"], e["rec"]["fields"]["seq"]) for e in d["chain"]["records"] if e["rec"]["name"] == "Script"]
        self.assertEqual(sorted(scripts), [("context", 99), ("direct", 5)])

    def test_only_the_schemas_transaction_messages_are_direct_in_the_chain(self):
        def chain(schema):
            tb = [TB(0, schema=schema), TB(1, schema=schema)]
            tb[0].txn(C.T_TXN_APPLIED, 100, 5, 1)
            tb[1].txn(C.T_TXN_APPLIED, 100, 5, 2)
            tb[0].net(True, 90, 1, 4, 5, b"\x04\x05\x01")
            tb[1].net(False, 95, 0, 4, 5, b"\x04\x05\x01")
            tb[0].net(True, 91, 1, 9, 5, b"\x09\x05\x01")
            tb[1].net(False, 96, 0, 9, 5, b"\x09\x05\x01")
            d = A.find_divergence(A.align([t.trace() for t in tb]))
            return sorted({e["rec"]["msg_type"] for e in d["chain"]["records"] if e["role"] == "direct" and e["rec"]["name"].startswith("Net")})
        self.assertEqual(chain(None), [4, 9])
        self.assertEqual(chain(S.Schema({"txn_messages": [4]})), [4])

    def test_double_apply(self):
        tb = [TB(0), TB(1)]
        for t in (100, 110):
            tb[1].txn(C.T_TXN_APPLIED, t, 5, 9)
        tb[0].txn(C.T_TXN_APPLIED, 90, 5, 9)
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["kind"], "txn-double")
        self.assertEqual(d["suspects"], [1])

    def test_missing_txn_needs_a_gap_or_grace(self):
        tb = [TB(0), TB(1)]
        tb[0].txn(C.T_TXN_APPLIED, 100, 5, 9)
        tb[0].txn(C.T_TXN_APPLIED, 200, 6, 9)
        tb[1].txn(C.T_TXN_APPLIED, 210, 6, 9)
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual((d["kind"], d["key"], d["suspects"]), ("txn-missing", "txn:5", [1]))
        # no later seq, and the trace ends within the grace time: just lag, not a bug
        tb = [TB(0), TB(1)]
        tb[0].txn(C.T_TXN_APPLIED, 100, 5, 9)
        tb[1].add(C.T_MARKER, 900, kind=0, text="x")
        self.assertIsNone(A.find_divergence(A.align([t.trace() for t in tb])))
        # same, but the machine kept recording well past the grace time
        tb[1].add(C.T_MARKER, 3500, kind=0, text="x")
        d = A.find_divergence(A.align([t.trace() for t in tb]))
        self.assertEqual(d["kind"], "txn-missing")
        self.assertIn("no apply within", d["detail"])


class ShrinkTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def fixture(self, n=120, with_dup=True):
        steps = [{"op": "deliver", "to": 1 + (i % 2), "from": 0, "hex": "01%02x" % (i % 200)} for i in range(n)]
        steps[n // 7] = {"op": "deliver", "to": 1, "from": 0, "hex": "ee01"}
        steps[n * 5 // 6] = {"op": "deliver", "to": 1, "from": 0, "hex": "ff01"}
        if with_dup:
            steps[n // 3] = {"op": "inject", "kind": "dup", "slot": 2}
        steps.append({"op": "quiesce", "pin": True})
        return {"version": 1, "config": {"seed": 3, "clients": 2}, "steps": steps}

    def test_ddmin_finds_exact_subset(self):
        need = {3, 77, 150}
        calls = []

        def fails(sel):
            calls.append(len(sel))
            return need <= set(sel)
        kept, runs, complete = SH.ddmin(300, fails)
        self.assertEqual(sorted(kept), sorted(need))
        self.assertTrue(complete)
        self.assertLess(runs, 150)

    def test_ddmin_budget_and_trivial(self):
        kept, runs, complete = SH.ddmin(300, lambda sel: {3, 77} <= set(sel), max_runs=5)
        self.assertFalse(complete)
        self.assertEqual(runs, 5)
        self.assertEqual(SH.ddmin(10, lambda sel: True)[0], [])
        self.assertEqual(SH.ddmin(0, lambda sel: True)[0], [])

    @slow
    def test_shrink_toy_to_two_steps_plus_pin(self):
        orc = SH.CommandOracle(REPLAY, tmpdir=self.tmp)
        out = SH.shrink_fixture(self.fixture(n=30), orc)
        ops = [(s["op"], s.get("hex")) for s in out["steps"]]
        self.assertEqual(ops, [("deliver", "ee01"), ("deliver", "ff01"), ("quiesce", None)])
        self.assertEqual(out["shrunk"]["original_steps"], 31)
        self.assertEqual(out["shrunk"]["reduced_steps"], 3)
        self.assertEqual(out["shrunk"]["signature"], "stale echo applied on slot #")
        self.assertTrue(out["shrunk"]["complete"])
        self.assertEqual(out["shrunk"]["violations"], ["stale echo applied on slot 1"])
        self.assertEqual(os.listdir(self.tmp), [])  # temp fixtures cleaned up
        # the fixture also holds the "dup" bug, a different failure; the shrink must keep the original one
        self.assertFalse(any(s.get("op") == "inject" for s in out["steps"]))

    def test_python_function_hook_shrinks_the_same_way(self):
        orc = SH.PyOracle("toy_replay:replay", path=SRC)
        out = SH.shrink_fixture(self.fixture(), orc)
        self.assertEqual([s.get("hex") for s in out["steps"]], ["ee01", "ff01", None])
        self.assertEqual(out["shrunk"]["signature"], "stale echo applied on slot #")
        self.assertGreater(orc.runs, 5)

    def test_python_hook_errors_are_reported(self):
        with self.assertRaises(SH.OracleError):
            SH.PyOracle("no_such_module_xyz:f")
        with self.assertRaises(SH.OracleError):
            SH.PyOracle("toy_replay_without_colon")
        with self.assertRaises(SH.OracleError):
            SH.PyOracle("toy_replay:replay", path=SRC)({"version": 2, "steps": []})        # the hook raises on a bad fixture
        orc = SH.PyOracle("json:dumps")                                                     # returns a string, not a result dict
        with self.assertRaises(SH.OracleError):
            orc({"a": 1})

    def test_pinned_steps_survive(self):
        def orc(fx):
            ops = [(s.get("hex")) for s in fx["steps"]]
            bad = "ee01" in ops and "ff01" in ops
            return {"diverged": bad, "violations": ["x"] if bad else []}
        out = SH.shrink_fixture(self.fixture(with_dup=False), orc)
        self.assertEqual(out["steps"][-1], {"op": "quiesce", "pin": True})
        self.assertEqual(len(out["steps"]), 3)

    def test_a_named_list_can_be_shrunk(self):
        fx = {"events": [{"v": "x%d" % i} for i in range(20)]}

        def orc2(f):
            vals = [e["v"] for e in f["events"]]
            return {"diverged": "x7" in vals and "x9" in vals, "violations": ["broke"]}
        out = SH.shrink_fixture(fx, orc2, list_key="events")
        self.assertEqual([e["v"] for e in out["events"]], ["x7", "x9"])
        self.assertEqual(out["shrunk"]["list_key"], "events")
        with self.assertRaises(SH.OracleError):
            SH.shrink_fixture(fx, orc2, list_key="nothing")

    @slow
    def test_budget_marks_incomplete(self):
        out = SH.shrink_fixture(self.fixture(n=30), SH.CommandOracle(REPLAY, tmpdir=self.tmp), max_runs=4)
        self.assertFalse(out["shrunk"]["complete"])
        self.assertGreater(out["shrunk"]["reduced_steps"], 3)

    @slow
    def test_nothing_to_shrink_and_errors(self):
        fx = self.fixture(with_dup=False)
        fx["steps"] = [s for s in fx["steps"] if s.get("hex") != "ff01"]
        with self.assertRaises(SH.OracleError):
            SH.shrink_fixture(fx, SH.CommandOracle(REPLAY, tmpdir=self.tmp))
        with self.assertRaises(SH.OracleError):
            SH.CommandOracle(REPLAY, tmpdir=self.tmp)({"version": 2, "steps": []})   # replay exits 2
        with self.assertRaises(SH.OracleError):
            SH.CommandOracle([sys.executable, "-I", TOY])                              # no {fixture}
        with self.assertRaises(SH.OracleError):
            SH.CommandOracle(["definitely-not-a-program-xyz", "{fixture}"], tmpdir=self.tmp)({"version": 1, "steps": []})

    @slow
    def test_violation_line_contract_shrinks_via_ops(self):
        fx = self.fixture(n=30)
        sim = {"format": "scenario", "seed": 5, "machines": 3, "ops": fx["steps"]}
        cmd = [sys.executable, "-I", TOY, "--lines", "{fixture}"]
        out = SH.shrink_fixture(sim, SH.CommandOracle(cmd, tmpdir=self.tmp))
        self.assertNotIn("steps", out)
        self.assertEqual([o.get("hex") for o in out["ops"]], ["ee01", "ff01", None])
        self.assertEqual(out["shrunk"]["signature"], "stale_echo_applied")
        self.assertEqual(out["format"], "scenario")
        paths = G.generate(out, "sim case", self.tmp, target="my_fuzzer", flag="--replay ")
        with open(paths[1]) as f:
            cm = f.read()
        self.assertIn("my_fuzzer", cm)
        self.assertIn("--replay", cm)
        self.assertIn("Steps: 3", cm)

    def test_signature_normalises_numbers(self):
        r = {"diverged": True, "violations": ["actor 0xDEADBEEF dup on slot 3 hash 0123456789abcdef"]}
        self.assertEqual(SH.signature(r), "actor # dup on slot # hash #")
        self.assertEqual(SH.signature({"diverged": True, "first": {"category": "story"}}), "diverged:story")
        self.assertIsNone(SH.signature({"diverged": False}))

    def test_fixture_from_traces(self):
        trs = scenario(bad_seq=3)
        # one truncated datagram is skipped and counted
        tb = TB(1)
        tb.net(False, 5, 0, 4, 1, b"\x04\x01\x01", flags=1)
        trs[1]["records"] += [dict(r, idx=999 + i) for i, r in enumerate(C.loads(tb.data())["records"])]
        fx = SH.fixture_from_traces(trs)
        deliver = [s for s in fx["steps"] if s["op"] == "deliver"]
        n_in = sum(1 for t in trs for r in t["records"] if r["type"] == C.T_NET_IN and not r["truncated"])
        self.assertEqual(len(deliver), n_in)
        self.assertEqual(fx["source"]["skipped_steps"], 1)
        self.assertEqual([s for s in fx["steps"] if s["op"] not in ("deliver", "quiesce")], [])
        self.assertEqual(fx["steps"][-1], {"op": "quiesce", "pin": True})
        ts = [s["t"] for s in fx["steps"] if "t" in s]
        self.assertEqual(ts, sorted(ts))
        self.assertEqual(fx["config"]["clients"], 2)
        self.assertEqual(bytes.fromhex(deliver[0]["hex"])[0] in (4, 5), True)


@slow
class GentestTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def shrunk(self):
        fx = ShrinkTests("test_ddmin_budget_and_trivial").fixture(n=20, with_dup=False)
        fx["steps"] = [fx["steps"][0], {"op": "deliver", "to": 1, "from": 0, "hex": "ee01"},
                       {"op": "deliver", "to": 1, "from": 0, "hex": "ff01"}, fx["steps"][-1]]
        fx["shrunk"] = {"signature": "stale echo applied on slot #"}
        return fx

    def test_generate_and_run(self):
        paths = G.generate(self.shrunk(), "Stale Echo!", self.tmp)
        self.assertEqual([os.path.basename(p) for p in paths], ["stale_echo.json", "stale_echo.cmake"])
        with open(paths[0]) as f:
            fx = json.load(f)
        self.assertEqual(fx["expect"], {"converges": True, "bug_signature": "stale echo applied on slot #"})
        with open(paths[1]) as f:
            cm = f.read()
        self.assertIn("add_test(NAME cofr_stale_echo", cm)
        self.assertIn("replay_tool", cm)
        self.assertEqual(G.safe_name("../../x y"), "x_y")
        self.assertEqual(G.safe_name("!!!"), "case")
        # the bug is still present in the toy, so the regression test must FAIL (it guards a bug that is not fixed yet)
        with contextlib.redirect_stdout(io.StringIO()):
            n, bad = RG.run_dir(self.tmp, SH.CommandOracle(REPLAY, tmpdir=self.tmp))
        self.assertEqual((n, bad), (1, 1))

    def test_regress_expectations(self):
        d = self.tmp
        good = {"version": 1, "steps": [{"op": "quiesce"}], "expect": {"converges": True}}
        known_bad = dict(self.shrunk(), expect={"converges": False})
        for name, fx in (("a_good", good), ("b_known_bad", known_bad)):
            with open(os.path.join(d, name + ".json"), "w") as f:
                json.dump(fx, f)
        with contextlib.redirect_stdout(io.StringIO()):
            n, bad = RG.run_dir(d, SH.CommandOracle(REPLAY, tmpdir=d))
            self.assertEqual((n, bad), (2, 0))
            self.assertEqual(RG.main([d, "--replay-cmd", subprocess.list2cmdline(REPLAY), "--tmpdir", d]), 0)
            self.assertEqual(RG.main([d, "--replay-py", "toy_replay:replay", "--replay-path", SRC]), 0)


class ReportTests(unittest.TestCase):
    def test_report_content_and_escaping(self):
        evil = "<script>alert(1)</script>"
        html = R.render(scenario(bad_seq=3, instance1=evil), {"steps": [{"op": "deliver", "hex": "<u>"}],
                                                              "shrunk": {"original_steps": 9, "reduced_steps": 1, "oracle_runs": 4, "signature": "<i>x"}})
        self.assertNotIn("<script>", html)
        self.assertIn("&lt;script&gt;alert(1)&lt;/script&gt;", html)
        self.assertNotIn("<u>", html)
        self.assertNotIn("<i>x", html)
        self.assertIn("First divergence", html)
        self.assertIn("txn:3", html)
        self.assertIn("<svg", html)
        self.assertIn("Minimal reproduction", html)
        self.assertNotIn("http://", html.replace("http://www.w3.org", ""))
        self.assertTrue(html.startswith("<!doctype html>"))

    def test_report_shows_the_narrowing(self):
        key = 0xAA650F118B7CB748
        tb = [TB(0), TB(1)]
        for t, val in zip(tb, (0, 77)):
            t.add(C.T_DIGEST, 900, category=4, epoch=1, seq=0, hash=val + 1, item_count=1, shards=[0] * 0x48 + [val] + [0] * (255 - 0x48), keys={}, bucketed=True)
            t.add(C.T_DIGEST, 901, category=4, epoch=1, seq=0, hash=0, item_count=1, shards=[], keys={key: val}, detail=True)
            t.add(C.T_MARKER, 903, kind=0, text="entry %016x %d quest_stage intro = <b>%d" % (key, val, val))
        html = R.render([t.trace() for t in tb])
        self.assertIn("Narrowing: digest, bucket, key, value", html)
        self.assertIn("0xAA650F118B7CB748", html)
        self.assertIn('<td class="m">quest_stage intro = &lt;b&gt;77</td>', html)
        self.assertNotIn("<b>77", html)

    def test_report_of_the_golden_desync_pair(self):
        html = R.render([read_golden("desync_host.cofr"), read_golden("desync_client.cofr")])
        self.assertIn("stage.q1 = 99", html)
        self.assertIn("ScriptWrite", html)

    def test_clean_report(self):
        html = R.render(scenario(digest2="same"))
        self.assertIn("No divergence found", html)
        self.assertNotIn("Minimal reproduction", html)


@slow
class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_cli(self, *args):
        p = subprocess.run([sys.executable, "-I", os.path.join(SRC, "flight.py")] + list(args), capture_output=True, text=True, errors="replace")
        return p.returncode, p.stdout + p.stderr

    def test_end_to_end(self):
        files = []
        for tr in scenario(bad_seq=3):
            p = os.path.join(self.tmp, "s%d.cofr" % tr["header"]["slot"])
            C.dump(p, tr["header"], [dict(r) for r in tr["records"]])
            files.append(p)
        rc, out = self.run_cli("print", files[0], "--type", "digest")
        self.assertEqual(rc, 0)
        self.assertIn("Digest", out)
        rc, out = self.run_cli("align", *files)
        self.assertIn("slots: [0, 1, 2]", out)
        rc, out = self.run_cli("diverge", *files, "--check")
        self.assertEqual(rc, 1)
        self.assertIn("FIRST DIVERGENCE (txn-hash)", out)
        rc, out = self.run_cli("diverge", *files, "--json")
        self.assertEqual(rc, 0)
        self.assertEqual(json.loads(out)["key"], "txn:3[0]")
        fx = os.path.join(self.tmp, "fx.json")
        rc, out = self.run_cli("fixture", *files, "-o", fx)
        self.assertEqual(rc, 0, out)
        # a hand-made failing fixture for the shrinker (the toy replay does not understand real datagrams)
        toyfx = ShrinkTests("test_ddmin_budget_and_trivial").fixture(n=30, with_dup=False)
        with open(fx, "w") as f:
            json.dump(toyfx, f)
        sh = os.path.join(self.tmp, "sh.json")
        rc, out = self.run_cli("shrink", fx, "--replay-cmd", subprocess.list2cmdline(REPLAY), "-o", sh, "--tmpdir", self.tmp)
        self.assertEqual(rc, 0, out)
        self.assertIn("-> 3 steps", out)
        sh2 = os.path.join(self.tmp, "sh2.json")
        rc, out = self.run_cli("shrink", fx, "--replay-py", "toy_replay:replay", "--replay-path", SRC, "-o", sh2)
        self.assertEqual(rc, 0, out)
        self.assertIn("-> 3 steps", out)
        rd = os.path.join(self.tmp, "reg")
        rc, out = self.run_cli("gentest", sh, "--name", "echo", "--out", rd)
        self.assertEqual(rc, 0, out)
        self.assertTrue(os.path.exists(os.path.join(rd, "echo.json")))
        rc, out = self.run_cli("regress", rd, "--replay-cmd", subprocess.list2cmdline(REPLAY), "--tmpdir", self.tmp)
        self.assertEqual(rc, 1)  # bug still present in the toy
        self.assertIn("FAIL echo.json", out)
        rep = os.path.join(self.tmp, "r.html")
        rc, out = self.run_cli("report", *files, "-o", rep, "--shrink", sh)
        self.assertEqual(rc, 0, out)
        with open(rep, encoding="utf-8") as f:
            self.assertIn("Minimal reproduction", f.read())
        rc, out = self.run_cli("diverge", files[0])
        self.assertNotEqual(rc, 0)
        self.assertIn("at least 2 traces", out)
        rc, out = self.run_cli("print", os.path.join(self.tmp, "nope.cofr"))
        self.assertNotEqual(rc, 0)

    def test_schema_command_and_option(self):
        rc, out = self.run_cli("schema")
        self.assertEqual(rc, 0)
        self.assertEqual(json.loads(out), S.default().to_dict())
        rc, out = self.run_cli("schema", os.path.join(SCHEMAS, "example_game.json"))
        self.assertEqual(rc, 0)
        self.assertIn("ScriptWrite", out)
        bad = os.path.join(self.tmp, "bad.json")
        with open(bad, "w") as f:
            f.write("{nope")
        rc, out = self.run_cli("schema", bad)
        self.assertNotEqual(rc, 0)
        self.assertIn("not valid JSON", out)
        # --schema overrides what the files say
        sch = os.path.join(self.tmp, "world.json")
        with open(sch, "w") as f:
            json.dump({"digest_categories": {"0": "mapstate"}}, f)
        host, cli = os.path.join(GOLDEN, "sample_host.cofr"), os.path.join(GOLDEN, "sample_client.cofr")
        rc, out = self.run_cli("print", host, "--type", "digest", "--schema", sch)
        self.assertIn("mapstate", out)
        rc, out = self.run_cli("print", host, "--type", "digest")
        self.assertIn("story", out)
        rc, out = self.run_cli("diverge", os.path.join(GOLDEN, "desync_host.cofr"), os.path.join(GOLDEN, "desync_client.cofr"), "--schema", sch, "--check")
        self.assertEqual(rc, 1)
        self.assertIn("mapstate", out)
        rc, out = self.run_cli("diverge", host, cli, "--check")
        self.assertEqual(rc, 0)
        self.assertIn("No divergence", out)


# ---------------------------------------------------------------- failure dir -> offline repro (from_failure.py, trace_replay.py)

BS = chr(92)
AP_SNAPSHOT = 1
CUSTOM_SCHEMA = {"game": "toy", "custom_records": {"40": {"name": "ScriptWrite", "fields": [{"name": "seq", "type": "u64"}, {"name": "key", "type": "str16"}]}}}


def pair(skip=(), heal=False, marker=False, digest_bad=False, n=6, bad_seq=None, far=False, schema=None, custom=False):
    """Host and one client. skip: seqs the client never applies. heal: a snapshot apply after the last txn. marker: the host's own desync marker, early.
    far: plenty of unrelated note markers minutes later (the part of a long recording a shrink should cut away). custom: game-defined records."""
    h, c = TB(0, schema=schema), TB(1, schema=schema)
    if marker:
        h.add(C.T_MARKER, 5, kind=1, text="host: a client digest did not match (mismatches=1)")
    for seq in range(1, n + 1):
        t = seq * 100
        raw = bytes([4, seq, 1, 0xAB])
        h.txn(C.T_TXN_CAPTURED, t - 1, seq, good_hash(seq))
        h.txn(C.T_TXN_APPLIED, t, seq, good_hash(seq))
        h.net(True, t, 1, 4, seq, raw)
        if custom:
            h.add(40, t + 1, fields={"seq": seq, "key": "stage.q%d" % seq})
        if seq in skip:
            continue
        c.net(False, t + 5, 0, 4, seq, raw)
        c.txn(C.T_TXN_APPLIED, t + 6, seq, 0xBAD if bad_seq == seq else good_hash(seq))
    if heal:
        c.txn(C.T_TXN_APPLIED, n * 100 + 50, 0, 0x77, flags=AP_SNAPSHOT)
    if far:
        for k in range(40):
            h.add(C.T_MARKER, 5000 + 5000 * k, kind=0, text="note %d" % k)
    h.digest(900, 0, 1, 0xAAAA, keys={0x10: 5})
    c.digest(900, 0, 1, 0xBBBB if digest_bad else 0xAAAA, keys={0x10: 6 if digest_bad else 5})
    return [h, c]


def make_run(root, tbs, run="run-x", fail="FAIL-1-01-the_step", status=None, same_names=False, dump_paths=True, ext=".cofr", encode=None,
             outdir="out", reason="desync counters rose", tag=""):
    """Lay out what the bots runner leaves behind: <root>/instances/<n>/gamefl/<file> and <root>/<outdir>/<run>/enemy_fight/<fail>/failure.json.
    encode turns the recording into another file format (a game's own). -> (failure_dir, instances_dir)."""
    inst = os.path.join(root, "instances")
    fdir = os.path.join(root, outdir, run, "enemy_fight", fail)
    os.makedirs(fdir, exist_ok=True)
    machines = []
    for i, tb in enumerate(tbs):
        d = os.path.join(inst, str(i), "gamefl")
        os.makedirs(d, exist_ok=True)
        fn = ("same" if same_names else "dump%s-%d" % (tag, i)) + ext
        with open(os.path.join(d, fn), "wb") as f:
            f.write(encode(tb.data()) if encode else tb.data())
        m = {"name": "host" if i == 0 else "client%d" % i, "role": "host" if i == 0 else "client", "instance": i, "status": (status or {}).get(i, {})}
        if dump_paths:
            m["flight_dump"] = {"path": "gamefl" + BS + fn}
        else:
            m["flight_dump_error"] = "flight dump failed (no folder configured)"
        machines.append(m)
    with open(os.path.join(fdir, "failure.json"), "w", encoding="utf-8") as f:
        json.dump({"playbook": "enemy_fight", "step": "01-the step", "reason": reason, "kind": "counters", "machines": machines}, f)
    return fdir, inst


def run_ff(*args):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = FF.main(list(args))
    return rc, buf.getvalue()


def read_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


class TraceReplayTests(unittest.TestCase):
    def test_split_roundtrip_and_truncated_tail(self):
        data = pair()[0].data()
        hdr, recs, h = TR.split_cofr(data)
        self.assertEqual(hdr + b"".join(recs), data)
        self.assertEqual(len(recs), len(C.loads(data)["records"]))
        self.assertEqual(h["slot"], 0)
        self.assertGreater(len(recs[0]), 18)
        hdr2, recs2, _ = TR.split_cofr(data + recs[0][:18])      # a half-written record at the end (whole header, cut payload) is dropped
        self.assertEqual(recs2, recs)

    def classify(self, **kw):
        return TR.classify([t.trace() for t in pair(**kw)])

    def test_classes(self):
        cl = self.classify(bad_seq=3)
        self.assertEqual((cl["primary"]["class"], cl["primary"]["key"]), ("divergence", "txn:3[0]"))
        cl = self.classify(digest_bad=True)
        self.assertEqual(cl["primary"]["class"], "digest-mismatch")
        self.assertEqual(cl["primary"]["suspects"], [0, 1])
        cl = self.classify(skip={2})
        self.assertEqual((cl["primary"]["class"], cl["primary"]["key"], cl["primary"]["suspects"]), ("stuck-sync", "txn:2", [1]))
        self.assertFalse(cl["primary"]["healed"])
        self.assertFalse(self.classify()["found"])
        self.assertEqual(self.classify()["counts"], {})

    def test_gap_healed_by_a_snapshot_is_not_a_failure(self):
        cl = self.classify(skip={2}, heal=True)
        self.assertFalse(cl["found"])
        self.assertIsNone(cl["primary"])
        self.assertEqual(cl["counts"], {"transient-gap": 1})
        self.assertTrue(cl["items"][0]["healed"])
        # a snapshot BEFORE the missing txn heals nothing
        h, c = pair(skip={5})
        c.txn(C.T_TXN_APPLIED, 20, 0, 0x77, flags=AP_SNAPSHOT)
        cl = TR.classify([h.trace(), c.trace()])
        self.assertEqual(cl["primary"]["class"], "stuck-sync")

    def test_what_restates_state_comes_from_the_schema(self):
        # in this game only flag 2 (repair) restates state, so a snapshot heals nothing
        sch = S.Schema({"restate_mask": 2})
        cl = TR.classify([t.trace() for t in pair(skip={2}, heal=True, schema=sch)])
        self.assertTrue(cl["found"])
        self.assertEqual((cl["primary"]["class"], cl["primary"]["key"]), ("stuck-sync", "txn:2"))
        cl = TR.classify([t.trace() for t in pair(skip={2}, heal=True, schema=S.Schema({"restate_mask": 1}))])
        self.assertFalse(cl["found"])

    def test_own_marker_leads_only_when_nothing_else_is_found(self):
        cl = self.classify(marker=True)
        self.assertEqual((cl["primary"]["class"], cl["primary"]["key"]), ("desync-marker", "marker:host"))
        cl = self.classify(marker=True, digest_bad=True)       # the marker comes first in time, the digest explains it
        self.assertEqual(cl["primary"]["class"], "digest-mismatch")
        self.assertEqual(cl["counts"]["desync-marker"], 1)
        res = TR.result_of(cl)
        self.assertTrue(res["violations"][0].startswith("digest-mismatch"), res["violations"])
        self.assertEqual(res["first"]["category"], "digest-mismatch")

    def fixture(self, **kw):
        tbs = pair(**kw)
        trs = [t.trace() for t in tbs]
        raw = [(t.h["slot"],) + TR.split_cofr(t.data())[:2] for t in tbs]
        return FF.build_fixture(raw, trs, {}), trs

    def test_fixture_replays_like_the_traces(self):
        fx, trs = self.fixture(skip={2})
        self.assertEqual(fx["format"], "cofr-trace-1")
        self.assertEqual(TR.run(fx), TR.result_of(TR.classify(trs)))
        self.assertTrue(TR.run(fx)["diverged"])
        fx["steps"].append({"op": "quiesce", "pin": True})        # steps that are not records are ignored
        self.assertTrue(TR.run(fx)["diverged"])
        self.assertTrue(all(st["op"] == "rec" for st in fx["steps"][:-1]))
        clean, _ = self.fixture()
        self.assertEqual(TR.run(clean), {"diverged": False, "violations": [], "first": None, "counts": {}})

    def test_the_header_record_count_follows_the_steps(self):
        fx, _ = self.fixture(skip={2})
        full = TR.traces_from_fixture(fx)
        self.assertEqual([t["header"]["record_count"] for t in full], [len(t["records"]) for t in full])
        thin = dict(fx, steps=[s for s in fx["steps"] if s["ty"] in ("TxnApplied", "Meta")])
        got = TR.traces_from_fixture(thin)
        self.assertEqual([t["header"]["record_count"] for t in got], [len(t["records"]) for t in got])
        self.assertLess(got[0]["header"]["record_count"], full[0]["header"]["record_count"])

    def test_a_schema_in_the_fixture_overrides_the_recorded_one(self):
        fx, _ = self.fixture(skip={2}, heal=True)
        self.assertFalse(TR.run(fx)["diverged"])                 # the default schema: the snapshot heals the gap
        fx["schema"] = json.dumps({"restate_mask": 2})
        self.assertTrue(TR.run(fx)["diverged"])
        fx["schema"] = "{nope"
        with self.assertRaises(TR.BadFixture):
            TR.run(fx)

    def test_bad_and_thin_fixtures(self):
        for bad in ({}, {"format": "cofr-trace-1"}, {"format": "toy", "steps": [], "headers": {}}, {"format": "cofr-trace-1", "steps": [{"op": "rec", "slot": 7, "hex": "00"}], "headers": {"0": "00"}}):
            with self.assertRaises(TR.BadFixture):
                TR.run(bad)
        fx, _ = self.fixture(skip={2})
        thin = dict(fx, steps=[s for s in fx["steps"] if s["slot"] == 0])     # a machine with no records left: nothing to compare, no crash
        res = TR.run(thin)
        self.assertFalse(res["diverged"])
        real_align = TR.A.align
        TR.A.align = lambda traces: [][0]         # an analysis that cannot cope with a thin candidate counts as "no failure", not a crash
        try:
            res = TR.run(fx)
        finally:
            TR.A.align = real_align
        self.assertFalse(res["diverged"])
        self.assertIn("IndexError", res["error"])

    @slow
    def test_command_line_contract(self):
        d = tempfile.mkdtemp(prefix="cofr_tr_")
        try:
            fx, _ = self.fixture(skip={2})
            p = os.path.join(d, "a.json")
            with open(p, "w") as f:
                json.dump(fx, f)
            r = subprocess.run([sys.executable, "-I", os.path.join(SRC, "trace_replay.py"), p], capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertTrue(json.loads(r.stdout)["diverged"])
            with open(p, "w") as f:
                f.write("{")
            r = subprocess.run([sys.executable, "-I", os.path.join(SRC, "trace_replay.py"), p], capture_output=True, text=True)
            self.assertEqual(r.returncode, 2)
            clean, _ = self.fixture()
            with open(p, "w") as f:
                json.dump(clean, f)
            r = subprocess.run([sys.executable, "-I", os.path.join(SRC, "trace_replay.py"), p], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0)
        finally:
            shutil.rmtree(d, ignore_errors=True)


GAME_CONVERTER = '''import base64

SEEN = []


def convert(path, info):
    SEEN.append(dict(info))
    with open(path, "rb") as f:
        return base64.b64decode(f.read())


def boom(path, info):
    raise ValueError("boom")


def wrong(path, info):
    return "text"


def gone(path, info):
    open(path + ".missing", "rb")
'''


class FromFailureTests(unittest.TestCase):
    modules = 0

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="cofr_ff_")
        self.cases = os.path.join(self.tmp, "cases")
        self.old_env = os.environ.pop(FF.ENV_ROOT, None)

    def tearDown(self):
        os.environ.pop(FF.ENV_ROOT, None)
        if self.old_env is not None:
            os.environ[FF.ENV_ROOT] = self.old_env
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_on(self, fdir, inst, *extra):
        return run_ff(fdir, "--instances", inst, "--out", self.cases, *extra)

    def case(self, i=0):
        return read_json(os.path.join(self.cases, sorted(os.listdir(self.cases))[i]))

    def converter_module(self):
        """Write the sample game converter into its own folder under a fresh module name. -> (module name, folder)."""
        FromFailureTests.modules += 1
        name = "gameconv_%d_%d" % (os.getpid(), FromFailureTests.modules)
        d = os.path.join(self.tmp, "conv")
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, name + ".py"), "w") as f:
            f.write(GAME_CONVERTER)
        return name, d

    def test_end_to_end_reproduce_shrink_and_regress(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}))
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 0, out)
        self.assertIn("REPRODUCED: stuck-sync (txn:2)", out)
        self.assertIn("replay the written case(s)", out)
        cases = os.listdir(self.cases)
        self.assertEqual(len(cases), 1)         # one json, no cmake: trace cases replay in process
        self.assertTrue(cases[0].endswith(".json"))
        with open(os.path.join(self.cases, cases[0])) as f:
            text = f.read()
        fx = json.loads(text)
        self.assertEqual(fx["format"], "cofr-trace-1")
        self.assertEqual(fx["expect"]["converges"], False)
        self.assertEqual(fx["expect"]["bug_signature"], "stuck-sync txn:#")
        self.assertLessEqual(len(fx["steps"]), 3)
        self.assertGreater(fx["shrunk"]["original_steps"], 30)
        self.assertEqual(fx["source"]["playbook"], "enemy_fight")
        self.assertEqual((fx["source"]["run"], fx["source"]["failure"]), ("run-x", "FAIL-1-01-the_step"))
        self.assertEqual({r["file"] for r in fx["source"]["recordings"]}, {"dump-0.cofr", "dump-1.cofr"})
        self.assertNotIn(self.tmp.replace(BS, "/"), text.replace(BS + BS, "/").replace(BS, "/"))     # no machine paths in a case that gets shared
        self.assertNotIn("gamefl", text)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(RG.run_dir(self.cases, None), (1, 0))      # the regression runner picks it up with no replay command
            self.assertEqual(RG.main([self.cases]), 0)

    def test_a_fixed_run_does_not_reproduce(self):
        fdir, inst = make_run(self.tmp, pair())
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 1, out)
        self.assertIn("NOT REPRODUCED", out)
        self.assertFalse(os.path.isdir(self.cases))
        # healed by a snapshot: also not a failure
        fdir, inst = make_run(os.path.join(self.tmp, "b"), pair(skip={2}, heal=True))
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 1, out)
        self.assertIn("transient gap", out)

    def test_expectation_flag_and_regress_flip(self):
        fdir, inst = make_run(self.tmp, pair(digest_bad=True))
        self.assertEqual(self.run_on(fdir, inst, "--expect-converges")[0], 0)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(RG.run_dir(self.cases, None), (1, 1))      # the recordings still show the failure, so a "converges" case fails
            shutil.rmtree(self.cases)
            self.assertEqual(self.run_on(fdir, inst)[0], 0)
            self.assertEqual(RG.run_dir(self.cases, None), (1, 0))
            p = os.path.join(self.cases, os.listdir(self.cases)[0])
            fx = read_json(p)
            fx["steps"] = [s for s in fx["steps"] if s["slot"] == 0]    # a case that stops reproducing is reported
            with open(p, "w") as f:
                json.dump(fx, f)
            self.assertEqual(RG.run_dir(self.cases, None), (1, 1))

    def test_shrink_keeps_the_same_failure(self):
        # a marker alone would also "fail", with fewer records; the shrink must keep the stuck sync it started from
        fdir, inst = make_run(self.tmp, pair(skip={2}, marker=True, far=True))
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 0, out)
        fx = self.case()
        self.assertEqual(fx["shrunk"]["signature"], "stuck-sync txn:#")
        self.assertEqual(len(fx["steps"]), 2)
        self.assertEqual({s["ty"] for s in fx["steps"]}, {"TxnApplied"})
        self.assertTrue(any(s.startswith("window") for s in fx["shrunk"]["stages"]), fx["shrunk"]["stages"])
        self.assertTrue(any(s.startswith("dropped all NetIn") for s in fx["shrunk"]["stages"]), fx["shrunk"]["stages"])
        self.assertTrue(fx["shrunk"]["complete"])

    def test_a_stage_that_changes_the_failure_is_refused(self):
        fx, trs = TraceReplayTests("test_classes").fixture(skip={2}, digest_bad=True)
        primary = TR.classify(trs)["primary"]
        self.assertEqual(primary["class"], "stuck-sync")        # earlier than the digest mismatch
        old = FF.DROP_ORDER
        FF.DROP_ORDER = ("TxnApplied",)       # dropping them would leave only the digest mismatch: another failure
        try:
            out = FF.reduce_fixture(fx, primary, 500, 10)
        finally:
            FF.DROP_ORDER = old
        self.assertEqual(out["shrunk"]["signature"], "stuck-sync txn:#")
        self.assertEqual({s["ty"] for s in out["steps"]}, {"TxnApplied"})
        self.assertFalse(any(s.startswith("dropped") for s in out["shrunk"]["stages"]))

    def test_the_schema_records_are_never_shrunk_away(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}, schema=S.Schema(CUSTOM_SCHEMA)))
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 0, out)
        fx = self.case()
        self.assertEqual({s["ty"] for s in fx["steps"]}, {"TxnApplied", "Schema"})
        self.assertTrue(all(s.get("pin") for s in fx["steps"] if s["ty"] == "Schema"))
        self.assertEqual(len([s for s in fx["steps"] if s["ty"] == "Schema"]), 2)
        self.assertEqual(TR.traces_from_fixture(fx)[0]["schema"].game, "toy")

    def test_the_time_window_keeps_the_schema_records(self):
        fdir, inst = make_run(self.tmp, pair(skip={80}, n=90, schema=S.Schema(CUSTOM_SCHEMA)))       # the failure is 8 s in, the schema records are at 0
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 0, out)
        fx = self.case()
        self.assertTrue(any(s.startswith("window +-1000ms") for s in fx["shrunk"]["stages"]), fx["shrunk"]["stages"])
        self.assertEqual(len([s for s in fx["steps"] if s["ty"] == "Schema"]), 2)
        self.assertEqual(TR.traces_from_fixture(fx)[0]["schema"].game, "toy")

    def test_ring_drops_are_reported(self):
        tbs = pair(skip={2})
        tbs[0].h["records_dropped"] = 7
        fdir, inst = make_run(self.tmp, tbs)
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 0, out)
        self.assertIn("note: host: ring dropped 7 record(s) before the dump", out)

    def test_where_a_named_recording_is_looked_for(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}))
        fj = os.path.join(fdir, "failure.json")
        manifest = read_json(fj)
        full = [os.path.join(inst, str(i), "gamefl", "dump-%d.cofr" % i) for i in (0, 1)]

        def found(paths, instance, root):
            for m, p in zip(manifest["machines"], paths):
                m["flight_dump"] = {"path": p}
                m["instance"] = instance if instance is not None else m["instance"]
            with open(fj, "w") as f:
                json.dump(manifest, f)
            return [m["path"] for m in FF.collect(fj, root)["machines"]]

        self.assertEqual([os.path.realpath(p) for p in found(full, None, None)], [os.path.realpath(p) for p in full])       # an absolute path
        for m in manifest["machines"]:
            m["instance"] = None
        self.assertEqual(found(["0/gamefl/dump-0.cofr", "1/gamefl/dump-1.cofr"], None, inst),
                         [os.path.join(inst, "0/gamefl/dump-0.cofr"), os.path.join(inst, "1/gamefl/dump-1.cofr")])       # relative to the root
        got = found(["gamefl/dump-0.cofr", "gamefl/dump-1.cofr"], None, inst)       # no instance given: the root's sub folders are searched
        self.assertEqual([os.path.realpath(p) for p in got], [os.path.realpath(p) for p in full])
        old = os.getcwd()
        os.chdir(self.tmp)
        try:
            self.assertEqual(found(["instances/0/gamefl/dump-0.cofr", "instances/1/gamefl/dump-1.cofr"], None, None),
                             ["instances/0/gamefl/dump-0.cofr", "instances/1/gamefl/dump-1.cofr"])      # as written, from the working folder
        finally:
            os.chdir(old)

    def test_game_defined_record_kinds_are_dropped_as_a_stage(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}, schema=S.Schema(CUSTOM_SCHEMA), custom=True))
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 0, out)
        fx = self.case()
        self.assertTrue(any(s.startswith("dropped all ScriptWrite") for s in fx["shrunk"]["stages"]), fx["shrunk"]["stages"])
        self.assertNotIn("ScriptWrite", {s["ty"] for s in fx["steps"]})

    def test_the_schema_option_is_kept_in_the_case(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}, heal=True))       # healed under the default schema
        sp = os.path.join(self.tmp, "game.json")
        with open(sp, "w") as f:
            json.dump({"restate_mask": 2}, f)
        self.assertEqual(self.run_on(fdir, inst, "--no-write")[0], 1)
        rc, out = self.run_on(fdir, inst, "--schema", sp)
        self.assertEqual(rc, 0, out)
        self.assertEqual(json.loads(self.case()["schema"]), {"restate_mask": 2})
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(RG.run_dir(self.cases, None), (1, 0))       # the case replays with that schema, not the default one
        rc, out = run_ff(fdir, "--instances", inst, "--schema", os.path.join(self.tmp, "none.json"))
        self.assertEqual(rc, 2)
        self.assertIn("cannot start", out)

    def test_no_shrink_and_no_write(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}))
        rc, out = self.run_on(fdir, inst, "--no-shrink")
        self.assertEqual(rc, 0, out)
        fx = self.case()
        self.assertEqual(len(fx["steps"]), fx["shrunk"]["original_steps"])
        self.assertGreater(len(fx["steps"]), 30)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(RG.run_dir(self.cases, None), (1, 0))
        shutil.rmtree(self.cases)
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 0, out)
        self.assertFalse(os.path.isdir(self.cases))
        self.assertNotIn("wrote ", out)

    def test_the_case_name_option(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}))
        rc, out = self.run_on(fdir, inst, "--name", "Lost Update!")
        self.assertEqual(rc, 0, out)
        self.assertEqual(os.listdir(self.cases), ["lost_update.json"])

    def test_case_names_are_stable_and_distinct(self):
        a = FF.case_name("fight-base", "FAIL-1-04-both_kill_enemy_1_every_hit_once_one_death_one_credit")
        b = FF.case_name("fight-base", "FAIL-1-04-both_kill_enemy_1_every_hit_once_one_death_one_credit_x")
        self.assertEqual(a, FF.case_name("fight-base", "FAIL-1-04-both_kill_enemy_1_every_hit_once_one_death_one_credit"))
        self.assertNotEqual(a, b)
        self.assertEqual(a[:30], b[:30])        # the readable part is cut short, the tag keeps them apart
        self.assertLessEqual(len(a), 48)
        self.assertEqual(FF.case_name("r", "f", "My Case!"), "my_case")

    def test_same_file_name_in_both_instances(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}), same_names=True)
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 0, out)            # each machine's recording is taken from its own instance folder
        self.assertIn("REPRODUCED", out)

    def test_missing_recordings_are_reported(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}), dump_paths=False)
        for i in (0, 1):      # recordings far older than the failure are nobody's
            p = os.path.join(inst, str(i), "gamefl", "dump-%d.cofr" % i)
            os.utime(p, (1_000_000, 1_000_000))
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 2, out)
        self.assertIn("NO REPRO", out)
        self.assertIn("no folder configured", out)
        # one machine unreadable, the other fine: still not enough
        fdir, inst = make_run(os.path.join(self.tmp, "b"), pair(skip={2}))
        with open(os.path.join(inst, "1", "gamefl", "dump-1.cofr"), "wb") as f:
            f.write(b"junk")
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 2, out)
        self.assertIn("cannot read dump-1.cofr", out)
        self.assertIn("--converter", out)
        self.assertIn("got 1", out)
        # a recording named but not there
        fdir, inst = make_run(os.path.join(self.tmp, "c"), pair(skip={2}))
        os.remove(os.path.join(inst, "0", "gamefl", "dump-0.cofr"))
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 2, out)
        self.assertIn("recording not found: gamefl" + BS + "dump-0.cofr", out)
        # a failure.json that cannot be read
        with open(os.path.join(fdir, "failure.json"), "w") as f:
            f.write("{")
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 2, out)
        self.assertIn("cannot read failure.json", out)

    def test_dump_guessed_from_file_time(self):
        import time
        fdir, inst = make_run(self.tmp, pair(skip={2}), dump_paths=False)
        now = time.time()
        for i in (0, 1):       # an old recording and a recent one per machine: the recent one is the failure's
            d = os.path.join(inst, str(i), "gamefl")
            old = os.path.join(d, "old.cofr")
            shutil.copy(os.path.join(d, "dump-%d.cofr" % i), old)
            os.utime(old, (now - 7200, now - 7200))
            os.utime(os.path.join(d, "dump-%d.cofr" % i), (now - 60, now - 60))
        fj = os.path.join(fdir, "failure.json")
        os.utime(fj, (now, now))
        c = FF.collect(fj, inst)
        self.assertEqual([os.path.basename(m["path"]) for m in c["machines"]], ["dump-0.cofr", "dump-1.cofr"])
        self.assertEqual({m["how"] for m in c["machines"]}, {"guessed from file time"})
        newer = os.path.join(inst, "0", "gamefl", "newer.cofr")      # of two recent ones, the newest
        shutil.copy(os.path.join(inst, "0", "gamefl", "dump-0.cofr"), newer)
        os.utime(newer, (now - 10, now - 10))
        c = FF.collect(fj, inst)
        self.assertEqual(os.path.basename(c["machines"][0]["path"]), "newer.cofr")
        os.utime(newer, (now + 3600, now + 3600))        # written long after the failure: not its recording
        c = FF.collect(fj, inst)
        self.assertEqual(os.path.basename(c["machines"][0]["path"]), "dump-0.cofr")
        self.assertEqual(FF.collect(fj, None)["machines"][0]["error"], "flight dump failed (no folder configured)")     # no root, nothing to search

    def test_recording_references_in_failure_json(self):
        self.assertEqual(FF.recording_ref({"flight_dump": "a.cofr"}), ("a.cofr", None))
        self.assertEqual(FF.recording_ref({"flight_dump": {"path": "a.cofr"}}), ("a.cofr", None))
        self.assertEqual(FF.recording_ref({"flight": {"file": "f.cofr"}}), ("f.cofr", None))
        self.assertEqual(FF.recording_ref({"recording": "r.cofr"}), ("r.cofr", None))
        self.assertEqual(FF.recording_ref({"capture": {"recording": "b.dat"}}), ("b.dat", None))         # by its key, whatever the file name
        self.assertEqual(FF.recording_ref({"capture": {"actions": "x.json", "dump": "c.COFR"}}), ("c.COFR", None))        # any capture value that matches the pattern
        self.assertEqual(FF.recording_ref({"capture": {"actions": "x.json", "dump": "c.trace"}}), (None, None))
        self.assertEqual(FF.recording_ref({"capture": {"dump": "c.trace"}}, "*.trace"), ("c.trace", None))
        self.assertEqual(FF.recording_ref({"flight_dump_error": "no dump"}), (None, "no dump"))
        self.assertEqual(FF.recording_ref({"capture_error": "no capture"}), (None, "no capture"))
        self.assertEqual(FF.recording_ref({"name": "host", "capture": {"count": 3}}), (None, None))
        self.assertEqual(FF.recording_ref({"capture": "not a dict"}), (None, None))

    def test_recording_in_the_failure_folder_named_by_capture(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}), dump_paths=False)
        fj = os.path.join(fdir, "failure.json")
        manifest = read_json(fj)
        for i, m in enumerate(manifest["machines"]):       # what an adapter's capture(dir) does: write next to the failure, under the machine's name
            d = os.path.join(fdir, m["name"])
            os.makedirs(d)
            shutil.copy(os.path.join(inst, str(i), "gamefl", "dump-%d.cofr" % i), d)
            m.pop("flight_dump_error")
            m["capture"] = {"actions": "x.json", "dump": "dump-%d.cofr" % i}
        with open(fj, "w") as f:
            json.dump(manifest, f)
        shutil.rmtree(inst)
        c = FF.collect(fj, None)
        self.assertEqual([m["how"] for m in c["machines"]], ["recorded", "recorded"])
        self.assertTrue(all(os.path.dirname(m["path"]).startswith(fdir) for m in c["machines"]))
        rc, out = run_ff(fdir, "--no-write")
        self.assertEqual(rc, 0, out)
        self.assertIn("REPRODUCED", out)
        # with no recorded name the failure sub folder is searched by file time
        for m in manifest["machines"]:
            m.pop("capture")
        with open(fj, "w") as f:
            json.dump(manifest, f)
        c = FF.collect(fj, None)
        self.assertEqual({m["how"] for m in c["machines"]}, {"guessed from file time"})
        self.assertEqual(run_ff(fdir, "--no-write")[0], 0)

    def test_instances_root_from_the_argument_or_the_environment(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}))
        self.assertIsNone(FF.instances_root())
        os.environ[FF.ENV_ROOT] = inst
        self.assertEqual(FF.instances_root(), inst)
        self.assertEqual(FF.instances_root("elsewhere"), "elsewhere")       # the option wins
        rc, out = run_ff(fdir, "--no-write")
        self.assertEqual(rc, 0, out)
        os.environ.pop(FF.ENV_ROOT)
        rc, out = run_ff(fdir, "--no-write")        # no root at all: the relative recording path leads nowhere
        self.assertEqual(rc, 2, out)
        self.assertIn("recording not found", out)
        os.environ[FF.ENV_ROOT] = os.path.join(self.tmp, "not-the-root")
        self.assertEqual(run_ff(fdir, "--instances", inst, "--no-write")[0], 0)

    def test_finding_failures(self):
        import time
        a, _ = make_run(self.tmp, pair(), run="run-a", outdir="coop-bots-out")
        b, _ = make_run(self.tmp, pair(), run="run-b", outdir="coop-bots-out")
        now = time.time()
        os.utime(os.path.join(a, "failure.json"), (now - 100, now - 100))
        os.utime(os.path.join(b, "failure.json"), (now, now))
        out_dir = os.path.join(self.tmp, "coop-bots-out")
        self.assertEqual(FF.find_failures([out_dir]), [os.path.join(b, "failure.json"), os.path.join(a, "failure.json")])      # newest first
        self.assertEqual(FF.find_failures([a]), [os.path.join(a, "failure.json")])
        self.assertEqual(FF.find_failures([os.path.join(a, "failure.json")]), [os.path.join(a, "failure.json")])
        old = os.getcwd()
        os.chdir(self.tmp)
        try:
            got = FF.find_failures([])        # no argument: the newest one under ./coop-bots-out
            self.assertEqual([os.path.realpath(p) for p in got], [os.path.realpath(os.path.join(b, "failure.json"))])
        finally:
            os.chdir(old)
        rc, out = run_ff(os.path.join(self.tmp, "nowhere", "failure.json"))
        self.assertEqual(rc, 2)
        os.chdir(os.path.join(self.tmp, "instances"))
        try:
            rc, out = run_ff()
            self.assertEqual(rc, 2)
            self.assertIn("no failure.json found", out)
        finally:
            os.chdir(old)

    def test_recordings_given_directly(self):
        _, inst = make_run(self.tmp, pair(skip={2}))
        files = [os.path.join(inst, str(i), "gamefl", "dump-%d.cofr" % i) for i in (0, 1)]
        rc, out = run_ff(*files, "--out", self.cases)
        self.assertEqual(rc, 0, out)
        self.assertEqual(len(os.listdir(self.cases)), 1)
        rc, out = run_ff(files[0], files[0], "--no-write")      # the same machine twice is not a pair
        self.assertEqual(rc, 2, out)
        self.assertIn("recorded twice", out)
        rc, out = run_ff(files[0])
        self.assertEqual(rc, 2, out)
        self.assertIn("two or more recordings", out)
        fdir, _ = make_run(os.path.join(self.tmp, "b"), pair(skip={2}))
        rc, out = run_ff(files[0], fdir)
        self.assertEqual(rc, 2, out)
        self.assertIn("not both", out)

    def test_status_evidence_and_agreement(self):
        sick = {"counters": {"mismatches": 2, "repairs": 2, "repaired_effects": 11, "bad_messages": 1, "apply_failures": 3, "rebases": 1}}
        host = {"name": "host", "role": "host", "status": dict(sick, seq=16, synced=False)}
        cli = {"name": "client1", "role": "client", "status": dict(sick, seq=12, connected=0)}
        cli2 = {"name": "client2", "role": "client", "status": {"seq": 16, "counters": {}}}
        ev = FF.status_evidence({"machines": [host, cli, cli2]})
        self.assertEqual((ev["mismatches"], ev["repairs"], ev["repaired_effects"], ev["bad_messages"], ev["apply_failures"], ev["rebases"]), (4, 4, 22, 2, 6, 2))
        self.assertEqual(ev["lag"], ["client1 at update 12 of 16"])
        self.assertEqual(ev["unsynced"], ["host"])
        self.assertEqual(ev["disconnected"], ["client1"])
        self.assertEqual(len(ev["symptoms"]), 9)
        self.assertIn("4 mismatch(es)", ev["symptoms"])
        quiet = FF.status_evidence({"machines": [{"name": "host", "role": "host", "status": {"seq": 3, "synced": True}},
                                                 {"name": "c", "role": "client", "status": {"seq": 3, "connected": 1}}]})
        self.assertEqual(quiet["symptoms"], [])
        unsure = FF.status_evidence({"machines": [{"name": "host", "role": "host", "status": {"seq": 3, "synced": False}}]})
        self.assertEqual(unsure["symptoms"], ["host reports synced=false"])
        ahead = FF.status_evidence({"machines": [{"name": "host", "role": "host", "status": {"seq": 3}}, {"name": "c", "role": "client", "status": {"seq": 4}}]})
        self.assertEqual(ahead["lag"], [])
        junk = FF.status_evidence({"machines": [{"name": "host", "role": "host", "status": {"seq": "x", "counters": {"mismatches": "many"}}}, {"name": "c", "status": "down"}]})
        self.assertEqual(junk["symptoms"], [])
        self.assertEqual(FF.status_evidence({})["symptoms"], [])
        found = {"found": True, "items": [{"class": "digest-mismatch"}]}
        self.assertIn("mismatches in status and in recordings", FF.agreement(ev, found))
        self.assertIn("sync lag in status but not in recordings", FF.agreement(ev, found))
        both = {"found": True, "items": [{"class": "divergence"}, {"class": "stuck-sync"}]}
        self.assertIn("mismatches in status and in recordings", FF.agreement(ev, both))
        self.assertIn("sync lag in status and in recordings", FF.agreement(ev, both))
        self.assertIn("mismatches in status but not in recordings", FF.agreement(ev, {"found": True, "items": [{"class": "stuck-sync"}]}))
        self.assertIn("NOT visible", FF.agreement(ev, {"found": False, "items": []}))
        self.assertIn("no sync symptoms", FF.agreement(quiet, found))
        self.assertIn("neither do the recordings", FF.agreement(quiet, {"found": False, "items": []}))
        hard = FF.status_evidence({"machines": [{"name": "host", "role": "host", "status": {"counters": {"bad_messages": 1}}}]})
        self.assertIn("different kind", FF.agreement(hard, found))

    def test_status_symptoms_reach_the_report(self):
        sick = {"counters": {"mismatches": 1, "repairs": 1, "repaired_effects": 3}}
        fdir, inst = make_run(self.tmp, pair(digest_bad=True), status={0: sick})
        rc, out = self.run_on(fdir, inst, "--no-write")
        self.assertEqual(rc, 0, out)
        self.assertIn("live status: 1 mismatch(es); 1 repair(s); 3 repaired effect(s)", out)
        self.assertIn("check: mismatches in status and in recordings", out)

    def test_json_output_and_report(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}))
        html = os.path.join(self.tmp, "r.html")
        rc, out = run_ff(fdir, "--instances", inst, "--no-write", "--json", "--report", html)
        self.assertEqual(rc, 0, out)
        rep = json.loads(out[:out.rindex("]") + 1])
        self.assertEqual(rep[0]["verdict"], "reproduced")
        self.assertEqual(rep[0]["primary"]["class"], "stuck-sync")
        self.assertNotIn("_traces", rep[0])
        self.assertEqual(rep[0]["machines"][0]["file"], "dump-0.cofr")
        with open(html, encoding="utf-8") as f:
            text = f.read()
        self.assertIn("First divergence", text)
        self.assertIn("txn:2", text)
        self.assertIn("Minimal reproduction", text)      # the shrunk case is in the report
        rc, out = run_ff(fdir, "--instances", inst, "--no-write", "--no-shrink", "--report", html)
        self.assertIn("wrote " + html, out)
        fdir, inst = make_run(os.path.join(self.tmp, "b"), pair())
        os.remove(html)
        rc, out = run_ff(fdir, "--instances", inst, "--json", "--report", html)
        self.assertEqual(rc, 1, out)
        self.assertEqual(json.loads(out[:out.rindex("]") + 1])[0]["verdict"], "not-reproduced")
        self.assertTrue(os.path.exists(html))
        with open(html, encoding="utf-8") as f:
            self.assertNotIn("Minimal reproduction", f.read())

    def test_several_failures_in_one_run_folder(self):
        _, inst = make_run(self.tmp, pair(skip={2}), fail="FAIL-1-a", tag="a")
        make_run(self.tmp, pair(), fail="FAIL-2-b", tag="b")
        make_run(self.tmp, pair(digest_bad=True), fail="FAIL-3-c", tag="c")
        rc, out = run_ff(os.path.join(self.tmp, "out", "run-x"), "--instances", inst, "--out", self.cases, "--name", "ignored")
        self.assertEqual(rc, 1, out)
        self.assertIn("3 failure(s): 1 not-reproduced, 2 reproduced", out)
        self.assertEqual(len(os.listdir(self.cases)), 2)       # --name is not used when there are several, so the cases do not overwrite each other
        self.assertNotIn("ignored.json", os.listdir(self.cases))

    def test_machine_paths_never_reach_a_case(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}), reason="the bot failed in %s and %s" % (os.getcwd(), os.getcwd().replace(BS, "/")))
        rc, out = self.run_on(fdir, inst)
        self.assertEqual(rc, 0, out)
        with open(os.path.join(self.cases, os.listdir(self.cases)[0])) as f:
            text = f.read()
        self.assertNotIn(os.getcwd().replace(BS, BS + BS), text)
        self.assertNotIn(os.getcwd().replace(BS, "/"), text)
        self.assertIn("<path>", text)

    # ---- converters: a game with its own recording format

    def b64_run(self, **kw):
        import base64
        return make_run(self.tmp, pair(skip={2}), ext=".trace", encode=base64.b64encode, **kw)

    def test_a_converter_turns_a_game_recording_into_cofr(self):
        mod, d = self.converter_module()
        fdir, inst = self.b64_run()
        rc, out = self.run_on(fdir, inst, "--converter", mod + ":convert", "--converter-path", d)
        self.assertEqual(rc, 0, out)
        self.assertIn("REPRODUCED: stuck-sync (txn:2)", out)
        seen = sys.modules[mod].SEEN
        self.assertEqual([(i["name"], i["role"], i["instance"]) for i in seen], [("host", "host", 0), ("client1", "client", 1)])
        fx = self.case()
        self.assertEqual(fx["format"], "cofr-trace-1")
        self.assertEqual({r["file"] for r in fx["source"]["recordings"]}, {"dump-0.trace", "dump-1.trace"})
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(RG.run_dir(self.cases, None), (1, 0))       # the case holds .cofr records: it replays without the converter

    def test_a_converter_in_a_file_and_for_recordings_given_directly(self):
        mod, d = self.converter_module()
        _, inst = self.b64_run()
        files = [os.path.join(inst, str(i), "gamefl", "dump-%d.trace" % i) for i in (0, 1)]
        rc, out = run_ff(*files, "--no-write", "--converter", os.path.join(d, mod + ".py") + ":convert")
        self.assertEqual(rc, 0, out)
        rc, out = run_ff(*files, "--no-write")
        self.assertEqual(rc, 2, out)
        self.assertIn("--converter module:function", out)

    def test_recordings_are_found_by_the_pattern_for_the_converter(self):
        mod, d = self.converter_module()
        fdir, inst = self.b64_run(dump_paths=False)
        self.assertEqual(self.run_on(fdir, inst, "--no-write")[0], 2)         # the default pattern is *.cofr
        rc, out = self.run_on(fdir, inst, "--no-write", "--pattern", "*.trace", "--converter", mod + ":convert", "--converter-path", d)
        self.assertEqual(rc, 0, out)
        self.assertIn("guessed from file time", out)

    def test_converter_failures_are_reported(self):
        mod, d = self.converter_module()
        fdir, inst = self.b64_run()
        rc, out = self.run_on(fdir, inst, "--no-write", "--converter", mod + ":boom", "--converter-path", d)
        self.assertEqual(rc, 2, out)
        self.assertIn("cannot convert dump-0.trace: ValueError: boom", out)
        rc, out = self.run_on(fdir, inst, "--no-write", "--converter", mod + ":wrong", "--converter-path", d)
        self.assertEqual(rc, 2, out)
        self.assertIn("the converter returned str, not bytes", out)
        rc, out = self.run_on(fdir, inst, "--no-write", "--converter", mod + ":gone", "--converter-path", d)      # a file error is a read error
        self.assertEqual(rc, 2, out)
        self.assertIn("cannot read dump-0.trace", out)
        self.assertIn("got 0", out)

    def test_converter_specs(self):
        mod, d = self.converter_module()
        self.assertIs(FF.load_converter(None), FF.identity_convert)
        self.assertTrue(callable(FF.load_converter(mod + ":convert", d)))
        with self.assertRaisesRegex(FF.ConvertError, "module:function"):
            FF.load_converter("noseparator")
        for bad in ("noseparator", ":fn", "mod:", "no_such_module_xyz:convert", mod + ":no_such_function", "json:__name__"):
            with self.assertRaises(FF.ConvertError, msg=bad):
                FF.load_converter(bad, d)
        with self.assertRaises(FF.ConvertError):
            FF.load_converter(os.path.join(d, "missing_file.py") + ":convert")
        rc, out = run_ff("a.cofr", "b.cofr", "--converter", "noseparator")
        self.assertEqual(rc, 2)
        self.assertIn("cannot start", out)

    def test_the_default_converter_passes_cofr_files_through(self):
        _, inst = make_run(self.tmp, pair())
        p = os.path.join(inst, "0", "gamefl", "dump-0.cofr")
        with open(p, "rb") as f:
            self.assertEqual(FF.identity_convert(p, {}), f.read())
        with self.assertRaises(FF.ConvertError):
            FF.convert_file(FF.identity_convert, os.path.join(inst, "nope.cofr"), {})


@slow
class FromFailureCliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="cofr_ffcli_")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def cli(self, *args, script="from_failure.py", env=None):
        p = subprocess.run([sys.executable, "-I", os.path.join(SRC, script)] + list(args), capture_output=True, text=True, errors="replace",
                           env=dict(os.environ, **(env or {})))
        return p.returncode, p.stdout + p.stderr

    def test_command_line_and_exit_codes(self):
        fdir, inst = make_run(self.tmp, pair(skip={2}))
        cases = os.path.join(self.tmp, "cases")
        rc, out = self.cli(fdir, "--instances", inst, "--out", cases)
        self.assertEqual(rc, 0, out)
        self.assertIn("REPRODUCED", out)
        self.assertEqual(len(os.listdir(cases)), 1)
        rc, out = self.cli("regress", cases, script="flight.py")      # the case replays with no replay hook
        self.assertEqual(rc, 0, out)
        self.assertIn("ALL OK", out)
        rc, out = self.cli(fdir, "--no-write", env={FF.ENV_ROOT: inst})      # the environment variable names the root
        self.assertEqual(rc, 0, out)
        fdir2, inst2 = make_run(os.path.join(self.tmp, "fixed"), pair())
        rc, out = self.cli(fdir2, "--instances", inst2, "--no-write")
        self.assertEqual(rc, 1, out)
        self.assertIn("NOT REPRODUCED", out)
        rc, out = self.cli(fdir2, "--no-write", env={FF.ENV_ROOT: ""})
        self.assertEqual(rc, 2, out)
        self.assertIn("NO REPRO", out)

    def test_flight_py_from_failure_subcommand(self):
        fdir, inst = make_run(self.tmp, pair(digest_bad=True))
        rc, out = self.cli("from-failure", fdir, "--instances", inst, "--no-write", "--json", script="flight.py")
        self.assertEqual(rc, 0, out)
        self.assertEqual(json.loads(out)[0]["primary"]["class"], "digest-mismatch")

    def test_default_case_folder_is_the_flight_regress_folder(self):
        self.assertEqual(FF.DEFAULT_OUT, os.path.join(SRC, "regress"))
        import flight
        self.assertEqual(os.path.abspath(flight.DEFAULT_REGRESS), FF.DEFAULT_OUT)


class RegressAndGentestForTraceCasesTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="cofr_rg_")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def trace_fixture(self):
        return TraceReplayTests("test_classes").fixture(skip={2})[0]

    def test_a_trace_case_gets_no_cmake_file_and_keeps_an_expectation_override(self):
        fx = self.trace_fixture()
        fx["shrunk"] = {"signature": "stuck-sync txn:#"}
        paths = G.generate(fx, "Lost Update", self.tmp, expect={"converges": False, "bug_signature": "x"})
        self.assertEqual([os.path.basename(p) for p in paths], ["lost_update.json"])
        self.assertEqual(os.listdir(self.tmp), ["lost_update.json"])
        self.assertEqual(read_json(paths[0])["expect"], {"converges": False, "bug_signature": "x"})
        paths = G.generate(fx, "Default", self.tmp)
        self.assertEqual(read_json(paths[0])["expect"], {"converges": True, "bug_signature": "stuck-sync txn:#"})
        paths = G.generate({"version": 1, "steps": [{"op": "quiesce"}]}, "plain", self.tmp)          # other fixtures still get theirs
        self.assertEqual([os.path.basename(p) for p in paths], ["plain.json", "plain.cmake"])

    def test_regress_replays_trace_cases_without_a_hook_and_other_cases_need_one(self):
        fx = dict(self.trace_fixture(), expect={"converges": False})
        with open(os.path.join(self.tmp, "a_trace.json"), "w") as f:
            json.dump(fx, f)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            self.assertEqual(RG.run_dir(self.tmp, None), (1, 0))
            self.assertEqual(RG.main([self.tmp]), 0)
        self.assertIn("PASS a_trace.json (diverged: stuck-sync txn:#)", buf.getvalue())
        with open(os.path.join(self.tmp, "b_other.json"), "w") as f:
            json.dump({"version": 1, "steps": [{"op": "quiesce"}], "expect": {"converges": True}}, f)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            self.assertEqual(RG.run_dir(self.tmp, None), (2, 1))
        self.assertIn("ERROR b_other.json: no replay hook given", buf.getvalue())
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(RG.run_dir(self.tmp, SH.PyOracle("toy_replay:replay", path=SRC)), (2, 0))
        bad = dict(fx, steps=[{"op": "rec", "slot": 9, "hex": "00"}])             # a damaged trace case is an error, not a crash
        with open(os.path.join(self.tmp, "a_trace.json"), "w") as f:
            json.dump(bad, f)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            self.assertEqual(RG.run_dir(self.tmp, SH.PyOracle("toy_replay:replay", path=SRC)), (2, 1))
        self.assertIn("ERROR a_trace.json: bad cofr-trace-1 fixture", buf.getvalue())


# ---------------------------------------------------------------- C++ write, Python read

def write_python_trace(path):
    """The file the C++ test 'reads_the_trace_written_by_python' parses."""
    sch = S.Schema({"game": "from-python"})
    h = {"slot": 3, "role": 1, "reason": 2, "proto_version": 5, "wall_unix_ms": 1700000000000, "dump_t_us": 9, "session": 8, "epoch": 2}
    recs = [
        {"type": C.T_META, "mono_us": 0, "frame": 0, "meta": {"label": "py"}},
        {"type": C.T_SCHEMA, "mono_us": 0, "frame": 0, "schema": sch},
        {"type": C.T_NET_IN, "mono_us": 10, "frame": 1, "peer": 2, "channel": 0xFF, "msg_type": 4, "decode": 0, "session": 8, "epoch": 2,
         "seq": 5, "seq_hi": 6, "raw": b"\x01\x02\x03"},
        {"type": C.T_TXN_APPLIED, "mono_us": 11, "frame": 1, "seq": 5, "epoch": 2, "origin": 0, "index": 1, "applied_flags": 4, "hash": (1 << 64) - 1},
        {"type": C.T_DIGEST, "mono_us": 12, "frame": 1, "category": 0, "epoch": 2, "seq": 5, "hash": 3, "item_count": 1, "shards": [7, 8],
         "keys": {1 << 63: 5}, "bucketed": True},
        {"type": C.T_MARKER, "mono_us": 13, "frame": 1, "kind": 4, "text": "written by python"},
    ]
    C.dump(path, h, recs, sch)


class CrossTests(unittest.TestCase):
    """The C++ recorder writes, the Python reader reads."""

    def setUp(self):
        global CROSS
        CROSS = os.environ.get("COFR_CROSS_DIR")
        if not CROSS or not os.path.isdir(CROSS):
            self.skipTest("needs the files the C++ tests write (run without --no-cpp)")

    def test_fresh_sample_traces_equal_the_checked_in_golden_files(self):
        for n in ("sample_host.cofr", "sample_client.cofr", "desync_host.cofr", "desync_client.cofr"):
            self.assertEqual(read_bytes(os.path.join(CROSS, "fresh_" + n)), read_bytes(os.path.join(GOLDEN, n)), n)

    def test_everything_trace_field_by_field(self):
        data = read_bytes(os.path.join(CROSS, "everything.cofr"))
        tr = C.loads(data, "everything")
        self.assertEqual(tr["warnings"], [])
        h = tr["header"]
        self.assertEqual((h["slot"], h["role_name"], h["reason_name"], h["proto_version"], h["session"], h["epoch"], h["wall_unix_ms"]),
                         (2, "client", "desync", 7, 9, 4, 1700000000123))
        self.assertEqual(h["record_count"], len(tr["records"]))
        self.assertEqual(tr["meta"], {"label": "everything", "build": "x", "peer.0": "0"})
        self.assertEqual(tr["schema"].game, "toy")
        by = {}
        for r in tr["records"]:
            by.setdefault(r["name"], []).append(r)
        self.assertEqual([r["name"] for r in tr["records"][:2]], ["Meta", "Schema"])
        out = by["NetOut"][0]
        self.assertEqual((out["peer"], out["channel"], out["mux"], out["decode"], out["raw_len"], len(out["raw"]), out["truncated"], out["frame"]),
                         (3, 1, 0xD1, C.DECODE_FAILED, 5000, 1000, True, 3))
        self.assertEqual(out["mono_us"], 100)
        batch, dmsg = by["NetIn"]
        self.assertEqual((batch["peer"], batch["msg_type"], batch["decode"], batch["session"], batch["epoch"], batch["seq"], batch["seq_hi"], batch["frame"]),
                         (0, 4, C.DECODED, 4, 2, 7, 9, 4))
        self.assertEqual(batch["mono_us"], 200)
        self.assertEqual(dmsg["seq"], 77)
        derived = [r for r in by["Digest"] if r["derived"]]
        self.assertEqual(len(derived), 1)
        self.assertEqual((derived[0]["category"], derived[0]["epoch"], derived[0]["seq"], len(derived[0]["shards"]), derived[0]["shards"][5]), (0, 2, 77, 128, 0xDEAD))
        cap = by["TxnCaptured"][0]
        self.assertEqual((cap["kind"], cap["intent_id"], cap["seq"], cap["effect_count"], cap["hash"], cap["cause"], cap["epoch"]),
                         (1, 5, 0, 2, 0xAABBCCDDEEFF0011, 3, 4))
        ap = by["TxnApplied"][0]
        self.assertEqual((ap["seq"], ap["epoch"], ap["origin"], ap["index"], ap["applied_flags"], ap["cause"], ap["effect_tag"], ap["hash"]),
                         (12, 4, 1, 2, 2 | 8, 3, 12, 0x1122334455667788))
        self.assertEqual(tr["schema"].flag_names(ap["applied_flags"]), ["repair", "catch-up"])
        own = by["Ownership"][0]
        self.assertEqual((own["op"], own["actor"], own["epoch"], own["from_slot"], own["to_slot"], own["reason"], own["txn_id"]),
                         (1, 0xABCDEF0123, 7, 0, 2, 2, 99))
        plain = [r for r in by["Digest"] if not r["derived"] and not r["detail"] and not r["bucketed"]][0]
        self.assertEqual((plain["category"], plain["seq"], plain["hash"], plain["shards"], plain["keys"]),
                         (1, 12, 0x1122334455667788, [1, 2, 3], {10: 100, 11: 101}))
        main = [r for r in by["Digest"] if r["bucketed"] and not r["detail"] and not r["derived"]][0]
        details = [r for r in by["Digest"] if r["detail"]]
        self.assertEqual(len(details), 2)                                      # 5000 entries split over two records
        entries = {}
        for r in details:
            entries.update(r["keys"])
        self.assertEqual((len(entries), main["item_count"]), (5000, 5000))
        shards, h64 = C.bucketed_digest(entries.items())                        # the Python digest equals the C++ one
        self.assertEqual((shards, h64), (main["shards"], main["hash"]))
        self.assertTrue(any(r["text"].startswith("entry %016x " % next(iter(entries))) for r in by["Marker"]))
        sw = by["ScriptWrite"][0]
        self.assertEqual(sw["fields"], {"seq": 4, "key": "stage.q1", "value": -2})
        unk = by["Unknown99"][0]
        self.assertEqual(unk["payload"], b"\x09\x09")
        mk = [r for r in by["Marker"] if r["kind"] == 5][0]
        self.assertEqual(mk["text"], "saved é")
        self.assertEqual(tr["schema"].marker_kind(9), "checkpoint")
        self.assertEqual(C.dumps(tr["header"], C.to_records(tr), tr["schema"]), data)   # the Python writer reproduces the C++ bytes

    def test_python_writer_output_is_what_the_cpp_reader_expects(self):
        # written by write_python_trace before the C++ phase; the C++ test parsed it. Here: the file is still a valid trace.
        tr = C.load(os.path.join(CROSS, "from_python.cofr"))
        self.assertEqual(tr["header"]["record_count"], 6)
        self.assertEqual(tr["schema"].game, "from-python")


# ---------------------------------------------------------------- mutation twin

MUTATIONS = [
    ("cofr", "        if len(data) - pos - RECORD_HEADER < plen:\n            break", "        if False:\n            break", "truncated tail not detected"),
    ("cofr", "rec[\"malformed\"] = True\n        elif rtype == T_SCHEMA:", "rec[\"malformed\"] = False\n        elif rtype == T_SCHEMA:", "malformed bodies hidden"),
    ("cofr", "if h[\"version\"] != VERSION:", "if False:", "version not checked"),
    ("cofr", "if len(data) < 4 or data[:4] != MAGIC:", "if False:", "magic not checked"),
    ("cofr", "r[\"bucketed\"] = bool(r[\"flags\"] & FLAG_BUCKETED)", "r[\"bucketed\"] = False", "bucketed flag ignored"),
    ("cofr", "return _splitmix((key ^ _splitmix(value_hash)) & M64) & 0xFFFFFFFF", "return _splitmix((key ^ _splitmix(value_hash)) & M64) & 0x7FFFFFFF", "bucket mix differs from the C++ one"),
    ("cofr", "h = ((h ^ b) * 1099511628211) & M64", "h = ((h * 1099511628211) ^ b) & M64", "hash differs from the C++ one"),
    ("cofr", "rec[\"fields\"] = S.decode_custom(spec, rec[\"payload\"])", "rec[\"fields\"] = {}", "game-defined records not decoded"),
    ("cofr", "sch = schema or file_schema or S.default()", "sch = file_schema or S.default()", "external schema ignored"),
    ("cofr", "sch = schema or file_schema or S.default()", "sch = S.default()", "schema in the file ignored"),
    ("schema", "self.restate_mask = mask", "self.restate_mask = 7", "restate mask ignored"),
    ("schema", "self.ownership_owner_ops = set(_int_list(\"ownership_owner_ops\", merged[\"ownership_owner_ops\"]))", "self.ownership_owner_ops = {0, 1}", "owner ops ignored"),
    ("schema", "return self.txn_messages is None or n in self.txn_messages", "return True", "transaction message list ignored"),
    ("schema", "merged.update(data)", "pass", "schema keys do not replace the default"),
    ("schema", "if not FIRST_CUSTOM <= t <= 255:", "if False:", "custom type range not checked"),
    ("schema", "if f[\"name\"] in seen:", "if False:", "duplicate field names accepted"),
    ("schema", "if f[\"type\"] == \"bytes\" and i != len(clean) - 1:", "if False:", "bytes field in the middle accepted"),
    ("analyze", "a.offset_us[s] = int((fwd - back) / 2)", "a.offset_us[s] = int(fwd - back)", "clock offset wrong"),
    ("analyze", "c.sort(key=lambda x: (x[\"aligned_us\"], _PRIO[x[\"kind\"]], x[\"key\"]))", "c.sort(key=lambda x: (-x[\"aligned_us\"], _PRIO[x[\"kind\"]], x[\"key\"]))", "latest instead of earliest"),
    ("analyze", "if len(set(vals.values())) <= 1 and not diff_keys", "if len(set(vals.values())) < 0 and not diff_keys", "agreeing digests flagged"),
    ("analyze", "if len(lst) > 1:", "if len(lst) > 2:", "double apply missed"),
    ("analyze", "return not r[\"applied_flags\"] & a.schema.restate_mask", "return True", "repair restatements counted as divergence"),
    ("analyze", "owner_ops = a.schema.ownership_owner_ops", "owner_ops = {0, 1}", "ownership ops not taken from the schema"),
    ("analyze", "if len(owners) > 1:", "if len(owners) > 2:", "owner conflict missed"),
    ("analyze", "r[\"epoch\"] < prev", "r[\"epoch\"] <= prev", "idempotent regrant flagged"),
    ("analyze", "if gap or late:", "if gap:", "late txn missed"),
    ("analyze", "if gap or late:", "if late:", "gap txn missed"),
    ("analyze", "            if key[1] in a.traces:\n                a.lost.append", "            if False:\n                a.lost.append", "lost messages not found"),
    ("analyze", "known[int(k[5:])] = int(v)", "pass", "transport peer ids not mapped"),
    ("analyze", "if (s, head) in seen:", "if False:", "repeated markers not folded"),
    ("analyze", "r[\"type\"] == C.T_MARKER and not r.get(\"malformed\") and r[\"kind\"] == C.MARKER_DESYNC:", "r[\"type\"] == C.T_MARKER and not r.get(\"malformed\") and r[\"kind\"] == 0:", "desync markers ignored"),
    ("analyze", "t_lo = _last_agree(a, cand)\n        if t_lo is None:", "t_lo = None\n        if t_lo is None:", "window ignores last agreeing digest"),
    ("analyze", "elif r[\"peer_slot\"] in a.traces:\n            a.orphans.append", "elif False:\n            a.orphans.append", "orphans ignored"),
    ("analyze", "if k % nb in diff_shards]", "if True]", "cross-digest keys not limited to the differing buckets"),
    ("analyze", "\"bucket\": ki % nb if bucketed else None", "\"bucket\": ki & 0xFF if bucketed else None", "bucket count fixed at 256"),
    ("analyze", "bucketed = nb > 0 and any(r.get(\"bucketed\") for r in per.values())", "bucketed = nb > 0", "plain shards treated as key buckets"),
    ("analyze", "or d[\"captured\"]:", ":", "host commit counted as a missing apply"),
    ("analyze", "\"texts\": {s: a.entries.get(s, {}).get(ki) for s in slots}", "\"texts\": {s: None for s in slots}", "value text dropped"),
    ("analyze", "and a.schema.is_txn_message(r[\"msg_type\"])):", "and True):", "chain ignores the schema's transaction messages"),
    ("analyze", "elif r.get(\"custom\") and \"fields\" in r and", "elif False and", "game-defined records never direct"),
    ("analyze", "or r.get(\"custom\")):", "):", "game-defined records not in the chain"),
    ("shrink", "return r.get(\"diverged\") and signature(r) == sig", "return r.get(\"diverged\")", "different failure accepted"),
    ("shrink", "pinned = [i for i, s in enumerate(steps) if s.get(\"pin\")]", "pinned = []", "pinned steps removed"),
    ("shrink", "if line.startswith(\"VIOLATION \"):", "if False:", "violation lines ignored"),
    ("shrink", "if comp and len(comp) < len(cur) and test(comp):", "if False:", "complements never tried"),
    ("shrink", "if r[\"truncated\"] or not r[\"raw\"]:", "if False:", "truncated datagrams replayed"),
    ("shrink", "if p.returncode not in (0, 1):", "if p.returncode not in (0, 1, 2):", "bad fixture treated as converged"),
    ("shrink", "if runs[0] >= max_runs or", "if False or", "budget ignored"),
    ("shrink", "if count and test([]):", "if False:", "empty failing set not found"),
    ("shrink", "if not isinstance(result, dict) or \"diverged\" not in result:", "if False:", "bad hook result accepted"),
    ("shrink", "raise OracleError(\"the replay function raised %s: %s\" % (type(e).__name__, e))", "return {\"diverged\": False}", "hook crash treated as converged"),
    ("shrink", "if list_key:\n        if list_key not in fixture:", "if False:\n        if list_key not in fixture:", "named list ignored"),
    ("gentest", "\"converges\": True", "\"converges\": False", "wrong expectation emitted"),
    ("gentest", "re.sub(r\"[^a-z0-9_]+\", \"_\", s.lower())", "s.lower()", "unsafe file names"),
    ("report", "if nw and (nw[\"buckets\"] or nw[\"keys\"]):", "if False:", "narrowing missing from the report"),
    ("report", "(k[\"texts\"].get(s) or", "(\"\" or", "value text missing from the report"),
    ("report", "e(hd[\"instance\"])", "hd[\"instance\"]", "report not escaped"),
    ("report", "e(_step_text(st))", "_step_text(st)", "steps not escaped"),
    ("regress", "ok = got == want", "ok = got", "expected-divergence fixtures fail"),
    ("flight", "d = analyze.find_divergence(al)\n        if a.json:", "d = None\n        if a.json:", "diverge never finds anything"),
    ("flight", "return 1 if (a.check and d) else 0", "return 0", "diverge --check always succeeds"),
]

# The twins of the failure-to-case tools (trace_replay.py, from_failure.py and their hooks in regress.py, gentest.py, flight.py).
# Their tests are all in these classes, so a twin runs only them.
ONLY_NEW = "TraceReplayTests,FromFailureTests,FromFailureCliTests,RegressAndGentestForTraceCasesTests"
NEW_MUTATIONS = [
    ("trace_replay", 'if c["kind"] == "txn-missing" and later:', "if False:", "gap healed by a snapshot counted as a failure", ONLY_NEW),
    ("trace_replay", "any(t >= c[\"aligned_us\"] for s", "any(t <= c[\"aligned_us\"] for s", "snapshot before the gap heals it", ONLY_NEW),
    ("trace_replay", "mask = a.schema.restate_mask", "mask = 7", "restate mask not taken from the schema", ONLY_NEW),
    ("trace_replay", 'own = [i for i in real if i["class"] != "desync-marker"]', "own = []", "own desync marker leads over a real finding", ONLY_NEW),
    ("trace_replay", "real = ([p] + [i for i in real if i is not p]) if p else real", "real = real", "failure signature taken from the marker", ONLY_NEW),
    ("trace_replay", "if len(data) - pos - C.RECORD_HEADER < plen:", "if False:", "half-written record kept", ONLY_NEW),
    ("trace_replay", 'if st.get("op") == "rec":', "if True:", "non-record steps replayed", ONLY_NEW),
    ("trace_replay", 'fx.get("format") != FORMAT or', "False or", "foreign fixture format accepted", ONLY_NEW),
    ("trace_replay", "except Exception as ex:   # noqa: BLE001", "except ZeroDivisionError as ex:   # noqa: BLE001", "thin shrink candidate crashes the replay", ONLY_NEW),
    ("trace_replay", 'return 1 if res["diverged"] else 0', "return 0", "replay exit code ignores divergence", ONLY_NEW),
    ("trace_replay", '"diverged": cl["found"],', '"diverged": True,', "clean traces reported as failing", ONLY_NEW),
    ("trace_replay", 'struct.pack_into("<Q", hdr, RECORD_COUNT_OFFSET, len(recs))', "pass", "stale record count in rebuilt headers", ONLY_NEW),
    ("trace_replay", 'schema = SC.from_json(fx["schema"]) if fx.get("schema") else None', "schema = None", "schema in the fixture ignored", ONLY_NEW),
    ("trace_replay", '"txn-missing": "stuck-sync"', '"txn-missing": "divergence"', "stuck sync reported as divergence", ONLY_NEW),
    ("from_failure", "cands.append(os.path.join(root, str(instance), rel))", "pass", "recording taken from the wrong instance folder", ONLY_NEW),
    ("from_failure", "if root:\n        cands.append(os.path.join(root, rel))", "if False:\n        cands.append(os.path.join(root, rel))", "recording path not tried under the root", ONLY_NEW),
    ("from_failure", "if name:\n        cands +=", "if False:\n        cands +=", "failure sub folder of the machine not searched", ONLY_NEW),
    ("from_failure", "os.path.join(fail_dir, base), rel]", "os.path.join(fail_dir, base)]", "recording path as written not tried", ONLY_NEW),
    ("from_failure", "return max(hits, key=os.path.getmtime)", "return None", "root sub folders not searched", ONLY_NEW),
    ("from_failure", "return max(near)[1] if near else None", "return min(near)[1] if near else None", "oldest recording guessed", ONLY_NEW),
    ("from_failure", "0 <= failure_time - os.path.getmtime(f) <= GUESS_WINDOW_S or abs(failure_time - os.path.getmtime(f)) < 5", "True", "any recording guessed", ONLY_NEW),
    ("from_failure", 'if m.get("name"):\n        files +=', 'if False:\n        files +=', "failure folder not searched when guessing", ONLY_NEW),
    ("from_failure", 'glob.glob(os.path.join(inst, "*", pattern))', "[]", "game copy sub folders not searched when guessing", ONLY_NEW),
    ("from_failure", "for holder in (m, cap):\n        for k in REF_KEYS:", "for holder in (m,):\n        for k in REF_KEYS:", "capture dict not searched for the recording", ONLY_NEW),
    ("from_failure", "for k in REF_KEYS:", "for k in ():", "named recording entries ignored", ONLY_NEW),
    ("from_failure", 'for k in ("path", "file"):', 'for k in ("path",):', "file entry of a recording reference ignored", ONLY_NEW),
    ("from_failure", "if p and fnmatch(", "if p or fnmatch(", "every capture value taken for a recording", ONLY_NEW),
    ("from_failure", "for k in ERROR_KEYS:", "for k in ():", "dump errors of failure.json not shown", ONLY_NEW),
    ("from_failure", 'if h["slot"] in slots:', "if False:", "same machine accepted twice", ONLY_NEW),
    ("from_failure", "ev[k] += _int(counters.get(k))", "ev[k] = _int(counters.get(k))", "counters not summed over machines", ONLY_NEW),
    ("from_failure", 'if name != host and seq < seqs[host]:', 'if name != host and seq > seqs[host]:', "sync lag not seen", ONLY_NEW),
    ("from_failure", 'if st.get("synced") is False:', "if False:", "host synced=false not seen", ONLY_NEW),
    ("from_failure", 'elif st.get("connected") in (0, False):', "elif False:", "lost link not seen", ONLY_NEW),
    ("from_failure", '"and" if classes & {"digest-mismatch", "divergence"} else "but not"', '"and" if False else "but not"', "status/recording agreement wrong for mismatches", ONLY_NEW),
    ("from_failure", '"and" if "stuck-sync" in classes else "but not"', '"and" if True else "but not"', "status/recording agreement wrong for lag", ONLY_NEW),
    ("from_failure", 'expect = {"converges": bool(opts.get("expect_converges")),', 'expect = {"converges": True,', "case expectation ignores the flag", ONLY_NEW),
    ("from_failure", 'return bool(r.get("diverged")) and S.signature(r) == sig', 'return bool(r.get("diverged"))', "shrink accepts a different failure", ONLY_NEW),
    ("from_failure", 'tag = hashlib.sha1(("%s/%s" % (run, fail)).encode()).hexdigest()[:8]', 'tag = ""', "case names collide", ONLY_NEW),
    ("from_failure", "found.sort(key=lambda f: (-os.path.getmtime(f), f))", "found.sort(key=lambda f: (os.path.getmtime(f), f))", "oldest failure first", ONLY_NEW),
    ("from_failure", 'found = glob.glob(os.path.join(os.getcwd(), FAILURE_DIR, "**", "failure.json"), recursive=True)', "found = []", "default failure folder not searched", ONLY_NEW),
    ("from_failure", "return found[:1] if not paths else found", "return found", "no argument takes every failure", ONLY_NEW),
    ("from_failure", "if len(traces) < 2:", "if len(traces) < 1:", "one recording treated as enough", ONLY_NEW),
    ("from_failure", 'return 1 if any(r["verdict"] == "not-reproduced" for r in reps) else 0', "return 0", "exit code ignores not-reproduced", ONLY_NEW),
    ("from_failure", '    if any(r["verdict"] == "no-recordings" for r in reps):\n        return 2', '    if any(r["verdict"] == "no-recordings" for r in reps):\n        return 0', "exit code ignores missing recordings", ONLY_NEW),
    ("from_failure", "for m in MARGINS_MS:", "for m in ():", "time window stage skipped", ONLY_NEW),
    ("from_failure", 'abs(s["at_ms"] - at) <= m', 'abs(s["at_ms"] - at) <= -1', "time window keeps nothing", ONLY_NEW),
    ("from_failure", 'or s.get("pin") or s["ty"] == "Meta"]', 'or s["ty"] == "Meta"]', "time window cuts the pinned schema records", ONLY_NEW),
    ("from_failure", "for ty in list(DROP_ORDER[:-1]) + extra + list(DROP_ORDER[-1:]):", "for ty in ():", "record-type stage skipped", ONLY_NEW),
    ("from_failure", "DROP_ORDER[:-1]) + extra + list", "DROP_ORDER[:-1]) + [] + list", "game-defined record kinds not dropped as a stage", ONLY_NEW),
    ("from_failure", "if r[\"type\"] == C.T_SCHEMA:", "if False:", "schema records can be shrunk away", ONLY_NEW),
    ("from_failure", 'if schema_text:\n        fx["schema"] = schema_text', 'if False:\n        fx["schema"] = schema_text', "schema option not kept in the case", ONLY_NEW),
    ("from_failure", '"reason": _scrub(manifest.get("reason"))[:300]', '"reason": str(manifest.get("reason"))[:300]', "machine paths in the case", ONLY_NEW),
    ("from_failure", 'run = os.path.basename(os.path.dirname(parent)) if os.path.basename(parent) else ""', 'run = ""', "run name missing from the case", ONLY_NEW),
    ("from_failure", "for m in machines if m.get(\"dropped\")]", "for m in machines if False]", "ring drops not reported", ONLY_NEW),
    ("from_failure", "return arg or os.environ.get(ENV_ROOT) or None", "return os.environ.get(ENV_ROOT) or arg or None", "the environment beats the option", ONLY_NEW),
    ("from_failure", "or os.environ.get(ENV_ROOT) or None", "or None", "environment variable ignored", ONLY_NEW),
    ("from_failure", "if not isinstance(data, (bytes, bytearray)):", "if False:", "converter result not checked", ONLY_NEW),
    ("from_failure", "except Exception as ex:   # noqa: BLE001  a game's converter can fail in any way", "except ZeroDivisionError as ex:   # noqa: BLE001  a game's converter can fail in any way", "converter crash not reported", ONLY_NEW),
    ("from_failure", 'raise ConvertError("cannot read %s: %s"', 'raise ConvertError("cannot convert %s: %s"', "file errors reported as converter errors", ONLY_NEW),
    ("from_failure", "if not sep or not mod or not fn:", "if False:", "converter spec not checked", ONLY_NEW),
    ("from_failure", 'if mod.lower().endswith(".py"):', "if False:", "converter in a file not supported", ONLY_NEW),
    ("from_failure", "if path and os.path.abspath(path) not in sys.path:", "if False:", "converter path ignored", ONLY_NEW),
    ("from_failure", "if not callable(func):", "if False:", "converter that is not a function accepted", ONLY_NEW),
    ("from_failure", "convert = load_converter(a.converter, a.converter_path)", "convert = identity_convert", "converter option ignored", ONLY_NEW),
    ("from_failure", 'opts.get("convert") or identity_convert', "identity_convert", "converter not used", ONLY_NEW),
    ("from_failure", '{k: m.get(k) for k in ("name", "role", "instance")})', "{})", "converter not told which machine", ONLY_NEW),
    ("from_failure", '" (a recording in another format needs --converter module:function)" if "magic" in str(ex) else ""', '""', "no hint for another recording format", ONLY_NEW),
    ("from_failure", 'recs = [p for p in a.paths if _is_recording(p)]', 'recs = [p for p in a.paths if p.lower().endswith(".cofr")]', "recordings in another format not accepted", ONLY_NEW),
    ("from_failure", "if len(recs) != len(a.paths):", "if False:", "recordings and failures mixed", ONLY_NEW),
    ("from_failure", "if len(recs) < 2:", "if len(recs) < 1:", "one recording accepted", ONLY_NEW),
    ("from_failure", "if a.name and len(fjs) > 1:", "if False:", "case name reused for several failures", ONLY_NEW),
    ("from_failure", 'f.write(R.render(reps[0]["_traces"], reps[0].get("_fixture")))', 'f.write(R.render(reps[0]["_traces"], None))', "report without the minimal reproduction", ONLY_NEW),
    ("from_failure", 'return {k: v for k, v in rep.items() if not k.startswith("_")}', "return dict(rep)", "internal data in the json output", ONLY_NEW),
    ("regress", "if fx.get(\"format\") == TR.FORMAT:", "if False:", "trace cases need a replay hook", ONLY_NEW),
    ("regress", "if oracle is None:", "if False:", "a missing replay hook is not reported", ONLY_NEW),
    ("regress", "except TR.BadFixture as e:", "except ZeroDivisionError as e:", "a damaged trace case crashes the runner", ONLY_NEW),
    ("gentest", "if fx.get(\"format\") == TR.FORMAT:", "if False:", "trace cases get a cmake file", ONLY_NEW),
    ("gentest", "dict(expect) if expect else", "None if expect else", "expectation override ignored", ONLY_NEW),
    ("flight", 'if argv and argv[0] == "from-failure":', "if False:", "from-failure subcommand missing", ONLY_NEW),
]
MUTATIONS += NEW_MUTATIONS
MUTATIONS += [
    ("shrink", "cmd = shlex.split(cmd, posix=True)", "cmd = shlex.split(cmd, posix=False)",
     "POSIX path quotes retained", "PortabilityTests"),
    ("shrink", "elif c == '\"':\n                if inq and i + 1 < n and s[i + 1] == '\"':",
     "elif c == '\"':\n                if False:", "Windows doubled quotes discarded", "PortabilityTests"),
]
TWIN_MODS = ["cofr", "schema", "analyze", "shrink", "gentest", "report", "regress", "flight", "toy_replay", "trace_replay", "from_failure"]


def _one_mutation(i, mod, old, new, why, only=None):
    d = tempfile.mkdtemp(prefix="cofr_mut%d_" % i)
    try:
        for m in TWIN_MODS:
            shutil.copy(os.path.join(HERE, m + ".py"), d)
        p = os.path.join(d, mod + ".py")
        with open(p, encoding="utf-8") as f:
            text = f.read()
        if text.count(old) != 1:
            return "TWIN BROKEN: %s: %s matches %d times" % (mod, why, text.count(old)), False
        with open(p, "w", encoding="utf-8") as f:
            f.write(text.replace(old, new))
        env = dict(os.environ, COFR_SRC=d)
        for extra in (["--quick"], []):      # the fast tests catch most mutants; the slow ones (replay programs, CLI) run only for the rest
            r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "run_tests.py"), "--no-cpp", "--no-twin", "--fail-fast"] + (["--only", only] if only else []) + extra,
                               capture_output=True, text=True, env=env, errors="replace")
            if r.returncode != 0:
                break
        if r.returncode == 0:
            return "SURVIVED: %s: %s" % (mod, why), False
        return "caught  : %s: %s" % (mod, why), True
    finally:
        shutil.rmtree(d, ignore_errors=True)


def twin():
    from concurrent.futures import ThreadPoolExecutor
    survived = []
    with ThreadPoolExecutor(max_workers=4) as ex:
        for msg, ok in ex.map(lambda t: _one_mutation(*t), [(i,) + m for i, m in enumerate(MUTATIONS)]):
            print(msg)
            if not ok:
                survived.append(msg)
    return survived


class PortabilityTests(unittest.TestCase):
    def test_windows_command_quotes(self):
        cases = [
            ('"C:\\Program Files\\Python\\python.exe" -I "C:\\replay tools\\check.py" {fixture}',
             [r"C:\Program Files\Python\python.exe", "-I", r"C:\replay tools\check.py", "{fixture}"]),
            ('"a""b" {fixture}', ['a"b', "{fixture}"]),
            ('"a""b""c" {fixture}', ['a"b"c', "{fixture}"]),
            ('"" {fixture}', ["", "{fixture}"]),
        ]
        for command, expected in cases:
            with self.subTest(command=command):
                self.assertEqual(SH._msvcrt_split(command), expected)
                with mock.patch.object(SH.os, "name", "nt"):
                    self.assertEqual(SH.CommandOracle(command).cmd, expected)

    def test_windows_list2cmdline_roundtrips(self):
        import random
        rng = random.Random(155)
        alphabet = 'abc XYZ\\"\t012'
        for _ in range(2000):
            args = [''.join(rng.choice(alphabet) for _ in range(rng.randrange(25)))
                    for _ in range(rng.randrange(1, 8))]
            self.assertEqual(SH._msvcrt_split(subprocess.list2cmdline(args)), args)

    def test_posix_command_quotes(self):
        command = '"/opt/replay tools/python" -I "/tmp/check script.py" {fixture}'
        with mock.patch.object(SH.os, "name", "posix"):
            self.assertEqual(SH.CommandOracle(command).cmd,
                             ["/opt/replay tools/python", "-I", "/tmp/check script.py", "{fixture}"])

    def test_visual_studio_cache_detection(self):
        self.assertTrue(_uses_visual_studio("CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026\n"))
        self.assertTrue(_uses_visual_studio("CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022\n"))
        self.assertFalse(_uses_visual_studio("CMAKE_CXX_COMPILER:FILEPATH=C:/Visual Studio/cl.exe\n"
                                            "CMAKE_GENERATOR:INTERNAL=Ninja\n"))
        self.assertFalse(_uses_visual_studio("CMAKE_GENERATOR:INTERNAL=NMake Makefiles\n"))
        self.assertFalse(_uses_visual_studio(""))

    def test_configure_generator_fallback_order(self):
        self.assertEqual(_configure_generators(True), [
            ["-G", "Visual Studio 18 2026", "-A", "x64"],
            ["-G", "Visual Studio 17 2022", "-A", "x64"],
            ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"],
        ])
        self.assertEqual(_configure_generators(False), [["-DCMAKE_BUILD_TYPE=Release"]])


# ---------------------------------------------------------------- C++ phase

def _configure_generators(windows):
    return ([
        ["-G", "Visual Studio 18 2026", "-A", "x64"],
        ["-G", "Visual Studio 17 2022", "-A", "x64"],
        ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"],
    ] if windows else [["-DCMAKE_BUILD_TYPE=Release"]])


def _uses_visual_studio(cache_text):
    return any(line.startswith("CMAKE_GENERATOR:INTERNAL=") and "Visual Studio" in line
               for line in cache_text.splitlines())

def cpp_phase(cross_dir):
    """Configure and build the recorder and its tests with cmake (one compile at a time),
    run every ctest case (unit, golden, perf and the mutation twins), then run the golden group once more with
    COFR_EMIT_DIR so it leaves the files the cross tests read. Returns (ok, summary)."""
    cmake, ctest = shutil.which("cmake"), shutil.which("ctest")
    if not cmake or not ctest:
        return False, "cmake and ctest are needed for the C++ tests (use --no-cpp to skip them)"
    build = os.environ.get("COFR_BUILD_DIR") or os.path.join(tempfile.gettempdir(), "cofr-flight-build")
    env = dict(os.environ)
    env.pop("CMAKE_TOOLCHAIN_FILE", None)       # a cross-compile toolchain set for another project would break the host build
    os.makedirs(build, exist_ok=True)
    cache = os.path.join(build, "CMakeCache.txt")
    if not os.path.exists(cache):
        gens = _configure_generators(os.name == "nt")
        for g in gens:
            r = subprocess.run([cmake, "-S", CPP_DIR, "-B", build] + g, capture_output=True, text=True, env=env, errors="replace")
            if r.returncode == 0:
                break
            shutil.rmtree(build, ignore_errors=True)
            os.makedirs(build, exist_ok=True)
        else:
            return False, "cmake configure failed:\n" + (r.stdout + r.stderr)[-1500:]
    vs = _uses_visual_studio(read_text(cache))
    cmd = [cmake, "--build", build, "--config", "Release", "--parallel", "1"]
    if vs:
        cmd += ["--", "/nologo", "/v:minimal", "/p:IgnoreWarnIntDirInTempDetected=true"]
    r = subprocess.run(cmd, capture_output=True, text=True, env=env, errors="replace")
    if r.returncode != 0:
        return False, "C++ build failed:\n" + (r.stdout + r.stderr)[-3000:]
    t = subprocess.run([ctest, "--test-dir", build, "-C", "Release", "--output-on-failure"], capture_output=True, text=True, env=env, errors="replace")
    tail = [ln for ln in t.stdout.splitlines() if "tests passed" in ln or "tests failed" in ln]
    if t.returncode != 0:
        return False, "C++ tests failed:\n" + t.stdout[-3000:]
    exe = [p for p in glob.glob(os.path.join(build, "**", "cofr_tests*"), recursive=True) if os.path.isfile(p) and p.endswith(("cofr_tests", "cofr_tests.exe"))]
    if not exe:
        return False, "cofr_tests was not built"
    e2 = dict(env, COFR_EMIT_DIR=cross_dir, COFR_GOLDEN_DIR=GOLDEN)
    r = subprocess.run([exe[0], "--group", "golden"], capture_output=True, text=True, env=e2, errors="replace")
    if r.returncode != 0:
        return False, "the C++ cross test files could not be written:\n" + r.stdout[-2000:]
    listing = subprocess.run([exe[0], "--list"], capture_output=True, text=True, env=env, errors="replace").stdout.split()
    n_twin = sum(1 for x in listing if x.startswith("twin."))
    return True, "C++: %s (%d tests in cofr_tests, %d of them mutation twins)" % (tail[0].strip() if tail else "ctest ok", len(listing), n_twin)


def read_text(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


# ---------------------------------------------------------------- main

def main():
    argv = sys.argv[1:]
    quick, no_cpp, no_twin = "--quick" in argv, "--no-cpp" in argv or "--quick" in argv, "--no-twin" in argv or "--quick" in argv
    cross = None
    try:
        if not no_cpp:
            cross = tempfile.mkdtemp(prefix="cofr_cross_")
            write_python_trace(os.path.join(cross, "from_python.cofr"))
            ok, msg = cpp_phase(cross)
            print(msg)
            if not ok:
                print("TESTS FAILED")
                return 1
            os.environ["COFR_CROSS_DIR"] = cross     # the cross tests, and the twin's child runs, read it
        suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
        if "--only" in argv:      # --only ClassA,ClassB : just those test classes
            want = argv[argv.index("--only") + 1].split(",")
            suite = unittest.TestSuite(t for group in suite for t in group if type(t).__name__ in want)
        res = unittest.TextTestRunner(verbosity=1, failfast="--fail-fast" in argv).run(suite)
        if not res.wasSuccessful():
            print("TESTS FAILED")
            return 1
        skipped = len(res.skipped)
        print("Python: %d tests run%s" % (res.testsRun, ", %d skipped" % skipped if skipped else ""))
        if quick:
            print("QUICK OK (fast Python tests only; run without --quick for ALL OK)")
            return 0
        if no_twin:
            print("TESTS OK (the mutation twin did not run, so this is not ALL OK)")
            return 0
        survived = twin()
        if survived:
            print("%d mutation(s) survived" % len(survived))
            return 1
        print("%d Python mutations, all caught" % len(MUTATIONS))
        if no_cpp:
            print("PYTHON OK (the C++ phase did not run, so this is not ALL OK)")
            return 0
        print("ALL OK")
        return 0
    finally:
        if cross:
            shutil.rmtree(cross, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
