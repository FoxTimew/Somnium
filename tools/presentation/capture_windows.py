#!/usr/bin/env python3
"""Capture real sandbox client pixels and exercise native mouse/keyboard input.
An unlocked interactive desktop is required; application-control policy is untouched.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gpu"))
from compare import read_png, write_png
from run import native_validation_error

ROOT = Path(__file__).resolve().parents[2]
user, gdi = c.WinDLL("user32", use_last_error=True), c.WinDLL("gdi32", use_last_error=True)


class Mouse(c.Structure):
    _fields_ = [("dx", w.LONG), ("dy", w.LONG), ("data", w.DWORD), ("flags", w.DWORD), ("time", w.DWORD), ("extra", w.WPARAM)]


class Keyboard(c.Structure):
    _fields_ = [("key", w.WORD), ("scan", w.WORD), ("flags", w.DWORD), ("time", w.DWORD), ("extra", w.WPARAM)]


class InputUnion(c.Union):
    _fields_ = [("mouse", Mouse), ("keyboard", Keyboard)]


class Input(c.Structure):
    _fields_ = [("type", w.DWORD), ("value", InputUnion)]


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
client_rect = api(user, "GetClientRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
window_rect = api(user, "GetWindowRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
client_origin = api(user, "ClientToScreen", w.BOOL, w.HWND, c.POINTER(w.POINT))
foreground = api(user, "SetForegroundWindow", w.BOOL, w.HWND)
get_foreground = api(user, "GetForegroundWindow", w.HWND)
cursor = api(user, "SetCursorPos", w.BOOL, c.c_int, c.c_int)
send_input = api(user, "SendInput", w.UINT, w.UINT, c.POINTER(Input), c.c_int)
position = api(user, "SetWindowPos", w.BOOL, w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT)
close_window = api(user, "PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
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


def capture(window, path):
    require(get_foreground() == window, "Sandbox is not foreground")
    width, height, origin = extent(window)
    screen = get_dc(None)
    memory = create_dc(screen)
    bitmap = create_bitmap(screen, width, height)
    old = select(memory, bitmap)
    try:
        require(blit(memory, 0, 0, width, height, screen, origin.x, origin.y, 0x00CC0020), "Screen capture failed")
        select(memory, old)
        info = BitmapInfo(40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0, 0)
        data = c.create_string_buffer(width * height * 4)
        require(bits(memory, bitmap, 0, height, data, c.byref(info), 0) == height, "Bitmap readback failed")
        bgra = data.raw
        rgba = bytearray(bgra)
        rgba[0::4], rgba[2::4], rgba[3::4] = bgra[2::4], bgra[0::4], b"\xff" * (width * height)
        write_png(path, width, height, rgba)
    finally:
        select(memory, old)
        delete_object(bitmap)
        delete_dc(memory)
        release_dc(None, screen)


def mouse(flags, data=0):
    event = Input(0, InputUnion(mouse=Mouse(0, 0, data, flags, 0, 0)))
    require(send_input(1, c.byref(event), c.sizeof(Input)) == 1, "Mouse input unavailable")


def reset():
    for flag in [0, 2]:
        event = Input(1, InputUnion(keyboard=Keyboard(0x52, 0, flag, 0, 0)))
        require(send_input(1, c.byref(event), c.sizeof(Input)) == 1, "Keyboard input unavailable")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--backend", choices=["direct3d12", "vulkan"], required=True)
    args = parser.parse_args()
    executable, output = args.executable.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {"origin": "Windows interactive desktop", "backend": args.backend,
              "source_commit": os.getenv("GITHUB_SHA") or subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
              "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
              "model_sha256": hashlib.sha256((ROOT / "assets/models/orbit-cube.glb").read_bytes()).hexdigest(),
              "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(), "status": "échec", "captures": []}
    process, window = None, None
    try:
        with (output / "validation.log").open("w", encoding="utf-8") as log:
            process = subprocess.Popen([str(executable), "--backend", args.backend], cwd=output,
                env=dict(os.environ, SDL_ASSERT="abort", SDL_LOGGING="*=verbose"), stdout=log, stderr=subprocess.STDOUT)
            for _ in range(300):
                if process.poll() is not None:
                    raise RuntimeError(f"Sandbox exited before presentation: {process.returncode}")
                window = find_window(process.pid)
                text = (output / "validation.log").read_text(encoding="utf-8", errors="replace")
                if window and "First model frame presented" in text:
                    break
                time.sleep(.1)
            else:
                raise RuntimeError("No presented sandbox window within 30 seconds")
            foreground(window)
            time.sleep(.5)
            def shot(name):
                capture(window, output / (name + ".png"))
                report["captures"].append(name + ".png")
            shot("model-overlay")
            width, height, origin = extent(window)
            x, y = origin.x + int(width * .6), origin.y + int(height * .6)
            require(cursor(x, y), "Cannot position pointer")
            time.sleep(.1)
            mouse(0x0008)
            for step in range(1, 13):
                require(cursor(x + 5 * step, y - 2 * step), "Cannot move pointer")
                time.sleep(.05)
            mouse(0x0010)
            time.sleep(1)
            shot("orbit")
            for _ in range(2):
                mouse(0x0800, 120)
                time.sleep(.15)
            time.sleep(1)
            shot("zoom")
            reset()
            time.sleep(1)
            shot("reset")
            images = [read_png(output / (name + ".png")) for name in ["model-overlay", "orbit", "zoom", "reset"]]
            region = [4 * (yy * width + xx) for yy in range(height // 3, height * 9 // 10) for xx in range(width // 3, width * 9 // 10)]
            def changed(a, b):
                return sum(a[i:i+3] != b[i:i+3] for i in region)
            counts = {"orbit": changed(images[0][2], images[1][2]), "zoom": changed(images[1][2], images[2][2]),
                      "reset": changed(images[0][2], images[3][2])}
            report["model_region_different_pixels"] = counts
            require(counts["orbit"] >= 500 and counts["zoom"] >= 500 and counts["reset"] == 0,
                    "Orbit/zoom must change the model; reset must restore exact pixels")
            outer = w.RECT()
            require(window_rect(window, c.byref(outer)), "Window bounds unavailable")
            require(position(window, None, 0, 0, 960 + outer.right - outer.left - width,
                             600 + outer.bottom - outer.top - height, 0x0002 | 0x0004), "Resize failed")
            time.sleep(.6)
            shot("resize")
            report["resized_extent"] = read_png(output / "resize.png")[:2]
            require(report["resized_extent"] == (960, 600), "Wrong resized extent")
            report["status"] = "validé"
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        report["reason"] = str(error)
    finally:
        if process:
            if process.poll() is None and window:
                close_window(window, 0x0010, 0, 0)
            try:
                report["exit_code"] = process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                report["exit_code"] = -1
    log = (output / "validation.log").read_text(encoding="utf-8", errors="replace") if (output / "validation.log").exists() else ""
    report["validation_enabled"] = "Validation layers enabled" in log
    report["validation_errors"] = bool(native_validation_error(log))
    report["expected_device"] = "6600M" in log
    if report.get("exit_code") != 0 or not report["validation_enabled"] or report["validation_errors"] or not report["expected_device"]:
        report["status"] = "échec"
    (output / "result.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report["status"] == "validé" else 1


if __name__ == "__main__":
    sys.exit(main())
