"""Reader, writer and pretty-printer for the co-op flight record format (.cofr, version 1). Spec: FORMAT.md.

A trace is {"header": {...}, "records": [ {...}, ... ], "truncated": bool, "warnings": [str], "name": str,
            "schema": Schema, "meta": {...}}.
Every record is a dict with at least: idx, type, name, flags, mono_us (the recorder's tUs), frame.
The reader never raises on a damaged tail: a half-written last record sets truncated=True.
It raises CofrError only when the file is not a v1 trace at all (bad magic, unsupported version, short header).
dumps / dump write the same layout (used by the tests and by tools that add records to a trace).
Engine-free: nothing here knows any game.
"""
import hashlib
import json
import struct

import schema as S

MAGIC = b"COFR"
VERSION = 1
HEADER_SIZE = 64
RECORD_HEADER = 16
NET_PREFIX = 36

T_META, T_NET_OUT, T_NET_IN, T_TXN_CAPTURED, T_TXN_APPLIED, T_OWNERSHIP, T_DIGEST, T_MARKER, T_SCHEMA = 1, 2, 3, 4, 5, 6, 7, 8, 9
FIRST_CUSTOM = 32
TYPE_NAMES = {1: "Meta", 2: "NetOut", 3: "NetIn", 4: "TxnCaptured", 5: "TxnApplied", 6: "Ownership", 7: "Digest", 8: "Marker", 9: "Schema"}
REASONS = {0: "manual", 1: "desync", 2: "crash", 3: "shutdown", 4: "test"}
ROLES = {0: "host", 1: "client", 2: "sim"}
NONE_SLOT = 255
FLAG_TRUNCATED, FLAG_DERIVED, FLAG_DETAIL, FLAG_BUCKETED = 1, 2, 4, 8     # record flags
MARKER_DESYNC, MARKER_TEST_STEP = 1, 4                                     # the marker kinds other tools rely on
DECODED, DECODE_FAILED, NOT_DECODED = 0, 1, 2

M64 = (1 << 64) - 1
HASH_SEED = 14695981039346656037     # FNV-1a 64 offset basis
DEFAULT_BUCKETS = 256


class CofrError(Exception):
    pass


class _Short(Exception):
    pass


# ---------------------------------------------------------------- hashing (same algorithms as the C++ recorder)

def hash64(data, seed=HASH_SEED):
    h = seed & M64
    for b in bytes(data):
        h = ((h ^ b) * 1099511628211) & M64
    return h


def _splitmix(x):
    x = (x + 0x9E3779B97F4A7C15) & M64
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & M64
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & M64
    return x ^ (x >> 31)


def bucket_mix(key, value_hash):
    return _splitmix((key ^ _splitmix(value_hash)) & M64) & 0xFFFFFFFF


def bucketed_digest(entries, buckets=DEFAULT_BUCKETS):
    """(shards, hash) of an iterable of (key, value_hash), exactly what the C++ bucketedDigest computes."""
    shards = [0] * buckets
    n = 0
    for key, vh in entries:
        shards[key % buckets] = (shards[key % buckets] + bucket_mix(key, vh)) & 0xFFFFFFFF
        n += 1
    raw = b"".join(struct.pack("<I", s) for s in shards)
    return shards, hash64(raw, HASH_SEED + n)


class Cursor:
    def __init__(self, data, pos=0, end=None):
        self.d = data
        self.p = pos
        self.end = len(data) if end is None else end

    def left(self):
        return self.end - self.p

    def take(self, n):
        if n < 0 or self.p + n > self.end:
            raise _Short()
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack("<H", self.take(2))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def u64(self):
        return struct.unpack("<Q", self.take(8))[0]

    def str16(self):
        return self.take(self.u16()).decode("utf-8", "replace")


class Buf:
    def __init__(self):
        self.b = bytearray()

    def u8(self, v):
        self.b.append(v & 0xFF)
        return self

    def u16(self, v):
        self.b += struct.pack("<H", v & 0xFFFF)
        return self

    def u32(self, v):
        self.b += struct.pack("<I", v & 0xFFFFFFFF)
        return self

    def u64(self, v):
        self.b += struct.pack("<Q", v & M64)
        return self

    def str16(self, v):
        raw = v.encode("utf-8")[:0xFFFF]
        return self.u16(len(raw)).raw(raw)

    def raw(self, v):
        self.b += v
        return self


# ---------------------------------------------------------------- record bodies

def _dec_meta(c, r):
    r["meta"] = {}
    while c.left() >= 4:
        k = c.str16()
        r["meta"][k] = c.str16()


def _enc_meta(b, r):
    for k, v in r.get("meta", {}).items():
        b.str16(k).str16(v)


def _dec_net(c, r):
    r["peer"] = c.u32()
    r["channel"] = c.u8()
    r["mux"] = c.u8()             # the first byte of the datagram
    r["msg_type"] = c.u8()
    r["decode"] = c.u8()          # 0 decoded, 1 the decoder refused or the datagram was cut, 2 no decoder
    r["session"] = c.u32()
    r["epoch"] = c.u32()
    r["seq"] = c.u64()
    r["seq_hi"] = c.u64()
    r["raw_len"] = c.u32()
    r["raw"] = c.take(c.left())
    r["dhash"] = int.from_bytes(hashlib.blake2b(r["raw"], digest_size=4).digest(), "little")   # pairs a send with its receive
    r["truncated"] = bool(r["flags"] & FLAG_TRUNCATED)


def _enc_net(b, r):
    raw = r.get("raw", b"")
    b.u32(r["peer"]).u8(r.get("channel", 0)).u8(r.get("mux", raw[0] if raw else 0)).u8(r.get("msg_type", 0)).u8(r.get("decode", NOT_DECODED))
    b.u32(r.get("session", 0)).u32(r.get("epoch", 0)).u64(r.get("seq", 0)).u64(r.get("seq_hi", r.get("seq", 0)))
    b.u32(r.get("raw_len", len(raw))).raw(raw)


def _dec_captured(c, r):
    r["kind"] = c.u8()            # 0 a commit the machine decided, 1 an intent it asked for
    r["intent_id"] = c.u32()
    r["seq"] = c.u64()
    r["effect_count"] = c.u32()
    r["hash"] = c.u64()
    r["cause"] = c.u8()
    r["epoch"] = c.u32()


def _enc_captured(b, r):
    b.u8(r.get("kind", 0)).u32(r.get("intent_id", 0)).u64(r.get("seq", 0)).u32(r.get("effect_count", 1)).u64(r.get("hash", 0))
    b.u8(r.get("cause", 0)).u32(r.get("epoch", 0))


def _dec_applied(c, r):
    r["seq"] = c.u64()
    r["epoch"] = c.u32()
    r["origin"] = c.u8()
    r["index"] = c.u32()
    r["applied_flags"] = c.u8()
    r["cause"] = c.u8()
    r["effect_tag"] = c.u8()
    r["hash"] = c.u64()


def _enc_applied(b, r):
    b.u64(r.get("seq", 0)).u32(r.get("epoch", 0)).u8(r.get("origin", 0)).u32(r.get("index", 0)).u8(r.get("applied_flags", 0))
    b.u8(r.get("cause", 0)).u8(r.get("effect_tag", 0)).u64(r.get("hash", 0))


def _dec_own(c, r):
    r["op"] = c.u8()
    r["actor"] = c.u64()
    r["ref"] = "actor:%X" % r["actor"]
    r["epoch"] = c.u32()
    r["from_slot"] = c.u8()
    r["to_slot"] = c.u8()
    r["reason"] = c.u8()
    r["txn_id"] = c.u64()


def _enc_own(b, r):
    b.u8(r["op"]).u64(r["actor"]).u32(r.get("epoch", 0)).u8(r.get("from_slot", NONE_SLOT)).u8(r.get("to_slot", NONE_SLOT))
    b.u8(r.get("reason", 0)).u64(r.get("txn_id", 0))


def _dec_digest(c, r):
    r["category"] = c.u8()
    r["epoch"] = c.u32()
    r["seq"] = c.u64()
    r["hash"] = c.u64()
    r["item_count"] = c.u32()
    n = c.u16()
    if n * 4 > c.left():
        raise _Short()
    r["shards"] = [c.u32() for _ in range(n)]
    n = c.u32()
    if n * 12 > c.left():
        raise _Short()
    r["keys"] = {}
    for _ in range(n):
        k = c.u64()
        r["keys"][k] = c.u32()
    r["derived"] = bool(r["flags"] & FLAG_DERIVED)
    r["detail"] = bool(r["flags"] & FLAG_DETAIL)       # carries only per-key entries (several records may belong to one digest)
    r["bucketed"] = bool(r["flags"] & FLAG_BUCKETED)   # the shards are key buckets: bucket = key mod number of shards


def _enc_digest(b, r):
    b.u8(r["category"]).u32(r.get("epoch", 0)).u64(r.get("seq", 0)).u64(r.get("hash", 0)).u32(r.get("item_count", 0))
    shards = r.get("shards", [])
    b.u16(len(shards))
    for s in shards:
        b.u32(s)
    keys = r.get("keys", {})
    b.u32(len(keys))
    for k, h in keys.items():
        b.u64(k).u32(h)


def _dec_marker(c, r):
    r["kind"] = c.u16()
    r["text"] = c.str16()


def _enc_marker(b, r):
    b.u16(r.get("kind", 0)).str16(r.get("text", ""))


_DEC = {1: _dec_meta, 2: _dec_net, 3: _dec_net, 4: _dec_captured, 5: _dec_applied, 6: _dec_own, 7: _dec_digest, 8: _dec_marker}
_ENC = {1: _enc_meta, 2: _enc_net, 3: _enc_net, 4: _enc_captured, 5: _enc_applied, 6: _enc_own, 7: _enc_digest, 8: _enc_marker}


# ---------------------------------------------------------------- file level

def parse_header(data):
    if len(data) < 4 or data[:4] != MAGIC:
        raise CofrError("not a .cofr file (bad magic)")
    if len(data) < HEADER_SIZE:
        raise CofrError("header is cut short")
    c = Cursor(data, 4)
    h = {}
    h["version"] = c.u16()
    h["header_size"] = c.u16()
    if h["version"] != VERSION:
        raise CofrError("unsupported .cofr version %d (this reader handles %d)" % (h["version"], VERSION))
    if h["header_size"] < HEADER_SIZE or h["header_size"] > len(data):
        raise CofrError("bad header size %d" % h["header_size"])
    h["slot"] = c.u8()
    h["role"] = c.u8()
    h["reason"] = c.u8()
    c.u8()
    h["proto_version"] = c.u32()
    h["wall_unix_ms"] = c.u64()
    h["dump_t_us"] = c.u64()
    h["records_dropped"] = c.u64()
    h["record_count"] = c.u64()
    h["session"] = c.u32()
    h["epoch"] = c.u32()
    h["role_name"] = ROLES.get(h["role"], "role%d" % h["role"])
    h["reason_name"] = REASONS.get(h["reason"], "reason%d" % h["reason"])
    h["instance"] = ""
    return h


def loads(data, name="<memory>", schema=None):
    """Parse a trace. `schema` (a Schema) overrides the schema stored in the file."""
    data = bytes(data)
    header = parse_header(data)
    raw = []
    pos = header["header_size"]
    while len(data) - pos >= RECORD_HEADER:
        rtype, flags, plen, frame, t_us = struct.unpack_from("<BBHIQ", data, pos)
        if len(data) - pos - RECORD_HEADER < plen:
            break
        raw.append((rtype, flags, plen, frame, t_us, pos))
        pos += RECORD_HEADER + plen
    truncated = len(data) - pos > 0
    warnings = []
    if truncated:
        warnings.append("file ends in the middle of a record (crash dump?); records before it are intact")

    # the schema first, so game-defined records can be decoded
    file_schema = None
    for rtype, flags, plen, frame, t_us, off in raw:
        if rtype == T_SCHEMA:
            try:
                file_schema = S.from_json(data[off + RECORD_HEADER:off + RECORD_HEADER + plen].decode("utf-8", "replace"))
            except S.SchemaError as e:
                warnings.append("the schema record in the file is not usable (%s); the default schema is used" % e)
            break
    sch = schema or file_schema or S.default()

    records = []
    for rtype, flags, plen, frame, t_us, off in raw:
        body = Cursor(data, off + RECORD_HEADER, off + RECORD_HEADER + plen)
        rec = {"idx": len(records), "type": rtype, "name": TYPE_NAMES.get(rtype, "Unknown%d" % rtype), "flags": flags,
               "mono_us": t_us, "frame": frame, "offset": off}
        if rtype >= FIRST_CUSTOM:
            rec["payload"] = bytes(body.take(body.left()))
            spec = sch.custom_records.get(rtype)
            if spec:
                rec["name"] = spec["name"]
                rec["custom"] = True
                try:
                    rec["fields"] = S.decode_custom(spec, rec["payload"])
                except S.ShortBody:
                    warnings.append("record %d (%s) at offset %d is shorter than its schema says; kept as raw bytes" % (rec["idx"], rec["name"], off))
                    rec["malformed"] = True
        elif rtype == T_SCHEMA:
            rec["json"] = bytes(body.take(body.left())).decode("utf-8", "replace")
        else:
            dec = _DEC.get(rtype)
            if dec is None:
                rec["payload"] = bytes(body.take(body.left()))
            else:
                try:
                    dec(body, rec)
                except _Short:
                    warnings.append("record %d (%s) at offset %d has a malformed body; kept without fields" % (rec["idx"], rec["name"], off))
                    rec["malformed"] = True
        records.append(rec)

    meta = {}
    for r in records:
        if r["type"] == T_META and "meta" in r:
            meta.update(r["meta"])
    header["instance"] = meta.get("label", "")
    return {"header": header, "records": records, "truncated": truncated, "warnings": warnings, "name": name, "schema": sch, "meta": meta}


def load(path, schema=None):
    with open(path, "rb") as f:
        return loads(f.read(), name=str(path), schema=schema)


def dumps(header, records, schema=None):
    """Encode a trace. header keys: slot, role, reason, proto_version, wall_unix_ms, dump_t_us, records_dropped, session, epoch.
    Each record: type (int), mono_us, frame, flags (optional) plus the fields of its type. A Meta record carries
    {"meta": {"label": "..."}}. A Schema record carries {"json": text} or {"schema": Schema}. A game-defined record
    carries {"payload": bytes} or {"fields": {...}} (the latter needs `schema` to know the layout)."""
    body = Buf()
    for r in records:
        p = Buf()
        t = r["type"]
        if t == T_SCHEMA:
            text = r["json"] if "json" in r else r["schema"].to_json()
            p.raw(text.encode("utf-8"))
        elif t >= FIRST_CUSTOM:
            if "payload" in r:
                p.raw(r["payload"])
            else:
                spec = (schema or S.default()).custom_records.get(t)
                if spec is None:
                    raise CofrError("record type %d has no payload and the schema does not describe it" % t)
                p.raw(S.encode_custom(spec, r.get("fields", {})))
        else:
            enc = _ENC.get(t)
            if enc is None:
                p.raw(r.get("payload", b""))
            else:
                enc(p, r)
        flags = r.get("flags", 0)
        if t in (T_NET_OUT, T_NET_IN) and r.get("truncated"):
            flags |= FLAG_TRUNCATED
        if t == T_DIGEST:
            if r.get("derived"):
                flags |= FLAG_DERIVED
            if r.get("detail"):
                flags |= FLAG_DETAIL
            if r.get("bucketed"):
                flags |= FLAG_BUCKETED
        if len(p.b) > 0xFFFF:
            raise CofrError("a record payload is longer than 65535 bytes")
        body.u8(t).u8(flags).u16(len(p.b)).u32(r.get("frame", 0)).u64(r.get("mono_us", 0)).raw(p.b)
    b = Buf()
    b.raw(MAGIC).u16(VERSION).u16(HEADER_SIZE)
    b.u8(header.get("slot", 0)).u8(header.get("role", 0)).u8(header.get("reason", 0)).u8(0)
    b.u32(header.get("proto_version", 0)).u64(header.get("wall_unix_ms", 0)).u64(header.get("dump_t_us", 0))
    b.u64(header.get("records_dropped", 0)).u64(len(records)).u32(header.get("session", 0)).u32(header.get("epoch", 0)).u64(0)
    return bytes(b.b) + bytes(body.b)


def dump(path, header, records, schema=None):
    with open(path, "wb") as f:
        f.write(dumps(header, records, schema))


def to_records(trace):
    """The records of a loaded trace in the form `dumps` takes (so a loaded trace can be written back unchanged)."""
    out = []
    for r in trace["records"]:
        r2 = dict(r)
        if r2.get("custom"):
            r2.pop("fields", None)   # the raw payload is what goes back
        out.append(r2)
    return out


# ---------------------------------------------------------------- pretty printing

def _hex(b, n=24):
    s = bytes(b[:n]).hex()
    return s + ("..." if len(b) > n else "")


def describe(r, schema=None):
    sch = schema or S.default()
    t = r["type"]
    if r.get("malformed"):
        return "malformed body"
    if t in (T_NET_IN, T_NET_OUT):
        arrow = "from" if t == T_NET_IN else "to"
        return "%s peer %d %s epoch=%d seq=%d..%d len=%d hash=%08x%s raw=%s" % (
            arrow, r["peer"], sch.message(r["msg_type"]) if r["decode"] == DECODED else "(not decoded)", r["epoch"], r["seq"], r["seq_hi"],
            r["raw_len"], r["dhash"], " (cut)" if r["truncated"] else "", _hex(r["raw"]))
    if t == T_TXN_CAPTURED:
        return "%s seq=%d intent=%d effects=%d hash=%016x epoch=%d" % (
            "commit" if r["kind"] == 0 else "intent", r["seq"], r["intent_id"], r["effect_count"], r["hash"], r["epoch"])
    if t == T_TXN_APPLIED:
        return "seq=%d[%d] origin=%d epoch=%d tag=%d hash=%016x %s" % (
            r["seq"], r["index"], r["origin"], r["epoch"], r["effect_tag"], r["hash"], ",".join(sch.flag_names(r["applied_flags"])))
    if t == T_OWNERSHIP:
        sl = lambda s: "-" if s == NONE_SLOT else str(s)
        return "%s %s epoch=%d %s->%s reason=%d txn=%d" % (
            sch.own_op(r["op"]), r["ref"], r["epoch"], sl(r["from_slot"]), sl(r["to_slot"]), r["reason"], r["txn_id"])
    if t == T_DIGEST:
        return "%s epoch=%d seq=%d hash=%016x items=%d shards=%d keys=%d%s" % (
            sch.category(r["category"]), r["epoch"], r["seq"], r["hash"], r["item_count"], len(r["shards"]),
            len(r["keys"]), " (derived from a message)" if r["derived"] else (" (key detail)" if r["detail"] else ""))
    if t == T_MARKER:
        return "[%s] %s" % (sch.marker_kind(r["kind"]), r["text"])
    if t == T_META:
        return " ".join("%s=%s" % kv for kv in r.get("meta", {}).items())
    if t == T_SCHEMA:
        return "game=%s (%d bytes of JSON)" % (sch.game, len(r.get("json", "")))
    if r.get("custom"):
        return " ".join("%s=%s" % (k, v if not isinstance(v, bytes) else _hex(v)) for k, v in r["fields"].items())
    return "payload %s" % _hex(r.get("payload", b""))


def format_header(h):
    return "slot=%d role=%s reason=%s instance=%r proto=%d session=%d dropped=%d wall_ms=%d" % (
        h["slot"], h["role_name"], h["reason_name"], h["instance"], h["proto_version"], h["session"], h["records_dropped"], h["wall_unix_ms"])


def format_record(r, schema=None):
    return "%6d %10.3fms f%-6d %-12s %s" % (r["idx"], r["mono_us"] / 1000.0, r["frame"], r["name"], describe(r, schema))


def pretty(trace, types=None, limit=None):
    sch = trace.get("schema")
    lines = ["# " + trace["name"], "# " + format_header(trace["header"])]
    n = 0
    for r in trace["records"]:
        if types and r["name"].lower() not in types:
            continue
        lines.append(format_record(r, sch))
        n += 1
        if limit and n >= limit:
            lines.append("... (limit %d)" % limit)
            break
    for w in trace["warnings"]:
        lines.append("# warning: " + w)
    return "\n".join(lines)
