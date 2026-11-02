"""Self-contained HTML report: machines, alignment summary, first divergence, causal chain, timeline, shrink result.
No scripts, no external files. Every value that came from a trace is HTML-escaped."""
from html import escape as e

import cofr as C
import analyze as A

CSS = """
:root{--bg:#fff;--fg:#1b1f24;--mut:#59636e;--line:#d0d7de;--bad:#cf222e;--ok:#1a7f37;--hi:#fff8c5;--card:#f6f8fa}
@media (prefers-color-scheme:dark){:root{--bg:#0d1117;--fg:#e6edf3;--mut:#8b949e;--line:#30363d;--bad:#ff7b72;--ok:#56d364;--hi:#3b2e00;--card:#161b22}}
body{background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,sans-serif;margin:0 auto;max-width:1100px;padding:16px}
h1{font-size:22px}h2{font-size:17px;margin-top:28px;border-bottom:1px solid var(--line);padding-bottom:4px}
table{border-collapse:collapse;width:100%;font-size:13px}th,td{border-bottom:1px solid var(--line);padding:3px 8px;text-align:left;vertical-align:top}
code,td.m{font-family:ui-monospace,Consolas,monospace;font-size:12px;word-break:break-all}
.bad{color:var(--bad);font-weight:600}.ok{color:var(--ok);font-weight:600}.mut{color:var(--mut)}
tr.direct td{background:var(--hi)}.card{background:var(--card);border:1px solid var(--line);border-radius:6px;padding:10px 14px;margin:8px 0}
svg{width:100%;height:auto;background:var(--card);border:1px solid var(--line);border-radius:6px}
"""

COLORS = {C.T_NET_IN: "#0969da", C.T_NET_OUT: "#8250df", C.T_TXN_CAPTURED: "#bf8700", C.T_TXN_APPLIED: "#1a7f37",
          C.T_OWNERSHIP: "#bc4c00", C.T_DIGEST: "#6e7781", C.T_MARKER: "#cf222e"}
CUSTOM_COLOR = "#0a7d8c"


def _timeline(a, d):
    lanes = a.slots
    lo = min((a.t(s, r) for s in lanes for r in a.traces[s]["records"]), default=0)
    hi = max((a.end_us[s] for s in lanes), default=lo + 1)
    span = max(1, hi - lo)
    W, H = 1000, 40 * len(lanes) + 30
    out = ['<svg viewBox="0 0 %d %d" role="img" aria-label="timeline of all machines">' % (W, H)]
    for i, s in enumerate(lanes):
        y = 30 + i * 40
        out.append('<text x="4" y="%d" font-size="11" fill="currentColor">slot %d</text>' % (y - 6, s))
        out.append('<line x1="50" x2="%d" y1="%d" y2="%d" stroke="#8b949e" stroke-width="0.5"/>' % (W - 10, y + 10, y + 10))
        for r in a.traces[s]["records"]:
            if r.get("malformed"):
                continue
            x = 50 + (a.t(s, r) - lo) / span * (W - 60)
            col = CUSTOM_COLOR if r.get("custom") else COLORS.get(r["type"], "#888")
            out.append('<line x1="%.1f" x2="%.1f" y1="%d" y2="%d" stroke="%s" stroke-width="1"/>' % (x, x, y, y + 20, col))
    if d:
        x = 50 + (d["aligned_us"] - lo) / span * (W - 60)
        out.append('<line x1="%.1f" x2="%.1f" y1="12" y2="%d" stroke="#cf222e" stroke-width="2" stroke-dasharray="4 3"/>' % (x, x, H - 4))
        out.append('<text x="%.1f" y="10" font-size="11" fill="#cf222e">first divergence</text>' % min(x, W - 110))
    out.append("</svg>")
    legend = " ".join('<span style="color:%s">&#9632;</span> %s' % (c, e(C.TYPE_NAMES[t])) for t, c in COLORS.items())
    legend += ' <span style="color:%s">&#9632;</span> game-defined' % CUSTOM_COLOR
    return "".join(out) + '<p class="mut">%s</p>' % legend


def render(traces, shrink_result=None, title="Co-op flight record report"):
    a = A.align(traces)
    d = A.find_divergence(a)
    h = ['<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">',
         "<title>%s</title><style>%s</style></head><body>" % (e(title), CSS), "<h1>%s</h1>" % e(title)]
    if d:
        h.append('<p class="bad">Machines diverged. First problem: %s</p>' % e(d["kind"]))
    else:
        h.append('<p class="ok">No divergence found in the traces.</p>')

    h.append("<h2>Machines</h2><table><tr><th>slot</th><th>role</th><th>instance</th><th>dump reason</th><th>records</th><th>dropped</th><th>clock offset</th><th>protocol</th></tr>")
    for s in a.slots:
        tr = a.traces[s]
        hd = tr["header"]
        h.append("<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%d%s</td><td>%d</td><td>%+.3f ms</td><td>%d</td></tr>" % (
            s, e(hd["role_name"]), e(hd["instance"]), e(hd["reason_name"]), len(tr["records"]),
            " (cut short)" if tr["truncated"] else "", hd["records_dropped"], a.offset_us[s] / 1000.0, hd["proto_version"]))
    h.append("</table>")

    h.append("<h2>Alignment</h2><div class=\"card\"><pre>%s</pre></div>" % e(A.format_alignment(a)))
    h.append("<h2>Timeline</h2>" + _timeline(a, d))

    h.append("<h2>First divergence</h2>")
    if not d:
        h.append("<p>Every compared txn, digest and ownership record agrees.</p>")
    else:
        h.append('<div class="card"><p class="bad">%s</p><p>key <code>%s</code>, suspect slot(s) <code>%s</code>, at %.3f ms, %d candidate(s) in total.</p>' % (
            e(d["detail"]), e(d["key"]), e(str(d.get("suspects"))), (d["aligned_us"] - a.t0) / 1000.0, d["total_candidates"]))
        if d.get("values"):
            h.append("<table><tr><th>slot</th><th>value</th></tr>%s</table>" % "".join(
                "<tr><td>%s</td><td class=\"m\">%s</td></tr>" % (e(str(s)), e(str(v))) for s, v in sorted(d["values"].items())))
        nw = d.get("narrowing")
        if nw and (nw["buckets"] or nw["keys"]):
            h.append("<h3>Narrowing: digest, bucket, key, value</h3><p>Differing bucket(s) <code>%s</code> of %d%s.</p>" % (
                e(", ".join(str(b) for b in nw["buckets"][:20]) or "none recorded"), nw["bucket_count"],
                " (keys from the %s)" % e(nw["key_source"]) if nw["key_source"] else ""))
            if nw["keys"]:
                slots = sorted(nw["keys"][0]["values"])
                h.append("<table><tr><th>key</th><th>bucket</th>%s</tr>" % "".join("<th>slot %d</th>" % s for s in slots))
                for k in nw["keys"][:30]:
                    cells = "".join("<td class=\"m\">%s</td>" % e("(absent)" if k["values"][s] is None else
                                    (k["texts"].get(s) or "value hash %d" % k["values"][s])) for s in slots)
                    h.append("<tr><td class=\"m\">%s</td><td>%s</td>%s</tr>" % (e(k["key"]), "" if k["bucket"] is None else k["bucket"], cells))
                h.append("</table>")
            else:
                h.append("<p>No per-key detail was recorded, so the key level is unknown (a desync dump has it).</p>")
        h.append("</div><h3>Causal chain</h3><table><tr><th>time ms</th><th>slot</th><th>kind</th><th>what</th></tr>")
        for en in d["chain"]["records"]:
            h.append('<tr class="%s"><td>%.3f</td><td>%d</td><td>%s</td><td class="m">%s</td></tr>' % (
                en["role"], (en["aligned_us"] - a.t0) / 1000.0, en["slot"], e(en["rec"]["name"]), e(C.describe(en["rec"], a.schema))))
        h.append("</table>")
        for n in d["chain"]["notes"]:
            h.append('<p class="bad">%s</p>' % e(n))
        if d["also"]:
            h.append("<h3>Later candidates</h3><table>%s</table>" % "".join(
                "<tr><td>%.3f ms</td><td>%s</td><td class=\"m\">%s</td></tr>" % ((c["aligned_us"] - a.t0) / 1000.0, e(c["kind"]), e(c["key"])) for c in d["also"]))

    if shrink_result:
        sh = shrink_result.get("shrunk", {})
        h.append("<h2>Minimal reproduction</h2><div class=\"card\"><p>Reduced from %s steps to <b>%s</b> in %s replays%s.</p><p>Failure: <code>%s</code></p></div>" % (
            e(str(sh.get("original_steps"))), e(str(sh.get("reduced_steps"))), e(str(sh.get("oracle_runs"))),
            "" if sh.get("complete", True) else " (budget ran out, so it may not be minimal)", e(str(sh.get("signature")))))
        h.append("<table><tr><th>#</th><th>step</th></tr>%s</table>" % "".join(
            "<tr><td>%d</td><td class=\"m\">%s</td></tr>" % (i, e(_step_text(st))) for i, st in enumerate((shrink_result.get("steps") or shrink_result.get("ops") or [])[:200])))
    h.append("</body></html>")
    return "".join(h)


def _step_text(st):
    parts = ["%s=%s" % (k, (v if len(str(v)) < 60 else str(v)[:57] + "...")) for k, v in st.items()]
    return " ".join(parts)
