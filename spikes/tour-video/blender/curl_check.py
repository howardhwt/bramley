# Blockout check for the R1 curl: the model's straight-back orthographic render, with the curve traced
# from Howard's close-up drawn over it in red (the lip's underside) and the photo's crop beside it.
import sys
from PIL import Image, ImageDraw, ImageEnhance
W = 0.072; H = W * 1.515; D2 = W * 0.254
LEDGE = H * (1 - 0.454); LED_H = 0.0028
h = H * 0.080; base = LEDGE + LED_H; z_lip = base + h * 0.58; face = z_lip - base; tc = base + h - z_lip
CURL = [(0.0, 0.62), (0.02, 0.48), (0.06, 0.33), (0.12, 0.24), (0.30, 0.12), (0.60, 0.04), (1.0, 0.0)]
def drop(u):
    for (ua, va), (ub, vb) in zip(CURL, CURL[1:]):
        if u <= ub: return va + (vb - va) * (u - ua) / (ub - ua)
    return 0.0
fx = lambda u: (u - 0.5) * W
xo, xi = fx(1.0), fx(1 - 0.236)          # R1: bu(0) and bu(0.236)
wid = xo - xi
scale = 1400 / (H * 1.25)
px = lambda x: 700 - x * scale           # the back view looks along -y, so x runs right to left
pz = lambda z: 700 - (z - H / 2) * scale
im = Image.open(sys.argv[1]).convert("RGBA"); bg = Image.new("RGBA", im.size, (255, 255, 255, 255)); bg.alpha_composite(im)
d = ImageDraw.Draw(bg)
pts = [(px(xo - u * wid), pz(base + h - tc - drop(u) * face)) for u in [i / 80 for i in range(81)]]
d.line(pts, fill=(255, 0, 0, 255), width=2)
pts = [(px(xo - u * wid), pz(base + h - drop(u) * face)) for u in [i / 80 for i in range(81)]]
d.line(pts, fill=(255, 120, 0, 255), width=2)
x0, x1 = px(xo) - 40, px(xi) + 40; z0, z1 = pz(base + h) - 40, pz(LEDGE) + 40
crop = bg.crop((int(x0), int(z0), int(x1), int(z1))).convert("RGB")
crop = crop.resize((int(crop.width * 500 / crop.height), 500))
ref = Image.open(sys.argv[2]).convert("RGB").crop((100, 1700, 1250, 2300)); ref = ImageEnhance.Brightness(ref).enhance(1.8)
ref = ref.resize((int(ref.width * 500 / ref.height), 500))
out = Image.new("RGB", (crop.width + ref.width + 20, 500), (255, 255, 255)); out.paste(ref, (0, 0)); out.paste(crop, (ref.width + 20, 0))
out.save(sys.argv[3])
