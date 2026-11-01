# Renders the launch film's shots from the Brick Hammer model (brick.py, saved with BRICK_SAVE).
# Each shot: a camera move, the device's turn, and what its screen shows. Frames come out with a
# transparent background; compose.py adds the background, titles and music. Not shipped.
#   Blender -b brick-hammer.blend -P film.py -- <screens-dir> <out-dir> <draft|final> [shot,...]
import bpy, sys, os, math
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
SCR, OUT, QUALITY = argv[0], argv[1], argv[2]
ONLY = argv[3].split(",") if len(argv) > 3 else None
SPLASH = "/Users/howard/Documents/GitHub/brick-hammer-launcher/docs/brand/bramley-splash.png"
W, H = 0.072, 0.072 * 1.515
D2 = W * 0.254
FPS = 30 if QUALITY == "draft" else 60
SCREEN_Z = H * (1 - 0.25)              # the display's centre
C = Vector((0, D2 / 2, H / 2))         # the device's centre, the turn's pivot

scene = bpy.context.scene
cam = scene.camera
cam.constraints.clear()
target = bpy.data.objects["target"]
tc = cam.constraints.new("TRACK_TO"); tc.target = target; tc.track_axis = "TRACK_NEGATIVE_Z"; tc.up_axis = "UP_Y"
lights = [o for o in scene.objects if o.type == "LIGHT"]
device = [o for o in scene.objects if o.type not in ("CAMERA", "LIGHT") and o.name != "target" and o.parent is None]

# the device turns about its centre
pivot = bpy.data.objects.new("pivot", None); scene.collection.objects.link(pivot); pivot.location = C
bpy.context.view_layer.update()
for o in device:
    mw = o.matrix_world.copy(); o.parent = pivot; o.matrix_world = mw
# the lights travel with the camera, so every shot is lit like a product photo from where it is seen
cam.location = (-0.17, -0.42, 0.20); bpy.context.view_layer.update()
for l in lights:
    mw = l.matrix_world.copy(); l.parent = cam; l.matrix_world = mw

# the screen's picture
screen_mat = bpy.data.materials["screen"]
tex = next(n for n in screen_mat.node_tree.nodes if n.type == "TEX_IMAGE")
emit = next(n for n in screen_mat.node_tree.nodes if n.type == "EMISSION")
black = bpy.data.images.new("black", 8, 6); black.pixels = [0, 0, 0, 1] * 48
def show(kind, frames):
    """kind: a folder of PNGs under SCR, 'splash', 'black' or 'still:<folder>' (its first frame)."""
    if kind == "black":
        tex.image = black; return
    if kind == "splash":
        tex.image = bpy.data.images.load(SPLASH, check_existing=True); return
    still = kind.startswith("still:")
    folder = os.path.join(SCR, kind.split(":", 1)[1] if still else kind)
    img = bpy.data.images.load(os.path.join(folder, "0001.png"))
    if still:
        tex.image = img; return
    img.source = "SEQUENCE"; tex.image = img
    n = len([f for f in os.listdir(folder) if f.endswith(".png")])
    iu = tex.image_user; iu.frame_duration = n; iu.frame_start = 1; iu.frame_offset = 0
    iu.use_auto_refresh = True; iu.use_cyclic = False
    # the recordings run at 30 fps; at 60 each picture holds two frames
    iu.frame_duration = n

# shots: (name, seconds, screen, camera from, camera to, look-at from, look-at to, turn from, turn to (degrees about z), screen glow ramp)
T = lambda x, z: (x, 0.0, z)
SHOTS = [
    ("s01", 4.24, "black", (0.10, -0.40, 0.09), (0.05, -0.37, 0.07), C[:], C[:], 205, 150, None),
    ("s02", 4.09, "splash", (-0.02, -0.43, 0.06), (-0.01, -0.41, 0.06), T(0.034, 0.058), T(0.034, 0.058), 22, 0, (0.2, 0.9)),
    ("s03", 4.08, "s3", (0.0, -0.40, 0.07), (0.0, -0.25, 0.085), T(0.03, 0.062), T(0.0, SCREEN_Z), 0, -6, None),
    ("s04", 3.80, "s4", (-0.16, -0.30, 0.11), (-0.12, -0.32, 0.10), T(-0.045, SCREEN_Z - 0.01), T(-0.045, SCREEN_Z - 0.01), 0, 0, None),
    ("s05", 4.40, "s5", (0.10, -0.30, 0.04), (0.04, -0.30, 0.07), T(0.0, SCREEN_Z - 0.006), T(0.0, SCREEN_Z), 18, 4, None),
    ("s06", 4.00, "s6", (0.0, -0.36, 0.055), (0.0, -0.33, 0.06), T(0.034, 0.06), T(0.034, 0.062), 0, 0, None),
    ("s07", 4.10, "s7", (0.12, -0.35, 0.09), (0.08, -0.37, 0.08), T(-0.032, 0.06), T(-0.032, 0.06), 0, 0, None),
    ("s08", 4.10, "s8", (-0.12, -0.27, 0.14), (-0.07, -0.29, 0.12), T(-0.045, 0.072), T(-0.045, 0.072), 0, 0, None),
    ("s09", 8.10, "still:s3", (0.0, -0.42, 0.06), (0.0, -0.42, 0.06), C[:], C[:], 0, 180, None),
    ("s10", 4.10, "still:s3", (0.0, -0.44, 0.06), (0.0, -0.52, 0.06), T(0.04, 0.055), T(0.045, 0.055), 180, 360, None),
]

def ease(t):  # cubic in-out
    return 4 * t ** 3 if t < 0.5 else 1 - (-2 * t + 2) ** 3 / 2

def key_shot(sh):
    name, secs, scr, c0, c1, t0, t1, r0, r1, glow = sh
    n = int(round(secs * FPS))
    scene.frame_start, scene.frame_end = 1, n
    for f in range(1, n + 1):
        k = ease((f - 1) / max(1, n - 1))
        cam.location = Vector(c0).lerp(Vector(c1), k); cam.keyframe_insert("location", frame=f)
        target.location = Vector(t0).lerp(Vector(t1), k); target.keyframe_insert("location", frame=f)
        pivot.rotation_euler = (0, 0, math.radians(r0 + (r1 - r0) * k)); pivot.keyframe_insert("rotation_euler", frame=f)
        s = 1.6
        if glow:
            g = min(1.0, max(0.0, ((f - 1) / FPS - glow[0]) / (glow[1] - glow[0])))
            s = 1.6 * g
        emit.inputs["Strength"].default_value = s; emit.inputs["Strength"].keyframe_insert("default_value", frame=f)
    show(scr, n)
    if scr not in ("black", "splash") and not scr.startswith("still:") and FPS == 60:
        tex.image_user.frame_duration = n   # holds each 30 fps picture for two frames via the speed below
    return n

def clear_keys():
    for o in (cam, target, pivot):
        o.animation_data_clear()
    screen_mat.node_tree.animation_data_clear()

# render settings
scene.render.engine = "BLENDER_EEVEE" if QUALITY == "draft" else "CYCLES"
if QUALITY == "draft":
    scene.eevee.taa_render_samples = 16
    scene.render.resolution_x, scene.render.resolution_y = 960, 540
else:
    scene.cycles.samples = 64
    scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
scene.render.fps = FPS
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"; scene.render.image_settings.color_mode = "RGBA"
cam.data.lens = 50

for sh in SHOTS:
    if ONLY and sh[0] not in ONLY: continue
    clear_keys()
    n = key_shot(sh)
    os.makedirs(os.path.join(OUT, sh[0]), exist_ok=True)
    scene.render.filepath = os.path.join(OUT, sh[0], "")
    bpy.ops.render.render(animation=True)
    print("rendered", sh[0], n, "frames")
