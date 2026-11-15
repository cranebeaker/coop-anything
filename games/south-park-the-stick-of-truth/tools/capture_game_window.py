"""Capture the game's client area as a PNG **and** as numbers a bot can assert on.

`PrintWindow` returns an all-black frame for this title (DX9 copy, Steam overlay), so the
foreground `BitBlt` of the client area is tried first and `PrintWindow` is only the
fallback. Nothing here copies pixels from the desktop when the game window is not the
foreground window: a capture that is not of the game is reported as `captured: False`.

    from capture_game_window import capture_game_window, grid_correlation
    frame = capture_game_window(pid, Path('frame.png'))
    frame['nonblack_ratio'], frame['luma_grid'], frame['dhash']

The metric block is what the gameplay detector compares, and it is also enough for a
human to check the PNG afterwards: every run keeps its frames on disk.
"""
import ctypes
import struct
import zlib
from ctypes import wintypes

CAPTUREBLT = 0x40000000
SRCCOPY = 0x00CC0020


class _Api:
    """user32/gdi32 entry points, declared once."""

    def __init__(self):
        self.user = ctypes.WinDLL('user32', use_last_error=True)
        self.gdi = ctypes.WinDLL('gdi32', use_last_error=True)
        user, gdi = self.user, self.gdi
        user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        user.GetWindowThreadProcessId.restype = wintypes.DWORD
        user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
        user.GetDC.argtypes = [wintypes.HWND]
        user.GetDC.restype = wintypes.HDC
        user.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
        user.PrintWindow.argtypes = [wintypes.HWND, wintypes.HDC, wintypes.UINT]
        user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        user.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
        user.SetForegroundWindow.argtypes = [wintypes.HWND]
        user.SetForegroundWindow.restype = wintypes.BOOL
        user.GetForegroundWindow.restype = wintypes.HWND
        user.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
        user.BringWindowToTop.argtypes = [wintypes.HWND]
        user.SetActiveWindow.argtypes = [wintypes.HWND]
        user.SetFocus.argtypes = [wintypes.HWND]
        user.AttachThreadInput.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.BOOL]
        user.AttachThreadInput.restype = wintypes.BOOL
        user.IsWindowVisible.argtypes = [wintypes.HWND]
        user.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
        user.GetCursorPos.argtypes = [ctypes.POINTER(wintypes.POINT)]
        user.mouse_event.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                                     wintypes.DWORD, ctypes.c_size_t]
        user.keybd_event.argtypes = [wintypes.BYTE, wintypes.BYTE, wintypes.DWORD, ctypes.c_size_t]
        gdi.BitBlt.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, wintypes.HDC, ctypes.c_int, ctypes.c_int, wintypes.DWORD]
        gdi.CreateCompatibleDC.argtypes = [wintypes.HDC]
        gdi.CreateCompatibleDC.restype = wintypes.HDC
        gdi.SelectObject.argtypes = [wintypes.HDC, wintypes.HANDLE]
        gdi.SelectObject.restype = wintypes.HANDLE
        gdi.DeleteObject.argtypes = [wintypes.HANDLE]
        gdi.DeleteDC.argtypes = [wintypes.HDC]
        gdi.CreateDIBSection.argtypes = [wintypes.HDC, ctypes.c_void_p, wintypes.UINT,
                                         ctypes.POINTER(ctypes.c_void_p), wintypes.HANDLE,
                                         wintypes.DWORD]
        gdi.CreateDIBSection.restype = wintypes.HANDLE
        try:
            self.dwm = ctypes.WinDLL('dwmapi')
        except OSError:                                  # pragma: no cover - always present
            self.dwm = None


_API = None


def api():
    global _API
    if _API is None:
        _API = _Api()
    return _API


def find_game_window(pid):
    """Largest visible, titled, >100x100 top-level window owned by `pid`."""
    handles = api().user
    windows = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def collect_window(hwnd, parameter):
        owner = wintypes.DWORD()
        handles.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value != pid:
            return True
        rect = wintypes.RECT()
        if not handles.GetClientRect(hwnd, ctypes.byref(rect)):
            return True
        width, height = rect.right, rect.bottom
        title = ctypes.create_unicode_buffer(512)
        handles.GetWindowTextW(hwnd, title, 512)
        klass = ctypes.create_unicode_buffer(256)
        handles.GetClassNameW(hwnd, klass, 256)
        if width > 100 and height > 100 and title.value:
            windows.append((width * height, hwnd, width, height, title.value, klass.value,
                            bool(handles.IsWindowVisible(hwnd))))
        return True

    callback = callback_type(collect_window)
    handles.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    handles.EnumWindows(callback, 0)
    if not windows:
        return None
    _, hwnd, width, height, title, klass, visible = max(windows, key=lambda item: item[0])
    return {'hwnd': hwnd, 'width': width, 'height': height, 'title': title,
            'class': klass, 'visible': visible}


def focus_window(hwnd):
    """Best-effort foreground without stealing from nothing: returns True if focused."""
    user = api().user
    current = ctypes.WinDLL('kernel32').GetCurrentThreadId()
    foreground = user.GetForegroundWindow()
    foreground_pid = wintypes.DWORD()
    foreground_thread = user.GetWindowThreadProcessId(foreground, ctypes.byref(foreground_pid))
    target_pid = wintypes.DWORD()
    target_thread = user.GetWindowThreadProcessId(hwnd, ctypes.byref(target_pid))
    attached = []
    if foreground_thread and foreground_thread != current:
        if user.AttachThreadInput(current, foreground_thread, True):
            attached.append(foreground_thread)
    if target_thread and target_thread != current and target_thread not in attached:
        if user.AttachThreadInput(current, target_thread, True):
            attached.append(target_thread)
    try:
        user.ShowWindow(hwnd, 9)                 # SW_RESTORE
        user.BringWindowToTop(hwnd)
        user.SetActiveWindow(hwnd)
        user.SetFocus(hwnd)
        user.SetForegroundWindow(hwnd)
        return user.GetForegroundWindow() == hwnd
    finally:
        for thread in attached:
            user.AttachThreadInput(current, thread, False)


def click_client(pid, fraction_x, fraction_y, settle=1.0):
    """Click a fractional position inside the client area (0.0-1.0, resolution free)."""
    import time
    window = find_game_window(pid)
    if window is None:
        return {'clicked': False, 'reason': 'no window'}
    if not focus_window(window['hwnd']):
        return {'clicked': False, 'reason': 'could not foreground the game window'}
    point = wintypes.POINT(int(window['width'] * fraction_x), int(window['height'] * fraction_y))
    api().user.ClientToScreen(window['hwnd'], ctypes.byref(point))
    api().user.SetCursorPos(point.x, point.y)
    time.sleep(0.15)
    api().user.mouse_event(0x0002, 0, 0, 0, 0)      # LEFT_DOWN
    time.sleep(0.08)
    api().user.mouse_event(0x0004, 0, 0, 0, 0)      # LEFT_UP
    time.sleep(settle)
    return {'clicked': True, 'screen': [point.x, point.y],
            'client': [int(window['width'] * fraction_x), int(window['height'] * fraction_y)]}


VK_CODES = {'enter': 0x0D, 'space': 0x20, 'escape': 0x1B, 'esc': 0x1B, 'tab': 0x09,
            'up': 0x26, 'down': 0x28, 'left': 0x25, 'right': 0x27,
            'w': 0x57, 'a': 0x41, 's': 0x53, 'd': 0x44}


def hold_key(pid, key, seconds=1.5):
    """Hold one key down for `seconds`: enough to see a character walk."""
    import time
    code = VK_CODES.get(str(key).lower())
    window = find_game_window(pid)
    if code is None:
        return {'held': False, 'reason': 'unknown key name ' + str(key)}
    if window is None:
        return {'held': False, 'reason': 'no window'}
    if not focus_window(window['hwnd']):
        return {'held': False, 'reason': 'could not foreground the game window'}
    api().user.keybd_event(code, 0, 0, 0)
    time.sleep(seconds)
    api().user.keybd_event(code, 0, 2, 0)
    time.sleep(0.4)
    return {'held': True, 'key': key, 'vk': hex(code), 'seconds': seconds}


def send_keys(pid, keys, settle=0.6):
    """Send virtual-key names to the game window, e.g. ['enter', 'space', 'escape']."""
    import time
    codes = {'enter': 0x0D, 'space': 0x20, 'escape': 0x1B, 'esc': 0x1B, 'tab': 0x09,
             'up': 0x26, 'down': 0x28, 'left': 0x25, 'right': 0x27,
             'w': 0x57, 'a': 0x41, 's': 0x53, 'd': 0x44}
    window = find_game_window(pid)
    if window is None:
        return {'sent': False, 'reason': 'no window'}
    if not focus_window(window['hwnd']):
        return {'sent': False, 'reason': 'could not foreground the game window'}
    sent = []
    for key in keys:
        code = codes.get(str(key).lower())
        if code is None:
            sent.append({'key': key, 'error': 'unknown key name'})
            continue
        api().user.keybd_event(code, 0, 0, 0)
        time.sleep(0.06)
        api().user.keybd_event(code, 0, 2, 0)
        time.sleep(settle)
        sent.append({'key': key, 'vk': hex(code)})
    return {'sent': True, 'keys': sent}


def _luma_grid(rgb, width, height, columns=8, rows=8):
    """Mean luma of a coarse grid, 0-255 integers: the frame's coarse structure."""
    sums = [0] * (columns * rows)
    counts = [0] * (columns * rows)
    step = 4
    for y in range(0, height, step):
        row = (y * columns // height) * columns
        base = y * width * 3
        for x in range(0, width, step):
            offset = base + x * 3
            luma = (rgb[offset] * 299 + rgb[offset + 1] * 587 + rgb[offset + 2] * 114) // 1000
            cell = row + x * columns // width
            sums[cell] += luma
            counts[cell] += 1
    return [int(sums[i] / counts[i]) if counts[i] else 0 for i in range(columns * rows)]


def _dhash(grid, columns=8, rows=8):
    """64-bit difference hash from the luma grid (left pixel brighter than right)."""
    value = 0
    for y in range(rows):
        for x in range(columns - 1):
            value = (value << 1) | (1 if grid[y * columns + x] > grid[y * columns + x + 1] else 0)
    return value


def frame_metrics(rgb, width, height):
    """Numbers that describe a frame: how much is drawn and where the bright parts are."""
    nonblack = 0
    total = 0
    luma_sum = 0
    luma_sq = 0
    step = 4
    for y in range(0, height, step):
        base = y * width * 3
        for x in range(0, width, step):
            offset = base + x * 3
            red, green, blue = rgb[offset], rgb[offset + 1], rgb[offset + 2]
            luma = (red * 299 + green * 587 + blue * 114) // 1000
            luma_sum += luma
            luma_sq += luma * luma
            if red > 12 or green > 12 or blue > 12:
                nonblack += 1
            total += 1
    mean = luma_sum / total if total else 0.0
    variance = max(0.0, luma_sq / total - mean * mean) if total else 0.0
    grid = _luma_grid(rgb, width, height)
    return {'nonblack_ratio': round(nonblack / total, 4) if total else 0.0,
            'mean_luma': round(mean, 2),
            'luma_std': round(variance ** 0.5, 2),
            'luma_grid': grid,
            'dhash': format(_dhash(grid), '016x')}


def grid_correlation(grid_a, grid_b):
    """Pearson correlation of two coarse luma grids; 1.0 is identical structure."""
    if not grid_a or not grid_b or len(grid_a) != len(grid_b):
        return 0.0
    count = len(grid_a)
    mean_a = sum(grid_a) / count
    mean_b = sum(grid_b) / count
    numerator = sum((a - mean_a) * (b - mean_b) for a, b in zip(grid_a, grid_b))
    left = sum((a - mean_a) ** 2 for a in grid_a) ** 0.5
    right = sum((b - mean_b) ** 2 for b in grid_b) ** 0.5
    if left == 0 or right == 0:
        return 0.0
    return round(numerator / (left * right), 4)


def dhash_distance(hash_a, hash_b):
    """Hamming distance between two 16-hex-digit dHashes (0-64)."""
    if not hash_a or not hash_b:
        return 64
    return bin(int(hash_a, 16) ^ int(hash_b, 16)).count('1')


def _write_png(destination, width, height, rgb):
    scanlines = b''.join(b'\0' + rgb[row * width * 3:(row + 1) * width * 3] for row in range(height))

    def chunk(kind, value):
        return (struct.pack('>I', len(value)) + kind + value
                + struct.pack('>I', zlib.crc32(kind + value) & 0xffffffff))

    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b'')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(png)


def capture_game_window(pid, destination, prefer='bitblt', minimum_nonblack=0.02):
    """Capture the client area. Returns `captured: False` rather than a misleading frame.

    `prefer='bitblt'` (default) needs the window foreground: it is the only method that
    produced readable frames for this title. `prefer='printwindow'` asks DWM for the
    window's own surface first, which is black here but costs nothing to try first when
    another window is in front. If neither path yields a non-black frame the result says
    so and no PNG is written.
    """
    window = find_game_window(pid)
    if window is None:
        return {'captured': False, 'reason': 'no game-owned window'}
    width, height = window['width'], window['height']
    if width * height > 16000000:
        return {'captured': False, 'reason': 'window too large', 'width': width, 'height': height}
    user, gdi = api().user, api().gdi
    focused = False
    if prefer == 'bitblt':
        focused = focus_window(window['hwnd'])
    screen_dc = user.GetDC(None)
    window_dc = user.GetDC(window['hwnd'])
    memory_dc = gdi.CreateCompatibleDC(window_dc or screen_dc)
    bitmap_info = ctypes.create_string_buffer(
        struct.pack('<IiiHHIIiiII', 40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0))
    pixels = ctypes.c_void_p()
    bitmap = gdi.CreateDIBSection(memory_dc, bitmap_info, 0, ctypes.byref(pixels), None, 0)
    if not bitmap:
        gdi.DeleteDC(memory_dc)
        if screen_dc:
            user.ReleaseDC(None, screen_dc)
        if window_dc:
            user.ReleaseDC(window['hwnd'], window_dc)
        return {'captured': False, 'reason': 'CreateDIBSection failed'}
    previous = gdi.SelectObject(memory_dc, bitmap)
    attempts = []
    bgra = b''

    def snapshot():
        return ctypes.string_at(pixels, width * height * 4)

    def nonblack(fragment, step=16):
        if not fragment:
            return 0.0
        hits = 0
        seen = 0
        for index in range(0, min(len(fragment), width * height * 4), step * 4):
            seen += 1
            if (fragment[index] or fragment[index + 1] or fragment[index + 2]):
                hits += 1
        return hits / seen if seen else 0.0

    try:
        order = ['bitblt', 'printwindow'] if prefer == 'bitblt' else ['printwindow', 'bitblt']
        for method in order:
            if method == 'bitblt':
                if not focused:
                    focused = focus_window(window['hwnd'])
                if not focused:
                    attempts.append({'method': method, 'skipped': 'game window is not foreground'})
                    continue
                if api().dwm is not None:
                    api().dwm.DwmFlush()
                point = wintypes.POINT(0, 0)
                user.ClientToScreen(window['hwnd'], ctypes.byref(point))
                ok = gdi.BitBlt(memory_dc, 0, 0, width, height, screen_dc,
                                point.x, point.y, SRCCOPY | CAPTUREBLT)
                fragment = snapshot() if ok else b''
                ratio = nonblack(fragment)
                attempts.append({'method': 'ForegroundClientBitBlt', 'nonblack': round(ratio, 4),
                                 'bitblt_ok': bool(ok)})
            else:
                if api().dwm is not None:
                    api().dwm.DwmFlush()
                ok = user.PrintWindow(window['hwnd'], memory_dc, 3)
                fragment = snapshot() if ok else b''
                ratio = nonblack(fragment)
                attempts.append({'method': 'PrintWindow(PW_RENDERFULLCONTENT)', 'ok': bool(ok),
                                 'nonblack': round(ratio, 4)})
            if fragment and ratio >= minimum_nonblack:
                bgra = fragment
                chosen = method
                break
        if not bgra:
            return {'captured': False, 'reason': 'every method returned a black frame',
                    'attempts': attempts, 'title': window['title'],
                    'width': width, 'height': height,
                    'foreground': bool(focused)}
        rgb = bytearray(width * height * 3)
        rgb[0::3] = bgra[2::4]
        rgb[1::3] = bgra[1::4]
        rgb[2::3] = bgra[0::4]
        metrics = frame_metrics(rgb, width, height)
        _write_png(destination, width, height, rgb)
        return {'captured': True, 'method': chosen, 'attempts': attempts,
                'title': window['title'], 'class': window['class'],
                'width': width, 'height': height, 'png': str(destination),
                'foreground': bool(focused), **metrics}
    finally:
        gdi.SelectObject(memory_dc, previous)
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(memory_dc)
        if window_dc:
            user.ReleaseDC(window['hwnd'], window_dc)
        if screen_dc:
            user.ReleaseDC(None, screen_dc)


if __name__ == '__main__':
    import argparse
    import json
    parser = argparse.ArgumentParser(description='Capture a game window owned by --pid')
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--png', default='frame.png')
    parser.add_argument('--prefer', default='bitblt', choices=['bitblt', 'printwindow'])
    arguments = parser.parse_args()
    from pathlib import Path
    print(json.dumps(capture_game_window(arguments.pid, Path(arguments.png),
                                         prefer=arguments.prefer), indent=2))
