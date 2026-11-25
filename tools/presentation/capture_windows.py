#!/usr/bin/env python3
"""Read-only Windows presentation capture; never changes focus or injects input."""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gpu"))
from compare import write_png
from window_visibility import require_unobscured
user, gdi = c.WinDLL("user32", use_last_error=True), c.WinDLL("gdi32", use_last_error=True)
dwm = c.WinDLL("dwmapi", use_last_error=True)

class BitmapInfo(c.Structure):
    _fields_ = [("size", w.DWORD), ("width", w.LONG), ("height", w.LONG), ("planes", w.WORD), ("bits", w.WORD),
                ("compression", w.DWORD), ("image_bytes", w.DWORD), ("xppm", w.LONG), ("yppm", w.LONG),
                ("used", w.DWORD), ("important", w.DWORD), ("color", w.DWORD)]


def api(library, name, result, *args):
    function = getattr(library, name)
    function.restype, function.argtypes = result, args
    return function


callback = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
enum_windows = api(user, "EnumWindows", w.BOOL, callback, w.LPARAM)
window_pid = api(user, "GetWindowThreadProcessId", w.DWORD, w.HWND, c.POINTER(w.DWORD))
visible = api(user, "IsWindowVisible", w.BOOL, w.HWND)
minimized = api(user, "IsIconic", w.BOOL, w.HWND)
get_window = api(user, "GetWindow", w.HWND, w.HWND, w.UINT)
window_rect = api(user, "GetWindowRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
window_attribute = api(dwm, "DwmGetWindowAttribute", c.c_long, w.HWND, w.DWORD, c.c_void_p, w.DWORD)
client_rect = api(user, "GetClientRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
client_origin = api(user, "ClientToScreen", w.BOOL, w.HWND, c.POINTER(w.POINT))
get_foreground = api(user, "GetForegroundWindow", w.HWND)
get_dc = api(user, "GetDC", w.HDC, w.HWND)
release_dc = api(user, "ReleaseDC", c.c_int, w.HWND, w.HDC)
create_dc = api(gdi, "CreateCompatibleDC", w.HDC, w.HDC)
create_bitmap = api(gdi, "CreateCompatibleBitmap", w.HBITMAP, w.HDC, c.c_int, c.c_int)
select = api(gdi, "SelectObject", w.HANDLE, w.HDC, w.HANDLE)
blit = api(gdi, "BitBlt", w.BOOL, w.HDC, c.c_int, c.c_int, c.c_int, c.c_int, w.HDC, c.c_int, c.c_int, w.DWORD)
bits = api(gdi, "GetDIBits", c.c_int, w.HDC, w.HBITMAP, w.UINT, w.UINT, c.c_void_p, c.POINTER(BitmapInfo), w.UINT)
delete_object = api(gdi, "DeleteObject", w.BOOL, w.HANDLE)
delete_dc = api(gdi, "DeleteDC", w.BOOL, w.HDC)
api(user, "SetProcessDpiAwarenessContext", w.BOOL, w.HANDLE)(w.HANDLE(-4))


def require(value, message):
    if not value:
        raise RuntimeError(f"{message} (Win32 {c.get_last_error()})")


def find_window(pid):
    found = []
    @callback
    def visit(window, _):
        owner = w.DWORD()
        window_pid(window, c.byref(owner))
        if owner.value == pid and visible(window):
            found.append(window)
        return True
    enum_windows(visit, 0)
    return found[0] if found else None


def extent(window):
    rect, origin = w.RECT(), w.POINT()
    require(client_rect(window, c.byref(rect)), "Client bounds unavailable")
    require(client_origin(window, c.byref(origin)), "Client origin unavailable")
    return rect.right, rect.bottom, origin



def unobscured_extent(window):
    require(get_foreground() == window, "Sandbox is not foreground")
    width, height, origin = extent(window)
    require(width > 0 and height > 0 and not minimized(window), "Sandbox is minimized or empty")
    above = []
    seen = {window}
    current = get_window(window, 3)  # GW_HWNDPREV: above this top-level window in Z order.
    while current:
        require(current not in seen, "Window order changed during capture")
        seen.add(current)
        if visible(current) and not minimized(current):
            cloaked = w.DWORD()
            require(window_attribute(current, 14, c.byref(cloaked), c.sizeof(cloaked)) >= 0,
                    "Window visibility unavailable")  # DWMWA_CLOAKED
            if not cloaked.value:
                rect = w.RECT()
                # DWM bounds omit invisible resize borders; fallback remains conservative.
                if window_attribute(current, 9, c.byref(rect), c.sizeof(rect)) < 0:
                    require(window_rect(current, c.byref(rect)), "Window bounds unavailable")
                above.append(((rect.left, rect.top, rect.right, rect.bottom), True, False, False))
        current = get_window(current, 3)
    require_unobscured((origin.x, origin.y, origin.x + width, origin.y + height), above)
    return width, height, origin.x, origin.y


def capture(window, path):
    width, height, x, y = unobscured_extent(window)
    screen = get_dc(None)
    memory = create_dc(screen)
    bitmap = create_bitmap(screen, width, height)
    old = select(memory, bitmap)
    try:
        require(blit(memory, 0, 0, width, height, screen, x, y, 0x00CC0020), "Screen capture failed")
        select(memory, old)
        info = BitmapInfo(40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0, 0)
        data = c.create_string_buffer(width * height * 4)
        require(bits(memory, bitmap, 0, height, data, c.byref(info), 0) == height, "Bitmap readback failed")
        bgra = data.raw
        rgba = bytearray(bgra)
        rgba[0::4], rgba[2::4], rgba[3::4] = bgra[2::4], bgra[0::4], b"\xff" * (width * height)
        require(unobscured_extent(window) == (width, height, x, y), "Sandbox moved during capture")
        write_png(path, width, height, rgba)
    finally:
        select(memory, old)
        delete_object(bitmap)
        delete_dc(memory)
        release_dc(None, screen)



def snapshot(pid, path):
    window = find_window(pid)
    require(window, "No sandbox window")
    capture(window, path)

if __name__ == "__main__":
    from capture import main
    sys.exit(main())
