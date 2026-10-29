#!/usr/bin/env python3
"""Four symbol ideas for the Bramley mark, in direction B (black, glass, light), each at icon size,
at 36 px and in one colour. Sketches for choosing an idea, not final drawings. Needs Pillow."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, squircle, grad, text_centre

SS = 4
GREEN = (90, 230, 120)

def bricks(S):
    # two stacked rounded blocks, the upper narrower, read as a B and as the Brick
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    d.rounded_rectangle((S*.30, S*.22, S*.62, S*.48), radius=S*.10, fill=255)
    d.rounded_rectangle((S*.30, S*.52, S*.70, S*.78), radius=S*.11, fill=255)
    return m

def pip(S):
    # a seed: the start of the first Bramley tree, 1809
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    pts = []
    for i in range(360):
        t = math.radians(i)
        r = S*.21*(1 - .35*math.sin(t))   # teardrop
        pts.append((S*.5 + r*math.cos(t)*.8, S*.53 + r*math.sin(t)*1.15))
    d.polygon(pts, fill=255)
    return m.rotate(-25, resample=Image.BICUBIC, center=(S*.5, S*.5))

def rings(S):
    # growth rings: an old tree, layered like the glass in the UI; the inner ring open like a b
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    w = int(S*.055)
    for r in (.30, .20):
        d.ellipse((S*(.5-r), S*(.5-r), S*(.5+r), S*(.5+r)), outline=255, width=w)
    d.ellipse((S*.44, S*.44, S*.56, S*.56), fill=255)
    gap = Image.new("L", (S, S)); ImageDraw.Draw(gap).pieslice((S*.1, S*.1, S*.9, S*.9), -100, -60, fill=255)
    return ImageChops.subtract(m, gap)

def focus(S):
    # the tvOS focus card: a lifted tile with its shadow and a light streak
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    d.rounded_rectangle((S*.27, S*.25, S*.73, S*.66), radius=S*.09, fill=255)
    d.rounded_rectangle((S*.33, S*.72, S*.67, S*.76), radius=S*.02, fill=150)
    streak = Image.new("L", (S, S)); ImageDraw.Draw(streak).polygon([(S*.30, S*.62), (S*.46, S*.25), (S*.54, S*.25), (S*.38, S*.62)], fill=255)
    return ImageChops.subtract(m, streak.point(lambda v: v // 3))

IDEAS = [("Bricks", "Two stacked blocks: a B, and the Brick it runs on", bricks),
         ("Pip", "The seed of the first Bramley tree, 1809", pip),
         ("Rings", "Growth rings: an old tree, layered like the glass UI", rings),
         ("Focus", "A lifted card, as focus works in Bramley's UI", focus)]

def tile(sym, s, accent=True):
    S = s * SS
    base = grad(S, (40, 44, 58), (8, 9, 14), diagonal=True)
    sheen = Image.new("L", (S, S)); ImageDraw.Draw(sheen).ellipse((-S*.4, -S*.9, S*1.2, S*.45), fill=55)
    base = Image.composite(Image.new("RGB", (S, S), (255, 255, 255)), base, sheen.filter(ImageFilter.GaussianBlur(S*.05)))
    m = sym(S)
    if accent:  # a green light behind the symbol
        glow = m.filter(ImageFilter.GaussianBlur(S*.06)).point(lambda v: int(v*.7))
        base = Image.composite(Image.new("RGB", (S, S), GREEN), base, glow)
    base.paste(Image.new("RGB", (S, S), (255, 255, 255)), (0, 0), m)
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0)); out.paste(base, (0, 0), squircle(S))
    rim = ImageChops.subtract(squircle(S), squircle(S).filter(ImageFilter.MinFilter(int(S*.012)*2+1)))
    out.paste(Image.new("RGBA", (S, S), (255, 255, 255, 80)), (0, 0), rim)
    return out.resize((s, s), Image.LANCZOS)

def main(out):
    W, H = 1600, 900
    b = Image.new("RGB", (W, H), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 40), "Symbol ideas  ·  direction B", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (name, note, sym) in enumerate(IDEAS):
        x = 60 + i * 380
        t = tile(sym, 220); b.paste(t, (x, 130), t)
        d.text((x, 370), f"{i+1}. {name}", font=font("SemiBold", 26), fill=(235, 235, 240))
        d.multiline_text((x, 408), note.replace(": ", ":\n"), font=font("Regular", 19), fill=(170, 170, 180), spacing=6)
        row = Image.new("RGB", (330, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
        t36 = tile(sym, 32); row.paste(t36, (14, 7), t36); rd.text((60, 9), "Bramley", font=font("Regular", 22), fill=(235, 235, 240))
        b.paste(row, (x, 500))
        one = Image.new("L", (440, 440)); one.paste(sym(440)); one = one.resize((110, 110), Image.LANCZOS)
        b.paste(Image.new("RGB", (110, 110), (255, 255, 255)), (x, 580), one)
        d.text((x + 130, 620), "one colour", font=font("Regular", 18), fill=(150, 150, 160))
        # with the wordmark, as on the boot screen
        bm = Image.new("RGB", (330, 150), (0, 0, 0)); bd = ImageDraw.Draw(bm)
        tb = tile(sym, 64); bm.paste(tb, (133, 14), tb); text_centre(bd, (165, 115), "BRAMLEY", font("Regular", 18), (255, 255, 255), track=6)
        b.paste(bm, (x, 720))
    b.save(os.path.join(out, "symbols.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
