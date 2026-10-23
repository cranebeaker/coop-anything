"""Align the traces of 2 to 4 machines and find the first divergence plus the messages that led to it.

align(traces)      -> Alignment
find_divergence(a) -> dict or None (the earliest problem; see _cand for the fields)
All times are microseconds on the reference machine's clock ("aligned_us").
Engine-free: everything game-specific comes from the traces and their schema.
"""
import re
import statistics

import cofr as C

GRACE_US = 2_000_000          # how long a machine may lag before a missing txn counts as a divergence
CHAIN_MAX = 60                # context records kept in a causal chain (direct records are always kept)
DEFAULT_WINDOW_US = 2_000_000
ENTRY_RE = re.compile(r"entry ([0-9a-fA-F]{16}) (\d+) ?(.*)", re.S)
REF_RE = re.compile(r"ref=#([0-9a-fA-F]+)")


class Alignment:
    def __init__(self):
        self.traces = {}          # slot -> trace
        self.slots = []
        self.ref_slot = None
        self.schema = None        # the schema of the first trace (names for categories, kinds, messages)
        self.offset_us = {}       # slot -> clock offset against the reference (positive = clock runs ahead)
        self.txns = {}            # seq -> slot -> {"captured": [rec], "applied": [rec]}
        self.digests = {}         # (category, epoch, seq, derived) -> slot -> rec (the last one if repeated)
        self.messages = []        # {"from","to","out","ins":[rec]}
        self.lost = []            # (from_slot, to_slot, out rec): sent, never seen arriving
        self.orphans = []         # (to_slot, from_slot, in rec): arrived, sender never logged it
        self.duplicates = []      # (from_slot, to_slot, out rec, extra in recs)
        self.anomalies = []
        self.t0 = 0
        self.entries = {}         # slot -> key -> text, from "entry <key> <hash> <text>" markers written at a desync
        self.end_us = {}          # slot -> aligned time of last record

    def t(self, slot, rec):
        return rec["mono_us"] + self._base[slot] - self.offset_us.get(slot, 0)

    _base = None


def _median(xs):
    return statistics.median(xs) if xs else None


def _map_peers(a):
    """A recorded `peer` is a transport peer id. It means the machine slot of the same number unless the trace says otherwise
    with a Meta entry `peer.<id>=<slot>` (the recorder's mapPeer). Fills r["peer_slot"] on every Net record."""
    for s in a.slots:
        tr = a.traces[s]
        known = {}
        for k, v in tr.get("meta", {}).items():
            if k.startswith("peer."):
                try:
                    known[int(k[5:])] = int(v)
                except ValueError:
                    pass
        for r in tr["records"]:
            if r["type"] in (C.T_NET_IN, C.T_NET_OUT) and not r.get("malformed"):
                r["peer_slot"] = known.get(r["peer"], r["peer"])


def align(traces):
    a = Alignment()
    a._base = {}
    a.schema = traces[0].get("schema") if traces else None
    slots_seen = set()
    for i, tr in enumerate(traces):
        s = tr["header"]["slot"]
        if s in slots_seen:
            a.anomalies.append("two traces claim slot %d; the second (%s) is renumbered to %d" % (s, tr["name"], 100 + i))
            s = 100 + i
        slots_seen.add(s)
        a.traces[s] = tr
        a._base[s] = tr["header"]["wall_unix_ms"] * 1000
    a.slots = sorted(a.traces)
    a.ref_slot = 0 if 0 in a.traces else a.slots[0]
    _map_peers(a)

    # message matching, FIFO per (sender, receiver, datagram hash, msg_type)
    outs = {}
    ins = []
    for s in a.slots:
        for r in a.traces[s]["records"]:
            if r.get("malformed"):
                continue
            if r["type"] == C.T_NET_OUT:
                outs.setdefault((s, r["peer_slot"], r["dhash"], r["msg_type"]), []).append(r)
            elif r["type"] == C.T_NET_IN:
                ins.append((s, r))
    # clock offsets need rough one-way deltas, using the raw clocks first
    pending = {k: list(v) for k, v in outs.items()}
    last_matched = {}
    deltas = {}
    for s, r in sorted(ins, key=lambda x: x[1]["mono_us"] + a._base[x[0]]):
        key = (r["peer_slot"], s, r["dhash"], r["msg_type"])
        q = pending.get(key)
        if q:
            o = q.pop(0)
            m = {"from": r["peer_slot"], "to": s, "out": o, "ins": [r]}
            a.messages.append(m)
            last_matched[key] = m
            deltas.setdefault((r["peer_slot"], s), []).append((r["mono_us"] + a._base[s]) - (o["mono_us"] + a._base[r["peer_slot"]]))
        elif key in last_matched:
            last_matched[key]["ins"].append(r)
        elif r["peer_slot"] in a.traces:
            a.orphans.append((s, r["peer_slot"], r))
        # sender not traced at all: nothing to compare with
    for key, q in pending.items():
        for o in q:
            if key[1] in a.traces:
                a.lost.append((key[0], key[1], o))
    for m in a.messages:
        if len(m["ins"]) > 1:
            a.duplicates.append((m["from"], m["to"], m["out"], m["ins"][1:]))

    # offsets: half the difference between the two directions removes symmetric latency
    for s in a.slots:
        if s == a.ref_slot:
            a.offset_us[s] = 0
            continue
        fwd = _median(deltas.get((a.ref_slot, s), []))
        back = _median(deltas.get((s, a.ref_slot), []))
        if fwd is not None and back is not None:
            a.offset_us[s] = int((fwd - back) / 2)
        elif fwd is not None or back is not None:
            a.offset_us[s] = 0
            a.anomalies.append("slot %d: messages only flow one way in the traces, clock offset taken as 0 (wall clocks trusted)" % s)
        else:
            a.offset_us[s] = 0

    for s in a.slots:
        tr = a.traces[s]
        a.end_us[s] = max((a.t(s, r) for r in tr["records"]), default=0)
        for r in tr["records"]:
            if r.get("malformed"):
                continue
            if r["type"] == C.T_TXN_CAPTURED:
                if r["seq"]:   # client intents carry no seq yet
                    a.txns.setdefault(r["seq"], {}).setdefault(s, {"captured": [], "applied": []})["captured"].append(r)
            elif r["type"] == C.T_TXN_APPLIED:
                a.txns.setdefault(r["seq"], {}).setdefault(s, {"captured": [], "applied": []})["applied"].append(r)
            elif r["type"] == C.T_DIGEST:
                per = a.digests.setdefault((r["category"], r["epoch"], r["seq"], r["derived"]), {})
                prev = per.get(s)
                if prev is None:
                    per[s] = dict(r, keys=dict(r["keys"]))
                elif r["detail"]:                       # more per-key entries of the same digest
                    prev["keys"].update(r["keys"])
                else:                                   # a newer main record: replaces hash and buckets, keeps entries seen so far
                    per[s] = dict(r, keys=dict(prev["keys"], **r["keys"]))
        ent = a.entries.setdefault(s, {})
        for r in tr["records"]:
            if r["type"] == C.T_MARKER and not r.get("malformed") and r["text"].startswith("entry "):
                m = ENTRY_RE.match(r["text"])
                if m:
                    ent[int(m.group(1), 16)] = m.group(3)
        if tr["header"]["records_dropped"]:
            a.anomalies.append("slot %d lost %d old records to ring wrap; early history is missing" % (s, tr["header"]["records_dropped"]))
        for w in tr["warnings"]:
            a.anomalies.append("slot %d: %s" % (s, w))
    a.t0 = min((a.t(s, r) for s in a.slots for r in a.traces[s]["records"]), default=0)   # printed times are relative to this
    return a


def format_alignment(a):
    out = ["slots: %s  (reference clock: slot %d)" % (a.slots, a.ref_slot)]
    for s in a.slots:
        tr = a.traces[s]
        out.append("  slot %d: %s  records=%d  clock offset=%+.3fms  ends at %.3fms" % (
            s, tr["header"]["role_name"], len(tr["records"]), a.offset_us[s] / 1000.0, (a.end_us[s] - a.t0) / 1000.0))
    out.append("messages matched: %d, lost: %d, duplicated in transit: %d, orphaned (no sender record): %d" % (
        len(a.messages), len(a.lost), len(a.duplicates), len(a.orphans)))
    out.append("txn seqs seen: %d  digests compared: %d" % (len(a.txns), len(a.digests)))
    for x in a.anomalies:
        out.append("  note: " + x)
    return "\n".join(out)


# ---------------------------------------------------------------- divergence

def _hx(k):
    return "0x%016X" % k


def _cand(kind, key, slots, t_us, detail, **extra):
    c = {"kind": kind, "key": key, "slots": sorted(slots), "aligned_us": t_us, "detail": detail}
    c.update(extra)
    return c


def _normal(a, r):
    """An applied effect that replays a committed txn (not a repair, baseline or snapshot restatement)."""
    return not r["applied_flags"] & a.schema.restate_mask


def _txn_candidates(a):
    cands = []
    for seq, per in sorted(a.txns.items()):
        # different content for the same effect of the same seq
        by_index = {}
        for s, d in per.items():
            for r in d["applied"]:
                if _normal(a, r):
                    by_index.setdefault(r["index"], {}).setdefault(s, set()).add(r["hash"])
        for idx in sorted(by_index):
            hashes = by_index[idx]
            flat = {}
            for s, hs in hashes.items():
                for h in hs:
                    flat.setdefault(h, set()).add(s)
            if len(flat) > 1:
                plural = max(flat.values(), key=len)
                everyone = set().union(*flat.values())
                odd = sorted(everyone - plural) if len(plural) > 1 else sorted(everyone)
                t = min(a.t(s, r) for s, d in per.items() for r in d["applied"] if _normal(a, r) and r["index"] == idx)
                cands.append(_cand("txn-hash", "txn:%d[%d]" % (seq, idx), hashes.keys(), t,
                                   "effect %d of txn seq %d differs between machines: %s" % (
                                       idx, seq, "; ".join("slot(s) %s hash %016x" % (sorted(v), h) for h, v in sorted(flat.items()))),
                                   values={s: sorted("%016x" % h for h in hs) for s, hs in hashes.items()}, suspects=odd, seq=seq))
                break   # one candidate per txn is enough; the chain shows the rest
        # applied twice
        for s, d in per.items():
            seen = {}
            for r in d["applied"]:
                if _normal(a, r):
                    seen.setdefault(r["index"], []).append(r)
            for idx, lst in sorted(seen.items()):
                if len(lst) > 1:
                    cands.append(_cand("txn-double", "txn:%d[%d]" % (seq, idx), [s], a.t(s, lst[1]),
                                       "slot %d applied effect %d of txn seq %d %d times" % (s, idx, seq, len(lst)),
                                       suspects=[s], seq=seq))
                    break
    # missing: applied somewhere, never applied on a machine that moved past it or had time to
    applied_by = {}
    for seq, per in a.txns.items():
        for s, d in per.items():
            if any(_normal(a, r) for r in d["applied"]) or d["captured"]:   # a host commit counts as applied on the host (it never logs an apply)
                applied_by.setdefault(s, set()).add(seq)
    for seq, per in sorted(a.txns.items()):
        have = {s for s in per if seq in applied_by.get(s, ())}
        if not have:
            continue
        times = []
        for s in have:
            norm = [r for r in per[s]["applied"] if _normal(a, r)]
            times += [a.t(s, r) for r in (norm or per[s]["captured"])]
        t_first = min(times)
        for s in a.slots:
            if s in have or s not in a.traces:
                continue
            mine = applied_by.get(s, set())
            gap = any(q > seq for q in mine)
            late = a.end_us[s] > t_first + GRACE_US
            if gap or late:
                cands.append(_cand("txn-missing", "txn:%d" % seq, [s] + sorted(have), t_first,
                                   "txn seq %d was applied on slot(s) %s but never on slot %d (%s)" % (
                                       seq, sorted(have), s, "it applied a later seq" if gap else "no apply within %.1fs while its trace kept going" % (GRACE_US / 1e6)),
                                   suspects=[s], seq=seq))
    return cands


def _digest_candidates(a):
    cands = []
    for (cat, epoch, seq, derived), per in sorted(a.digests.items()):
        if len(per) < 2:
            continue
        vals = {s: r["hash"] for s, r in per.items()}
        diff_keys = _diff_keys(per)
        diff_shards = _diff_shards(per)
        if len(set(vals.values())) <= 1 and not diff_keys and not diff_shards:
            continue
        groups = {}
        for s, v in vals.items():
            groups.setdefault(v, []).append(s)
        plural = max(groups.values(), key=len)
        suspects = sorted(s for s in vals if s not in plural) if len(plural) > 1 else sorted(vals)
        t = min(a.t(s, r) for s, r in per.items())
        what = diff_keys[:5] if diff_keys else ["shard %d" % i for i in diff_shards[:5]]
        key = diff_keys[0] if diff_keys else ("shard:%d" % diff_shards[0] if diff_shards else "seq:%d" % seq)
        cname = a.schema.category(cat)
        texts = []
        for k in diff_keys[:5]:
            ki = int(k, 16)
            texts.append("%s: %s" % (k, " vs ".join("slot %d %s" % (s, repr(a.entries.get(s, {}).get(ki, "(no text)"))) for s in sorted(per))))
        narrowing = narrow(a, per, diff_shards, diff_keys)
        cands.append(_cand("digest", "%s:%s" % (cname, key), per.keys(), t,
                           "%s digest (epoch %d, seq %d) differs: %s; differing %s" % (
                               cname, epoch, seq, "; ".join("slot(s) %s = %016x" % (sorted(v), h) for h, v in sorted(groups.items())),
                               "; ".join(texts) if texts else (", ".join(what) if what else "(no per-key data recorded)")),
                           values={s: "%016x" % v for s, v in vals.items()}, suspects=suspects, category=cat, digest_epoch=epoch,
                           digest_seq=seq, diff_keys=diff_keys, diff_shards=diff_shards, narrowing=narrowing))
    return cands


def narrow(a, per, diff_shards, diff_keys):
    """Digest -> bucket -> key -> each machine's value text.

    `per` is {slot: digest record} of one compared digest. Buckets come from its bucket hashes when they are key buckets
    (bucket = key mod number of buckets). Keys come from the per-key detail of the same digest; when it has none, from the
    newest other digest with detail records (the dump a desync writes) whose differing keys fall into the differing buckets.
    Value text comes from each machine's `entry` markers.
    """
    slots = sorted(per)
    nb = max((len(r["shards"]) for r in per.values()), default=0)
    bucketed = nb > 0 and any(r.get("bucketed") for r in per.values())
    source = "this digest" if diff_keys else None
    keys_by_slot = {s: per[s]["keys"] for s in slots}
    cand_keys = list(diff_keys)
    if not cand_keys and diff_shards and bucketed:
        # look in the other digests for per-key detail whose differing keys land in the differing buckets
        best = None
        for (cat, ep, sq, dv), other in a.digests.items():
            if other is per or len(other) < 2 or not any(r["keys"] for r in other.values()):
                continue
            ks = [k for k in (int(x, 16) for x in _diff_keys(other)) if k % nb in diff_shards]
            if ks:
                t = max(a.t(s, r) for s, r in other.items())
                if best is None or t > best[0]:
                    best = (t, other, ks, "%s digest (epoch %d, seq %d)" % (a.schema.category(cat), ep, sq))
        if best:
            keys_by_slot = {s: best[1].get(s, {"keys": {}})["keys"] for s in slots}
            cand_keys = [_hx(k) for k in sorted(best[2])]
            source = best[3]
    keys = []
    for k in cand_keys:
        ki = int(k, 16)
        keys.append({
            "key": k, "bucket": ki % nb if bucketed else None,
            "in_diff_bucket": (ki % nb) in diff_shards if bucketed else None,
            "values": {s: keys_by_slot.get(s, {}).get(ki) for s in slots},       # None = this machine has no such entry
            "texts": {s: a.entries.get(s, {}).get(ki) for s in slots},
        })
    return {"buckets": list(diff_shards) if bucketed else [], "bucket_count": nb if bucketed else 0, "keys": keys, "key_source": source}


def format_narrowing(nw, slots=None):
    if not nw or not (nw["buckets"] or nw["keys"]):
        return []
    out = ["  narrowing: %s" % ("bucket(s) %s of %d" % (", ".join(str(b) for b in nw["buckets"][:12]), nw["bucket_count"]) if nw["buckets"] else "no bucket data")]
    if not nw["keys"]:
        out.append("    key level: no per-key detail recorded (a desync dump or a digest with key detail is needed)")
    for k in nw["keys"][:10]:
        out.append("    key %s%s%s%s" % (k["key"], "" if k["bucket"] is None else " (bucket %d)" % k["bucket"],
                                        "" if k["in_diff_bucket"] in (True, None) else " NOT in a differing bucket",
                                        "  [from %s]" % nw["key_source"] if nw["key_source"] else ""))
        for s in sorted(k["values"]):
            v = k["values"][s]
            txt = k["texts"].get(s)
            out.append("      slot %d: %s%s" % (s, "(absent)" if v is None else "value hash %d" % v, "  " + txt if txt else ""))
    return out


def _diff_keys(per):
    allk = set()
    for r in per.values():
        allk |= set(r["keys"])
    out = []
    for k in sorted(allk):
        vs = {s: r["keys"].get(k) for s, r in per.items()}
        if len(set(vs.values())) > 1:
            out.append(_hx(k))
    return out


def _diff_shards(per):
    n = max((len(r["shards"]) for r in per.values()), default=0)
    out = []
    for i in range(n):
        vs = {r["shards"][i] if i < len(r["shards"]) else None for r in per.values()}
        if len(vs) > 1:
            out.append(i)
    return out


def _ownership_candidates(a):
    cands = []
    owner_ops = a.schema.ownership_owner_ops
    by = {}
    for s in a.slots:
        for r in a.traces[s]["records"]:
            if r["type"] == C.T_OWNERSHIP and not r.get("malformed") and r["op"] in owner_ops:
                by.setdefault((r["ref"], r["epoch"]), []).append((s, r))
    for (ref, ep), lst in sorted(by.items(), key=lambda kv: kv[0]):
        owners = {r["to_slot"] for _, r in lst}
        if len(owners) > 1:
            t = min(a.t(s, r) for s, r in lst)
            cands.append(_cand("ownership-conflict", "owner:%s" % ref, {s for s, _ in lst}, t,
                               "%s has more than one owner at epoch %d: slots %s" % (ref, ep, sorted(owners)),
                               suspects=sorted(owners), ref=ref, owner_epoch=ep))
    for s in a.slots:
        top = {}
        for r in a.traces[s]["records"]:
            if r["type"] == C.T_OWNERSHIP and not r.get("malformed") and r["op"] in owner_ops:
                prev = top.get(r["ref"])
                if prev is not None and r["epoch"] < prev:
                    cands.append(_cand("ownership-stale", "owner:%s" % r["ref"], [s], a.t(s, r),
                                       "slot %d accepted ownership of %s at epoch %d after already seeing epoch %d" % (s, r["ref"], r["epoch"], prev),
                                       suspects=[s], ref=r["ref"], owner_epoch=r["epoch"]))
                top[r["ref"]] = max(prev or 0, r["epoch"])
    return cands


def _marker_candidates(a):
    """A machine's own desync Marker (kind 1): the recorder or a test saw a problem that is not a visible state mismatch
    between traces (e.g. a stale update accepted). One candidate per distinct message head and slot; lowest priority."""
    cands = []
    seen = set()
    for s in a.slots:
        for r in a.traces[s]["records"]:
            if r["type"] == C.T_MARKER and not r.get("malformed") and r["kind"] == C.MARKER_DESYNC:
                head = r["text"].split(":")[0][:60]
                if (s, head) in seen:
                    continue
                seen.add((s, head))
                m = REF_RE.search(r["text"])
                extra = {"ref": "actor:%X" % int(m.group(1), 16)} if m else {}
                cands.append(_cand("desync-marker", "marker:%s" % head, [s], a.t(s, r), "slot %d reported: %s" % (s, r["text"][:300]),
                                   suspects=[s], **extra))
    return cands


_PRIO = {"desync-marker": 6, "txn-hash": 0, "txn-double": 1, "txn-missing": 2, "ownership-conflict": 3, "ownership-stale": 4, "digest": 5}


def find_all(a):
    c = _txn_candidates(a) + _digest_candidates(a) + _ownership_candidates(a) + _marker_candidates(a)
    c.sort(key=lambda x: (x["aligned_us"], _PRIO[x["kind"]], x["key"]))
    return c


def find_divergence(a, window_us=DEFAULT_WINDOW_US):
    cands = find_all(a)
    if not cands:
        return None
    first = dict(cands[0])
    first["chain"] = causal_chain(a, first, window_us)
    first["also"] = [{"kind": c["kind"], "key": c["key"], "aligned_us": c["aligned_us"]} for c in cands[1:12]]
    first["total_candidates"] = len(cands)
    return first


def _last_agree(a, cand):
    """Aligned time of the newest digest of the same category that all machines agreed on, before cand."""
    if "category" not in cand:
        return None
    best = None
    for (cat, ep, sq, dv), per in a.digests.items():
        if cat != cand["category"] or len(per) < 2 or len(set(r["hash"] for r in per.values())) != 1 or _diff_keys(per) or _diff_shards(per):
            continue
        t = max(a.t(s, r) for s, r in per.items())
        if t < cand["aligned_us"] and (best is None or t > best):
            best = t
    return best


def causal_chain(a, cand, window_us=DEFAULT_WINDOW_US):
    t_hi = cand["aligned_us"]
    if cand["kind"] in ("txn-hash", "txn-double", "txn-missing"):
        t_lo = min([t_hi] + [a.t(s, r) for s, d in a.txns.get(cand["seq"], {}).items() for r in d["captured"] + d["applied"]]) - window_us // 4
        t_hi += window_us // 4
    elif cand["kind"] == "desync-marker":
        t_lo = a.t0
    else:
        t_lo = _last_agree(a, cand)
        if t_lo is None:
            t_lo = t_hi - window_us
    involved = set(cand["slots"]) | set(cand.get("suspects", []))
    seq = cand.get("seq")
    ref = cand.get("ref")
    direct, context = [], []
    for s in a.slots:
        for r in a.traces[s]["records"]:
            if r.get("malformed"):
                continue
            t = a.t(s, r)
            if t < t_lo or t > t_hi:
                continue
            ty = r["type"]
            is_direct = False
            if ty in (C.T_TXN_CAPTURED, C.T_TXN_APPLIED) and seq is not None and r["seq"] == seq:
                is_direct = True
            elif (ty in (C.T_NET_IN, C.T_NET_OUT) and seq is not None and r["decode"] == C.DECODED and r["seq_hi"] > 0
                  and r["seq"] <= seq <= r["seq_hi"] and a.schema.is_txn_message(r["msg_type"])):
                is_direct = True
            elif ty == C.T_OWNERSHIP and ref is not None and r["ref"] == ref:
                is_direct = True
            elif ty == C.T_MARKER and r["kind"] == C.MARKER_DESYNC:
                is_direct = True
            elif ty == C.T_DIGEST and (r["category"], r["epoch"], r["seq"]) == (cand.get("category"), cand.get("digest_epoch"), cand.get("digest_seq")):
                is_direct = True
            elif r.get("custom") and "fields" in r and ((seq is not None and r["fields"].get("seq") == seq) or (ref is not None and r["fields"].get("ref") == ref)):
                is_direct = True   # a game-defined record that names the txn seq or the actor in a field called seq or ref
            entry = {"slot": s, "aligned_us": t, "rec": r}
            if is_direct:
                entry["role"] = "direct"
                direct.append(entry)
            elif s in involved and (ty in (C.T_TXN_APPLIED, C.T_TXN_CAPTURED, C.T_OWNERSHIP, C.T_DIGEST) or r.get("custom")):
                entry["role"] = "context"
                context.append(entry)
            elif ty in (C.T_NET_IN, C.T_NET_OUT) and s in involved and r["peer_slot"] in involved:
                entry["role"] = "context"
                context.append(entry)
    # keep the context nearest to the end of the window
    context.sort(key=lambda e: -e["aligned_us"])
    context = context[:max(0, CHAIN_MAX - len(direct))]
    chain = sorted(direct + context, key=lambda e: (e["aligned_us"], e["slot"], e["rec"]["idx"]))
    # transit anomalies inside the window
    notes = []
    for f, t_, o in a.lost:
        if f in involved and t_ in involved and t_lo <= a.t(f, o) <= t_hi:
            notes.append("message lost in transit: slot %d -> %d (%s, seq %d, hash %08x) at %.3fms" % (
                f, t_, a.schema.message(o["msg_type"]), o["seq"], o["dhash"], (a.t(f, o) - a.t0) / 1000.0))
    for f, t_, o, extra in a.duplicates:
        if f in involved and t_ in involved and t_lo <= a.t(f, o) <= t_hi:
            notes.append("message delivered %d times: slot %d -> %d (%s, seq %d, hash %08x)" % (
                len(extra) + 1, f, t_, a.schema.message(o["msg_type"]), o["seq"], o["dhash"]))
    return {"window_us": [t_lo, t_hi], "records": chain, "notes": notes}


def format_divergence(a, d):
    if d is None:
        return "No divergence found: every compared txn, digest and ownership record agrees."
    out = ["FIRST DIVERGENCE (%s) at %.3fms: %s" % (d["kind"], (d["aligned_us"] - a.t0) / 1000.0, d["detail"]),
           "  key: %s   suspect slot(s): %s   (%d candidate(s) in total)" % (d["key"], d.get("suspects"), d["total_candidates"])]
    lo, hi = d["chain"]["window_us"]
    out.append("  causal chain, window %.3fms .. %.3fms:" % ((lo - a.t0) / 1000.0, (hi - a.t0) / 1000.0))
    for e in d["chain"]["records"]:
        out.append("   %s %10.3fms slot %d %-12s %s" % ("*" if e["role"] == "direct" else " ", (e["aligned_us"] - a.t0) / 1000.0, e["slot"], e["rec"]["name"], C.describe(e["rec"], a.schema)))
    for n in d["chain"]["notes"]:
        out.append("   ! " + n)
    out.append("  (* = directly about the diverging key, others are surrounding context)")
    out[2:2] = format_narrowing(d.get("narrowing"))
    return "\n".join(out)
