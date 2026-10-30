# Apple-style console tiles, take 2: filled symbols (Phosphor, MIT, as Settings uses) and the
# Settings icon renderer. V1 iOS widget (app icon on glass), V2 colour tile. Not shipped.
import importlib.util, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
L = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/"
spec = importlib.util.spec_from_file_location("si", L + "tools/settings-icons.py")
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
si.FONT_BYTES = si.phosphor_fill_font()
F = lambda px, w="SemiBold": ImageFont.truetype(L + f"res/Inter-{w}.ttf", px)

def handheld(S):  # filled, cut-out screen and buttons, SF-like weight
    m, d, p = si.canvas(S)
    d.rounded_rectangle(p(0.30, 0.12, 0.70, 0.88), radius=S * 0.08, fill=255)
    d.rounded_rectangle(p(0.36, 0.19, 0.64, 0.47), radius=S * 0.03, fill=0)
    d.rectangle(p(0.37, 0.62, 0.47, 0.66), fill=0); d.rectangle(p(0.40, 0.58, 0.44, 0.70), fill=0)
    d.ellipse(p(0.53, 0.63, 0.58, 0.68), fill=0); d.ellipse(p(0.58, 0.57, 0.63, 0.62), fill=0)
    return si.soften(m, S)

SYM = {"handheld": handheld, "console": si.phosphor(0xE26E), "disc": si.phosphor(0xE564),
       "arcade": si.phosphor(0xEA5E), "computer": si.phosphor(0xE2D8)}
# Apple's dark-mode system colours
GREEN, BLUE, INDIGO, ORANGE, RED, TEAL, PURPLE, BROWN = (48, 209, 88), (10, 132, 255), (94, 92, 230), (255, 159, 10), (255, 69, 58), (64, 200, 224), (191, 90, 242), (172, 142, 104)
SYSTEMS = [("Game Boy", "handheld", GREEN, 2), ("Super Nintendo", "console", INDIGO, 8), ("PlayStation", "disc", BLUE, 3),
           ("Arcade", "arcade", RED, 14), ("Commodore 64", "computer", BROWN, 5), ("Game Boy Advance", "handheld", PURPLE, 6),
           ("Mega Drive", "console", TEAL, 2), ("Doom", "computer", ORANGE, 2)]
W, H, R = 200, 120, 16

def rounded(img, r):
    m = Image.new("L", img.size, 0); ImageDraw.Draw(m).rounded_rectangle([0, 0, img.size[0] - 1, img.size[1] - 1], r, fill=255)
    out = Image.new("RGBA", img.size); out.paste(img, (0, 0), m); return out

def v1(name, kind, col, n, bg):
    # glass: the blurred wallpaper behind, darkened, with a hairline rim
    t = bg.filter(ImageFilter.GaussianBlur(10)).convert("RGBA")
    t = Image.alpha_composite(t, Image.new("RGBA", t.size, (28, 28, 34, 150)))
    t = rounded(t, R); d = ImageDraw.Draw(t)
    d.rounded_rectangle([0, 0, W - 1, H - 1], R, outline=(255, 255, 255, 34), width=1)
    icon = si.tile(col, SYM[kind], 150).resize((40, 40), Image.LANCZOS)
    t.alpha_composite(icon, (14, 14))
    d.text((16, H - 50), name, font=F(17, "Bold"), fill=(255, 255, 255, 245))
    d.text((16, H - 27), f"{n} games", font=F(14, "Regular"), fill=(235, 235, 245, 150))
    return t

def v2(name, kind, col, n, bg):
    top = si.light(col, 0.10); bot = si.dark(col, 0.28)
    g = Image.new("RGBA", (1, H))
    for y in range(H):
        k = y / (H - 1); g.putpixel((0, y), tuple(int(top[i] + (bot[i] - top[i]) * k) for i in range(3)) + (255,))
    t = rounded(g.resize((W, H)), R); d = ImageDraw.Draw(t)
    d.rounded_rectangle([0, 0, W - 1, H - 1], R, outline=(255, 255, 255, 50), width=1)
    m = si.fit(SYM[kind](600), 600) if hasattr(si, "fit") else None
    sym = SYM[kind](600).resize((34, 34), Image.LANCZOS) if m is None else m.resize((34, 34), Image.LANCZOS)
    white = Image.new("RGBA", sym.size, (255, 255, 255, 255)); white.putalpha(sym)
    t.alpha_composite(white, (16, 16))
    d.text((16, H - 50), name, font=F(17, "Bold"), fill=(255, 255, 255, 250))
    d.text((16, H - 27), f"{n} games", font=F(14, "Regular"), fill=(255, 255, 255, 175))
    return t

def board(fn, out):
    bg = Image.open(L + "res/wallpapers/bloom-blur.jpg").convert("RGB").resize((1024, 768)).crop((0, 300, 1024, 630))
    canvas = Image.blend(bg, Image.new("RGB", bg.size), 0.35).convert("RGBA")
    ImageDraw.Draw(canvas).text((56, 24), "Consoles", font=F(18), fill=(235, 235, 240))
    for i, s in enumerate(SYSTEMS):
        x, y = 56 + (i % 4) * 230, 60 + (i // 4) * 140
        canvas.alpha_composite(fn(*s, canvas.crop((x, y, x + W, y + H)).convert("RGB")), (x, y))
    canvas.convert("RGB").save(out)

board(v1, sys.argv[1] + "/tiles-V1-widget.png")
board(v2, sys.argv[1] + "/tiles-V2-colour.png")
