#!/usr/bin/env python3
"""Apple pip (seed shape 2) at four lengths, in direction B. Needs Pillow."""
import os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font, text_centre
from symbols import tile
from seeds import pips

IDEAS = [("2. As before", "Length 0.21", pips("seed", L=.21, W=.115, r=.19)),
         ("2a. A little longer", "Length 0.24", pips("seed", L=.24, W=.115, r=.205)),
         ("2b. Longer", "Length 0.27", pips("seed", L=.27, W=.112, r=.22)),
         ("2c. Longest", "Length 0.30, slimmer", pips("seed", L=.30, W=.105, r=.235))]

def main(out):
    W, H = 1800, 640
    b = Image.new("RGB", (W, H), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((60, 36), "Apple pip  ·  longer", font=font("SemiBold", 34), fill=(235, 235, 240))
    for i, (name, note, sym) in enumerate(IDEAS):
        x = 60 + i * 430; y = 110
        t = tile(sym, 200); b.paste(t, (x, y), t)
        d.text((x, y + 215), name, font=font("SemiBold", 24), fill=(235, 235, 240))
        d.text((x, y + 250), note, font=font("Regular", 18), fill=(170, 170, 180))
        row = Image.new("RGB", (300, 46), (44, 44, 52)); rd = ImageDraw.Draw(row)
        t36 = tile(sym, 32); row.paste(t36, (14, 7), t36); rd.text((60, 9), "Bramley", font=font("Regular", 22), fill=(235, 235, 240))
        b.paste(row, (x, y + 290))
        one = sym(400).resize((90, 90), Image.LANCZOS)
        b.paste(Image.new("RGB", (90, 90), (255, 255, 255)), (x, y + 355), one)
        bm = Image.new("RGB", (200, 110), (0, 0, 0)); bd = ImageDraw.Draw(bm)
        tb = tile(sym, 48); bm.paste(tb, (76, 8), tb); text_centre(bd, (100, 84), "BRAMLEY", font("Regular", 14), (255, 255, 255), track=4)
        b.paste(bm, (x + 110, y + 345))
    b.save(os.path.join(out, "seeds-longer.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
