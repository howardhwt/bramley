#!/usr/bin/env python3
"""More colour treatments for the Bramley mark, from the apple itself: the flesh, the seed, the
orchard, cider and the Bramley's red blush. Needs Pillow."""
import os, sys
from PIL import Image, ImageDraw, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, text_centre, grad
from colours import mark, first_pip, glass_base, finish, SS

IVORY = (243, 235, 214)     # apple flesh
SEED = (176, 112, 70)       # a ripe pip: polished brown, lit to copper
ORCHARD = ((30, 64, 44), (8, 22, 15))   # British racing green / orchard at dusk
CIDER = (232, 168, 64)      # amber
BLUSH = (196, 74, 64)       # a Bramley's red flush

def metal(S, m, col):
    # a soft top-to-bottom shine across the pips, for a metallic seed
    lit = grad(S, tuple(min(255, int(c * 1.35)) for c in col), tuple(int(c * .7) for c in col))
    return lit, m

def icon(style, s):
    S = s * SS; m = mark(S)
    if style == "ivory":
        base = glass_base(S); base.paste(Image.new("RGB", (S, S), IVORY), (0, 0), m)
    elif style == "seed":
        base = glass_base(S); lit, _ = metal(S, m, SEED); base.paste(lit, (0, 0), m)
    elif style == "orchard":
        base = glass_base(S, *ORCHARD); base.paste(Image.new("RGB", (S, S), IVORY), (0, 0), m)
    elif style == "cider":
        base = glass_base(S)
        glow = m.filter(ImageFilter.GaussianBlur(S * .06)).point(lambda v: int(v * .65))
        base = Image.composite(Image.new("RGB", (S, S), CIDER), base, glow)
        base.paste(Image.new("RGB", (S, S), IVORY), (0, 0), m)
    elif style == "blush":
        base = glass_base(S, *ORCHARD); base.paste(Image.new("RGB", (S, S), IVORY), (0, 0), m)
        base.paste(Image.new("RGB", (S, S), BLUSH), (0, 0), first_pip(S))
    elif style == "orchard-seed":
        base = glass_base(S, *ORCHARD); lit, _ = metal(S, m, (205, 150, 95)); base.paste(lit, (0, 0), m)
    return finish(base, S).resize((s, s), Image.LANCZOS)

STYLES = [("ivory", "Flesh", "Ivory pips on black glass: the apple's flesh"),
          ("seed", "Seed", "Copper pips: a ripe pip's polished brown"),
          ("orchard", "Orchard", "Racing green glass, ivory pips: British, quiet"),
          ("cider", "Cider", "Ivory pips, warm amber light"),
          ("blush", "Blush", "Orchard green, one pip in a Bramley's red flush"),
          ("orchard-seed", "Orchard and seed", "Racing green glass, copper pips")]

def main(out):
    W, H = 1960, 1280
    b = Image.new("RGB", (W, H), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 36), "Bramley mark  ·  colours from the apple", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (st, name, note) in enumerate(STYLES):
        x = 60 + (i % 3) * 640; y = 110 + (i // 3) * 580
        t = icon(st, 200); b.paste(t, (x, y), t)
        d.text((x, y + 215), f"{chr(69+i)}. {name}", font=font("SemiBold", 24), fill=(235, 235, 240))
        d.text((x, y + 250), note, font=font("Regular", 18), fill=(170, 170, 180))
        row = Image.new("RGB", (300, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
        t36 = icon(st, 32); row.paste(t36, (14, 7), t36); rd.text((60, 9), "Bramley", font=font("Regular", 22), fill=(235, 235, 240))
        b.paste(row, (x, y + 290))
        bm = Image.new("RGB", (330, 230), (0, 0, 0)); bd = ImageDraw.Draw(bm)
        tb = icon(st, 96); bm.paste(tb, (117, 30), tb); text_centre(bd, (165, 178), "BRAMLEY", font("Regular", 20), (243, 235, 214) if st != "seed" else (255, 255, 255), track=8)
        b.paste(bm, (x + 230, y))
    b.save(os.path.join(out, "colour-apple.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
