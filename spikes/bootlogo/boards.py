#!/usr/bin/env python3
"""Mood boards for two logo directions, A (calm, precise, warm) and B (premium, cinematic, glassy):
the tile at icon size and at 36 px in a sidebar row, the wordmark, and the boot screen. Not a final
logo; for choosing a direction. Needs Pillow."""
import os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "..", "workspace", "all", "launcher", "res")
SS = 4

def font(w, size): return ImageFont.truetype(os.path.join(RES, f"Inter-{w}.ttf"), int(size))

def squircle(s, n=5.0):
    m = Image.new("L", (s, s)); px = m.load(); r = s / 2
    for y in range(s):
        for x in range(s):
            if abs((x + .5 - r) / r) ** n + abs((y + .5 - r) / r) ** n <= 1: px[x, y] = 255
    return m

def grad(s, top, bottom, diagonal=False):
    g = Image.new("RGB", (s, s)); d = ImageDraw.Draw(g)
    for i in range(s * (2 if diagonal else 1)):
        t = i / (s * (2 if diagonal else 1))
        c = tuple(int(top[k] + (bottom[k] - top[k]) * t) for k in range(3))
        if diagonal: d.line([(i, 0), (0, i)], fill=c)
        else: d.line([(0, i), (s, i)], fill=c)
    return g

def text_centre(d, xy, t, f, fill, track=0):
    ws = [d.textlength(c, font=f) for c in t]; tw = sum(ws) + track * (len(t) - 1)
    x = xy[0] - tw / 2; bb = f.getbbox("B"); y = xy[1] - (bb[1] + bb[3]) / 2
    for c, w in zip(t, ws): d.text((x, y), c, font=f, fill=fill); x += w + track

def tile(style, s):
    S = s * SS
    if style == "A":
        base = grad(S, (120, 200, 110), (70, 160, 80))
        glyph_col = (255, 251, 240)
        g = Image.new("L", (S, S)); d = ImageDraw.Draw(g)
        text_centre(d, (S / 2, S / 2 + S * .02), "b", font("Bold", S * .62), 255)
    else:
        base = grad(S, (40, 44, 58), (8, 9, 14), diagonal=True)
        sheen = Image.new("L", (S, S)); ImageDraw.Draw(sheen).ellipse((-S * .4, -S * .9, S * 1.2, S * .45), fill=60)
        base = Image.composite(Image.new("RGB", (S, S), (255, 255, 255)), base, sheen.filter(ImageFilter.GaussianBlur(S * .05)))
        glyph_col = (255, 255, 255)
        g = Image.new("L", (S, S)); d = ImageDraw.Draw(g)
        text_centre(d, (S / 2, S / 2), "B", font("SemiBold", S * .56), 255)
    base.paste(Image.new("RGB", (S, S), glyph_col), (0, 0), g)
    if style == "B":  # a thin green glow under the B: the apple, as light
        glow = Image.new("L", (S, S)); ImageDraw.Draw(glow).rounded_rectangle((S * .3, S * .78, S * .7, S * .8), radius=S * .01, fill=255)
        glow = glow.filter(ImageFilter.GaussianBlur(S * .02))
        base = Image.composite(Image.new("RGB", (S, S), (90, 230, 120)), base, glow)
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0)); out.paste(base, (0, 0), squircle(S))
    if style == "B":  # glass rim
        rim = ImageChops.subtract(squircle(S), squircle(S).filter(ImageFilter.MinFilter(int(S * .012) * 2 + 1)))
        out.paste(Image.new("RGBA", (S, S), (255, 255, 255, 90)), (0, 0), rim)
    return out.resize((s, s), Image.LANCZOS)

def wordmark(style, h, bg):
    W = int(h * 6); img = Image.new("RGB", (W, h), bg); d = ImageDraw.Draw(img)
    if style == "A": text_centre(d, (W / 2, h / 2), "bramley", font("SemiBold", h * .62), (40, 44, 40), track=-h * .02)
    else: text_centre(d, (W / 2, h / 2), "BRAMLEY", font("Regular", h * .42), (255, 255, 255), track=h * .18)
    return img

def boot(style):
    W, H = 1024, 768
    if style == "A":
        img = Image.new("RGB", (W, H), (246, 242, 232)); t = tile("A", 168)
        img.paste(t, ((W - 168) // 2, H // 2 - 150), t)
        d = ImageDraw.Draw(img); text_centre(d, (W / 2, H / 2 + 80), "bramley", font("SemiBold", 52), (40, 44, 40), track=-1)
    else:
        img = Image.new("RGB", (W, H), (0, 0, 0))
        glow = Image.new("L", (W, H)); ImageDraw.Draw(glow).ellipse((W / 2 - 220, H / 2 - 220, W / 2 + 220, H / 2 + 140), fill=40)
        img = Image.composite(Image.new("RGB", (W, H), (40, 110, 70)), img, glow.filter(ImageFilter.GaussianBlur(90)))
        t = tile("B", 168); img.paste(t, ((W - 168) // 2, H // 2 - 150), t)
        d = ImageDraw.Draw(img); text_centre(d, (W / 2, H / 2 + 80), "BRAMLEY", font("Regular", 30), (255, 255, 255), track=10)
    return img

def board(style, out):
    W, H = 1600, 1000
    bg = (246, 242, 232) if style == "A" else (14, 14, 18); ink = (40, 44, 40) if style == "A" else (235, 235, 240)
    b = Image.new("RGB", (W, H), bg); d = ImageDraw.Draw(b)
    title = "A  ·  calm, precise, warm" if style == "A" else "B  ·  premium, cinematic, glassy"
    d.text((60, 40), title, font=font("SemiBold", 34), fill=ink)
    # boot screen
    bs = boot(style).resize((720, 540), Image.LANCZOS); b.paste(bs, (60, 120))
    d.rectangle((59, 119, 780, 660), outline=(128, 128, 128))
    d.text((60, 672), "Boot screen", font=font("Regular", 20), fill=ink)
    # big tile and wordmark
    t = tile(style, 220); b.paste(t, (860, 120), t)
    d.text((860, 352), "App icon", font=font("Regular", 20), fill=ink)
    wm = wordmark(style, 90, bg); b.paste(wm, (1100, 180))
    d.text((1110, 352), "Wordmark", font=font("Regular", 20), fill=ink)
    # sidebar row at real size, on Settings' dark glass
    row = Image.new("RGB", (330, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
    t36 = tile(style, 32); row.paste(t36, (14, 7), t36); rd.text((60, 9), "About Bramley", font=font("Regular", 22), fill=(235, 235, 240))
    b.paste(row, (860, 430)); d.text((860, 486), "36 px, as in Settings", font=font("Regular", 20), fill=ink)
    # one colour version
    one = Image.new("RGB", (220, 220), (0, 0, 0)); od = ImageDraw.Draw(one)
    text_centre(od, (110, 110), "b" if style == "A" else "B", font("Bold" if style == "A" else "SemiBold", 130), (255, 255, 255))
    b.paste(one, (1180, 430)); d.text((1180, 662), "One colour", font=font("Regular", 20), fill=ink)
    d.text((60, 740), ("Cream and apple green, lower-case and rounded: homely, like the cooking apple.\nThe B tile could be a soft leaf-free mark; warmth comes from colour and shape, not imagery."
                       if style == "A" else
                       "Black, glass and light: the tvOS look you built. The apple is only a thin green glow.\nSpaced capitals read as a premium label; the tile matches the Settings icons."),
           font=font("Regular", 22), fill=ink, spacing=10)
    b.save(out)

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "boards")
    os.makedirs(out, exist_ok=True)
    for s in "AB": board(s, os.path.join(out, f"direction-{s}.png"))
