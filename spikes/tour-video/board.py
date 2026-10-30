# Storyboard stills for an Apple-style tour video: the device on black, title cards, an end card.
# The device image is not in the repo: pass a front render or photo of the Brick (with the screen
# at the box below) as the first argument. Not shipped.
#   python board.py device.jpg frames-dir out.png
import sys, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
R = "/Users/howard/Documents/GitHub/brick-hammer-launcher/"
F = lambda px, w="Bold": ImageFont.truetype(R + f"workspace/all/launcher/res/Inter-{w}.ttf", px)
SCREEN = (364, 157, 1139, 738)  # the display inside the 1500 px render

def cutout(path):
    im = Image.open(path).convert("RGB")
    a = np.asarray(im).astype(int)
    white = (a.min(2) > 235).astype(np.uint8) * 255
    m = Image.fromarray(white).copy()  # a copy: floodfill does nothing on an array-backed image
    for c in [(0, 0), (im.width - 1, 0), (0, im.height - 1), (im.width - 1, im.height - 1)]:
        ImageDraw.floodfill(m, c, 128)
    alpha = Image.fromarray(np.where(np.asarray(m) == 128, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1))
    out = im.convert("RGBA"); out.putalpha(alpha)
    return out

def device_with(dev, screen):
    d = dev.copy()
    w, h = SCREEN[2] - SCREEN[0], SCREEN[3] - SCREEN[1]
    d.paste(screen.convert("RGB").resize((w, h), Image.LANCZOS), SCREEN[:2])
    return d

def stage(dev, screen, scale=0.62, dx=0, dy=0, glow=True):
    c = Image.new("RGBA", (1920, 1080), (0, 0, 0, 255))
    if glow:
        g = Image.new("L", (1920, 1080), 0); ImageDraw.Draw(g).ellipse((560, 140, 1360, 940), fill=60)
        c.alpha_composite(Image.merge("RGBA", [g, g, g.point(lambda v: int(v * 1.2)), g]).filter(ImageFilter.GaussianBlur(160)))
    d = device_with(dev, screen)
    d = d.resize((int(d.width * scale), int(d.height * scale)), Image.LANCZOS)
    c.alpha_composite(d, ((1920 - d.width) // 2 + dx, (1080 - d.height) // 2 + dy))
    return c

def card(lines, sub=None):
    c = Image.new("RGBA", (1920, 1080), (0, 0, 0, 255)); dr = ImageDraw.Draw(c)
    y = 540 - 60 * len(lines)
    for l in lines:
        w = dr.textlength(l, font=F(84)); dr.text(((1920 - w) / 2, y), l, font=F(84), fill="white"); y += 110
    if sub:
        w = dr.textlength(sub, font=F(34, "Regular")); dr.text(((1920 - w) / 2, y + 20), sub, font=F(34, "Regular"), fill=(160, 160, 166))
    return c

def zoom(dev, screen, box):
    # the camera moves in on part of the screen: crop of the staged device
    s = stage(dev, screen, scale=1.2, dy=260)
    return s

dev = cutout(sys.argv[1]); fr = sys.argv[2].rstrip("/") + "/"
home = Image.open(fr + "home.png"); lib = Image.open(fr + "library.png"); np_ = Image.open(fr + "nowplaying.png")
settings = Image.open(fr + "settings.png"); hero = Image.open(fr + "continue.png")
icon = Image.open(R + "docs/brand/bramley-icon-512.png").convert("RGBA").resize((220, 220), Image.LANCZOS)
f1 = card([]); f1.alpha_composite(icon, (850, 330)); ImageDraw.Draw(f1).text((960 - ImageDraw.Draw(f1).textlength("Bramley", font=F(72)) / 2, 590), "Bramley", font=F(72), fill="white")
frames = [
    ("1  Logo, 2 s", f1),
    ("2  The device, Continue Playing, 3 s", stage(dev, hero)),
    ("3  Title card, 1.5 s", card(["Everything you play.", "One calm place."])),
    ("4  Camera moves in: the consoles row, 3 s", zoom(dev, home, None)),
    ("5  A library, 2.5 s", stage(dev, lib, dx=0)),
    ("6  Title card, 1.5 s", card(["Music that", "follows you."])),
    ("7  Now Playing, 3 s", stage(dev, np_)),
    ("8  Settings, 2.5 s", stage(dev, settings)),
    ("9  End card, 3 s", card(["Bramley"], "Free for the TrimUI Brick and Brick Hammer.  github.com/howardhwt/bramley")),
]
T = 640; th = 360
S = Image.new("RGB", (3 * T + 40, 3 * (th + 50)), (24, 24, 26)); dd = ImageDraw.Draw(S)
for i, (lab, im) in enumerate(frames):
    x, y = (i % 3) * (T + 20), (i // 3) * (th + 50)
    S.paste(im.convert("RGB").resize((T, th), Image.LANCZOS), (x, y + 40)); dd.text((x + 6, y + 8), lab, font=F(24, "SemiBold"), fill="white")
S.save(sys.argv[3])
