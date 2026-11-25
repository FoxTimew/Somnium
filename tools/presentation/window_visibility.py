"""Conservative geometry check for windows above a presentation surface."""


def overlaps(first, second):
    """Rectangles use exclusive right/bottom edges, like Win32 RECT."""
    return (max(first[0], second[0]) < min(first[2], second[2])
            and max(first[1], second[1]) < min(first[3], second[3]))


def require_unobscured(bounds, windows_above):
    # Foreground alone is insufficient: a topmost picture-in-picture can cover it.
    for rect, visible, minimized, cloaked in windows_above:
        if visible and not minimized and not cloaked and overlaps(bounds, rect):
            raise RuntimeError("Sandbox is obscured by another window; close overlays before capturing")
