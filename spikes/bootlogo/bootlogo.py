#!/usr/bin/env python3
"""Draws Bramley boot logo options as 1024x768 24-bit BMPs, the format the Brick's boot partition
takes (the same as NextUI's Bootlogo app). Not shipped yet; for choosing a design.
No apple shapes: the mark is a wordmark, a monogram tile, or a leaf.
Needs Pillow:  python3 bootlogo.py <out-dir>
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "..", "workspace", "all", "launcher", "res")
W, H, SS = 1024, 768, 4
GREEN = (48, 209, 88)


def font(weight, size):
    return ImageFont.truetype(os.path.join(RES, f"Inter-{weight}.ttf"), size * SS)


def canvas():
    return Image.new("RGB", (W * SS, H * SS), (0, 0, 0))


def centred_text(d, text, f, cy, fill=(255, 255, 255), tracking=0):
    # draws text centred on cy, with extra letter spacing (in px at 1x)
    widths = [d.textlength(c, font=f) for c in text]
    total = sum(widths) + tracking * SS * (len(text) - 1)
    x = (W * SS - total) / 2
    # centre the capital height on cy, so descenders hang below as they should
    top, bottom = f.getbbox("B")[1], f.getbbox("B")[3]
    y = cy * SS - (top + bottom) / 2
    for c, w in zip(text, widths):
        d.text((x, y), c, font=f, fill=fill)
        x += w + tracking * SS


def finish(img, path):
    img.resize((W, H), Image.LANCZOS).save(path, "BMP")


def squircle(size, n=5.0):
    m = Image.new("L", (size, size), 0)
    px = m.load()
    r = size / 2
    for y in range(size):
        for x in range(size):
            u, v = abs((x + 0.5 - r) / r), abs((y + 0.5 - r) / r)
            if u ** n + v ** n <= 1:
                px[x, y] = 255
    return m


def leaf(size, angle):
    # two circle arcs meeting at points: a simple leaf, then turned
    m = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(m)
    w, R = size * 0.42, size * 0.62
    cx, cy = size / 2, size / 2
    a = Image.new("L", (size, size), 0)
    b = Image.new("L", (size, size), 0)
    ImageDraw.Draw(a).ellipse((cx - w - R + w, cy - R, cx - w + R + w, cy + R), fill=255)
    off = R - w * 0.55
    a = Image.new("L", (size, size), 0)
    ImageDraw.Draw(a).ellipse((cx - off - R, cy - R, cx - off + R, cy + R), fill=255)
    ImageDraw.Draw(b).ellipse((cx + off - R, cy - R, cx + off + R, cy + R), fill=255)
    from PIL import ImageChops
    m = ImageChops.multiply(a, b)
    return m.rotate(angle, resample=Image.BICUBIC)


def option_a(out):
    img = canvas()
    centred_text(ImageDraw.Draw(img), "Bramley", font("SemiBold", 88), H / 2, tracking=-1)
    finish(img, os.path.join(out, "bootlogo-A-wordmark.bmp"))


def option_b(out):
    img = canvas()
    s = 168 * SS
    tile = Image.new("RGB", (s, s))
    top, bottom = (88, 224, 120), (28, 160, 70)
    for y in range(s):
        t = y / s
        ImageDraw.Draw(tile).line([(0, y), (s, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    x, y = (W * SS - s) // 2, int((H / 2 - 70) * SS - s / 2)
    img.paste(tile, (x, y), squircle(s))
    d = ImageDraw.Draw(img)
    f = font("Bold", 112)
    tw = d.textlength("B", font=f)
    asc, desc = f.getmetrics()
    d.text((x + (s - tw) / 2, y + (s - asc) / 2 + desc * 0.15), "B", font=f, fill=(255, 255, 255))
    centred_text(d, "Bramley", font("SemiBold", 40), H / 2 + 72)
    finish(img, os.path.join(out, "bootlogo-B-tile.bmp"))


def option_c(out):
    img = canvas()
    d = ImageDraw.Draw(img)
    f = font("SemiBold", 88)
    centred_text(d, "Bramley", f, H / 2 + 20, tracking=-1)
    s = 96 * SS
    lf = leaf(s, -35)
    green = Image.new("RGB", (s, s), GREEN)
    img.paste(green, ((W * SS - s) // 2, int((H / 2 - 105) * SS - s / 2)), lf)
    finish(img, os.path.join(out, "bootlogo-C-leaf.bmp"))


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else HERE
    os.makedirs(out, exist_ok=True)
    option_a(out)
    option_b(out)
    option_c(out)
