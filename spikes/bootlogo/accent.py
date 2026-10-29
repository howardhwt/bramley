#!/usr/bin/env python3
"""The interface's accent colour, three ways: today's (Apple blue and iOS green), one orchard
green, and ivory. Each shown on a switch, a slider, a picker column, a progress bar and a toast dot."""
import os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import font

SS = 2
def rr(d, box, r, fill): d.rounded_rectangle([v * SS for v in box], radius=r * SS, fill=fill)

OPTIONS = [("Today", "Apple blue and iOS green", (10, 132, 255), (52, 199, 89)),
           ("Orchard", "One green, lifted from the mark", (74, 178, 120), (74, 178, 120)),
           ("Ivory", "The mark's ivory; green only for 'on'", (243, 235, 214), (74, 178, 120))]

def panel(name, note, accent, on):
    W, H = 520, 560
    im = Image.new("RGB", (W * SS, H * SS), (22, 22, 28)); d = ImageDraw.Draw(im)
    f = lambda w, s: font(w, s * SS)
    d.text((24 * SS, 20 * SS), name, font=f("SemiBold", 28), fill=(240, 240, 245))
    d.text((24 * SS, 58 * SS), note, font=f("Regular", 17), fill=(160, 160, 170))
    # a row with a switch
    rr(d, (24, 100, 496, 152), 12, (44, 44, 52)); d.text((44 * SS, 112 * SS), "Wi-Fi", font=f("Regular", 22), fill=(235, 235, 240))
    rr(d, (414, 109, 472, 143), 17, on); d.ellipse([v * SS for v in (441, 112, 469, 140)], fill=(255, 255, 255))
    # slider
    rr(d, (24, 168, 496, 220), 12, (44, 44, 52)); d.text((44 * SS, 180 * SS), "Brightness", font=f("Regular", 22), fill=(235, 235, 240))
    rr(d, (272, 191, 472, 197), 3, (80, 80, 90)); rr(d, (272, 191, 412, 197), 3, accent); d.ellipse([v * SS for v in (400, 182, 424, 206)], fill=(255, 255, 255))
    # picker columns
    rr(d, (24, 238, 496, 340), 14, (240, 240, 244))
    for i, (lab, sel) in enumerate((("28", True), ("Oct", False), ("2026", False))):
        x = 110 + i * 110
        rr(d, (x, 268, x + 96, 312), 12, accent if sel else (225, 225, 230))
        tc = (20, 20, 24) if (sel and accent[0] > 200) else ((255, 255, 255) if sel else (30, 30, 34))
        d.text(((x + 48) * SS, 290 * SS), lab, font=f("Regular", 22), fill=tc, anchor="mm")
    # progress
    d.text((24 * SS, 360 * SS), "12 of 40 unlocked", font=f("Regular", 18), fill=(200, 200, 210))
    rr(d, (24, 392, 496, 400), 4, (60, 60, 70)); rr(d, (24, 392, 166, 400), 4, accent)
    # toast
    rr(d, (24, 430, 340, 480), 25, (30, 30, 34)); d.ellipse([v * SS for v in (42, 445, 62, 465)], fill=accent)
    d.text((76 * SS, 443 * SS), "Saved to slot 3", font=f("Regular", 20), fill=(240, 240, 245))
    # focused button text tint
    d.text((24 * SS, 506 * SS), "Selected text and links", font=f("SemiBold", 20), fill=accent)
    return im.resize((W, H), Image.LANCZOS)

def main(out):
    b = Image.new("RGB", (1660, 640), (14, 14, 18)); d = ImageDraw.Draw(b)
    d.text((40, 24), "Interface accent  ·  three ways", font=font("SemiBold", 32), fill=(235, 235, 240))
    for i, o in enumerate(OPTIONS): b.paste(panel(*o), (40 + i * 540, 70))
    b.save(os.path.join(out, "accent.png"))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "boards"))
