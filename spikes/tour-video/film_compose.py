# Composes the launch film: film.py's transparent shots over the background, with a soft shadow,
# the titles (rise 10 px and resolve from blur, design-to-motion B2) and the music. Not shipped.
#   python film_compose.py <shots-dir> <music.mp3> <out.mp4> <fps>
import os, sys, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SHOTS, MUSIC, OUT, FPS = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
R = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/res/"
F = lambda w, px: ImageFont.truetype(R + f"Inter-{w}.ttf", px)
names = sorted(os.listdir(SHOTS))
W, H = Image.open(os.path.join(SHOTS, names[0], "0001.png")).size
u = W / 1920                                    # sizes below are for 1920 x 1080
INK, SUB = (29, 29, 31), (110, 110, 115)

# titles: shot -> list of (text, weight, size, colour), anchor ("left" | "right" | "bottom"), delay
TITLES = {
    "s02": ([("Bramley.", "Bold", 110, INK)], "right", 2.2),
    "s04": ([("Everything you play,", "Bold", 64, INK), ("in one place.", "Bold", 64, INK)], "left", 0.4),
    "s06": ([("Press A.", "Bold", 76, INK), ("You're in.", "Bold", 76, INK)], "right", 0.4),
    "s07": ([("Music that", "Bold", 72, INK), ("follows you.", "Bold", 72, INK),
             ("SELECT opens Now Playing from anywhere.", "SemiBold", 28, SUB)], "left", 0.4),
    "s08": ([("Every detail,", "Bold", 64, INK), ("considered.", "Bold", 64, INK)], "left", 0.4),
    "s10": ([("Bramley", "Bold", 96, INK), ("For TrimUI Brick and Brick Hammer.", "SemiBold", 32, SUB),
             ("github.com/howardhwt/bramley", "SemiBold", 28, (0, 113, 227))], "right", 0.6),
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

bg_white = np.array([245, 245, 247], np.float32)
music_len = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", MUSIC],
                                 capture_output=True, text=True).stdout)
enc = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
                        "-i", MUSIC, "-map", "0:v", "-map", "1:a", "-shortest", "-c:v", "libx264", "-preset", "medium", "-crf", "18",
                        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "256k", "-movflags", "+faststart", OUT], stdin=subprocess.PIPE)
total = 0
for name in names:
    files = sorted(f for f in os.listdir(os.path.join(SHOTS, name)) if f.endswith(".png"))
    tl = TITLES.get(name)
    layer = title_layer(*tl[:2]) if tl else None
    for i, fn in enumerate(files):
        t = i / FPS
        dev = Image.open(os.path.join(SHOTS, name, fn)).convert("RGBA")
        # the background: dark for the cold open, brightening to white as the screen wakes
        if name == "s01": b = 0.0
        elif name == "s02": b = ease(min(1.0, t / 1.6))   # brightens as the camera pulls back
        else: b = 1.0
        base = np.full((H, W, 3), bg_white * b + np.array([0, 0, 0], np.float32) * (1 - b), np.float32)
        frame = Image.fromarray(base.astype(np.uint8)).convert("RGBA")
        # a soft contact shadow from the device's own silhouette
        a = dev.getchannel("A")
        sh = a.filter(ImageFilter.GaussianBlur(int(28 * u))).point(lambda v: int(v * 0.22 * b))
        shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0)); shadow.putalpha(sh)
        frame.alpha_composite(shadow, (0, int(22 * u)))
        frame.alpha_composite(dev)
        if layer is not None:
            k = max(0.0, min(1.0, (t - tl[2]) / 0.75))
            if k > 0:
                lay = layer.filter(ImageFilter.GaussianBlur(4 * (1 - ease(k)))) if k < 1 else layer
                lay = lay.copy(); lay.putalpha(lay.getchannel("A").point(lambda v: int(v * ease(k))))
                frame.alpha_composite(lay, (0, int(10 * u * (1 - ease(k)))))
        enc.stdin.write(frame.convert("RGB").tobytes()); total += 1
enc.stdin.close(); enc.wait()
print("frames", total, "seconds", round(total / FPS, 2), "music", round(music_len, 2))
