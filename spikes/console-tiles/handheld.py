# Handheld glyph variants for the console tiles, matched to Material Symbols Rounded's weight.
# Generic handhelds: rounded on every corner, no particular device's shape. Not shipped.
import importlib.util, sys
from PIL import Image, ImageDraw, ImageFont
here = "/Users/howard/Documents/GitHub/brick-hammer-launcher/spikes/console-tiles/"
spec = importlib.util.spec_from_file_location("g", here + "glyphs.py")
src = open(here + "glyphs.py").read().split("ROWS = [")[0]  # reuse helpers, not the sheet
g = {}
exec(compile(src, "glyphs", "exec"), g)
si, material = g["si"], g["material"]
L = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/"

def dpad(d, p, r, cx, cy, s, t):  # a plus with rounded arms
    d.rounded_rectangle(p(cx - s, cy - t, cx + s, cy + t), radius=r(t * 0.9), fill=0)
    d.rounded_rectangle(p(cx - t, cy - s, cx + t, cy + s), radius=r(t * 0.9), fill=0)

def variant(n):
    def draw(S):
        m, d, p = si.canvas(S)
        r = lambda v: S * v
        if n == 1:  # portrait, screen, d-pad and two buttons
            d.rounded_rectangle(p(0.25, 0.06, 0.75, 0.94), radius=r(0.12), fill=255)
            d.rounded_rectangle(p(0.33, 0.14, 0.67, 0.47), radius=r(0.05), fill=0)
            dpad(d, p, r, 0.40, 0.66, 0.075, 0.028)
            d.ellipse(p(0.555, 0.655, 0.625, 0.725), fill=0); d.ellipse(p(0.615, 0.585, 0.685, 0.655), fill=0)
        elif n == 2:  # landscape: screen in the middle, d-pad left, buttons right
            d.rounded_rectangle(p(0.04, 0.24, 0.96, 0.76), radius=r(0.20), fill=255)
            d.rounded_rectangle(p(0.31, 0.32, 0.69, 0.68), radius=r(0.05), fill=0)
            dpad(d, p, r, 0.175, 0.50, 0.07, 0.027)
            d.ellipse(p(0.78, 0.40, 0.85, 0.47), fill=0); d.ellipse(p(0.80, 0.53, 0.87, 0.60), fill=0)
        elif n == 3:  # portrait, larger screen, a simpler control row
            d.rounded_rectangle(p(0.24, 0.06, 0.76, 0.94), radius=r(0.13), fill=255)
            d.rounded_rectangle(p(0.31, 0.13, 0.69, 0.55), radius=r(0.06), fill=0)
            dpad(d, p, r, 0.39, 0.73, 0.07, 0.026)
            d.ellipse(p(0.57, 0.695, 0.65, 0.775), fill=0)
        elif n == 4:  # square handheld, like a modern pocket console
            d.rounded_rectangle(p(0.10, 0.10, 0.90, 0.90), radius=r(0.16), fill=255)
            d.rounded_rectangle(p(0.20, 0.18, 0.80, 0.52), radius=r(0.05), fill=0)
            dpad(d, p, r, 0.32, 0.71, 0.075, 0.028)
            d.ellipse(p(0.60, 0.68, 0.68, 0.76), fill=0); d.ellipse(p(0.69, 0.61, 0.77, 0.69), fill=0)
        elif n == 5:  # portrait, a screen with a play mark, no controls
            d.rounded_rectangle(p(0.25, 0.06, 0.75, 0.94), radius=r(0.12), fill=255)
            d.rounded_rectangle(p(0.33, 0.14, 0.67, 0.52), radius=r(0.05), fill=0)
            d.polygon(p(0.46, 0.27, 0.46, 0.39, 0.56, 0.33), fill=255)
            d.rounded_rectangle(p(0.43, 0.70, 0.57, 0.76), radius=r(0.03), fill=0)
        elif n == 6:  # 4 in Game Boy Color proportions (0.56 : 1): screen in a bezel, d-pad, two buttons
            d.rounded_rectangle(p(0.235, 0.05, 0.765, 0.95), radius=r(0.10), fill=255)
            d.rounded_rectangle(p(0.30, 0.12, 0.70, 0.47), radius=r(0.05), fill=0)
            dpad(d, p, r, 0.37, 0.66, 0.07, 0.027)
            d.ellipse(p(0.575, 0.645, 0.645, 0.715), fill=0); d.ellipse(p(0.63, 0.575, 0.70, 0.645), fill=0)
        elif n == 7:  # the same, with Start and Select
            d.rounded_rectangle(p(0.235, 0.05, 0.765, 0.95), radius=r(0.10), fill=255)
            d.rounded_rectangle(p(0.30, 0.12, 0.70, 0.45), radius=r(0.05), fill=0)
            dpad(d, p, r, 0.37, 0.62, 0.07, 0.027)
            d.ellipse(p(0.575, 0.605, 0.645, 0.675), fill=0); d.ellipse(p(0.63, 0.535, 0.70, 0.605), fill=0)
            d.rounded_rectangle(p(0.39, 0.80, 0.47, 0.83), radius=r(0.015), fill=0)
            d.rounded_rectangle(p(0.53, 0.80, 0.61, 0.83), radius=r(0.015), fill=0)
        elif n == 8:  # softer: rounder body, the screen nearer square
            d.rounded_rectangle(p(0.235, 0.05, 0.765, 0.95), radius=r(0.15), fill=255)
            d.rounded_rectangle(p(0.31, 0.13, 0.69, 0.46), radius=r(0.07), fill=0)
            dpad(d, p, r, 0.375, 0.65, 0.07, 0.028)
            d.ellipse(p(0.57, 0.635, 0.645, 0.71), fill=0); d.ellipse(p(0.625, 0.565, 0.70, 0.64), fill=0)
        elif n in (9, 10, 11, 12):  # original Game Boy proportions, 0.61 : 1
            x0, x1, y0, y1 = 0.225, 0.775, 0.05, 0.95
            if n == 12:  # the original's large bottom-right curve, for comparison only
                d.rounded_rectangle(p(x0, y0, x1, y1), radius=r(0.05), fill=255)
                d.rectangle(p(0.62, 0.80, x1, y1), fill=0)
                d.pieslice(p(0.47, 0.65, x1, y1), 0, 90, fill=255)
            else:
                d.rounded_rectangle(p(x0, y0, x1, y1), radius=r(0.09 if n != 11 else 0.13), fill=255)
            if n in (10, 11, 12):  # a bezel ring around the screen
                d.rounded_rectangle(p(0.28, 0.10, 0.72, 0.46), radius=r(0.05), fill=0)
                d.rounded_rectangle(p(0.325, 0.145, 0.675, 0.415), radius=r(0.025), fill=255)
            else:
                d.rounded_rectangle(p(0.29, 0.11, 0.71, 0.45), radius=r(0.045), fill=0)
            dpad(d, p, r, 0.36, 0.62, 0.065, 0.025)
            d.ellipse(p(0.575, 0.61, 0.64, 0.675), fill=0); d.ellipse(p(0.635, 0.55, 0.70, 0.615), fill=0)
            d.rounded_rectangle(p(0.40, 0.75, 0.47, 0.775), radius=r(0.012), fill=0)
            d.rounded_rectangle(p(0.51, 0.75, 0.58, 0.775), radius=r(0.012), fill=0)
            if n in (11, 12):  # speaker slots
                for k in range(3):
                    off = k * 0.035
                    d.line([*p(0.60 + off, 0.89), *p(0.66 + off, 0.83)], fill=0, width=int(r(0.016)))
        elif n in (13, 14, 15):  # D4 wider, with the large bottom-right curve drawn properly
            half = {13: 0.29, 14: 0.31, 15: 0.33}[n]  # half widths: 0.58, 0.62, 0.66 of the height
            x0, x1, y0, y1 = 0.5 - half, 0.5 + half, 0.05, 0.95
            R = 0.17  # the big corner
            d.rounded_rectangle(p(x0, y0, x1, y1), radius=r(0.05), fill=255)
            d.rectangle(p(x1 - R, y1 - R, x1, y1), fill=0)
            d.pieslice(p(x1 - 2 * R, y1 - 2 * R, x1, y1), 0, 90, fill=255)
            cx = 0.5
            d.rounded_rectangle(p(x0 + 0.055, 0.10, x1 - 0.055, 0.46), radius=r(0.05), fill=0)
            d.rounded_rectangle(p(x0 + 0.10, 0.145, x1 - 0.10, 0.415), radius=r(0.025), fill=255)
            dpad(d, p, r, x0 + 0.135, 0.62, 0.065, 0.025)
            d.ellipse(p(x1 - 0.20, 0.61, x1 - 0.135, 0.675), fill=0); d.ellipse(p(x1 - 0.14, 0.55, x1 - 0.075, 0.615), fill=0)
            d.rounded_rectangle(p(cx - 0.10, 0.75, cx - 0.03, 0.775), radius=r(0.012), fill=0)
            d.rounded_rectangle(p(cx + 0.01, 0.75, cx + 0.08, 0.775), radius=r(0.012), fill=0)
        return si.soften(m, S)
    return draw

import os
if os.environ.get("SET") == "dmgwide":
    names = ["D4 as before", "D4 wider (0.58)", "D4 wider (0.62)", "D4 widest (0.66)"]
    order = [12, 13, 14, 15]
elif os.environ.get("SET") == "dmg":
    names = ["D1 Game Boy shape", "D2 with bezel", "D3 bezel, speaker", "D4 original corner"]
    order = [9, 10, 11, 12]
elif os.environ.get("SET") == "gbc":
    names = ["4 Square", "4a GBC shape", "4b GBC, Start/Select", "4c GBC, softer"]
    order = [4, 6, 7, 8]
else:
    names = ["1 Portrait", "2 Landscape", "3 Big screen", "4 Square", "5 Play mark"]
    order = [1, 2, 3, 4, 5]
GREEN, INDIGO = (48, 209, 88), (94, 92, 230)
F = ImageFont.truetype(L + "res/Inter-SemiBold.ttf", 15)
sheet = Image.new("RGB", (len(names) * 170 + 40, 330), (24, 24, 30))
d = ImageDraw.Draw(sheet)
ref = si.tile(INDIGO, material(0xF135), 150).resize((40, 40), Image.LANCZOS)
for i, nm in enumerate(names):
    x = 30 + i * 170
    big = si.tile(GREEN, variant(order[i]), 150).resize((120, 120), Image.LANCZOS)
    sheet.paste(big, (x, 30), big)
    d.text((x, 160), nm, font=F, fill=(220, 220, 230))
    small = si.tile(GREEN, variant(order[i]), 150).resize((40, 40), Image.LANCZOS)
    sheet.paste(small, (x, 210), small); sheet.paste(ref, (x + 50, 210), ref)
    d.text((x, 262), "at 40 px, beside", font=ImageFont.truetype(L + "res/Inter-Regular.ttf", 13), fill=(150, 150, 160))
    d.text((x, 280), "the controller", font=ImageFont.truetype(L + "res/Inter-Regular.ttf", 13), fill=(150, 150, 160))
sheet.save(sys.argv[1])
