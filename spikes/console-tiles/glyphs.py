# Glyph candidates for the console tiles, side by side: Phosphor Fill (MIT), Material Symbols
# Rounded filled (Apache 2.0) and Bramley's own. Rendered as the Settings-style app icon. Not shipped.
import importlib.util, sys
from PIL import Image, ImageDraw, ImageFont
L = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/"
spec = importlib.util.spec_from_file_location("si", L + "tools/settings-icons.py")
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
si.FONT_BYTES = si.phosphor_fill_font()
MAT = L + "build/cache/MaterialSymbolsRounded.ttf"

def material(code):
    def draw(S):
        f = ImageFont.truetype(MAT, S)
        axes = {a["name"].decode() if isinstance(a["name"], bytes) else a["name"]: a for a in f.get_variation_axes()}
        vals = []
        for a in f.get_variation_axes():
            n = a["name"].decode() if isinstance(a["name"], bytes) else a["name"]
            vals.append({"Fill": 1, "FILL": 1, "Weight": 500, "wght": 500, "Grade": 0, "GRAD": 0,
                         "Optical size": 48, "opsz": 48}.get(n, a["default"]))
        f.set_variation_by_axes(vals)
        m = Image.new("L", (S * 2, S * 2), 0)
        ImageDraw.Draw(m).text((S // 2, S // 2), chr(code), font=f, fill=255)
        return m.crop(m.getbbox())
    return draw

# Bramley's own, on a 0-1 grid, filled with cut-outs, rounded like SF Symbols
def own(kind):
    def draw(S):
        m, d, p = si.canvas(S)
        r = lambda v: S * v
        if kind == "handheld":
            d.rounded_rectangle(p(0.27, 0.06, 0.73, 0.94), radius=r(0.10), fill=255)
            d.rounded_rectangle(p(0.34, 0.14, 0.66, 0.46), radius=r(0.04), fill=0)
            d.rounded_rectangle(p(0.335, 0.615, 0.475, 0.665), radius=r(0.02), fill=0)
            d.rounded_rectangle(p(0.38, 0.57, 0.43, 0.71), radius=r(0.02), fill=0)
            d.ellipse(p(0.545, 0.635, 0.605, 0.695), fill=0); d.ellipse(p(0.605, 0.565, 0.665, 0.625), fill=0)
            d.rounded_rectangle(p(0.44, 0.80, 0.56, 0.83), radius=r(0.015), fill=0)
        elif kind == "console":
            d.ellipse(p(0.06, 0.26, 0.42, 0.82), fill=255); d.ellipse(p(0.58, 0.26, 0.94, 0.82), fill=255)
            d.rounded_rectangle(p(0.20, 0.26, 0.80, 0.62), radius=r(0.14), fill=255)
            d.rounded_rectangle(p(0.17, 0.42, 0.33, 0.48), radius=r(0.02), fill=0)
            d.rounded_rectangle(p(0.22, 0.37, 0.28, 0.53), radius=r(0.02), fill=0)
            d.ellipse(p(0.66, 0.36, 0.73, 0.43), fill=0); d.ellipse(p(0.71, 0.45, 0.78, 0.52), fill=0)
        elif kind == "disc":
            d.ellipse(p(0.08, 0.08, 0.92, 0.92), fill=255); d.ellipse(p(0.40, 0.40, 0.60, 0.60), fill=0)
            d.arc(p(0.20, 0.20, 0.80, 0.80), 200, 260, fill=0, width=int(r(0.045)))
        elif kind == "arcade":
            d.rounded_rectangle(p(0.10, 0.60, 0.90, 0.90), radius=r(0.08), fill=255)
            d.rounded_rectangle(p(0.30, 0.30, 0.38, 0.62), radius=r(0.04), fill=255)
            d.ellipse(p(0.20, 0.08, 0.48, 0.36), fill=255)
            d.ellipse(p(0.56, 0.68, 0.66, 0.78), fill=0); d.ellipse(p(0.72, 0.68, 0.82, 0.78), fill=0)
        elif kind == "computer":
            d.rounded_rectangle(p(0.08, 0.12, 0.92, 0.70), radius=r(0.07), fill=255)
            d.rounded_rectangle(p(0.15, 0.19, 0.85, 0.61), radius=r(0.03), fill=0)
            d.rounded_rectangle(p(0.42, 0.70, 0.58, 0.82), radius=r(0.0), fill=255)
            d.rounded_rectangle(p(0.28, 0.80, 0.72, 0.88), radius=r(0.04), fill=255)
        return si.soften(m, S)
    return draw

ROWS = [
    ("Handheld", [("Bramley", own("handheld")), ("Material: videogame", material(0xE338)), ("Phosphor: phone", si.phosphor(0xE1E0))]),
    ("TV console", [("Bramley", own("console")), ("Material: esports", material(0xEA28)), ("Material: controller", material(0xF135)), ("Phosphor", si.phosphor(0xE26E))]),
    ("Disc", [("Bramley", own("disc")), ("Material: album", material(0xE019)), ("Phosphor", si.phosphor(0xE564))]),
    ("Arcade", [("Bramley", own("arcade")), ("Material: joystick", material(0xF5EE)), ("Phosphor", si.phosphor(0xEA5E))]),
    ("Computer", [("Bramley", own("computer")), ("Material: desktop", material(0xE30C)), ("Material: keyboard", material(0xE312)), ("Phosphor: desktop", si.phosphor(0xE560))]),
]
COL = [(48, 209, 88), (94, 92, 230), (10, 132, 255), (255, 69, 58), (172, 142, 104)]
F = ImageFont.truetype(L + "res/Inter-SemiBold.ttf", 15)
cw, rh = 170, 175
sheet = Image.new("RGB", (160 + 4 * cw, 40 + len(ROWS) * rh), (24, 24, 30))
d = ImageDraw.Draw(sheet)
for ri, (title, cands) in enumerate(ROWS):
    y = 40 + ri * rh
    d.text((20, y + 50), title, font=ImageFont.truetype(L + "res/Inter-Bold.ttf", 18), fill=(240, 240, 245))
    for ci, (label, sym) in enumerate(cands):
        icon = si.tile(COL[ri], sym, 150).resize((120, 120), Image.LANCZOS)
        x = 160 + ci * cw
        sheet.paste(icon, (x, y), icon)
        d.text((x, y + 128), label, font=F, fill=(200, 200, 210))
sheet.save(sys.argv[1])
