"""Canonical RGBA8 PNG captures and D5's independent acceptance criteria.
Exact witness centers cannot be hidden by the whole-image error budget.
"""
from pathlib import Path
import struct
import zlib


def write_png(path, width, height, rgba):
    if len(rgba) != width * height * 4:
        raise ValueError("Wrong RGBA byte count")

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\0" + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def read_png(path, *, allow_rgb=False):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Not a PNG (check Git LFS checkout)")
    offset, payload, dimensions = 8, bytearray(), None
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        content = data[offset + 8:offset + 8 + length]
        if zlib.crc32(kind + content) != struct.unpack_from(">I", data, offset + 8 + length)[0]:
            raise ValueError("PNG CRC mismatch")
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", content)
            if (depth, compression, filtering, interlace) != (8, 0, 0, 0) or color not in ([2, 6] if allow_rgb else [6]):
                raise ValueError("Reference must be non-interlaced RGBA8")
            dimensions = width, height
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            payload.extend(content)
        elif kind == b"IEND":
            break
        offset += length + 12
    if dimensions is None:
        raise ValueError("Missing PNG dimensions")
    width, height = dimensions
    rows = zlib.decompress(payload)
    stride = width * channels
    if len(rows) != height * (stride + 1):
        raise ValueError("Invalid PNG data size")
    result = bytearray(width * height * channels)
    for y in range(height):
        mode = rows[y * (stride + 1)]
        if mode > 4:
            raise ValueError("Invalid PNG filter")
        for x in range(stride):
            index = y * stride + x
            left = result[index - channels] if x >= channels else 0
            up = result[index - stride] if y else 0
            corner = result[index - stride - channels] if y and x >= channels else 0
            p = left + up - corner
            distances = [abs(p - left), abs(p - up), abs(p - corner)]
            paeth = [left, up, corner][distances.index(min(distances))]
            predict = [0, left, up, (left + up) // 2, paeth][mode]
            result[index] = (rows[y * (stride + 1) + 1 + x] + predict) & 255
    if channels == 3:
        rgba = bytearray(width * height * 4)
        for channel in range(3):
            rgba[channel::4] = result[channel::3]
        rgba[3::4] = b"\xff" * (width * height)
        result = rgba
    return width, height, bytes(result)


def probes(scene, actual):
    return [{**probe, "actual": list(actual[(probe["y"] * scene["width"] + probe["x"]) * 4:
                                            (probe["y"] * scene["width"] + probe["x"]) * 4 + 4])}
            for probe in scene["probes"]]


def compare(scene, actual, expected):
    count = scene["width"] * scene["height"]
    if len(actual) != count * 4 or len(expected) != count * 4:
        raise ValueError("Image dimensions differ")
    differences = [abs(a - b) for a, b in zip(actual, expected)]
    changed = sum(max(differences[i:i + 4]) > scene["channel_tolerance"] for i in range(0, len(actual), 4))
    samples = probes(scene, actual)
    reference_samples = probes(scene, expected)
    exact = all(p["actual"] == p["rgba"] for p in samples + reference_samples)
    fraction = changed / count
    report = dict(passed=exact and fraction <= scene["max_different_fraction"], exact_probes=exact,
                  different_pixels=changed, different_fraction=fraction, max_channel_error=max(differences),
                  probes=samples, reference_probes=reference_samples)
    diff = bytes(value for i in range(0, len(actual), 4) for value in [*differences[i:i + 3], 255])
    return report, diff
