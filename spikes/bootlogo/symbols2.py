#!/usr/bin/env python3
"""Apple-without-the-apple symbol ideas in direction B: the star of pips inside every apple, the
pie lattice, the five-petal blossom and a peel spiral. Sketches for choosing. Needs Pillow."""
import math, os, sys
from PIL import Image, ImageDraw, ImageChops, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, text_centre
from symbols import tile

def pip_shape(d, cx, cy, ang, L, W):
    # a pip: a teardrop pointing at the centre
    pts = []
    for i in range(120):
        t = i / 119 * 2 * math.pi
        r = 1 - 0.55 * (1 + math.cos(t)) / 2  # narrow at t=0 (the tip)
        x, y = math.cos(t) * L / 2, math.sin(t) * W / 2 * r * 1.6
        pts.append((cx + x * math.cos(ang) - y * math.sin(ang), cy + x * math.sin(ang) + y * math.cos(ang)))
    d.polygon(pts, fill=255)

def star_core(S):
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    for k in range(5):
        a = -math.pi / 2 + k * 2 * math.pi / 5
        r = S * .19
        pip_shape(d, S * .5 + math.cos(a) * r, S * .5 + math.sin(a) * r, a + math.pi, S * .2, S * .1)
    return m

def star_core_ring(S):
    # the same, with the core's five-point chamber drawn thin around them
    m = star_core(S); d = ImageDraw.Draw(m)
    pts = []
    for k in range(10):
        a = -math.pi / 2 + k * math.pi / 5
        r = S * (.34 if k % 2 == 0 else .2)
        pts.append((S * .5 + math.cos(a) * r, S * .5 + math.sin(a) * r))
    d.line(pts + [pts[0]], fill=255, width=max(1, int(S * .018)), joint="curve")
    return m

def lattice(S):
    # three strips each way, over and under in turn, a dark gap where one passes over another
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    n, w, g = 3, S * .11, S * .05
    span = n * w + (n - 1) * g; o = (S - span) / 2; e = S * .04
    pos = [o + k * (w + g) for k in range(n)]
    for q in pos: d.rounded_rectangle((o - e, q, o + span + e, q + w), radius=w / 2, fill=255)
    for p in pos: d.rounded_rectangle((p, o - e, p + w, o + span + e), radius=w / 2, fill=255)
    gap = S * .022
    for i, p in enumerate(pos):
        for j, q in enumerate(pos):
            if (i + j) % 2 == 0:  # the horizontal strip passes over the vertical one
                d.rectangle((p - 1, q - gap, p + w + 1, q + w + gap), fill=0)
                d.rectangle((p - 1, q, p + w + 1, q + w), fill=255)
            else:                 # the vertical strip passes over
                d.rectangle((p - gap, q - 1, p + w + gap, q + w + 1), fill=0)
                d.rectangle((p, q - 1, p + w, q + w + 1), fill=255)
    return m

def blossom(S):
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    for k in range(5):
        a = -math.pi / 2 + k * 2 * math.pi / 5
        cx, cy, r = S * .5 + math.cos(a) * S * .14, S * .5 + math.sin(a) * S * .14, S * .12
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    r = S * .065
    d.ellipse((S*.5 - r, S*.5 - r, S*.5 + r, S*.5 + r), fill=0)
    r = S * .03
    d.ellipse((S*.5 - r, S*.5 - r, S*.5 + r, S*.5 + r), fill=255)
    return m

def peel(S):
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    w = S * .05
    prev = None
    for i in range(400):
        t = i / 399
        a = -math.pi / 2 + t * 2.6 * 2 * math.pi
        r = S * (.31 - .22 * t)
        p = (S * .5 + math.cos(a) * r, S * .5 + math.sin(a) * r * .9 + S * .0)
        ww = w * (1 - .5 * t)
        d.ellipse((p[0] - ww, p[1] - ww, p[0] + ww, p[1] + ww), fill=255)
    return m

IDEAS = [("Star core", "The five pips in every\napple's cross-section", star_core),
         ("Star core, chamber", "The pips with the core's\nstar drawn around them", star_core_ring),
         ("Pie lattice", "A Bramley is the pie apple:\nthe woven crust", lattice),
         ("Blossom", "Apple blossom's\nfive petals", blossom),
         ("Peel", "One unbroken\nribbon of peel", peel)]

def main(out):
    W, H = 1900, 900
    b = Image.new("RGB", (W, H), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 40), "Apple, without the apple  ·  direction B", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (name, note, sym) in enumerate(IDEAS):
        x = 60 + i * 370
        t = tile(sym, 220); b.paste(t, (x, 130), t)
        d.text((x, 370), f"{i+1}. {name}", font=font("SemiBold", 26), fill=(235, 235, 240))
        d.multiline_text((x, 408), note, font=font("Regular", 19), fill=(170, 170, 180), spacing=6)
        row = Image.new("RGB", (330, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
        t36 = tile(sym, 32); row.paste(t36, (14, 7), t36); rd.text((60, 9), "Bramley", font=font("Regular", 22), fill=(235, 235, 240))
        b.paste(row, (x, 500))
        one = sym(440).resize((110, 110), Image.LANCZOS)
        b.paste(Image.new("RGB", (110, 110), (255, 255, 255)), (x, 580), one)
        d.text((x + 130, 620), "one colour", font=font("Regular", 18), fill=(150, 150, 160))
        bm = Image.new("RGB", (330, 150), (0, 0, 0)); bd = ImageDraw.Draw(bm)
        tb = tile(sym, 64); bm.paste(tb, (133, 14), tb); text_centre(bd, (165, 115), "BRAMLEY", font("Regular", 18), (255, 255, 255), track=6)
        b.paste(bm, (x, 720))
    b.save(os.path.join(out, "symbols-apple.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
