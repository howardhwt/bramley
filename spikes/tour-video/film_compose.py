# Composes the launch film: film.py's transparent shots over the background, with a soft shadow,
# the titles (rise 10 px and resolve from blur, design-to-motion B2) and the music. Not shipped.
#   python film_compose.py <shots-dir> <music.mp3> <out.mp4> <fps>
#   python film_compose.py <stills-dir> - <out-dir> <fps>     (stills: composes each frame film.py rendered, as PNGs)
import os, sys, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SHOTS, MUSIC, OUT, FPS = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
STILLS = MUSIC == "-"
SHOT_SECS = {"s01": 2.03, "s02": 4.09, "s03": 4.08, "s04": 3.80, "s05": 4.40, "s06": 4.00, "s07": 4.10, "s08": 4.10, "s09": 8.10, "s10": 4.10}
# the cold open was cut from two bars (4.24 s) to one (2.03 s); the music skips the same 2.21 s,
# so every later cut still lands on its beat
MUSIC_SKIP = 4.24 - SHOT_SECS["s01"]
END_FADE = 0.8                                  # the last shot fades to white over its final 0.8 s
R = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/res/"
F = lambda w, px: ImageFont.truetype(R + f"Inter-{w}.ttf", px)
names = sorted(d for d in os.listdir(SHOTS) if os.path.isdir(os.path.join(SHOTS, d)) and not d.startswith("_"))
first = sorted(f for f in os.listdir(os.path.join(SHOTS, names[0])) if f.endswith(".png"))[0]
W, H = Image.open(os.path.join(SHOTS, names[0], first)).size
u = W / 1920                                    # sizes below are for 1920 x 1080
# FILM_THEME=dark (default): white titles on black, as in the cold open, for every shot.
# FILM_THEME=light: dark grey titles on Apple's off-white, the earlier look.
DARK = os.environ.get("FILM_THEME", "dark") == "dark"
INK, SUB, LINK = ((245, 245, 247), (161, 161, 166), (41, 151, 255)) if DARK else ((29, 29, 31), (110, 110, 115), (0, 113, 227))
BG = (0, 0, 0) if DARK else (245, 245, 247)

# titles: shot -> list of (text, weight, size, colour), anchor ("left" | "right" | "bottom"), delay
TITLES = {
    "s02": ([("Bramley.", "Bold", 110, INK)], "right", 2.9),
    "s04": ([("Everything you play,", "Bold", 64, INK), ("in one place.", "Bold", 64, INK)], "left", 0.3),
    "s06": ([("Press A.", "Bold", 76, INK), ("You're in.", "Bold", 76, INK)], "right", 0.3),
    "s07": ([("Music that", "Bold", 72, INK), ("follows you.", "Bold", 72, INK),
             ("SELECT opens Now Playing from anywhere.", "SemiBold", 28, SUB)], "left", 0.3),
    "s08": ([("Every detail,", "Bold", 64, INK), ("considered.", "Bold", 64, INK)], "left", 0.3),
    "s10": ([("Bramley", "Bold", 96, INK), ("For TrimUI Brick and Brick Hammer.", "SemiBold", 32, SUB),
             ("github.com/howardhwt/bramley", "SemiBold", 28, LINK)], "right", 0.15),
}

def title_layer(lines, anchor):
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
    fonts = [F(w, int(px * u * 1.4)) for _, w, px, _ in lines]   # larger, so the words read on a phone
    heights = [int(px * u * 1.4 * 1.22) for _, _, px, _ in lines]
    total = sum(heights)
    if anchor == "right": x0, y = int(W * 0.56), (H - total) // 2
    elif anchor == "left": x0, y = int(W * 0.08), (H - total) // 2
    else: x0, y = int(W * 0.07), int(H * 0.86) - total
    for (t, _, _, col), f, h in zip(lines, fonts, heights):
        d.text((x0, y), t, font=f, fill=col); y += h
    return layer

def ease(t): return 1 - (1 - t) ** 3

bg_white = np.array(BG, np.float32)
if STILLS:
    os.makedirs(OUT, exist_ok=True)
else:
    film_len = sum(SHOT_SECS[n] for n in names)
    enc = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
                            "-ss", f"{MUSIC_SKIP:.3f}", "-i", MUSIC, "-map", "0:v", "-map", "1:a", "-shortest",
                            "-af", f"afade=t=out:st={film_len - 1.5:.2f}:d=1.5",   # the music fades with the picture
                            "-c:v", "libx264", "-preset", "medium", "-crf", "18",
                            "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "256k", "-movflags", "+faststart", OUT], stdin=subprocess.PIPE)
total = 0
for name in names:
    files = sorted(f for f in os.listdir(os.path.join(SHOTS, name)) if f.endswith(".png"))
    tl = TITLES.get(name)
    layer = title_layer(*tl[:2]) if tl else None
    for fn in files:
        t = (int(fn[:-4]) - 1) / FPS             # time within the shot, from the frame number
        dev = Image.open(os.path.join(SHOTS, name, fn)).convert("RGBA")
        # the background: dark for the cold open, brightening to white as the screen wakes
        if DARK: b = 1.0                               # black throughout, so the cold open flows straight on
        elif name == "s01": b = 0.0
        elif name == "s02": b = ease(min(1.0, t / 0.8))   # brightens quickly as the camera pulls back
        else: b = 1.0
        base = np.full((H, W, 3), bg_white * b + np.array([0, 0, 0], np.float32) * (1 - b), np.float32)
        frame = Image.fromarray(base.astype(np.uint8)).convert("RGBA")
        # a soft contact shadow from the device's own silhouette
        a = dev.getchannel("A")
        sh = a.filter(ImageFilter.GaussianBlur(int(28 * u))).point(lambda v: 0 if DARK else int(v * 0.22 * b))   # no shadow on black
        shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0)); shadow.putalpha(sh)
        frame.alpha_composite(shadow, (0, int(22 * u)))
        frame.alpha_composite(dev)
        if layer is not None:
            k = max(0.0, min(1.0, (t - tl[2]) / 0.75))
            if k > 0:
                lay = layer.filter(ImageFilter.GaussianBlur(4 * (1 - ease(k)))) if k < 1 else layer
                lay = lay.copy(); lay.putalpha(lay.getchannel("A").point(lambda v: int(v * ease(k))))
                frame.alpha_composite(lay, (0, int(10 * u * (1 - ease(k)))))
        if name == names[-1] and name == "s10":   # the end: fade to the background
            k = max(0.0, min(1.0, (t - (SHOT_SECS[name] - END_FADE)) / END_FADE))
            if k > 0:
                frame = Image.blend(frame, Image.new("RGBA", (W, H), (*BG, 255)), ease(k))
        if STILLS:
            frame.convert("RGB").save(os.path.join(OUT, f"{name}-{fn}")); total += 1; continue
        enc.stdin.write(frame.convert("RGB").tobytes()); total += 1
if not STILLS:
    enc.stdin.close(); enc.wait()
print("frames", total, "seconds", round(total / FPS, 2))
