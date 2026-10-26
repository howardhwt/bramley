#!/usr/bin/env python3
"""Draws a demo cover for screenshots: a diagonal two-colour gradient with a title in Inter.
Needs Pillow.  cover.py out.png width height 0xRRGGBB 0xRRGGBB "Title" font.ttf"""
import sys
from PIL import Image, ImageDraw, ImageFont

out, w, h, c0, c1, title, font_path = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5], sys.argv[6], sys.argv[7]
a = tuple(int(c0[2:][i:i + 2], 16) for i in (0, 2, 4))
b = tuple(int(c1[2:][i:i + 2], 16) for i in (0, 2, 4))
img = Image.new("RGB", (w, h))
px = img.load()
for y in range(h):
    for x in range(w):
        t = (x / w + y / h) / 2
        px[x, y] = tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))
draw = ImageDraw.Draw(img)
font = ImageFont.truetype(font_path, max(12, w // 9))
words, lines, line = title.split(), [], ""
for word in words:
    trial = (line + " " + word).strip()
    if draw.textlength(trial, font=font) > w * 0.84 and line:
        lines.append(line)
        line = word
    else:
        line = trial
lines.append(line)
# Centred, so the title survives the crop when tall box art fills a wide card.
y = (h - font.size * 1.15 * len(lines)) / 2
for ln in lines:
    tw = draw.textlength(ln, font=font)
    draw.text(((w - tw) / 2, y), ln, font=font, fill=(255, 255, 255))
    y += font.size * 1.15
img.save(out)
