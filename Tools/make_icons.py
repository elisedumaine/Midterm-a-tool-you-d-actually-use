"""Draw the World Builder panel icons as white PNGs (tinted in Unreal).

Pure standard library: each icon is a few simple shapes (circles, rings,
thick strokes, polygons) rasterised with 4x4 supersampling for smooth edges.

    python Tools/make_icons.py <output folder>
"""
import math
import os
import struct
import sys
import zlib

SIZE = 64
SAMPLES = 4


# --- Shapes: each returns True when the point (x, y in 0..1) is inside -------
def circle(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def ring(cx, cy, r, w):
    return lambda x, y: (r - w / 2) ** 2 <= (x - cx) ** 2 + (y - cy) ** 2 <= (r + w / 2) ** 2


def arc(cx, cy, r, w, start, end):
    """Ring between two angles in degrees (0 = right, counter-clockwise, y up)."""
    def inside(x, y):
        d2 = (x - cx) ** 2 + (y - cy) ** 2
        if not (r - w / 2) ** 2 <= d2 <= (r + w / 2) ** 2:
            return False
        a = math.degrees(math.atan2(cy - y, x - cx)) % 360
        return (start <= a <= end) if start <= end else (a >= start or a <= end)
    return inside


def stroke(points, w):
    """A thick polyline with round caps."""
    segments = list(zip(points, points[1:]))

    def inside(x, y):
        for (ax, ay), (bx, by) in segments:
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy or 1e-9)))
            if (x - ax - t * dx) ** 2 + (y - ay - t * dy) ** 2 <= (w / 2) ** 2:
                return True
        return False
    return inside


def polygon(points):
    def inside(x, y):
        hit = False
        for (ax, ay), (bx, by) in zip(points, points[1:] + points[:1]):
            if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
                hit = not hit
        return hit
    return inside


def rect(x0, y0, x1, y1, radius=0.0):
    def inside(x, y):
        if not (x0 <= x <= x1 and y0 <= y <= y1):
            return False
        cx, cy = min(max(x, x0 + radius), x1 - radius), min(max(y, y0 + radius), y1 - radius)
        return (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius + 1e-9
    return inside


def ellipse(cx, cy, rx, ry, angle=0.0):
    c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    return lambda x, y: (((x - cx) * c + (y - cy) * s) / rx) ** 2 + ((-(x - cx) * s + (y - cy) * c) / ry) ** 2 <= 1.0


def sun_rays(cx, cy, r0, r1, count, w, start=0.0, spread=360.0):
    rays = []
    for i in range(count):
        a = math.radians(start + spread * i / (count - 1 if spread < 360 else count))
        rays.append(stroke([(cx + math.cos(a) * r0, cy - math.sin(a) * r0), (cx + math.cos(a) * r1, cy - math.sin(a) * r1)], w))
    return rays


def cloud(dy=0.0, scale=1.0):
    k = scale
    return [circle(0.36, 0.56 + dy, 0.15 * k), circle(0.54, 0.47 + dy, 0.2 * k), circle(0.7, 0.58 + dy, 0.13 * k),
            rect(0.24, 0.56 + dy, 0.8, 0.71 + dy, 0.07)]


def pine(x, base, height, width):
    return [polygon([(x, base - height), (x - width / 2, base - height * 0.25), (x + width / 2, base - height * 0.25)]),
            rect(x - 0.025, base - height * 0.28, x + 0.025, base, 0.01)]


# --- Icons ---------------------------------------------------------------------
ICONS = {
    "sun": ([circle(0.5, 0.5, 0.17)] + sun_rays(0.5, 0.5, 0.27, 0.4, 8, 0.07), []),
    "golden_hour": ([circle(0.5, 0.66, 0.2), stroke([(0.1, 0.78), (0.9, 0.78)], 0.06)]
                    + sun_rays(0.5, 0.66, 0.3, 0.42, 5, 0.06, 20, 140), [rect(0, 0.7, 1, 0.74)]),
    "sunset": ([circle(0.5, 0.55, 0.22), stroke([(0.12, 0.66), (0.88, 0.66)], 0.06), stroke([(0.24, 0.78), (0.76, 0.78)], 0.06),
                stroke([(0.36, 0.9), (0.64, 0.9)], 0.06)], [rect(0, 0.6, 1, 0.63), rect(0, 0.69, 1, 1)]),
    "moon": ([circle(0.48, 0.52, 0.3), polygon([(0.78, 0.14), (0.81, 0.21), (0.88, 0.24), (0.81, 0.27), (0.78, 0.34), (0.75, 0.27), (0.68, 0.24), (0.75, 0.21)])],
             [circle(0.62, 0.42, 0.25)]),
    "cloud": (cloud(), []),
    "rain": (cloud(-0.12) + [stroke([(0.36, 0.7), (0.3, 0.86)], 0.06), stroke([(0.54, 0.7), (0.48, 0.86)], 0.06), stroke([(0.72, 0.7), (0.66, 0.86)], 0.06)], []),
    "snow": (cloud(-0.12) + [circle(0.34, 0.8, 0.045), circle(0.52, 0.86, 0.045), circle(0.7, 0.8, 0.045)], []),
    "spring": ([circle(0.5 + 0.2 * math.cos(math.radians(a)), 0.5 - 0.2 * math.sin(math.radians(a)), 0.13) for a in range(90, 450, 72)],
               [circle(0.5, 0.5, 0.08)]),
    "summer": ([circle(0.5, 0.4, 0.27), rect(0.45, 0.55, 0.55, 0.9, 0.02)], []),
    "autumn": ([ellipse(0.5, 0.48, 0.22, 0.34, 40), stroke([(0.3, 0.84), (0.42, 0.66)], 0.06)], [stroke([(0.38, 0.64), (0.64, 0.3)], 0.04)]),
    "winter": ([stroke([(0.5 + 0.36 * math.cos(math.radians(a)), 0.5 - 0.36 * math.sin(math.radians(a))), (0.5 - 0.36 * math.cos(math.radians(a)), 0.5 + 0.36 * math.sin(math.radians(a)))], 0.06) for a in (90, 30, 150)]
               + [stroke([(0.5 + 0.24 * math.cos(math.radians(a)) + 0.08 * math.cos(math.radians(a + s)), 0.5 - 0.24 * math.sin(math.radians(a)) - 0.08 * math.sin(math.radians(a + s))),
                           (0.5 + 0.24 * math.cos(math.radians(a)), 0.5 - 0.24 * math.sin(math.radians(a)))], 0.05) for a in range(30, 390, 60) for s in (140, -140)], []),
    "none": ([ring(0.5, 0.5, 0.3, 0.07), stroke([(0.29, 0.71), (0.71, 0.29)], 0.07)], []),
    "trail": ([stroke([(0.3, 0.9), (0.42, 0.72), (0.62, 0.62), (0.66, 0.46), (0.5, 0.32), (0.56, 0.12)], 0.08)], []),
    "network": ([stroke([(0.5, 0.9), (0.5, 0.52), (0.22, 0.16)], 0.08), stroke([(0.5, 0.52), (0.8, 0.2)], 0.08), stroke([(0.5, 0.66), (0.86, 0.66)], 0.07)], []),
    "scale": ([stroke([(0.18, 0.18), (0.82, 0.82)], 0.06), stroke([(0.18, 0.18), (0.38, 0.18)], 0.06), stroke([(0.18, 0.18), (0.18, 0.38)], 0.06),
               stroke([(0.82, 0.82), (0.62, 0.82)], 0.06), stroke([(0.82, 0.82), (0.82, 0.62)], 0.06)], []),
    "hills": ([circle(0.34, 0.86, 0.36), circle(0.74, 0.92, 0.3)], [rect(0, 0.86, 1, 1)]),
    "vegetation": ([stroke([(0.5, 0.88), (0.5, 0.42)], 0.06), ellipse(0.34, 0.42, 0.17, 0.09, -35), ellipse(0.66, 0.32, 0.18, 0.09, 35)], []),
    "forest": (pine(0.36, 0.86, 0.66, 0.4) + pine(0.66, 0.86, 0.5, 0.32), []),
    "wildlife": ([ellipse(0.5, 0.66, 0.19, 0.16), circle(0.26, 0.42, 0.08), circle(0.41, 0.28, 0.08), circle(0.59, 0.28, 0.08), circle(0.74, 0.42, 0.08)], []),
    "warmth": ([rect(0.43, 0.14, 0.57, 0.66, 0.07), circle(0.5, 0.74, 0.14)], [rect(0.47, 0.2, 0.53, 0.6, 0.03)]),
    "ground": ([circle(0.5, 1.15, 0.6)] + [circle(x, y, 0.035) for x, y in ((0.3, 0.42), (0.5, 0.34), (0.7, 0.44))], []),
    "haze": ([stroke([(0.18, 0.32), (0.82, 0.32)], 0.07), stroke([(0.28, 0.5), (0.72, 0.5)], 0.07), stroke([(0.14, 0.68), (0.62, 0.68)], 0.07),
              stroke([(0.38, 0.84), (0.86, 0.84)], 0.07)], []),
    "sparkles": ([polygon([(0.42, 0.12), (0.5, 0.38), (0.76, 0.46), (0.5, 0.54), (0.42, 0.82), (0.34, 0.54), (0.08, 0.46), (0.34, 0.38)]),
                  polygon([(0.76, 0.1), (0.8, 0.22), (0.92, 0.26), (0.8, 0.3), (0.76, 0.42), (0.72, 0.3), (0.6, 0.26), (0.72, 0.22)])], []),
    "world": ([polygon([(0.06, 0.84), (0.38, 0.32), (0.62, 0.7), (0.72, 0.56), (0.94, 0.84)]), circle(0.74, 0.24, 0.1)], []),
    "leaf": ([ellipse(0.5, 0.46, 0.24, 0.36, 35), stroke([(0.26, 0.86), (0.4, 0.68)], 0.06)], [stroke([(0.36, 0.66), (0.66, 0.26)], 0.04)]),
    "build": ([polygon([(0.42, 0.12), (0.5, 0.38), (0.76, 0.46), (0.5, 0.54), (0.42, 0.82), (0.34, 0.54), (0.08, 0.46), (0.34, 0.38)]),
               circle(0.78, 0.22, 0.07), circle(0.82, 0.72, 0.05)], []),
    "refresh": ([arc(0.5, 0.5, 0.28, 0.08, 40, 320), polygon([(0.66, 0.16), (0.9, 0.26), (0.7, 0.42)])], []),
    "preview": ([stroke([(0.08 + 0.84 * t / 20, 0.5 - 0.27 * math.sin(math.pi * t / 20)) for t in range(21)], 0.07),
                 stroke([(0.08 + 0.84 * t / 20, 0.5 + 0.27 * math.sin(math.pi * t / 20)) for t in range(21)], 0.07),
                 circle(0.5, 0.5, 0.12)], []),
    "save": ([stroke([(0.5, 0.12), (0.5, 0.56)], 0.08), polygon([(0.3, 0.42), (0.7, 0.42), (0.5, 0.66)]),
              stroke([(0.16, 0.6), (0.16, 0.84), (0.84, 0.84), (0.84, 0.6)], 0.08)], []),
    "load": ([rect(0.12, 0.26, 0.88, 0.8, 0.06), rect(0.12, 0.18, 0.44, 0.3, 0.04)], [rect(0.18, 0.38, 0.82, 0.74, 0.03)]),
    "reset": ([arc(0.5, 0.5, 0.28, 0.08, 220, 140), polygon([(0.34, 0.16), (0.1, 0.26), (0.3, 0.42)])], []),
    "delete": ([rect(0.24, 0.3, 0.76, 0.88, 0.05), rect(0.16, 0.18, 0.84, 0.26, 0.03), rect(0.4, 0.1, 0.6, 0.2, 0.02)],
               [rect(0.36, 0.4, 0.42, 0.78, 0.02), rect(0.58, 0.4, 0.64, 0.78, 0.02)]),
    "seed": ([rect(0.16, 0.16, 0.84, 0.84, 0.12)], [circle(0.34, 0.34, 0.07), circle(0.66, 0.66, 0.07), circle(0.5, 0.5, 0.07), circle(0.66, 0.34, 0.07), circle(0.34, 0.66, 0.07)]),
    "mountains": ([polygon([(0.04, 0.84), (0.34, 0.3), (0.56, 0.62), (0.7, 0.42), (0.96, 0.84)])], [polygon([(0.34, 0.3), (0.27, 0.43), (0.41, 0.43)])]),
    "character": ([circle(0.5, 0.28, 0.13), rect(0.3, 0.46, 0.7, 0.9, 0.18)], []),
    "palette": ([circle(0.5, 0.5, 0.36)], [circle(0.38, 0.36, 0.06), circle(0.56, 0.3, 0.06), circle(0.7, 0.44, 0.06), circle(0.62, 0.66, 0.1)]),
    "clear": ([circle(0.5, 0.5, 0.15)] + sun_rays(0.5, 0.5, 0.25, 0.36, 8, 0.06), []),
}


def render(adds, subtracts):
    pixels = bytearray()
    step = 1.0 / (SIZE * SAMPLES)
    for py in range(SIZE):
        pixels.append(0)  # PNG filter: none
        for px in range(SIZE):
            hits = 0
            for sy in range(SAMPLES):
                y = (py * SAMPLES + sy + 0.5) * step
                for sx in range(SAMPLES):
                    x = (px * SAMPLES + sx + 0.5) * step
                    if any(shape(x, y) for shape in adds) and not any(shape(x, y) for shape in subtracts):
                        hits += 1
            alpha = round(255 * hits / (SAMPLES * SAMPLES))
            pixels += bytes((255, 255, 255, alpha))
    return bytes(pixels)


def write_png(path, raw):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)
    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    folder = sys.argv[1] if len(sys.argv) > 1 else "icons"
    os.makedirs(folder, exist_ok=True)
    for name, (adds, subtracts) in ICONS.items():
        write_png(os.path.join(folder, "ICO_{}.png".format(name)), render(adds, subtracts))
    print("wrote", len(ICONS), "icons to", folder)
