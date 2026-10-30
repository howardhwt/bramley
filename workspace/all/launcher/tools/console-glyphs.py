#!/usr/bin/env python3
"""Renders Bramley's console and app icons for Home into res/brand/consoles/ as PNGs.

Each system without an icon of its own shows a tile in the style of an iOS widget: an app icon
(the Settings icon style, from tools/settings-icons.py) with a white symbol on one of Apple's
dark-mode system colours, then the system's name and game count, drawn by the launcher.

The symbols are generic, never a company's logo: a controller, a disc, a joystick and a keyboard
from Material Symbols Rounded (Apache 2.0, Google, downloaded at a pinned commit and checked), and
a handheld drawn here. Design: spikes/console-tiles (V1 widget, handheld D4 widest).

Needs Pillow and numpy:  python3 tools/console-glyphs.py
Writes <TAG>-80.png (the tile) and <TAG>-200.png (the large preview) for every system below,
plus default-80.png and default-200.png for any other folder, and app-<name>-80.png and -200.png
for Favorites, other collections, Music, Activity, Files, Settings and any other Pak (a star, a
list, a note, a chart, a folder and an apps grid from Material Symbols; Settings' gear is the one
tools/settings-icons.py draws).
"""
import hashlib
import importlib.util
import os
import urllib.request

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "res", "brand", "consoles")
CACHE = os.path.join(HERE, "..", "build", "cache")
MATERIAL_COMMIT = "10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7"
MATERIAL_URL = ("https://raw.githubusercontent.com/google/material-design-icons/" + MATERIAL_COMMIT +
                "/variablefont/MaterialSymbolsRounded%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf")
MATERIAL_SHA256 = "7e74e36533ccee12c2b81e75bfcf3ccf123d3bb7e7e46d65d94445f23db0c97c"
SIZES = (80, 200)

spec = importlib.util.spec_from_file_location("settings_icons", os.path.join(HERE, "settings-icons.py"))
si = importlib.util.module_from_spec(spec)
spec.loader.exec_module(si)

# Apple's dark-mode system colours (published values, not assets)
RED, ORANGE, YELLOW, GREEN, MINT = (255, 69, 58), (255, 159, 10), (255, 214, 10), (48, 209, 88), (99, 230, 190)
TEAL, CYAN, BLUE, INDIGO, PURPLE = (64, 200, 224), (100, 210, 255), (10, 132, 255), (94, 92, 230), (191, 90, 242)
PINK, BROWN, GRAY = (255, 55, 95), (172, 142, 104), (142, 142, 147)

# folder tag: kind of device, colour
SYSTEMS = {
    "FC": ("console", RED), "FDS": ("disc", RED), "SFC": ("console", INDIGO), "SUPA": ("console", INDIGO),
    "GB": ("handheld", GREEN), "GBC": ("handheld", TEAL), "GBA": ("handheld", PURPLE), "MGBA": ("handheld", PURPLE),
    "SGB": ("handheld", MINT), "VB": ("handheld", RED), "PKM": ("handheld", YELLOW),
    "MD": ("console", BLUE), "SMS": ("console", BLUE), "SG1000": ("console", BLUE), "32X": ("console", BLUE),
    "GG": ("handheld", CYAN), "SEGACD": ("disc", BLUE), "PS": ("disc", GRAY), "PCE": ("console", ORANGE),
    "NGP": ("handheld", MINT), "NGPC": ("handheld", TEAL), "FBN": ("arcade", RED),
    "A2600": ("console", BROWN), "A5200": ("console", BROWN), "A7800": ("console", BROWN), "LYNX": ("handheld", ORANGE),
    "C64": ("computer", BROWN), "C128": ("computer", BROWN), "VIC": ("computer", BROWN), "PET": ("computer", BROWN),
    "PLUS4": ("computer", BROWN), "PUAE": ("computer", GRAY), "CPC": ("computer", GRAY), "MSX": ("computer", GRAY),
    "COLECO": ("console", ORANGE), "P8": ("handheld", PINK), "PRBOOM": ("computer", RED),
}


def material_font():
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, "MaterialSymbolsRounded-" + MATERIAL_COMMIT[:12] + ".ttf")
    if not os.path.exists(path):
        urllib.request.urlretrieve(MATERIAL_URL, path + ".part")
        os.rename(path + ".part", path)
    if hashlib.sha256(open(path, "rb").read()).hexdigest() != MATERIAL_SHA256:
        raise SystemExit("Material Symbols download does not match the pinned hash")
    return path


def material(path, code):
    """A filled, weight 500, 48 pt Material Symbols Rounded glyph as a mask."""
    def draw(S):
        f = ImageFont.truetype(path, S)
        values = []
        for axis in f.get_variation_axes():
            name = axis["name"].decode() if isinstance(axis["name"], bytes) else axis["name"]
            values.append({"Fill": 1, "FILL": 1, "Weight": 500, "wght": 500, "Grade": 0, "GRAD": 0,
                           "Optical size": 48, "opsz": 48}.get(name, axis["default"]))
        f.set_variation_by_axes(values)
        m = Image.new("L", (S * 2, S * 2), 0)
        ImageDraw.Draw(m).text((S // 2, S // 2), chr(code), font=f, fill=255)
        return m.crop(m.getbbox())
    return draw


def handheld(S):
    """A handheld in the proportions of the first pocket consoles: a screen in a bezel, a d-pad,
    two buttons and two small keys, with a larger curve at the bottom right."""
    m, d, p = si.canvas(S)
    r = lambda v: S * v
    x0, x1, y0, y1 = 0.17, 0.83, 0.05, 0.95
    R = 0.17
    d.rounded_rectangle(p(x0, y0, x1, y1), radius=r(0.05), fill=255)
    d.rectangle(p(x1 - R, y1 - R, x1, y1), fill=0)
    d.pieslice(p(x1 - 2 * R, y1 - 2 * R, x1, y1), 0, 90, fill=255)
    d.rounded_rectangle(p(x0 + 0.055, 0.10, x1 - 0.055, 0.46), radius=r(0.05), fill=0)
    d.rounded_rectangle(p(x0 + 0.10, 0.145, x1 - 0.10, 0.415), radius=r(0.025), fill=255)
    cx, cy, s, t = x0 + 0.135, 0.62, 0.065, 0.025
    d.rounded_rectangle(p(cx - s, cy - t, cx + s, cy + t), radius=r(t * 0.9), fill=0)
    d.rounded_rectangle(p(cx - t, cy - s, cx + t, cy + s), radius=r(t * 0.9), fill=0)
    d.ellipse(p(x1 - 0.20, 0.61, x1 - 0.135, 0.675), fill=0)
    d.ellipse(p(x1 - 0.14, 0.55, x1 - 0.075, 0.615), fill=0)
    d.rounded_rectangle(p(0.40, 0.75, 0.47, 0.775), radius=r(0.012), fill=0)
    d.rounded_rectangle(p(0.51, 0.75, 0.58, 0.775), radius=r(0.012), fill=0)
    return si.soften(m, S)


def main():
    si.FONT_BYTES = si.phosphor_fill_font()
    font = material_font()
    symbols = {
        "handheld": handheld,
        "console": material(font, 0xF135),   # stadia_controller
        "disc": material(font, 0xE019),      # album
        "arcade": material(font, 0xF5EE),    # joystick
        "computer": material(font, 0xE312),  # keyboard
    }
    os.makedirs(OUT, exist_ok=True)
    for tag, (kind, colour) in list(SYSTEMS.items()) + [("default", ("console", GRAY))]:
        for size in SIZES:
            path = os.path.join(OUT, f"{tag}-{size}.png")
            si.tile(colour, symbols[kind], size).save(path, optimize=True)
    apps = {
        "favorites": (YELLOW, material(font, 0xF09A)),   # star
        "collection": (BLUE, material(font, 0xE241)),    # format_list_bulleted
        "music": (PINK, material(font, 0xE405)),         # music_note
        "activity": (ORANGE, material(font, 0xE26B)),    # bar_chart
        "files": (BLUE, material(font, 0xE2C7)),         # folder
        "settings": (GRAY, si.gear),                     # Settings' own gear
        "default": (GRAY, material(font, 0xE5C3)),       # apps
    }
    for name, (colour, symbol) in apps.items():
        for size in SIZES:
            si.tile(colour, symbol, size).save(os.path.join(OUT, f"app-{name}-{size}.png"), optimize=True)
    print(f"wrote {len(SYSTEMS) + 1} console and {len(apps)} app icons in {len(SIZES)} sizes to {OUT}")


if __name__ == "__main__":
    main()
