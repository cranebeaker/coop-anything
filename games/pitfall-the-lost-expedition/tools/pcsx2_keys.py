"""Posts key presses to one PCSX2 process's windows without taking focus.

Usage: python -I tools/pcsx2_keys.py <pid> <key>[:hold_ms] [<key>[:hold_ms] ...]
Keys: up down left right return backspace escape space tab f1..f12, or a single letter/digit.
"wait:<ms>" pauses between keys; "<key>:down" / "<key>:up" press or release without the other.
Only windows owned by <pid> receive messages.
"""
import ctypes
import sys
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)

WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
MAPVK_VK_TO_VSC = 0

NAMED_KEYS = {
    "up": 0x26, "down": 0x28, "left": 0x25, "right": 0x27,
    "return": 0x0D, "backspace": 0x08, "escape": 0x1B, "space": 0x20, "tab": 0x09,
}
NAMED_KEYS.update({f"f{n}": 0x6F + n for n in range(1, 13)})
EXTENDED = {0x25, 0x26, 0x27, 0x28}

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


def windows_of(pid):
    found = []

    def collect(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(WNDENUMPROC(collect), 0)
    return found


def vk_of(name):
    name = name.lower()
    if name in NAMED_KEYS:
        return NAMED_KEYS[name]
    if len(name) == 1 and name.isalnum():
        return ord(name.upper())
    raise ValueError(f"unknown key {name!r}")


def lparam(vk, up):
    scan = user32.MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)
    value = 1 | (scan << 16)
    if vk in EXTENDED:
        value |= 1 << 24
    if up:
        value |= (1 << 30) | (1 << 31)
    return value


def press(hwnds, vk, hold_ms):
    for hwnd in hwnds:
        user32.PostMessageW(hwnd, WM_KEYDOWN, vk, lparam(vk, False))
    time.sleep(hold_ms / 1000)
    for hwnd in hwnds:
        user32.PostMessageW(hwnd, WM_KEYUP, vk, lparam(vk, True))


def main():
    pid = int(sys.argv[1])
    hwnds = windows_of(pid)
    if not hwnds:
        sys.exit(f"no visible windows for pid {pid}")
    for step in sys.argv[2:]:
        name, _, ms = step.partition(":")
        if name == "wait":
            time.sleep(int(ms) / 1000)
            continue
        if ms in ("down", "up"):  # e.g. j:down ... j:up holds a key across other commands
            msg = WM_KEYDOWN if ms == "down" else WM_KEYUP
            for hwnd in hwnds:
                user32.PostMessageW(hwnd, msg, vk_of(name), lparam(vk_of(name), ms == "up"))
            continue
        press(hwnds, vk_of(name), int(ms) if ms else 120)
        time.sleep(0.15)


if __name__ == "__main__":
    main()
