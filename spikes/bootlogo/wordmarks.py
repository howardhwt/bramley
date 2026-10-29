#!/usr/bin/env python3
"""Wordmark options next to the Orchard mark: stacked on the boot screen, and side by side."""
import os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, text_centre
from colours2 import icon, IVORY

def word(d, xy, style, size, anchor="mm"):
    t, w, track = {"caps": ("BRAMLEY", "Regular", .32), "title": ("Bramley", "SemiBold", -.01),
                   "lower": ("bramley", "SemiBold", -.01), "light": ("Bramley", "Regular", .02)}[style]
    f = font(w, size); x, y = xy
    ws = [d.textlength(c, font=f) for c in t]; tw = sum(ws) + track * size * (len(t) - 1)
    if anchor == "mm": x -= tw / 2
    bb = f.getbbox("B"); y -= (bb[1] + bb[3]) / 2
    for c, cw in zip(t, ws): d.text((x, y), c, font=f, fill=IVORY); x += cw + track * size
    return tw

STYLES = [("caps", "A. Spaced capitals", "Premium label, like a watch or a car"),
          ("title", "B. Title case, semibold", "Like Apple's product names"),
          ("lower", "C. Lower case, semibold", "Friendlier, more modern"),
          ("light", "D. Title case, regular", "Quietest; lets the mark lead")]

def main(out):
    b = Image.new("RGB", (1800, 980), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 36), "Wordmark  ·  with the Orchard mark", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (st, name, note) in enumerate(STYLES):
        x = 60 + i * 430
        boot = Image.new("RGB", (400, 300), (0, 0, 0)); bd = ImageDraw.Draw(boot)
        t = icon("orchard", 96); boot.paste(t, (152, 50), t)
        word(bd, (200, 205), st, 26 if st == "caps" else 34)
        b.paste(boot, (x, 110))
        lock = Image.new("RGB", (400, 120), (0, 0, 0)); ld = ImageDraw.Draw(lock)
        t = icon("orchard", 64); lock.paste(t, (24, 28), t)
        word(ld, (104, 60), st, 22 if st == "caps" else 32, anchor="lm")
        b.paste(lock, (x, 430))
        d.text((x, 575), name, font=font("SemiBold", 24), fill=(235, 235, 240))
        d.text((x, 610), note, font=font("Regular", 18), fill=(170, 170, 180))
        # small: as in About and the README
        sm = Image.new("RGB", (400, 70), (30, 30, 36)); sd = ImageDraw.Draw(sm)
        t = icon("orchard", 32); sm.paste(t, (18, 19), t)
        word(sd, (62, 35), st, 15 if st == "caps" else 22, anchor="lm")
        b.paste(sm, (x, 660))
    b.save(os.path.join(out, "wordmarks.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
