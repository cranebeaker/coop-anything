"""Small INI text editor that keeps the rest of the file as it was (line endings, comments, order).

Section and key names are matched case-insensitively. Games read their own INI quirks (spaces in key names,
empty values), so this works on text lines and never re-serialises the file.
"""
from __future__ import annotations

import re
from typing import Optional


def ini_set(text: str, section: str, key: str, value: str) -> str:
    """Set key in [section], adding the section or the key when missing. Keeps CRLF files CRLF."""
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(nl) if text else []
    sec_re = re.compile(r"^\s*\[(.+?)\]\s*$")
    key_re = re.compile(r"^\s*" + re.escape(key) + r"\s*=", re.I)
    cur = None
    sec_end = None          # index after the last non-blank line of the target section
    for idx, ln in enumerate(lines):
        m = sec_re.match(ln)
        if m:
            if cur is not None and cur.lower() == section.lower():
                break
            cur = m.group(1)
            if cur.lower() == section.lower():
                sec_end = idx + 1
            continue
        if cur is not None and cur.lower() == section.lower():
            if key_re.match(ln):
                lines[idx] = f"{key}={value}"
                return nl.join(lines)
            if ln.strip():
                sec_end = idx + 1
    if sec_end is None:
        if lines and lines[-1].strip():
            lines.append("")
        lines += [f"[{section}]", f"{key}={value}"]
    else:
        lines.insert(sec_end, f"{key}={value}")
    return nl.join(lines)


def ini_get(text: str, section: str, key: str) -> Optional[str]:
    cur = None
    for ln in text.splitlines():
        m = re.match(r"^\s*\[(.+?)\]\s*$", ln)
        if m:
            cur = m.group(1)
            continue
        if cur and cur.lower() == section.lower():
            m = re.match(r"^\s*" + re.escape(key) + r"\s*=\s*(.*?)\s*$", ln, re.I)
            if m:
                return m.group(1)
    return None
