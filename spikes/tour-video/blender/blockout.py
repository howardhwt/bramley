# Blockout check: lines up each orthographic render of the model with TrimUI's matching flat drawing
# and draws both outlines, TrimUI's in red and the model's in cyan. Not shipped.
#   python blockout.py <ortho-sheet.png> <render-prefix> <out.png>
import sys, numpy as np
from PIL import Image, ImageFilter, ImageDraw, ImageOps
sheet = Image.open(sys.argv[1]).convert("RGB"); prefix = sys.argv[2]
# the six views on TrimUI's sheet (1280 x 1012), boxes measured by occupancy
REF = {"front": (185, 166, 623, 829), "back": (818, 167, 1254, 827), "right": (656, 168, 785, 825),
       "left": (26, 168, 154, 827), "top": (188, 28, 623, 139), "bottom": (187, 855, 623, 986)}
def edges(mask):
    m = Image.fromarray((mask * 255).astype(np.uint8))
    e = np.asarray(m.filter(ImageFilter.FIND_EDGES)) > 40
    return np.asarray(Image.fromarray((e * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
def detail_edges(img):
    g = np.asarray(img.convert("L").filter(ImageFilter.FIND_EDGES)).astype(int)
    return g > 28
tiles = []
for view, box in REF.items():
    r = Image.open(f"{prefix}-ortho-{view}.png").convert("RGBA")
    a = np.asarray(r)[..., 3] > 20
    ys, xs = np.where(a); r = r.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
    model = Image.new("RGB", r.size, (255, 255, 255)); model.paste(r, (0, 0), r)
    ref = sheet.crop(box).resize(r.size, Image.LANCZOS)
    mm = np.asarray(r)[..., 3] > 20
    best = None
    for flip in (False, True):
        rr = ImageOps.mirror(ref) if flip else ref
        rm = np.asarray(rr).min(2) < 235
        iou = (mm & rm).sum() / max(1, (mm | rm).sum())
        if best is None or iou > best[0]: best = (iou, rr, rm, flip)
    iou, rr, rm, flip = best
    # left: the reference with the model's outline (cyan); right: the model with TrimUI's lines (red)
    a1 = np.asarray(rr).copy(); a2 = np.asarray(model).copy()
    a1[edges(mm) | (detail_edges(model) & mm)] = (0, 200, 255)
    a2[edges(rm) | (detail_edges(rr) & rm)] = (255, 40, 40)
    t = Image.new("RGB", (r.width * 2 + 10, r.height + 40), (255, 255, 255))
    t.paste(Image.fromarray(a1), (0, 40)); t.paste(Image.fromarray(a2), (r.width + 10, 40))
    ImageDraw.Draw(t).text((4, 6), f"{view}  overlap {iou:.3f}{'  (flipped)' if flip else ''}", fill=(0, 0, 0))
    tiles.append(t)
H = max(t.height for t in tiles)
tiles = [t.resize((int(t.width * 700 / t.height), 700)) if t.height > 400 else t for t in tiles]
row1 = [t for t in tiles if t.height == 700]; row2 = [t for t in tiles if t.height != 700]
W = Image.new("RGB", (max(sum(t.width for t in row1), sum(t.width for t in row2)) + 60, 700 + max(t.height for t in row2) + 30), (255, 255, 255))
x = 0
for t in row1: W.paste(t, (x, 0)); x += t.width + 15
x = 0
for t in row2: W.paste(t, (x, 715)); x += t.width + 15
W.save(sys.argv[3])
print("done")
