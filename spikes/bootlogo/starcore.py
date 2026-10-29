#!/usr/bin/env python3
"""Eight renditions of the star core (the five pips in every apple's cross-section) in direction B.
Each at icon size, at 36 px and in one colour. Needs Pillow."""
import math, os, sys
from PIL import Image, ImageDraw, ImageChops
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, text_centre
from symbols import tile

def pip(d, cx, cy, ang, L, W, fill=255, sharp=0.55):
    # a teardrop: round at one end, pointed at the end facing ang
    pts = []
    for i in range(160):
        t = i / 159 * 2 * math.pi
        r = 1 - sharp * (1 + math.cos(t)) / 2
        x, y = math.cos(t) * L / 2, math.sin(t) * W / 2 * r * 1.6
        pts.append((cx + x * math.cos(ang) - y * math.sin(ang), cy + x * math.sin(ang) + y * math.cos(ang)))
    d.polygon(pts, fill=fill)

def ring_of(S, r, L, W, inward=True, twist=0, sharp=0.55, d=None, fill=255):
    m = None
    if d is None:
        m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    for k in range(5):
        a = -math.pi / 2 + k * 2 * math.pi / 5
        pip(d, S*.5 + math.cos(a)*S*r, S*.5 + math.sin(a)*S*r, a + (math.pi if inward else 0) + twist, S*L, S*W, fill, sharp)
    return m

def v_classic(S): return ring_of(S, .19, .2, .1)
def v_outward(S): return ring_of(S, .2, .2, .1, inward=False)
def v_dot(S):
    m = ring_of(S, .21, .18, .09); d = ImageDraw.Draw(m); r = S*.045
    d.ellipse((S*.5-r, S*.5-r, S*.5+r, S*.5+r), fill=255); return m
def v_ring(S):
    m = ring_of(S, .15, .15, .075); d = ImageDraw.Draw(m)
    d.ellipse((S*.2, S*.2, S*.8, S*.8), outline=255, width=int(S*.03)); return m
def v_capsule(S):
    m = Image.new("L", (S, S))
    for k in range(5):
        a = -90 + k * 72
        c = Image.new("L", (S, S)); cd = ImageDraw.Draw(c)
        cd.rounded_rectangle((S*.455, S*.2, S*.545, S*.4), radius=S*.045, fill=255)
        m = ImageChops.lighter(m, c.rotate(-(a + 90), resample=Image.BICUBIC, center=(S*.5, S*.5)))
    return m
def v_slice(S):
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    d.ellipse((S*.2, S*.2, S*.8, S*.8), fill=255)
    ring_of(S, .14, .15, .075, d=d, fill=0); return m
def v_twist(S): return ring_of(S, .19, .2, .1, twist=math.radians(28))
def v_first(S): return ring_of(S, .19, .2, .1)  # same shape; the first pip is green (see ACCENT)

IDEAS = [("Classic", "Tips meet in the middle", v_classic),
         ("Outward", "Tips point out, a burst", v_outward),
         ("Centre dot", "The core marked", v_dot),
         ("In a ring", "A slice's outline, round", v_ring),
         ("Capsules", "Geometric, like a UI glyph", v_capsule),
         ("Slice", "Pips cut out of a disc", v_slice),
         ("Turning", "A twist: motion, launch", v_twist),
         ("First seed", "One pip green: the 1809 tree", v_first)]

def first_seed_tile(s):
    # v_first: draw the white pips, then recolour the top one green
    from PIL import ImageFilter
    t = tile(v_first, s).convert("RGBA"); S = s * 4
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    pip(d, S*.5, S*.5 - S*.19, math.pi/2, S*.2, S*.1)
    m = m.resize((s, s), Image.LANCZOS)
    t.paste(Image.new("RGBA", (s, s), (90, 230, 120, 255)), (0, 0), m)
    return t

def main(out):
    W, H = 1800, 1150
    b = Image.new("RGB", (W, H), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 36), "Star core  ·  eight renditions", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (name, note, sym) in enumerate(IDEAS):
        x = 60 + (i % 4) * 430; y = 110 + (i // 4) * 520
        t = first_seed_tile(200) if name == "First seed" else tile(sym, 200)
        b.paste(t, (x, y), t)
        d.text((x, y + 215), f"{chr(65+i)}. {name}", font=font("SemiBold", 24), fill=(235, 235, 240))
        d.text((x, y + 250), note, font=font("Regular", 18), fill=(170, 170, 180))
        row = Image.new("RGB", (300, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
        t36 = first_seed_tile(32) if name == "First seed" else tile(sym, 32)
        row.paste(t36, (14, 7), t36); rd.text((60, 9), "Bramley", font=font("Regular", 22), fill=(235, 235, 240))
        b.paste(row, (x, y + 290))
        one = sym(400).resize((90, 90), Image.LANCZOS)
        b.paste(Image.new("RGB", (90, 90), (255, 255, 255)), (x, y + 355), one)
        bm = Image.new("RGB", (200, 110), (0, 0, 0)); bd = ImageDraw.Draw(bm)
        tb = first_seed_tile(48) if name == "First seed" else tile(sym, 48)
        bm.paste(tb, (76, 8), tb); text_centre(bd, (100, 84), "BRAMLEY", font("Regular", 14), (255, 255, 255), track=4)
        b.paste(bm, (x + 110, y + 345))
    b.save(os.path.join(out, "starcore.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
