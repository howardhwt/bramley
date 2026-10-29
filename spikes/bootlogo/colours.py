#!/usr/bin/env python3
"""The Bramley mark (star core 2b, with a small gap at the centre) in four colour treatments, each
as the app icon, at 36 px, and on the boot screen. Needs Pillow."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, text_centre, squircle, grad
from seeds import shape

SS = 4
GREEN = (52, 199, 89)        # apple green: Apple's systemGreen is avoided on purpose? see notes
BRAMLEY = (124, 196, 72)     # Bramley skin: a yellow-green

def mark(S, L=.27, W=.112, r=.225):
    """The five pips, white on black (an L mask). r is a touch larger than 2b's so the tips keep a gap."""
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    for k in range(5):
        a = -math.pi / 2 + k * 2 * math.pi / 5
        cx, cy, ang = S*.5 + math.cos(a)*S*r, S*.5 + math.sin(a)*S*r, a + math.pi
        pts = []
        for i in range(240):
            t = i / 239 * 2 * math.pi
            u, v = shape("seed", t); u, v = u*S*L, v*S*W
            pts.append((cx + u*math.cos(ang) - v*math.sin(ang), cy + u*math.sin(ang) + v*math.cos(ang)))
        d.polygon(pts, fill=255)
    return m

def first_pip(S, L=.27, W=.112, r=.225):
    m = Image.new("L", (S, S)); d = ImageDraw.Draw(m)
    a = -math.pi / 2; cx, cy, ang = S*.5 + math.cos(a)*S*r, S*.5 + math.sin(a)*S*r, a + math.pi
    pts = []
    for i in range(240):
        t = i / 239 * 2 * math.pi
        u, v = shape("seed", t); u, v = u*S*L, v*S*W
        pts.append((cx + u*math.cos(ang) - v*math.sin(ang), cy + u*math.sin(ang) + v*math.cos(ang)))
    d.polygon(pts, fill=255)
    return m

def glass_base(S, top=(40, 44, 58), bottom=(8, 9, 14)):
    base = grad(S, top, bottom, diagonal=True)
    sheen = Image.new("L", (S, S)); ImageDraw.Draw(sheen).ellipse((-S*.4, -S*.9, S*1.2, S*.45), fill=55)
    return Image.composite(Image.new("RGB", (S, S), (255, 255, 255)), base, sheen.filter(ImageFilter.GaussianBlur(S*.05)))

def finish(base, S, rim=80):
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0)); out.paste(base, (0, 0), squircle(S))
    ring = ImageChops.subtract(squircle(S), squircle(S).filter(ImageFilter.MinFilter(int(S*.012)*2+1)))
    out.paste(Image.new("RGBA", (S, S), (255, 255, 255, rim)), (0, 0), ring)
    return out

def icon(style, s):
    S = s * SS; m = mark(S)
    if style == "glow":      # white pips, green light behind (current)
        base = glass_base(S)
        glow = m.filter(ImageFilter.GaussianBlur(S*.06)).point(lambda v: int(v*.7))
        base = Image.composite(Image.new("RGB", (S, S), (90, 230, 120)), base, glow)
        base.paste(Image.new("RGB", (S, S), (255, 255, 255)), (0, 0), m)
    elif style == "green":   # green pips on black glass
        base = glass_base(S)
        base.paste(Image.new("RGB", (S, S), BRAMLEY), (0, 0), m)
    elif style == "tile":    # white pips on a Bramley-green tile
        base = grad(S, (150, 214, 92), (84, 160, 52))
        sheen = Image.new("L", (S, S)); ImageDraw.Draw(sheen).ellipse((-S*.4, -S*.9, S*1.2, S*.45), fill=45)
        base = Image.composite(Image.new("RGB", (S, S), (255, 255, 255)), base, sheen.filter(ImageFilter.GaussianBlur(S*.05)))
        base.paste(Image.new("RGB", (S, S), (255, 255, 255)), (0, 0), m)
    elif style == "first":   # white pips, the first pip green (the 1809 seed)
        base = glass_base(S)
        base.paste(Image.new("RGB", (S, S), (255, 255, 255)), (0, 0), m)
        base.paste(Image.new("RGB", (S, S), BRAMLEY), (0, 0), first_pip(S))
    return finish(base, S).resize((s, s), Image.LANCZOS)

STYLES = [("glow", "White, green glow", "Today's look: the light is the green"),
          ("green", "Green pips", "The seeds are the colour"),
          ("tile", "Green tile", "Boldest; a green square like an app"),
          ("first", "First seed green", "Four white, one green: the 1809 pip")]

def main(out):
    W, H = 1800, 760
    b = Image.new("RGB", (W, H), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 36), "Bramley mark  ·  colour", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (st, name, note) in enumerate(STYLES):
        x = 60 + i * 430; y = 110
        t = icon(st, 200); b.paste(t, (x, y), t)
        d.text((x, y + 215), f"{chr(65+i)}. {name}", font=font("SemiBold", 24), fill=(235, 235, 240))
        d.text((x, y + 250), note, font=font("Regular", 18), fill=(170, 170, 180))
        row = Image.new("RGB", (300, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
        t36 = icon(st, 32); row.paste(t36, (14, 7), t36); rd.text((60, 9), "Bramley", font=font("Regular", 22), fill=(235, 235, 240))
        b.paste(row, (x, y + 290))
        bm = Image.new("RGB", (360, 250), (0, 0, 0)); bd = ImageDraw.Draw(bm)
        tb = icon(st, 96); bm.paste(tb, (132, 36), tb); text_centre(bd, (180, 190), "BRAMLEY", font("Regular", 20), (255, 255, 255), track=8)
        b.paste(bm, (x, y + 360))
    b.save(os.path.join(out, "colour.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
