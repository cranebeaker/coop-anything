"""Self-contained HTML report (no external files or scripts) for an animcheck report dict."""
from __future__ import annotations

import html
from typing import List


def _table(rows: List[List[str]], head: List[str]) -> str:
    h = "".join(f"<th>{html.escape(c)}</th>" for c in head)
    b = "".join("<tr>" + "".join(f"<td>{html.escape(str(c))}</td>" for c in r) + "</tr>" for r in rows)
    return f"<table><thead><tr>{h}</tr></thead><tbody>{b}</tbody></table>"


def render(report: dict) -> str:
    c = report["counters"]
    ok = report.get("ok", False)
    parts = [f"<h1>Animation check{(': ' + html.escape(report['label'])) if report.get('label') else ''}</h1>",
             f"<p class='{'ok' if ok else 'bad'}'>{'PASS' if ok else 'FAIL'}"
             + ("" if ok else ": " + html.escape("; ".join(report.get("failures", [])))) + "</p>",
             f"<p>Profile: {html.escape(str(report.get('profile', 'generic')))}</p>"]
    keys = ["samples", "actors", "compared", "pose_err_p50", "pose_err_p95", "pose_err_max", "pos_err_p95",
            "facing_err_p95_deg", "group_mismatch_pct", "group_mismatch_episodes", "wrong_weapon", "wrong_facing",
            "dead_mismatch", "unknown_groups", "glitches"] + sorted(k for k in c if k.startswith("glitch."))
    parts.append("<h2>Counters</h2>" + _table([[k, c[k]] for k in keys if k in c], ["counter", "value"]))
    unknown = report.get("unknown_groups") or {}
    if unknown:
        parts.append("<h2>Animation groups the profile does not know</h2><p>Add them to the profile, or the "
                     "detectors treat them as unknown animations.</p>"
                     + _table([[g, n] for g, n in sorted(unknown.items(), key=lambda kv: -kv[1])[:100]],
                              ["group", "samples"]))
    pr = [[p["source"], p["observer"], p["actors"], p["compared"], p["pose_err"]["p95"], p["pos_err"]["p95"],
           p["group_mismatch_pct"], p["wrong_weapon"], p["wrong_facing"]] for p in report["pairs"]]
    if pr:
        parts.append("<h2>Machine pairs (source vs observer)</h2>" + _table(
            pr, ["source", "observer", "actors", "compared", "pose p95", "pos p95", "group mismatch %", "wrong weapon", "wrong facing"]))
    if report.get("avatars"):
        av = [[a["name"], a["owner"], a["observer"], a["compared"], a["pose_err"]["p95"], a["facing_err_deg"]["p95"],
               a["aim_err_deg"]["p95"], a["group_mismatch_episodes"], a["wrong_weapon"], a["wrong_aim"], a["stance_mismatch"]]
              for a in report["avatars"]]
        parts.append("<h2>Avatar fidelity (a player's own character vs their puppet on another machine)</h2>" + _table(
            av, ["avatar", "owner", "observer", "compared", "pose p95", "facing p95 deg", "aim p95 deg", "anim mismatches",
                 "wrong weapon", "wrong aim", "stance"])
            + _table([[k, v] for k, v in sorted(c.items()) if k.startswith("avatar.")], ["counter", "value"]))
    ev = [[m, e["kind"], e["ref"], e["t0_ms"], e["t1_ms"], e["value"], e["detail"]]
          for m, d in report["machines"].items() for e in d["events"]]
    parts.append(f"<h2>Glitch events ({len(ev)})</h2>" + (_table(ev[:500], ["machine", "kind", "actor", "from ms", "to ms", "value", "detail"]) if ev else "<p>none</p>"))
    css = ("body{font:14px system-ui,sans-serif;margin:24px;background:#fff;color:#111}table{border-collapse:collapse;margin:8px 0}"
           "td,th{border:1px solid #bbb;padding:3px 8px;text-align:left}.ok{color:#075}.bad{color:#b00}"
           "@media(prefers-color-scheme:dark){body{background:#111;color:#eee}td,th{border-color:#555}.ok{color:#5d9}.bad{color:#f66}}")
    return f"<!doctype html><meta charset='utf-8'><title>Animation check</title><style>{css}</style>{''.join(parts)}"
