#!/usr/bin/env python3
"""Generates Bramley's brand assets into docs/brand/: the mark as SVG (ivory, black, white), the
app icon (orchard glass tile) at several sizes, lockups, the boot logo (1024x768 24-bit BMP, the
format the Brick's boot partition takes) and a one-page brand sheet. The mark is drawn from
geometry here, so every size is exact. Design: docs/brand/BRAND.md.

Needs Pillow:  python3 tools/brand.py
"""
import math
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "res")
OUT = os.path.join(HERE, "..", "..", "..", "..", "docs", "brand")

# Colors (docs/brand/BRAND.md)
ORCHARD = (30, 64, 44)        # #1E402C
ORCHARD_NIGHT = (8, 22, 15)   # #08160F
IVORY = (243, 235, 214)       # #F3EBD6
ACCENT = (74, 178, 120)       # #4AB278
INK = (14, 14, 18)            # #0E0E12

# The mark: five apple pips meeting at the centre, as in an apple cut across. Unit square.
PIP_LENGTH, PIP_WIDTH, PIP_RADIUS, POINTS = 0.27, 0.112, 0.225, 160


def pip_outline(t):
    """A pip's outline at angle t (0 at the tip): (along, across), each about -0.5..0.5."""
    x, y = math.cos(t) / 2, math.sin(t) / 2
    k = (1 + math.cos(t)) / 2
    return x, y * (1 - 0.75 * k ** 1.6) * 1.55


def pips(size=1.0, cx=0.5, cy=0.5):
    """The five pips as polygons in a square of side size, centred at (cx, cy) * size."""
    out = []
    for n in range(5):
        a = -math.pi / 2 + n * 2 * math.pi / 5
        px, py, ang = cx + math.cos(a) * PIP_RADIUS, cy + math.sin(a) * PIP_RADIUS, a + math.pi
        poly = []
        for i in range(POINTS):
            u, v = pip_outline(i / POINTS * 2 * math.pi)
            u, v = u * PIP_LENGTH, v * PIP_WIDTH
            poly.append(((px + u * math.cos(ang) - v * math.sin(ang)) * size, (py + u * math.sin(ang) + v * math.cos(ang)) * size))
        out.append(poly)
    return out


def mark_mask(s, scale=1.0):
    m = Image.new("L", (s, s))
    d = ImageDraw.Draw(m)
    off = (1 - scale) / 2 * s
    for poly in pips(s * scale):
        d.polygon([(x + off, y + off) for x, y in poly], fill=255)
    return m


def svg(path, colour, background=None):
    hexc = "#%02X%02X%02X" % colour
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 1000">', '<title>Bramley</title>']
    if background:
        parts.append('<rect width="1000" height="1000" fill="#%02X%02X%02X"/>' % background)
    for poly in pips(1000):
        parts.append('<path fill="%s" d="M%s Z"/>' % (hexc, " L".join("%.1f %.1f" % p for p in poly)))
    parts.append("</svg>")
    with open(path, "w") as f:
        f.write("\n".join(parts) + "\n")


def squircle(s, n=5.0):
    m = Image.new("L", (s, s))
    px = m.load()
    r = s / 2
    for y in range(s):
        for x in range(s):
            if abs((x + 0.5 - r) / r) ** n + abs((y + 0.5 - r) / r) ** n <= 1:
                px[x, y] = 255
    return m


def icon(s):
    """The app icon: orchard glass tile with ivory pips. Drawn at 4x, then scaled down."""
    S = s * 4
    base = Image.new("RGB", (S, S))
    d = ImageDraw.Draw(base)
    for i in range(S * 2):  # diagonal gradient, orchard to orchard night
        t = i / (S * 2)
        d.line([(i, 0), (0, i)], fill=tuple(int(ORCHARD[k] + (ORCHARD_NIGHT[k] - ORCHARD[k]) * t) for k in range(3)))
    sheen = Image.new("L", (S, S))
    ImageDraw.Draw(sheen).ellipse((-S * 0.4, -S * 0.9, S * 1.2, S * 0.45), fill=55)
    base = Image.composite(Image.new("RGB", (S, S), (255, 255, 255)), base, sheen.filter(ImageFilter.GaussianBlur(S * 0.05)))
    base.paste(Image.new("RGB", (S, S), IVORY), (0, 0), mark_mask(S))
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sq = squircle(S)
    out.paste(base, (0, 0), sq)
    rim = ImageChops.subtract(sq, sq.filter(ImageFilter.MinFilter(int(S * 0.012) * 2 + 1)))
    out.paste(Image.new("RGBA", (S, S), (255, 255, 255, 80)), (0, 0), rim)
    return out.resize((s, s), Image.LANCZOS)


def font(weight, size):
    return ImageFont.truetype(os.path.join(RES, "Inter-%s.ttf" % weight), int(size))


def wordmark(d, x, y, size, weight, colour=IVORY, anchor="ls"):
    d.text((x, y), "Bramley", font=font(weight, size), fill=colour, anchor=anchor)


def boot_logo():
    """The boot logo, as Apple starts its devices: the bare mark in white, centred a little above the
    middle of a black screen. No tile and no name. The launcher's start-up splash puts the mark in
    the same place."""
    W, H, s = 1024, 768, 150
    img = Image.new("RGB", (W, H), (0, 0, 0))
    m = mark_mask(s * 4).resize((s, s), Image.LANCZOS)
    img.paste(Image.new("RGB", (s, s), (255, 255, 255)), ((W - s) // 2, (H - s) // 2 - 20), m)
    return img


def lockups():
    # stacked, for the boot screen and splash; horizontal, for headers
    st = Image.new("RGBA", (600, 600), (0, 0, 0, 0))
    t = icon(280)
    st.paste(t, (160, 70), t)
    wordmark(ImageDraw.Draw(st), 300, 470, 72, "Regular", anchor="ms")
    hz = Image.new("RGBA", (900, 240), (0, 0, 0, 0))
    t = icon(200)
    hz.paste(t, (20, 20), t)
    wordmark(ImageDraw.Draw(hz), 260, 150, 104, "Regular")
    return st, hz


def sheet(boot):
    W, H = 2000, 1300
    b = Image.new("RGB", (W, H), INK)
    d = ImageDraw.Draw(b)
    d.text((80, 60), "Bramley", font=font("SemiBold", 56), fill=(240, 240, 245))
    d.text((80, 135), "Brand sheet  ·  the five seeds inside every apple", font=font("Regular", 26), fill=(160, 160, 170))
    t = icon(360)
    b.paste(t, (80, 210), t)
    d.text((80, 590), "App icon", font=font("Regular", 22), fill=(160, 160, 170))
    # sizes
    x = 500
    for s in (150, 64, 36, 20):
        t = icon(s)
        b.paste(t, (x, 210 + (150 - s)), t)
        d.text((x, 375), "%d px" % s, font=font("Regular", 20), fill=(160, 160, 170))
        x += s + 50
    # mark alone, three colours
    for i, (bg, fg, label) in enumerate(((INK, IVORY, "Ivory"), ((250, 250, 250), (0, 0, 0), "Black"), (ORCHARD, (255, 255, 255), "White"))):
        sw = Image.new("RGB", (170, 170), bg)
        sw.paste(Image.new("RGB", (170, 170), fg), (0, 0), mark_mask(170, 0.8))
        b.paste(sw, (500 + i * 200, 430))
        d.text((500 + i * 200, 610), "Mark, " + label.lower(), font=font("Regular", 20), fill=(160, 160, 170))
    # boot
    bs = boot.resize((640, 480), Image.LANCZOS)
    b.paste(bs, (1280, 210))
    d.text((1280, 700), "Boot logo", font=font("Regular", 22), fill=(160, 160, 170))
    # colours
    y = 760
    d.text((80, y), "Color", font=font("SemiBold", 30), fill=(240, 240, 245))
    for i, (c, name, hexv, use) in enumerate(((ORCHARD, "Orchard", "#1E402C", "Tile, brand"), (ORCHARD_NIGHT, "Orchard Night", "#08160F", "Tile shade"),
                                              (IVORY, "Ivory", "#F3EBD6", "Mark, wordmark"), (ACCENT, "Accent", "#4AB278", "Interface"),
                                              ((229, 72, 77), "Alert", "#E5484D", "Warnings only"), (INK, "Ink", "#0E0E12", "Backgrounds"))):
        x = 80 + i * 300
        d.rounded_rectangle((x, y + 60, x + 260, y + 200), radius=18, fill=c, outline=(70, 70, 80))
        d.text((x, y + 215), name, font=font("SemiBold", 24), fill=(240, 240, 245))
        d.text((x, y + 250), hexv + "  ·  " + use, font=font("Regular", 20), fill=(160, 160, 170))
    # type
    y = 1080
    d.text((80, y), "Type", font=font("SemiBold", 30), fill=(240, 240, 245))
    d.text((80, y + 60), "Bramley", font=font("Regular", 64), fill=IVORY)
    d.text((80, y + 150), "Wordmark: Inter Regular, title case. Semibold when small.", font=font("Regular", 22), fill=(160, 160, 170))
    d.text((900, y + 60), "Inter everywhere.", font=font("SemiBold", 40), fill=(240, 240, 245))
    d.text((900, y + 120), "Calm and plain. Say what happened and what to do next.", font=font("Regular", 26), fill=(200, 200, 210))
    return b


def main():
    os.makedirs(OUT, exist_ok=True)
    svg(os.path.join(OUT, "bramley-mark.svg"), IVORY)
    svg(os.path.join(OUT, "bramley-mark-black.svg"), (0, 0, 0))
    svg(os.path.join(OUT, "bramley-mark-white.svg"), (255, 255, 255))
    for s in (1024, 512, 150, 36):
        icon(s).save(os.path.join(OUT, "bramley-icon-%d.png" % s))
    st, hz = lockups()
    st.save(os.path.join(OUT, "bramley-lockup-stacked.png"))
    hz.save(os.path.join(OUT, "bramley-lockup-horizontal.png"))
    boot = boot_logo()
    boot.save(os.path.join(OUT, "bramley-bootlogo.bmp"))
    boot.save(os.path.join(OUT, "bramley-bootlogo.png"))
    sheet(boot).save(os.path.join(OUT, "brand-sheet.png"))
    # the icon for the OS itself (power screen, About), installed to .system/res/bramley/brand
    res_brand = os.path.join(RES, "brand")
    os.makedirs(res_brand, exist_ok=True)
    for s in (176, 64):
        icon(s).save(os.path.join(res_brand, "icon-%d.png" % s))
    # the bare mark as the boot logo draws it (150 px, white), for the launcher's start-up splash
    m = mark_mask(600).resize((150, 150), Image.LANCZOS)
    mark = Image.new("RGBA", (150, 150), (255, 255, 255, 0))
    mark.putalpha(m)
    mark.save(os.path.join(res_brand, "mark-150.png"))
    # the install screen's picture (show2, logoheight 144): the bare mark in white, as the boot logo
    m = mark_mask(2048).resize((512, 512), Image.LANCZOS)  # the size of NextUI's logo it replaces
    mark = Image.new("RGBA", (512, 512), (255, 255, 255, 0))
    mark.putalpha(m)
    mark.save(os.path.join(OUT, "bramley-install-logo.png"))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
