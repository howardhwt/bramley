# Mockups of Bramley's default console tiles: a generic glyph by kind of device, the system's
# full name and a soft colour of its own. No company's logo or device shape. Not shipped.
import colorsys, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
R = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/res/"
S = 2  # drawn at 2x, shown at 1x
W, H, RAD = 200 * S, 120 * S, 16 * S
semi = lambda px: ImageFont.truetype(R + "Inter-SemiBold.ttf", px * S)

def glyph(d, kind, cx, cy, s, col):
    w = max(2, int(2.4 * S * s))
    def rr(x0, y0, x1, y1, r):
        d.rounded_rectangle([cx + x0 * s * S, cy + y0 * s * S, cx + x1 * s * S, cy + y1 * s * S], r * s * S, outline=col, width=w)
    def ln(x0, y0, x1, y1):
        d.line([cx + x0 * s * S, cy + y0 * s * S, cx + x1 * s * S, cy + y1 * s * S], fill=col, width=w)
    def dot(x, y, r):
        d.ellipse([cx + (x - r) * s * S, cy + (y - r) * s * S, cx + (x + r) * s * S, cy + (y + r) * s * S], fill=col)
    def ring(x, y, r):
        d.ellipse([cx + (x - r) * s * S, cy + (y - r) * s * S, cx + (x + r) * s * S, cy + (y + r) * s * S], outline=col, width=w)
    if kind == "handheld":
        rr(-11, -17, 11, 17, 4); rr(-7, -13, 7, -2, 1.5)
        ln(-6, 7, -2, 7); ln(-4, 5, -4, 9); dot(4, 8, 1.6); dot(7, 5, 1.6)
    elif kind == "console":
        rr(-20, -8, 20, 9, 3); ln(-12, -2, 6, -2); dot(13, 3, 1.8)
    elif kind == "disc":
        ring(0, 0, 15); ring(0, 0, 4)
    elif kind == "arcade":
        rr(-18, 4, 18, 14, 3); ln(-8, 4, -8, -8); ring(-8, -11, 4); dot(6, 9, 2); dot(12, 9, 2)
    elif kind == "computer":
        rr(-21, -8, 21, 10, 3)
        for row, y in enumerate((-3, 1.5)):
            for k in range(7):
                dot(-15 + k * 5 + row * 2.5, y, 1.1)
        ln(-9, 6, 9, 6)

SYSTEMS = [("Game Boy", "handheld", 0.33), ("Super Nintendo", "console", 0.72), ("PlayStation", "disc", 0.60),
           ("Arcade", "arcade", 0.02), ("Commodore 64", "computer", 0.10), ("Game Boy Advance", "handheld", 0.78),
           ("Mega Drive", "console", 0.55), ("Doom", "computer", 0.97)]

def tile(name, kind, hue, layout):
    im = Image.new("RGBA", (W, H))
    top = tuple(int(c * 255) for c in colorsys.hsv_to_rgb(hue, 0.30, 0.74))
    bot = tuple(int(c * 255) for c in colorsys.hsv_to_rgb((hue + 0.03) % 1, 0.42, 0.50))
    grad = Image.new("RGBA", (1, H))
    for y in range(H):
        t = y / (H - 1)
        grad.putpixel((0, y), tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)) + (255,))
    grad = grad.resize((W, H))
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, W - 1, H - 1], RAD, fill=255)
    im.paste(grad, (0, 0), mask)
    d = ImageDraw.Draw(im)
    col = (255, 255, 255, 235)
    f = semi(15 if layout == "A" else 16)
    if layout == "A":
        glyph(d, kind, W / 2, H * 0.40, 1.05, col)
        tw = d.textlength(name, font=f)
        d.text(((W - tw) / 2, H * 0.68), name, font=f, fill=col)
    else:
        glyph(d, kind, 16 * S + 22 * S, 14 * S + 18 * S, 0.85, col)
        d.text((16 * S, H - 16 * S - 18 * S), name, font=f, fill=col)
    return im.resize((W // S, H // S), Image.LANCZOS)

def board(layout, out):
    bg = Image.open(R + "wallpapers/bloom-blur.jpg").convert("RGB").resize((1024, 768))
    canvas = bg.crop((0, 300, 1024, 300 + 330)).filter(ImageFilter.GaussianBlur(4))
    dark = Image.new("RGB", canvas.size, (0, 0, 0))
    canvas = Image.blend(canvas, dark, 0.35)
    d = ImageDraw.Draw(canvas)
    d.text((56, 24), "Consoles", font=ImageFont.truetype(R + "Inter-SemiBold.ttf", 18), fill=(235, 235, 240))
    for i, (n, k, h) in enumerate(SYSTEMS[:4]):
        t = tile(n, k, h, layout)
        canvas.paste(t, (56 + i * 230, 60), t)
    for i, (n, k, h) in enumerate(SYSTEMS[4:]):
        t = tile(n, k, h, layout)
        canvas.paste(t, (56 + i * 230, 200), t)
    canvas.save(out)

board("A", sys.argv[1] + "/tiles-A.png")
board("B", sys.argv[1] + "/tiles-B.png")
