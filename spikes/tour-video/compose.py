# Apple-style tour video, 4:5 for Reddit: each shot is a screen recording placed in the device,
# with a headline, a slow push in, fades through black and music. Not shipped.
#   python compose.py device.png-or-jpg shots-dir out.mp4
# shots-dir holds home.mp4, consoles.mp4, activity.mp4, music.mp4, settings.png and aria.mp3.
import subprocess, sys, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
R = "/Users/howard/Documents/GitHub/brick-hammer-launcher/"
F = lambda px, w="Bold": ImageFont.truetype(R + f"workspace/all/launcher/res/Inter-{w}.ttf", px)
W, H, FPS = 1080, 1350, 30
SCREEN = (364, 157, 1139, 738)          # the display in the 1500 px device image
DEV_SCALE, DEV_TOP = 1.12, 330          # device size and where its top edge sits
FADE = 0.35

sys.path.insert(0, R + "spikes/tour-video")
exec(open(R + "spikes/tour-video/board.py").read().split("def device_with")[0])  # cutout()
dev = cutout(sys.argv[1]); D = sys.argv[2].rstrip("/") + "/"
icon = Image.open(R + "docs/brand/bramley-icon-512.png").convert("RGBA")

SHOTS = [  # source, start, seconds, headline, glow colour
    ("home.mp4", 3.8, 5.2, "Pick up where you left off.", "Your last games, one press away.", (110, 70, 200)),
    ("consoles.mp4", 3.2, 5.6, "Every console, at a glance.", "And when you last played it.", (90, 90, 230)),
    ("activity.mp4", 7.3, 5.0, "See where your time goes.", "Time played, by day and by game.", (230, 140, 30)),
    ("music.mp4", 12.6, 5.6, "Music that follows you.", "SELECT opens Now Playing from anywhere.", (230, 60, 100)),
    ("settings.png", 0, 4.6, "Settings you already know.", "A split view, like iPad.", (120, 120, 130)),
]

def glow(colour):
    g = Image.new("L", (W, H), 0)
    ImageDraw.Draw(g).ellipse((40, 300, W - 40, H + 300), fill=70)
    g = g.filter(ImageFilter.GaussianBlur(170))
    a = np.asarray(g, np.float32)[..., None] / 255.0
    rgb = (a * np.array(colour, np.float32)).clip(0, 255).astype(np.uint8)
    return Image.fromarray(rgb, "RGB")

def headline(text, sub=None):
    t = Image.new("RGBA", (W, 300), (0, 0, 0, 0)); d = ImageDraw.Draw(t)
    f = F(62); w = d.textlength(text, font=f); d.text(((W - w) / 2, 130), text, font=f, fill="white")
    if sub:
        f = F(32, "Regular"); w = d.textlength(sub, font=f); d.text(((W - w) / 2, 218), sub, font=f, fill=(165, 165, 172))
    return t

def frames(src, start, secs):
    if src.endswith(".png"):
        im = Image.open(D + src).convert("RGB")
        for _ in range(int(secs * FPS)): yield im
        return
    cmd = ["ffmpeg", "-v", "error", "-ss", str(start), "-t", str(secs), "-i", D + src, "-vf", f"fps={FPS}",
           "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    n = 1024 * 768 * 3
    while True:
        b = p.stdout.read(n)
        if len(b) < n: break
        yield Image.frombytes("RGB", (1024, 768), b)
    p.wait()

out = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
                        "-i", "-", "-i", D + "aria.mp3", "-filter_complex", "[1:a]afade=t=in:d=1.2,apad[a]",
                        "-map", "0:v", "-map", "[a]", "-shortest", "-c:v", "libx264", "-preset", "slow", "-crf", "17",
                        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", sys.argv[3]],
                       stdin=subprocess.PIPE)
total = [0]

def emit(img, fade):
    a = np.asarray(img.convert("RGB"), np.float32) * fade
    out.stdin.write(a.clip(0, 255).astype(np.uint8).tobytes()); total[0] += 1

def ease(x): return 1 - (1 - x) ** 3

def card(lines, secs, big_icon=True):
    n = int(secs * FPS)
    for i in range(n):
        t = i / FPS
        c = Image.new("RGB", (W, H), (0, 0, 0)); d = ImageDraw.Draw(c)
        k = min(1, t / 0.6); k2 = min(1, max(0, (secs - t) / 0.5))
        y = 520
        if big_icon:
            ic = icon.resize((190, 190), Image.LANCZOS)
            c.paste(ic, ((W - 190) // 2, y - 210 + int((1 - ease(k)) * 16)), ic)
        for txt, size, weight, col in lines:
            f = F(size, weight); w = d.textlength(txt, font=f)
            d.text(((W - w) / 2, y + int((1 - ease(k)) * 16)), txt, font=f, fill=col); y += int(size * 1.45)
        emit(c, ease(k) * k2)

card([("Bramley", 84, "Bold", "white")], 2.6)
for src, start, secs, text, sub, colour in SHOTS:
    bg = glow(colour); head = headline(text, sub); n = int(secs * FPS)
    for i, fr in enumerate(frames(src, start, secs)):
        if i >= n: break
        t = i / FPS
        d = dev.copy()
        d.paste(fr.resize((SCREEN[2] - SCREEN[0], SCREEN[3] - SCREEN[1]), Image.LANCZOS), SCREEN[:2])
        s = DEV_SCALE * (1.0 + 0.04 * t / secs)          # slow push in
        dw, dh = int(d.width * s), int(d.height * s)
        d = d.resize((dw, dh), Image.BILINEAR)
        c = bg.copy().convert("RGBA")
        cy = DEV_TOP - int((s - DEV_SCALE) * 1500 * 0.20)  # grow around the screen, not the top edge
        c.alpha_composite(d, ((W - dw) // 2, cy))
        hk = ease(min(1, max(0, (t - 0.25) / 0.7)))
        h = head.copy(); h.putalpha(h.getchannel("A").point(lambda v: int(v * hk)))
        c.alpha_composite(h, (0, int((1 - hk) * 18)))
        fade = min(1, t / FADE, (secs - t) / FADE)
        emit(c, max(0, fade))
card([("Bramley", 84, "Bold", "white"), ("Free for the TrimUI Brick", 36, "Regular", (170, 170, 176)),
      ("and Brick Hammer.", 36, "Regular", (170, 170, 176)), ("github.com/howardhwt/bramley", 32, "SemiBold", (120, 160, 255))], 4.2)
out.stdin.close(); out.wait()
print(f"{total[0]} frames, {total[0] / FPS:.1f} s")
