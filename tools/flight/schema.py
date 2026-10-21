"""The schema of a co-op flight record: names for the numbers a game chose, and the layout of game-defined records.

A game keeps its schema as JSON (the recorder writes it into every file as a Schema record; the analyzer can also load
a file with --schema). Every key is optional: a key that is present replaces the default for that key, a key that is
missing keeps the default. See FORMAT.md section 5.

  game                 free text
  digest_categories    {"0": "story", ...}   names of the digest categories your game uses
  marker_kinds         {"1": "desync", ...}  names of Marker kinds (kind 1 = desync and 4 = test step keep their meaning)
  ownership_ops        {"0": "grant", ...}   names of Ownership ops
  ownership_owner_ops  [0, 1]                the ops that give an actor an owner (the analyzer checks only these)
  message_types        {"4": "batch", ...}   names of the message types a decoder reports for Net records
  txn_messages         [4, 5]                message types whose seq range is a transaction range; null = every decoded message
  apply_flags          {"1": "snapshot", ...} names of the bits of a TxnApplied record's flags
  restate_mask         7                     the flag bits of applies that restate state instead of replaying a transaction
  custom_records       {"40": {"name": "Spawn", "fields": [{"name": "id", "type": "u32"}, ...]}}  record types 32..255
"""
import json
import struct

FIELD_TYPES = {"u8": "<B", "u16": "<H", "u32": "<I", "u64": "<Q", "i8": "<b", "i16": "<h", "i32": "<i", "i64": "<q", "f32": "<f", "f64": "<d"}
VARIABLE_TYPES = ("str16", "bytes16", "bytes")   # str16 / bytes16: u16 length first; bytes: the rest of the payload
FIRST_CUSTOM = 32

DEFAULT = {
    "game": "example",
    "digest_categories": {"0": "story", "1": "actors", "2": "containers", "3": "objects", "4": "other"},
    "marker_kinds": {"0": "note", "1": "desync", "2": "join", "3": "leave", "4": "test-step", "5": "save", "6": "load", "7": "rebase"},
    "ownership_ops": {"0": "grant", "1": "transfer", "2": "release", "3": "rejectedStale", "4": "rejectedEcho", "5": "rejectedNonOwner"},
    "ownership_owner_ops": [0, 1],
    "apply_flags": {"1": "snapshot", "2": "repair", "4": "baseline", "8": "catch-up"},
    "restate_mask": 7,
}
KNOWN_KEYS = set(DEFAULT) | {"message_types", "txn_messages", "custom_records"}


class SchemaError(Exception):
    pass


def _int_table(name, value):
    if not isinstance(value, dict):
        raise SchemaError("%s must be an object like {\"0\": \"name\"}" % name)
    out = {}
    for k, v in value.items():
        try:
            n = int(k)
        except (TypeError, ValueError):
            raise SchemaError("%s: key %r is not a number" % (name, k))
        if not isinstance(v, str):
            raise SchemaError("%s: the name for %s must be a string" % (name, k))
        out[n] = v
    return out


def _int_list(name, value):
    if not isinstance(value, list) or not all(isinstance(x, int) and not isinstance(x, bool) for x in value):
        raise SchemaError("%s must be a list of numbers" % name)
    return list(value)


class Schema:
    def __init__(self, data=None):
        merged = json.loads(json.dumps(DEFAULT))
        if data:
            if not isinstance(data, dict):
                raise SchemaError("a schema is a JSON object")
            merged.update(data)
        self.game = str(merged.get("game", ""))
        self.digest_categories = _int_table("digest_categories", merged["digest_categories"])
        self.marker_kinds = _int_table("marker_kinds", merged["marker_kinds"])
        self.ownership_ops = _int_table("ownership_ops", merged["ownership_ops"])
        self.ownership_owner_ops = set(_int_list("ownership_owner_ops", merged["ownership_owner_ops"]))
        self.message_types = _int_table("message_types", merged.get("message_types", {}))
        txn = merged.get("txn_messages")
        self.txn_messages = None if txn is None else set(_int_list("txn_messages", txn))
        self.apply_flags = _int_table("apply_flags", merged["apply_flags"])
        mask = merged["restate_mask"]
        if not isinstance(mask, int) or isinstance(mask, bool) or not 0 <= mask <= 255:
            raise SchemaError("restate_mask must be a number from 0 to 255")
        self.restate_mask = mask
        self.custom_records = {}
        raw = merged.get("custom_records", {})
        if not isinstance(raw, dict):
            raise SchemaError("custom_records must be an object")
        for k, spec in raw.items():
            try:
                t = int(k)
            except (TypeError, ValueError):
                raise SchemaError("custom_records: key %r is not a number" % (k,))
            if not FIRST_CUSTOM <= t <= 255:
                raise SchemaError("custom_records: type %d is outside 32..255" % t)
            if not isinstance(spec, dict) or not isinstance(spec.get("name"), str) or not spec["name"]:
                raise SchemaError("custom_records %d needs a name" % t)
            fields = spec.get("fields", [])
            if not isinstance(fields, list):
                raise SchemaError("custom_records %d: fields must be a list" % t)
            seen = set()
            clean = []
            for f in fields:
                if not isinstance(f, dict) or not isinstance(f.get("name"), str) or f.get("type") not in list(FIELD_TYPES) + list(VARIABLE_TYPES):
                    raise SchemaError("custom_records %d: each field needs a name and one of the types %s" % (t, ", ".join(list(FIELD_TYPES) + list(VARIABLE_TYPES))))
                if f["name"] in seen:
                    raise SchemaError("custom_records %d: field %s appears twice" % (t, f["name"]))
                seen.add(f["name"])
                clean.append({"name": f["name"], "type": f["type"]})
            for i, f in enumerate(clean):
                if f["type"] == "bytes" and i != len(clean) - 1:
                    raise SchemaError("custom_records %d: a bytes field must be the last one" % t)
            self.custom_records[t] = {"name": spec["name"], "fields": clean}
        self._extra = {k: v for k, v in merged.items() if k not in KNOWN_KEYS}

    # -- names --
    def category(self, n):
        return self.digest_categories.get(n, "cat%d" % n)

    def marker_kind(self, n):
        return self.marker_kinds.get(n, "kind%d" % n)

    def own_op(self, n):
        return self.ownership_ops.get(n, "op%d" % n)

    def message(self, n):
        return self.message_types.get(n, "msg%d" % n)

    def flag_names(self, flags):
        return [name for bit, name in sorted(self.apply_flags.items()) if flags & bit]

    def is_txn_message(self, n):
        return self.txn_messages is None or n in self.txn_messages

    # -- output --
    def to_dict(self):
        d = {
            "game": self.game,
            "digest_categories": {str(k): v for k, v in sorted(self.digest_categories.items())},
            "marker_kinds": {str(k): v for k, v in sorted(self.marker_kinds.items())},
            "ownership_ops": {str(k): v for k, v in sorted(self.ownership_ops.items())},
            "ownership_owner_ops": sorted(self.ownership_owner_ops),
            "apply_flags": {str(k): v for k, v in sorted(self.apply_flags.items())},
            "restate_mask": self.restate_mask,
        }
        if self.message_types:
            d["message_types"] = {str(k): v for k, v in sorted(self.message_types.items())}
        if self.txn_messages is not None:
            d["txn_messages"] = sorted(self.txn_messages)
        if self.custom_records:
            d["custom_records"] = {str(k): v for k, v in sorted(self.custom_records.items())}
        d.update(self._extra)
        return d

    def to_json(self):
        return json.dumps(self.to_dict(), indent=1, sort_keys=False)


def from_json(text):
    try:
        data = json.loads(text)
    except ValueError as e:
        raise SchemaError("the schema is not valid JSON: %s" % e)
    return Schema(data)


def load(path):
    with open(path, encoding="utf-8") as f:
        return from_json(f.read())


def default():
    return Schema()


# ---------------------------------------------------------------- game-defined record bodies

class ShortBody(Exception):
    pass


def decode_custom(spec, body):
    """Decode the payload bytes of a custom record into {field: value}. Raises ShortBody when the payload is too short."""
    out = {}
    pos = 0
    for f in spec["fields"]:
        t = f["type"]
        if t in FIELD_TYPES:
            size = struct.calcsize(FIELD_TYPES[t])
            if pos + size > len(body):
                raise ShortBody()
            out[f["name"]] = struct.unpack_from(FIELD_TYPES[t], body, pos)[0]
            pos += size
        elif t in ("str16", "bytes16"):
            if pos + 2 > len(body):
                raise ShortBody()
            n = struct.unpack_from("<H", body, pos)[0]
            pos += 2
            if pos + n > len(body):
                raise ShortBody()
            raw = bytes(body[pos:pos + n])
            out[f["name"]] = raw.decode("utf-8", "replace") if t == "str16" else raw
            pos += n
        else:  # bytes: the rest
            out[f["name"]] = bytes(body[pos:])
            pos = len(body)
    return out


def encode_custom(spec, values):
    out = bytearray()
    for f in spec["fields"]:
        t = f["type"]
        v = values.get(f["name"], 0 if t in FIELD_TYPES else (b"" if t != "str16" else ""))
        if t in FIELD_TYPES:
            out += struct.pack(FIELD_TYPES[t], v)
        elif t in ("str16", "bytes16"):
            raw = v.encode("utf-8") if t == "str16" else bytes(v)
            raw = raw[:0xFFFF]
            out += struct.pack("<H", len(raw)) + raw
        else:
            out += bytes(v)
    return bytes(out)
