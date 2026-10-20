"""Sekiro Online co-op test agent (run on each PC).

Watches the game window, OCRs the mod's HUD every couple of seconds, and writes
one JSON line per sample. The HUD is the mod's own status line, for example:

  [Sekiro Online v2.05 - By Luke Yui] [Steam: Ready] [Server: Unavailable] [Connection: Offline]

Usage:
  python agent.py --seconds 600 --out host.jsonl [--label host]

Requires: pip install pillow winsdk  (Windows OCR, no network).
"""
import argparse
import asyncio
import ctypes
import ctypes.wintypes
import json
import os
import re
import subprocess
import sys
import time

from PIL import Image, ImageOps
from winsdk.windows.globalization import Language
from winsdk.windows.graphics.imaging import BitmapDecoder
from winsdk.windows.media.ocr import OcrEngine
from winsdk.windows.storage import FileAccessMode, StorageFile

user32 = ctypes.windll.user32


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


def sekiro_pid():
    out = subprocess.run(
        ["powershell", "-NoProfile", "-Command",
         "(Get-Process sekiro -ErrorAction SilentlyContinue | Select-Object -First 1).Id"],
        capture_output=True, text=True).stdout.strip()
    return int(out) if out.isdigit() else 0


def game_window(pid):
    found = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    def cb(hwnd, _):
        p = ctypes.c_ulong()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(cb, None)
    return found[0] if found else 0


def grab(hwnd):
    r = RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    pt = ctypes.wintypes.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    box = (pt.x, pt.y, pt.x + r.right, pt.y + r.bottom)
    from PIL import ImageGrab
    return ImageGrab.grab(bbox=box)


async def ocr_image(img):
    import tempfile
    fd, path = tempfile.mkstemp(suffix=".png")
    os.close(fd)
    try:
        img.save(path)
        f = await StorageFile.get_file_from_path_async(path)
        stream = await f.open_async(FileAccessMode.READ)
        dec = await BitmapDecoder.create_async(stream)
        bmp = await dec.get_software_bitmap_async()
        eng = OcrEngine.try_create_from_language(Language("en-US"))
        res = await eng.recognize_async(bmp)
        return res.text
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


def parse(text):
    """Pull the mod's status fields out of a noisy OCR line.

    OCR on a 3D scene mangles punctuation, so match loosely and only trust a
    field when the expected word is present after its label.
    """
    flat = " ".join(text.split())
    out = {"raw": flat}
    compact = re.sub(r"[^A-Za-z0-9]", "", flat).lower()

    m = re.search(r"Sekiro\s*Online\s*(v[\d.]+)", flat, re.I)
    if m:
        out["version"] = m.group(1)

    def field(label, values):
        # look for <label> then any of the expected values within ~24 chars
        for vm in re.finditer(label, compact):
            tail = compact[vm.end():vm.end() + 24]
            for v in values:
                if tail.startswith(v):
                    return v
        return None

    steam = field("steam", ["ready", "offline", "notready", "unavailable"])
    server = field("server", ["available", "unavailable", "online", "offline"])
    conn = field("connection", ["offline", "online", "hosting", "connected", "joining"])
    if steam:
        out["steam"] = "Ready" if steam == "ready" else steam.capitalize()
    if server:
        out["server"] = "Unavailable" if server == "unavailable" else server.capitalize()
    if conn:
        out["connection"] = conn.capitalize()
    if re.search(r"controller\s*connected", flat, re.I):
        out["controller"] = True
    return out


def connections(pid):
    try:
        out = subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             f"Get-NetTCPConnection -OwningProcess {pid} -ErrorAction SilentlyContinue | "
             "Select-Object State,RemoteAddress,RemotePort | ConvertTo-Json -Compress"],
            capture_output=True, text=True).stdout.strip()
        if not out:
            return []
        data = json.loads(out)
        return data if isinstance(data, list) else [data]
    except Exception:
        return []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=600)
    ap.add_argument("--out", default="session.jsonl")
    ap.add_argument("--label", default="local")
    ap.add_argument("--interval", type=float, default=2.0)
    ap.add_argument("--save-on-change", action="store_true",
                    help="save a PNG when the HUD changes")
    args = ap.parse_args()

    shots = os.path.join(os.path.dirname(os.path.abspath(args.out)), "shots")
    os.makedirs(shots, exist_ok=True)
    deadline = time.time() + args.seconds
    last_key = None
    samples = 0

    with open(args.out, "a", encoding="utf-8") as fh:
        while time.time() < deadline:
            pid = sekiro_pid()
            if not pid:
                fh.write(json.dumps({"t": time.time(), "label": args.label, "game": "absent"}) + "\n")
                fh.flush()
                time.sleep(args.interval)
                continue
            hwnd = game_window(pid)
            if not hwnd:
                time.sleep(args.interval)
                continue
            img = grab(hwnd)
            gray = img.convert("L")
            # full frame at 2x reads the whole HUD line best; a tight crop of the
            # top strip is a fallback when the scene behind it is noisy
            text = asyncio.run(ocr_image(ImageOps.autocontrast(
                gray.resize((gray.width * 2, gray.height * 2), Image.LANCZOS))))
            hud = parse(text)
            if "connection" not in hud:
                band = gray.crop((0, 0, gray.width, max(48, int(gray.height * 0.12))))
                text = asyncio.run(ocr_image(ImageOps.autocontrast(
                    band.resize((band.width * 4, band.height * 4), Image.LANCZOS))))
                alt = parse(text)
                for k, v in alt.items():
                    hud.setdefault(k, v)
                if "connection" in alt:
                    hud["raw"] = alt["raw"]
            sample = {"t": time.time(), "label": args.label, "pid": pid, "hud": hud,
                      "tcp": connections(pid) if samples % 5 == 0 else []}
            fh.write(json.dumps(sample) + "\n")
            fh.flush()
            samples += 1
            key = (hud.get("connection"), hud.get("server"), hud.get("steam"), "Controller Connected" in hud.get("raw", ""))
            if key != last_key:
                if args.save_on_change:
                    img.save(os.path.join(shots, f"{args.label}-{int(time.time())}.png"))
                print(f"[{time.strftime('%H:%M:%S')}] {hud.get('raw', '')[:160]}", flush=True)
                last_key = key
            time.sleep(args.interval)

    print(f"done: {samples} samples -> {args.out}", flush=True)


if __name__ == "__main__":
    main()
