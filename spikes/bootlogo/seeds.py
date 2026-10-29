#!/usr/bin/env python3
"""Star core A (five pips meeting in the middle) with eight seed shapes, in direction B. Needs Pillow."""
import math, os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, text_centre
from symbols import tile

def shape(kind, t):
    """Outline point at angle t (0 at the tip, which faces the centre): (along, across) in -0.5..0.5."""
    x, y = math.cos(t) / 2, math.sin(t) / 2
    if kind == "drop":      # classic teardrop: sharp tip, round back
        return x, y * (1 - .6 * (1 + math.cos(t)) / 2) * 1.5
    if kind == "almond":    # pointed both ends
        return x, y * abs(math.sin(t)) ** .35 * .95
    if kind == "bean":      # soft oval
        return x * .92, y * .8
    if kind == "seed":      # a real apple pip: pointed tip, flattened shoulders, rounded base
        k = (1 + math.cos(t)) / 2
        return x, y * (1 - .75 * k ** 1.6) * 1.55
    if kind == "plump":     # short and round, a small tip
        return x * .8, y * (1 - .35 * (1 + math.cos(t)) / 2) * 1.35
    if kind == "slender":   # long and thin
        return x * 1.15, y * (1 - .6 * (1 + math.cos(t)) / 2) * 1.0
    if kind == "comma":     # a drop whose tip curls a little
        k = (1 + math.cos(t)) / 2
        return x, y * (1 - .6 * k) * 1.5 + .12 * k ** 2
    if kind == "flame":     # tip drawn out long, like a candle flame
        k = (1 + math.cos(t)) / 2
        return x * (1 + .25 * k), y * (1 - k) ** .8 * 1.3
    raise ValueError(kind)

def pips(kind, L=.21, W=.115, r=.19):
    def sym(S):
        m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
        for k in range(5):
            a = -math.pi / 2 + k * 2 * math.pi / 5
            cx, cy, ang = S*.5 + math.cos(a)*S*r, S*.5 + math.sin(a)*S*r, a + math.pi
            pts = []
            for i in range(240):
                t = i / 239 * 2 * math.pi
                u, v = shape(kind, t)
                u, v = u * S * L, v * S * W
                pts.append((cx + u*math.cos(ang) - v*math.sin(ang), cy + u*math.sin(ang) + v*math.cos(ang)))
            d.polygon(pts, fill=255)
        return m
    return sym

IDEAS = [("Drop", "The A you picked", pips("drop")),
         ("Apple pip", "Flat shoulders, like a real seed", pips("seed")),
         ("Almond", "Pointed at both ends", pips("almond", W=.1)),
         ("Bean", "Soft oval, no point", pips("bean", W=.12)),
         ("Plump", "Short and round", pips("plump", L=.19, W=.13)),
         ("Slender", "Long and thin", pips("slender", L=.23, W=.1, r=.2)),
         ("Comma", "The tip curls, as if turning", pips("comma")),
         ("Flame", "A drawn-out tip, like light", pips("flame", L=.23, W=.12, r=.2))]

def main(out):
    W, H = 1800, 1150
    b = Image.new("RGB", (W, H), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 36), "Star core A  ·  eight seed shapes", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (name, note, sym) in enumerate(IDEAS):
        x = 60 + (i % 4) * 430; y = 110 + (i // 4) * 520
        t = tile(sym, 200); b.paste(t, (x, y), t)
        d.text((x, y + 215), f"{i+1}. {name}", font=font("SemiBold", 24), fill=(235, 235, 240))
        d.text((x, y + 250), note, font=font("Regular", 18), fill=(170, 170, 180))
        row = Image.new("RGB", (300, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
        t36 = tile(sym, 32); row.paste(t36, (14, 7), t36); rd.text((60, 9), "Bramley", font=font("Regular", 22), fill=(235, 235, 240))
        b.paste(row, (x, y + 290))
        one = sym(400).resize((90, 90), Image.LANCZOS)
        b.paste(Image.new("RGB", (90, 90), (255, 255, 255)), (x, y + 355), one)
        bm = Image.new("RGB", (200, 110), (0, 0, 0)); bd = ImageDraw.Draw(bm)
        tb = tile(sym, 48); bm.paste(tb, (76, 8), tb); text_centre(bd, (100, 84), "BRAMLEY", font("Regular", 14), (255, 255, 255), track=4)
        b.paste(bm, (x + 110, y + 345))
    b.save(os.path.join(out, "seeds.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
