# Home with tiles in the style of iOS Shortcuts: a full colour tile, a white symbol top left, the
# name bottom left. Compared with the current widget tiles. Not shipped.
import importlib.util, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
L = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/"
spec = importlib.util.spec_from_file_location("cg", L + "tools/console-glyphs.py")
cg = importlib.util.module_from_spec(spec); spec.loader.exec_module(cg)
si = cg.si
si.FONT_BYTES = si.phosphor_fill_font()
font = cg.material_font()
SYM = {"handheld": cg.handheld, "console": cg.material(font, 0xF135), "disc": cg.material(font, 0xE019),
       "arcade": cg.material(font, 0xF5EE), "computer": cg.material(font, 0xE312), "star": cg.material(font, 0xF09A),
       "music": cg.material(font, 0xE405)}
F = lambda px, w="SemiBold": ImageFont.truetype(L + f"res/Inter-{w}.ttf", px)

ROW = [("Doom", "computer", cg.RED, "2 games"), ("Game Boy", "handheld", cg.GREEN, "2 games"),
       ("Game Boy Color", "handheld", cg.TEAL, "1 game"), ("Genesis", "console", cg.BLUE, "1 game"),
       ("NES", "console", cg.RED, "3 games")]
FOCUS = 1

def glyph_mask(kind, px):
    m = SYM[kind](600)
    m = m.crop(m.getbbox())
    k = px / max(m.size)
    return m.resize((max(1, round(m.width * k)), max(1, round(m.height * k))), Image.LANCZOS)

def gradient(w, h, col, diag=True):
    top, bot = np.array(si.light(col, 0.18), np.float32), np.array(si.dark(col, 0.22), np.float32)
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    t = ((x / w) * 0.35 + (y / h) * 0.65) if diag else y / h
    rgb = top + (bot - top) * t[..., None]
    return Image.fromarray(np.concatenate([rgb, np.full((h, w, 1), 255, np.float32)], 2).astype(np.uint8), "RGBA")

def rounded(img, r):
    m = Image.new("L", img.size, 0); ImageDraw.Draw(m).rounded_rectangle([0, 0, img.width - 1, img.height - 1], r, fill=255)
    img.putalpha(m); return img

def shortcut(name, kind, col, sub, w=210, h=125, gsize=40, big=False):
    t = gradient(w, h, col)
    d = ImageDraw.Draw(t)
    g = glyph_mask(kind, gsize)
    white = Image.new("RGBA", g.size, (255, 255, 255, 255)); white.putalpha(g)
    if big:
        t.alpha_composite(white, ((w - g.width) // 2, (h - g.height) // 2))
    else:
        t.alpha_composite(white, (16, 16))
        d.text((16, h - (52 if sub else 36)), name, font=F(19, "SemiBold"), fill=(255, 255, 255, 255))
        if sub: d.text((16, h - 29), sub, font=F(15, "SemiBold"), fill=(255, 255, 255, 190))
    t = rounded(t, 18 if not big else 28)
    rim = Image.new("RGBA", t.size, (0, 0, 0, 0))
    ImageDraw.Draw(rim).rounded_rectangle([0, 0, w - 1, h - 1], 18 if not big else 28, outline=(255, 255, 255, 60), width=1)
    return Image.alpha_composite(t, rim)

def shadow(canvas, box, r, a=110, blur=14, dy=8):
    s = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(s).rounded_rectangle([box[0], box[1] + dy, box[2], box[3] + dy], r, fill=(0, 0, 0, a))
    canvas.alpha_composite(s.filter(ImageFilter.GaussianBlur(blur)))

def home(sub=True, dim=False):
    bg = Image.open(L + "res/wallpapers/bloom-blur.jpg").convert("RGBA").resize((1024, 768))
    c = bg.copy(); d = ImageDraw.Draw(c)
    d.text((808, 24), "14:32", font=F(19), fill="white"); d.text((880, 25), "100%", font=F(18, "Regular"), fill=(255, 255, 255, 200))
    name, kind, col, n = ROW[FOCUS]
    shadow(c, (57, 81, 555, 419), 28, 120, 24, 14)
    c.alpha_composite(shortcut(name, kind, col, None, 498, 338, 150, big=True), (57, 81))
    d.text((597, 106), "CONSOLE", font=F(18), fill=(255, 255, 255, 130))
    d.text((596, 138), name, font=F(44, "Bold"), fill="white")
    d.text((597, 198), n, font=F(21, "Regular"), fill=(255, 255, 255, 170))
    pill = Image.new("RGBA", c.size, (0, 0, 0, 0)); ImageDraw.Draw(pill).rounded_rectangle((597, 366, 710, 405), 20, fill=(255, 255, 255, 40)); c.alpha_composite(pill); d.text((614, 375), "A   Open", font=F(18, "SemiBold"), fill="white")
    d.text((58, 502), "Consoles", font=F(19), fill=(255, 255, 255, 220))
    x = 57
    for i, (nm, k, cl, s) in enumerate(ROW):
        t = shortcut(nm, k, cl, s if sub else None)
        if i == FOCUS:
            t = t.resize((226, 135), Image.LANCZOS)
            shadow(c, (x - 8, 528, x - 8 + 226, 663), 20, 140, 16, 10)
            c.alpha_composite(t, (x - 8, 528))
        else:
            if dim:
                a = t.getchannel("A"); t = Image.alpha_composite(t, Image.new("RGBA", t.size, (0, 0, 0, 80))); t.putalpha(a)
            c.alpha_composite(t, (x, 535))
        x += 232
    d.text((58, 696), "Collections", font=F(19), fill=(255, 255, 255, 160))
    c.alpha_composite(shortcut("Favorites", "star", cg.YELLOW, "1 item").crop((0, 0, 210, 40)), (57, 728))
    return c.convert("RGB")

cur = Image.open(sys.argv[1]).convert("RGB")
a = home(True); b = home(False, dim=True)
W = Image.new("RGB", (1024 * 3 + 40, 768 + 60), (20, 20, 22)); dd = ImageDraw.Draw(W)
for i, (im, lab) in enumerate([(cur, "Now (0.9.3): widget tiles"), (a, "S1: Shortcuts tiles, name + count"), (b, "S2: Shortcuts tiles, name only, others dimmed")]):
    W.paste(im, (i * 1044, 60)); dd.text((i * 1044 + 10, 14), lab, font=F(30), fill="white")
W.save(sys.argv[2])
a.save(sys.argv[2].replace(".png", "-S1.png")); b.save(sys.argv[2].replace(".png", "-S2.png"))
