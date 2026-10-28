#!/usr/bin/env python3
"""Renders the Settings section icons into res/settings/ as PNGs.

Each icon is a squircle tile in the iOS 26 style (Apple's published dark-mode colour values, a
diagonal gradient, a glass rim brighter at the top left, a soft sheen) with a white symbol.
The gear, Wi-Fi, Bluetooth, gamepad, switch, face-button and battery symbols are drawn here from simple
shapes for this project. The palette, sun and info symbols come from Phosphor Icons 2.1.2
(MIT), downloaded and checked against a pinned hash. No Apple artwork is used.

Needs Pillow and numpy:  python3 tools/settings-icons.py
Design notes: TDD section 12.4.3, mockups 17 to 22.
"""
import hashlib
import io
import math
import os
import tarfile
import urllib.request

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "res", "settings")
CACHE = os.path.join(HERE, "..", "build", "cache")
PHOSPHOR_URL = "https://registry.npmjs.org/@phosphor-icons/web/-/web-2.1.2.tgz"
PHOSPHOR_SHA256 = "40e3096099ca818c047979cece6a3764944d5200c67b64092f7bcd8bcd1d2b08"
SIZES = (36, 150)  # the sidebar row, and the summary card in the pane
SS = 4  # drawn at 4x, then scaled down

# Apple's dark-mode system colours (published values, not assets).
BLUE, GRAY, INDIGO, PINK, ORANGE, GRAY2 = (10, 132, 255), (142, 142, 147), (94, 92, 230), (255, 55, 95), (255, 159, 10), (99, 99, 102)
GREEN = (48, 209, 88)


def phosphor_fill_font():
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, "phosphor-web-2.1.2.tgz")
    if not os.path.exists(path):
        urllib.request.urlretrieve(PHOSPHOR_URL, path + ".part")
        os.rename(path + ".part", path)
    data = open(path, "rb").read()
    if hashlib.sha256(data).hexdigest() != PHOSPHOR_SHA256:
        raise SystemExit("Phosphor download does not match the pinned hash")
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        return tar.extractfile("package/src/fill/Phosphor-Fill.ttf").read()


FONT_BYTES = None


def phosphor(code):
    def draw(S):
        f = ImageFont.truetype(io.BytesIO(FONT_BYTES), S)
        m = Image.new("L", (S * 2, S * 2), 0)
        ImageDraw.Draw(m).text((S // 2, S // 2), chr(code), font=f, fill=255)
        return m.crop(m.getbbox())
    return draw


def soften(m, S):
    # rounds every corner a little, as Apple's symbols are
    m = m.filter(ImageFilter.GaussianBlur(S * 0.008)).point(lambda v: 255 if v > 127 else 0)
    return m.filter(ImageFilter.GaussianBlur(S * 0.002))


def canvas(S):
    m = Image.new("L", (S, S), 0)
    return m, ImageDraw.Draw(m), (lambda *p: [v * S for v in p])


def gear(S):
    m, d, _ = canvas(S)
    pts = []
    for i in range(720):
        a = 2 * math.pi * i / 720
        t = max(0.0, min(1.0, (math.cos(8 * a) + 0.25) / 0.5))
        r = 0.285 + 0.075 * t * t * (3 - 2 * t)
        pts.append((S * (0.5 + r * math.cos(a)), S * (0.5 + r * math.sin(a))))
    d.polygon(pts, fill=255)
    r = 0.115
    d.ellipse((S * (0.5 - r), S * (0.5 - r), S * (0.5 + r), S * (0.5 + r)), fill=0)
    return soften(m, S)


def wifi(S):
    m, d, _ = canvas(S)
    cx, cy, th = 0.5 * S, 0.76 * S, 0.095 * S
    for R in (0.27, 0.45):
        R *= S
        d.arc((cx - R, cy - R, cx + R, cy + R), -135, -45, fill=255, width=int(th))
        for a in (-135, -45):
            rr = R - th / 2
            x, y = cx + rr * math.cos(math.radians(a)), cy + rr * math.sin(math.radians(a))
            d.ellipse((x - th / 2, y - th / 2, x + th / 2, y + th / 2), fill=255)
    r = 0.062 * S
    d.ellipse((cx - r, cy - r - 0.03 * S, cx + r, cy + r - 0.03 * S), fill=255)
    return soften(m, S)


def bluetooth(S):
    m, d, _ = canvas(S)
    w = int(0.085 * S)
    P = [(x * S, y * S) for x, y in ((0.33, 0.35), (0.66, 0.64), (0.5, 0.79), (0.5, 0.21), (0.66, 0.36), (0.33, 0.65))]
    d.line(P, fill=255, width=w, joint="curve")
    for x, y in (P[0], P[-1], P[2], P[3]):
        d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=255)
    return soften(m, S)


def gamepad(S):
    m, d, u = canvas(S)
    d.rounded_rectangle(u(0.14, 0.30, 0.86, 0.62), radius=0.15 * S, fill=255)
    d.ellipse(u(0.12, 0.40, 0.42, 0.74), fill=255)
    d.ellipse(u(0.58, 0.40, 0.88, 0.74), fill=255)
    d.rounded_rectangle(u(0.235, 0.445, 0.405, 0.495), radius=0.012 * S, fill=0)
    d.rounded_rectangle(u(0.295, 0.385, 0.345, 0.555), radius=0.012 * S, fill=0)
    for x, y in ((0.68, 0.42), (0.68, 0.52), (0.63, 0.47), (0.73, 0.47)):
        d.ellipse(u(x - 0.03, y - 0.03, x + 0.03, y + 0.03), fill=0)
    return soften(m, S)


def switch(S):
    m, d, u = canvas(S)
    d.rounded_rectangle(u(0.18, 0.30, 0.82, 0.70), radius=0.20 * S, fill=255)
    d.ellipse(u(0.50, 0.38, 0.74, 0.62), fill=0)
    return soften(m, S)


def face_buttons(S):
    m, d, u = canvas(S)
    r = 0.115
    for x, y in ((0.5, 0.27), (0.5, 0.73), (0.27, 0.5), (0.73, 0.5)):
        d.ellipse(u(x - r, y - r, x + r, y + r), fill=255)
    return soften(m, S)


def battery(S):
    m, d, u = canvas(S)
    d.rounded_rectangle(u(0.16, 0.33, 0.78, 0.67), radius=0.09 * S, fill=255)
    d.rounded_rectangle(u(0.215, 0.385, 0.725, 0.615), radius=0.045 * S, fill=0)
    d.rounded_rectangle(u(0.255, 0.425, 0.58, 0.575), radius=0.025 * S, fill=255)
    d.rounded_rectangle(u(0.80, 0.43, 0.85, 0.57), radius=0.025 * S, fill=255)
    return soften(m, S)


# slug, tile colour, symbol
ICONS = [
    ("network", BLUE, wifi),
    ("bluetooth", BLUE, bluetooth),
    ("display", BLUE, phosphor(0xE472)),     # sun
    ("appearance", INDIGO, phosphor(0xE6C8)),  # palette
    ("in-game", PINK, gamepad),
    ("buttons", GRAY2, face_buttons),
    ("fn-switch", ORANGE, switch),
    ("battery", GREEN, battery),
    ("system", GRAY, gear),
    ("about", GRAY, phosphor(0xE2CE)),       # info
]


def fit(m, S):
    # Every symbol fits one frame: at most 0.70 of the tile wide (0.76 for wide symbols) and
    # 0.62 tall, centred on its ink box.
    g = m.crop(m.getbbox())
    w, h = g.size
    k = min((0.76 if w > 1.4 * h else 0.70) * S / w, 0.62 * S / h)
    g = g.resize((max(1, round(w * k)), max(1, round(h * k))), Image.LANCZOS)
    out = Image.new("L", (S, S), 0)
    out.paste(g, ((S - g.width) // 2, (S - g.height) // 2))
    return out


def squircle(S, n=5.0):
    r = S / 2
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) + 0.5
    inside = np.abs((x - r) / r) ** n + np.abs((y - r) / r) ** n <= 1
    return Image.fromarray((inside * 255).astype(np.uint8))


def light(c, a):
    return tuple(min(255, int(ch + (255 - ch) * a)) for ch in c)


def dark(c, a):
    return tuple(int(ch * (1 - a)) for ch in c)


def ramp(S, a, b, diagonal):
    y, x = np.mgrid[0:S, 0:S].astype(np.float32)
    t = (x + y) / (2 * S - 2) if diagonal else y / (S - 1)
    return t, (a + (b - a) * t)


def colour_field(S, top, bottom, diagonal):
    t, _ = ramp(S, 0, 1, diagonal)
    a, b = np.array(top, np.float32), np.array(bottom, np.float32)
    rgb = a + (b - a) * t[..., None]
    alpha = np.full((S, S, 1), 255, np.float32)
    return Image.fromarray(np.concatenate([rgb, alpha], 2).astype(np.uint8), "RGBA")


def tile(base, symbol, size):
    S = size * SS
    M = squircle(S)
    t = colour_field(S, light(base, 0.22), dark(base, 0.16), True)
    # soft sheen across the upper part, like light on glass
    sheen = Image.new("L", (S, S), 0)
    ImageDraw.Draw(sheen).ellipse((-S * 0.4, -S * 0.9, S * 1.4, S * 0.45), fill=34)
    t = Image.composite(Image.new("RGBA", (S, S), (255, 255, 255, 255)), t, sheen.filter(ImageFilter.GaussianBlur(S * 0.14)))
    gm = fit(symbol(S), S)
    shadow = Image.new("RGBA", (S, S), dark(base, 0.6) + (0,))
    shadow.putalpha(gm.filter(ImageFilter.GaussianBlur(S * 0.02)).point(lambda v: int(v * 0.45)))
    sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sh.paste(shadow, (0, int(S * 0.015)))
    t = Image.alpha_composite(t, sh)
    glyph = colour_field(S, (255, 255, 255), light(base, 0.84), False)
    glyph.putalpha(gm)
    t = Image.alpha_composite(t, glyph)
    # glass rim: bright on the top-left edge, faint on the bottom-right
    q = S // 8
    padded = Image.new("L", (S + 2 * q, S + 2 * q), 0)
    padded.paste(M, (q, q))
    inner = padded.filter(ImageFilter.GaussianBlur(S * 0.012)).point(lambda v: 255 if v >= 254 else 0).crop((q, q, q + S, q + S))
    ring = ImageChops.subtract(M, inner).filter(ImageFilter.GaussianBlur(S * 0.003))
    _, weight = ramp(S, 0.85, 0.18, True)
    ring = Image.fromarray((np.asarray(ring, np.float32) * weight).astype(np.uint8))
    rim = Image.new("RGBA", (S, S), (255, 255, 255, 0))
    rim.putalpha(ring)
    t = Image.alpha_composite(t, rim)
    t.putalpha(M)
    return t.resize((size, size), Image.LANCZOS)


def main():
    global FONT_BYTES
    FONT_BYTES = phosphor_fill_font()
    os.makedirs(OUT, exist_ok=True)
    for slug, colour, symbol in ICONS:
        for size in SIZES:
            path = os.path.join(OUT, f"{slug}-{size}.png")
            tile(colour, symbol, size).save(path, optimize=True)
            print(path)


if __name__ == "__main__":
    main()
