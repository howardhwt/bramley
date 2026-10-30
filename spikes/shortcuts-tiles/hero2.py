# Four more ideas for Home when a console is focused, with Shortcuts tiles. Not shipped.
#  H3 tvOS Top Shelf: no panel; the top of the screen shows the console's games and takes its colour
#  H4 Top Shelf inset: the last game played on that console, wide, with Continue
#  H5 no hero: Home is a grid of tiles like the Shortcuts app, games in a row above
#  H6 large widget: the focused tile drawn big, name and count inside it, nothing beside
import sys, numpy as np
from PIL import Image, ImageDraw, ImageFilter
sys.argv = [sys.argv[0], "/Users/howard/.claude/jobs/55a591bc/tmp/cur-home.png", "/tmp/_unused.png"]
exec(open("mock.py").read().split("cur = Image.open")[0])
ART = "/Users/howard/Documents/GitHub/brick-hammer-launcher/spikes/demo-library/art/"
name, kind, col, n = ROW[FOCUS]

def base():
    c = Image.open(L + "res/wallpapers/bloom-blur.jpg").convert("RGBA").resize((1024, 768))
    d = ImageDraw.Draw(c)
    d.text((808, 24), "14:32", font=F(19), fill="white"); d.text((880, 25), "100%", font=F(18, "Regular"), fill=(255, 255, 255, 200))
    return c

def row(c, y, label="Consoles", items=ROW, focus=FOCUS):
    ImageDraw.Draw(c).text((58, y - 33), label, font=F(19), fill=(255, 255, 255, 220))
    x = 57
    for i, (nm, k, cl, s) in enumerate(items):
        t = shortcut(nm, k, cl, s)
        if i == focus:
            t = t.resize((226, 135), Image.LANCZOS); shadow(c, (x - 8, y - 7, x + 218, y + 128), 20, 140, 16, 10); c.alpha_composite(t, (x - 8, y - 7))
        else:
            c.alpha_composite(t, (x, y))
        x += 232

def cover(path, h, r=12):
    im = Image.open(ART + path).convert("RGBA")
    return rounded(im.resize((round(im.width * h / im.height), h), Image.LANCZOS), r)

def h3():
    c = base()
    tint = Image.new("RGBA", c.size, (0, 0, 0, 0))
    y, x = np.mgrid[0:768, 0:1024].astype(np.float32)
    a = np.clip(1 - y / 470, 0, 1) * 150
    tint = Image.fromarray(np.dstack([np.full_like(a, col[0] * 0.55), np.full_like(a, col[1] * 0.55), np.full_like(a, col[2] * 0.55), a]).astype(np.uint8), "RGBA")
    c.alpha_composite(tint)
    x = 57
    for p in ["tobu-dx.png", "libbet.png"]:
        cv = cover(p, 290); shadow(c, (x, 90, x + cv.width, 380), 12, 130, 18, 10); c.alpha_composite(cv, (x, 90)); x += cv.width + 24
    d = ImageDraw.Draw(c)
    d.text((58, 398), f"{name}  ·  {n}", font=F(18), fill=(255, 255, 255, 190))
    row(c, 535); return c

def h4():
    c = base()
    art = Image.open(ART + "tobu-dx.png").convert("RGBA")
    W, H = 910, 330
    k = max(W / art.width, H / art.height)
    big = art.resize((round(art.width * k), round(art.height * k)), Image.LANCZOS).filter(ImageFilter.GaussianBlur(18))
    big = big.crop(((big.width - W) // 2, (big.height - H) // 2, (big.width - W) // 2 + W, (big.height - H) // 2 + H))
    big = Image.alpha_composite(big, Image.new("RGBA", big.size, (0, 0, 0, 90)))
    cv = cover("tobu-dx.png", 270); big.alpha_composite(cv, (30, 30))
    dd = ImageDraw.Draw(big)
    dd.text((330, 70), "GAME BOY  ·  LAST PLAYED", font=F(16), fill=(255, 255, 255, 170))
    dd.text((330, 98), "Tobu Tobu Girl Deluxe", font=F(38, "Bold"), fill="white")
    dd.text((330, 150), "Yesterday  ·  42 minutes", font=F(19, "Regular"), fill=(255, 255, 255, 190))
    pill = Image.new("RGBA", big.size, (0, 0, 0, 0)); ImageDraw.Draw(pill).rounded_rectangle((330, 250, 480, 292), 21, fill=(255, 255, 255, 235)); big.alpha_composite(pill)
    dd.text((350, 259), "A  Continue", font=F(18), fill=(20, 20, 24))
    big = rounded(big, 24); shadow(c, (57, 81, 57 + W, 81 + H), 24, 120, 22, 12); c.alpha_composite(big, (57, 81))
    row(c, 535); return c

def h5():
    c = base(); d = ImageDraw.Draw(c)
    d.text((57, 22), "Home", font=F(34, "Bold"), fill="white")
    d.text((58, 82), "Continue Playing", font=F(19), fill=(255, 255, 255, 220))
    x = 57
    for p in ["tobu-dx.png", "ucity.png", "freedm.png", "libbet.png", "thwaite-title.png"]:
        cv = cover(p, 130); c.alpha_composite(cv, (x, 115)); x += cv.width + 16
    items = [(nm, k, cl, s) for nm, k, cl, s in ROW[:4]] + [("NES", "console", cg.RED, "3 games"), ("PlayStation", "disc", cg.GRAY, "4 games"),
             ("Super NES", "console", cg.INDIGO, "6 games"), ("Arcade", "arcade", cg.RED, "9 games")]
    d.text((58, 278), "Consoles", font=F(19), fill=(255, 255, 255, 220))
    for i, (nm, k, cl, s) in enumerate(items):
        x, y = 57 + (i % 4) * 232, 311 + (i // 4) * 145
        t = shortcut(nm, k, cl, s)
        if i == FOCUS:
            t = t.resize((226, 135), Image.LANCZOS); shadow(c, (x - 8, y - 7, x + 218, y + 128), 20, 140, 16, 10); c.alpha_composite(t, (x - 8, y - 7))
        else: c.alpha_composite(t, (x, y))
    d.text((58, 620), "Apps", font=F(19), fill=(255, 255, 255, 220))
    apps = [("Music", "music", cg.PINK, None), ("Favorites", "star", cg.YELLOW, None)]
    for i, (nm, k, cl, s) in enumerate(apps):
        c.alpha_composite(shortcut(nm, k, cl, s).crop((0, 0, 210, 100)), (57 + i * 232, 653))
    return c

def h6():
    c = base()
    W, H = 600, 338
    t = gradient(W, H, col); d = ImageDraw.Draw(t)
    g = glyph_mask(kind, 70); w = Image.new("RGBA", g.size, (255, 255, 255, 255)); w.putalpha(g); t.alpha_composite(w, (30, 28))
    d.text((30, H - 112), name, font=F(52, "Bold"), fill="white")
    d.text((32, H - 52), n + "  ·  last played yesterday", font=F(22, "Regular"), fill=(255, 255, 255, 200))
    t = rounded(t, 30); shadow(c, (57, 81, 57 + W, 81 + H), 30, 120, 24, 14); c.alpha_composite(t, (57, 81))
    row(c, 535); return c

ims = [(h3(), "H3: Apple TV Top Shelf, its games"), (h4(), "H4: Top Shelf inset, last game played"),
       (h5(), "H5: no hero, a grid like the Shortcuts app"), (h6(), "H6: the tile itself, large")]
S = Image.new("RGB", (1024 * 2 + 20, (768 + 60) * 2), (20, 20, 22)); dd = ImageDraw.Draw(S)
for i, (im, lab) in enumerate(ims):
    x, y = (i % 2) * 1044, (i // 2) * 828
    S.paste(im.convert("RGB"), (x, y + 60)); dd.text((x + 10, y + 14), lab, font=F(30), fill="white")
S.save("shots/hero2.png")
