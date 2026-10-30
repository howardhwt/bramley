# The hero panel for a focused console with Shortcuts tiles: H0 the symbol centred (S1 as shown),
# H1 the symbol top left as on the small tile, H2 the console's own box art on its colour, like
# the App Library's folders show what is inside. Not shipped.
import importlib.util, sys
from PIL import Image, ImageDraw, ImageFilter
spec = importlib.util.spec_from_file_location("m", "mock.py")
sys.argv = [sys.argv[0], "/Users/howard/.claude/jobs/55a591bc/tmp/cur-home.png", "/tmp/_unused.png"]
src = open("mock.py").read().split("cur = Image.open")[0]
exec(src)
ART = "/Users/howard/Documents/GitHub/brick-hammer-launcher/spikes/demo-library/art/"

def hero_h1(name, kind, col):
    t = gradient(498, 338, col)
    g = glyph_mask(kind, 84); w = Image.new("RGBA", g.size, (255, 255, 255, 255)); w.putalpha(g)
    t.alpha_composite(w, (30, 28))
    return rounded(t, 28)

def hero_h2(name, kind, col, arts):
    t = gradient(498, 338, col)
    g = glyph_mask(kind, 44); w = Image.new("RGBA", g.size, (255, 255, 255, 255)); w.putalpha(g)
    t.alpha_composite(w, (26, 24))
    ims = [Image.open(ART + a).convert("RGBA") for a in arts]
    H = min(220, int((498 - 68 - 18 * (len(ims) - 1)) / sum(i.width / i.height for i in ims)))
    covers = [rounded(im.resize((round(im.width * H / im.height), H), Image.LANCZOS), 12) for im in ims]
    total = sum(c.width for c in covers) + 18 * (len(covers) - 1)
    x = 498 - 34 - total
    for c in covers:
        s = Image.new("RGBA", t.size, (0, 0, 0, 0))
        ImageDraw.Draw(s).rounded_rectangle([x, 300 - H + 8, x + c.width, 300 + 8], 12, fill=(0, 0, 0, 120))
        t.alpha_composite(s.filter(ImageFilter.GaussianBlur(10)))
        t.alpha_composite(c, (x, 300 - H)); x += c.width + 18
    return rounded(t, 28)

def page(hero):
    c = Image.fromarray(__import__("numpy").asarray(home(True).convert("RGBA")))
    bg = Image.open(L + "res/wallpapers/bloom-blur.jpg").convert("RGBA").resize((1024, 768))
    c.alpha_composite(bg.crop((40, 70, 570, 440)), (40, 70))   # clear the old hero
    shadow(c, (57, 81, 555, 419), 28, 120, 24, 14)
    c.alpha_composite(hero, (57, 81))
    return c.convert("RGB")

name, kind, col, n = ROW[FOCUS]
h0 = home(True)
h1 = page(hero_h1(name, kind, col))
h2 = page(hero_h2(name, kind, col, ["libbet.png", "tobu-dx.png"]))
W = Image.new("RGB", (1024 * 3 + 40, 768 + 60), (20, 20, 22)); dd = ImageDraw.Draw(W)
for i, (im, lab) in enumerate([(h0, "H0: symbol centred (S1)"), (h1, "H1: symbol top left, like the tile"), (h2, "H2: its games on its colour")]):
    W.paste(im, (i * 1044, 60)); dd.text((i * 1044 + 10, 14), lab, font=F(30), fill="white")
W.save("shots/hero.png")
