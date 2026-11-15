import ctypes
import struct
import zlib
from ctypes import wintypes


def capture_game_window(pid, destination):
    user = ctypes.WinDLL('user32', use_last_error=True)
    gdi = ctypes.WinDLL('gdi32', use_last_error=True)
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.GetDC.argtypes = [wintypes.HWND]
    user.GetDC.restype = wintypes.HDC
    user.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
    user.PrintWindow.argtypes = [wintypes.HWND, wintypes.HDC, wintypes.UINT]
    user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user.SetForegroundWindow.argtypes = [wintypes.HWND]
    user.GetForegroundWindow.restype = wintypes.HWND
    user.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    gdi.BitBlt.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.HDC, ctypes.c_int, ctypes.c_int, wintypes.DWORD]
    gdi.CreateCompatibleDC.argtypes = [wintypes.HDC]
    gdi.CreateCompatibleDC.restype = wintypes.HDC
    gdi.SelectObject.argtypes = [wintypes.HDC, wintypes.HANDLE]
    gdi.SelectObject.restype = wintypes.HANDLE
    gdi.DeleteObject.argtypes = [wintypes.HANDLE]
    gdi.DeleteDC.argtypes = [wintypes.HDC]
    gdi.CreateDIBSection.argtypes = [wintypes.HDC, ctypes.c_void_p, wintypes.UINT, ctypes.POINTER(ctypes.c_void_p), wintypes.HANDLE, wintypes.DWORD]
    gdi.CreateDIBSection.restype = wintypes.HANDLE
    windows = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    @callback_type
    def collect(hwnd, parameter):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        rect = wintypes.RECT()
        if owner.value == pid and user.GetClientRect(hwnd, ctypes.byref(rect)):
            width, height = rect.right, rect.bottom
            if width > 100 and height > 100:
                title = ctypes.create_unicode_buffer(512)
                user.GetWindowTextW(hwnd, title, 512)
                windows.append((width * height, hwnd, width, height, title.value))
        return True

    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.EnumWindows(collect, 0)
    if not windows:
        return {'captured': False, 'reason': 'No game-owned window'}
    _, hwnd, width, height, title = max(windows, key=lambda item: item[0])
    if width * height > 16000000:
        return {'captured': False, 'reason': 'Window too large'}
    dc = user.GetDC(hwnd)
    memory_dc = gdi.CreateCompatibleDC(dc)
    bitmap_info = ctypes.create_string_buffer(struct.pack('<IiiHHIIiiII', 40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0))
    pixels = ctypes.c_void_p()
    bitmap = gdi.CreateDIBSection(dc, bitmap_info, 0, ctypes.byref(pixels), None, 0)
    if not bitmap:
        gdi.DeleteDC(memory_dc)
        user.ReleaseDC(hwnd, dc)
        return {'captured': False, 'reason': 'CreateDIBSection failed'}
    previous = gdi.SelectObject(memory_dc, bitmap)
    try:
        success = bool(user.PrintWindow(hwnd, memory_dc, 3))
        if not success:
            return {'captured': False, 'reason': 'PrintWindow failed', 'title': title}
        bgra = ctypes.string_at(pixels, width * height * 4)
        method = 'PrintWindow'
        if not any(bgra):
            user.ShowWindow(hwnd, 9)
            user.SetForegroundWindow(hwnd)
            ctypes.WinDLL('dwmapi').DwmFlush()
            if user.GetForegroundWindow() == hwnd:
                point = wintypes.POINT(0, 0)
                user.ClientToScreen(hwnd, ctypes.byref(point))
                screen = user.GetDC(None)
                try:
                    if gdi.BitBlt(memory_dc, 0, 0, width, height, screen, point.x, point.y, 0x40cc0020):
                        bgra = ctypes.string_at(pixels, width * height * 4)
                        method = 'ForegroundClientBitBlt'
                finally:
                    user.ReleaseDC(None, screen)
        rgb = bytearray(width * height * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = bgra[2::4], bgra[1::4], bgra[0::4]
        scanlines = b''.join(b'\0' + rgb[row * width * 3:(row + 1) * width * 3] for row in range(height))

        def chunk(kind, value):
            return struct.pack('>I', len(value)) + kind + value + struct.pack('>I', zlib.crc32(kind + value) & 0xffffffff)

        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
        png += chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b'')
        destination.write_bytes(png)
        return {'captured': True, 'method': method, 'title': title, 'width': width, 'height': height,
                'nonblack_bytes': sum(value != 0 for value in rgb)}
    finally:
        gdi.SelectObject(memory_dc, previous)
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(memory_dc)
        user.ReleaseDC(hwnd, dc)
