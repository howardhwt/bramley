# H6 with Game Boy in other Apple system colours. Not shipped.
import sys
from PIL import Image, ImageDraw
exec(open("hero2.py").read().split("ims = [")[0])
OPTS = [("Mint", cg.MINT), ("Indigo", cg.INDIGO), ("Gray", cg.GRAY), ("Cyan", cg.CYAN)]
S = Image.new("RGB", (1024 * 2 + 20, (768 + 60) * 2), (20, 20, 22)); dd = ImageDraw.Draw(S)
for i, (lab, c2) in enumerate(OPTS):
    col = c2
    ROW[1] = ("Game Boy", "handheld", c2, "2 games")
    im = h6()
    x, y = (i % 2) * 1044, (i // 2) * 828
    S.paste(im.convert("RGB"), (x, y + 60)); dd.text((x + 10, y + 14), lab, font=F(30), fill="white")
S.save("shots/h6-colours.png")
