# Apple-style tour video, 4:5 for Reddit, built on design-to-motion's rules: the device is a still
# surface for the whole film and only its screen and the headline change; it opens on the empty
# state (the device with a dark screen) and dissolves back to it, so the loop has no seam; text
# rises 10 px and resolves from a 4 px blur; no camera moves. Not shipped.
#   python compose.py device-image shots-dir out.mp4
# shots-dir holds home.mp4, consoles.mp4, activity.mp4, music.mp4 (60 fps), settings.png, aria.mp3.
import subprocess, sys, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
R = "/Users/howard/Documents/GitHub/brick-hammer-launcher/"
F = lambda px, w="Bold": ImageFont.truetype(R + f"workspace/all/launcher/res/Inter-{w}.ttf", px)
W, H, FPS = 1080, 1350, 60
SCREEN = (364, 157, 1139, 738)          # the display in the 1500 px device image
DEV_SCALE, DEV_TOP = 1.12, 330

exec(open(R + "spikes/tour-video/board.py").read().split("def device_with")[0])  # cutout()
D = sys.argv[2].rstrip("/") + "/"

# easing (design-to-motion B6)
def bez(x1, y1, x2, y2):
    def f(x):
        lo, hi = 0.0, 1.0
        for _ in range(30):
            t = (lo + hi) / 2
            bx = 3 * (1 - t) ** 2 * t * x1 + 3 * (1 - t) * t * t * x2 + t ** 3
            lo, hi = (t, hi) if bx < x else (lo, t)
        return 3 * (1 - t) ** 2 * t * y1 + 3 * (1 - t) * t * t * y2 + t ** 3
    return f
E, E2, IO = bez(0.16, 1, 0.3, 1), bez(0.33, 1, 0.68, 1), bez(0.65, 0, 0.35, 1)
clamp = lambda v: max(0.0, min(1.0, v))

# the still surfaces: background and device, placed once
dev = cutout(sys.argv[1])
dev = dev.resize((int(dev.width * DEV_SCALE), int(dev.height * DEV_SCALE)), Image.LANCZOS)
DX = (W - dev.width) // 2
SX, SY = DX + int(SCREEN[0] * DEV_SCALE), DEV_TOP + int(SCREEN[1] * DEV_SCALE)
SW, SH = int((SCREEN[2] - SCREEN[0]) * DEV_SCALE), int((SCREEN[3] - SCREEN[1]) * DEV_SCALE)
g = Image.new("L", (W, H), 0); ImageDraw.Draw(g).ellipse((40, 300, W - 40, H + 300), fill=70)
g = np.asarray(g.filter(ImageFilter.GaussianBlur(170)), np.float32)[..., None] / 255.0
base = Image.fromarray((g * np.array((96, 80, 210), np.float32)).astype(np.uint8), "RGB").convert("RGBA")
base.alpha_composite(dev, (DX, DEV_TOP))
dark = Image.new("RGB", (SW, SH), (0, 0, 0))

def text_layer(title, sub, title_size=62):
    t = Image.new("RGBA", (W, 300), (0, 0, 0, 0)); d = ImageDraw.Draw(t)
    f = F(title_size); w = d.textlength(title, font=f); d.text(((W - w) / 2, 130), title, font=f, fill="white")
    if sub:
        f = F(32, "Regular"); w = d.textlength(sub, font=f); d.text(((W - w) / 2, 222), sub, font=f, fill=(165, 165, 172))
    return t

def place_text(c, layer, t_in, t_out, t):
    """Rise 10 px and resolve from 4 px blur (1.05 s transform, 0.75 s opacity); leave faster."""
    if t < t_in or t > t_out + 0.45: return
    if t < t_out:
        k = t - t_in
        y = 10 * (1 - E(clamp(k / 1.05))); a = E2(clamp(k / 0.75)); b = 4 * (1 - a)
    else:
        k = clamp((t - t_out) / 0.45); y = -6 * IO(k); a = 1 - E2(k); b = 4 * k
    l = layer.filter(ImageFilter.GaussianBlur(b)) if b > 0.2 else layer
    l = l.copy(); l.putalpha(l.getchannel("A").point(lambda v: int(v * a)))
    c.alpha_composite(l, (0, int(round(y))))

def clip(src, start, secs):
    if src.endswith(".png"):
        im = Image.open(D + src).convert("RGB").resize((SW, SH), Image.LANCZOS)
        return [im] * int(secs * FPS)
    cmd = ["ffmpeg", "-v", "error", "-ss", str(start), "-t", str(secs), "-i", D + src, "-vf", f"fps={FPS},scale={SW}:{SH}:flags=lanczos",
           "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    raw = subprocess.run(cmd, capture_output=True).stdout
    n = SW * SH * 3
    return [Image.frombytes("RGB", (SW, SH), raw[i:i + n]) for i in range(0, len(raw) - n + 1, n)]

# the film: (screen source, start, seconds, title, line). Screens cross over 0.5 s.
SHOTS = [
    ("splash", 0, 3.0, "Bramley", "A calm, refined system for the TrimUI Brick."),
    ("home.mp4", 3.8, 5.0, "Pick up where you left off.", "Your last games, one press away."),
    ("consoles.mp4", 3.2, 5.4, "Every console, at a glance.", "And when you last played it."),
    ("activity.mp4", 7.3, 4.8, "See where your time goes.", "Time played, by day and by game."),
    ("music.mp4", 12.6, 5.4, "Music that follows you.", "SELECT opens Now Playing from anywhere."),
    ("settings.png", 0, 4.4, "Settings you already know.", "A split view, like iPad."),
    ("home-end", 0, 4.2, "Free, and never sold.", "github.com/howardhwt/bramley"),
]
EMPTY, LOAD, XF, DISSOLVE = 0.5, 0.9, 0.5, 0.6
splash = Image.open(R + "docs/brand/bramley-splash.png").convert("RGB").resize((SW, SH), Image.LANCZOS)
screens = []
for src, start, secs, title, sub in SHOTS:
    if src == "splash": fr = [splash] * int(secs * FPS)
    elif src == "home-end": fr = clip("consoles.mp4", 11.0, 0.1)[:1] * int(secs * FPS)
    else: fr = clip(src, start, secs)
    screens.append((fr, text_layer(title, sub, 84 if title == "Bramley" else 62)))

starts, t0 = [], EMPTY
for (fr, _), s in zip(screens, SHOTS):
    starts.append(t0); t0 += s[2]
END = t0; TOTAL = END + DISSOLVE + EMPTY

out = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
                        "-i", "-", "-i", D + "aria.mp3", "-filter_complex",
                        f"[1:a]atrim=0:{TOTAL},afade=t=in:d=1.5,afade=t=out:st={TOTAL - 1.8}:d=1.8[a]",
                        "-map", "0:v", "-map", "[a]", "-c:v", "libx264", "-preset", "slow", "-crf", "15",
                        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", sys.argv[3]],
                       stdin=subprocess.PIPE)

def screen_at(t):
    """The screen's picture: dark when empty, each shot loading in from blur, crossing to the next."""
    if t < EMPTY or t >= END + DISSOLVE: return dark
    i = max(j for j in range(len(starts)) if starts[j] <= t) if t < END else len(starts) - 1
    fr, _ = screens[i]
    cur = fr[min(len(fr) - 1, int((t - starts[i]) * FPS))]
    if i == 0 and t - EMPTY < LOAD:                       # first load: from dark, resolving from blur
        k = E2(clamp((t - EMPTY) / LOAD))
        return Image.blend(dark, cur.filter(ImageFilter.GaussianBlur(14 * (1 - k))), k)
    if i > 0 and t - starts[i] < XF:                       # cross from the previous screen
        pf, _ = screens[i - 1]; prev = pf[-1]
        k = IO(clamp((t - starts[i]) / XF))
        return Image.blend(prev, cur.filter(ImageFilter.GaussianBlur(4 * (1 - k))) if k < 1 else cur, k)
    if t >= END:                                           # dissolve back to the empty state
        k = E2(clamp((t - END) / DISSOLVE))
        return Image.blend(cur, dark, k)
    return cur

frames = int(round(TOTAL * FPS))
for f in range(frames):
    t = f / FPS
    c = base.copy()
    c.paste(screen_at(t), (SX, SY))
    for i, (s, (fr, layer)) in enumerate(zip(SHOTS, screens)):
        t_in = starts[i] + (0.35 if i == 0 else 0.15)
        t_out = (starts[i + 1] - 0.3) if i + 1 < len(starts) else END
        place_text(c, layer, t_in, t_out, t)
    out.stdin.write(c.convert("RGB").tobytes())
out.stdin.close(); out.wait()
print(f"{frames} frames, {TOTAL:.1f} s at {FPS} fps")
