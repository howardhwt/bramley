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

# dark grey, as Howard's: the anodised metal darker, and the studio lights softer so it never reads as silver
def srgb(h):
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple((x / 12.92) if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)
for mname in ("anodised", "back"):
    bsdf = bpy.data.materials[mname].node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*srgb("4a4b50"), 1)
for l in lights: l.data.energy *= 0.55
# FILM_RIM=1 (default): a soft light behind the device, facing the camera, so the dark metal's outline
# separates from a black background. It travels with the camera like the others.
if os.environ.get("FILM_RIM", "1") == "1":
    rim = bpy.data.lights.new("rim", "AREA"); rim.shape = "DISK"; rim.size = float(os.environ.get("FILM_RIM_SIZE", "0.8"))
    rim.energy = float(os.environ.get("FILM_RIM_W", "150")); rim.color = (0.92, 0.95, 1.0)
    rim_ob = bpy.data.objects.new("rim", rim); scene.collection.objects.link(rim_ob)
    rim_ob.parent = cam; rim_ob.location = (0.0, 0.22, -0.75); rim_ob.rotation_euler = (math.radians(200), 0, 0)
# the screen's picture: sharp texture sampling, and the glass reflecting only a little
for n_ in bpy.data.materials["screen"].node_tree.nodes:
    if n_.type == "MATH": n_.inputs[1].default_value = 0.12
# the screen's picture
screen_mat = bpy.data.materials["screen"]
tex = next(n for n in screen_mat.node_tree.nodes if n.type == "TEX_IMAGE"); tex.interpolation = "Cubic"
emit = next(n for n in screen_mat.node_tree.nodes if n.type == "EMISSION")
black = bpy.data.images.new("black", 8, 6); black.pixels = [0, 0, 0, 1] * 48
def show(kind, frames):
    """kind: a folder of PNGs under SCR, 'splash', 'black' or 'still:<folder>' (its first frame)."""
    if kind == "black":
        tex.image = black; return
    if kind == "splash":
        tex.image = bpy.data.images.load(SPLASH, check_existing=True); return
    still = kind.startswith("still:")
    kind = kind.split(":", 1)[1] if still else kind
    first, last = 1, None
    if "@" in kind:    # 's5@1-106': only those pictures of the recording, then hold the last
        kind, rng = kind.split("@"); first, last = (int(x) for x in rng.split("-"))
    folder = os.path.join(SCR, kind)
    if not still:
        # Blender's image user ignores offsets and holds badly, so each shot gets its own folder of links:
        # the chosen pictures renumbered from 1, then the last repeated to the shot's end (the recordings run at 30 fps, so at 60 each picture fills two frames)
        src = sorted(f for f in os.listdir(folder) if f.endswith(".png"))
        src = src[first - 1:last or len(src)]
        cut = os.path.join(OUT, "_scr", kind + "_%d_%d_%d" % (first, last or 0, FPS))
        os.makedirs(cut, exist_ok=True)
        for i in range(frames):
            dst = os.path.join(cut, "%04d.png" % (i + 1))
            if not os.path.exists(dst): os.symlink(os.path.join(folder, src[min(i * 30 // FPS, len(src) - 1)]), dst)
        folder, first, last = cut, 1, frames
    img = bpy.data.images.load(os.path.join(folder, "%04d.png" % first))
    if still:
        tex.image = img; return
    img.source = "SEQUENCE"; tex.image = img
    n = (last or len([f for f in os.listdir(folder) if f.endswith(".png")])) - first + 1
    iu = tex.image_user; iu.frame_duration = n; iu.frame_start = 1; iu.frame_offset = 0
    iu.use_auto_refresh = True; iu.use_cyclic = False

# shots: (name, seconds, screen, camera from, camera to, look-at from, look-at to, turn from, turn to (degrees about z), screen glow ramp)
T = lambda x, z: (x, 0.0, z)
SHOTS = [   # opens on the front, as Apple's launch films do: the screen waking up close, then the reveal
    ("s01", 4.24, "splash", (0.07, -0.10, 0.10), (-0.05, -0.11, 0.094), T(0.0, SCREEN_Z), T(0.0, SCREEN_Z), 0, 0, (0.15, 1.1)),
    ("s02", 4.09, "still:s3", (0.0, -0.16, 0.083), (-0.01, -0.40, 0.06), T(0.0, SCREEN_Z), T(0.034, 0.066), 0, 0, None),
    ("s03", 4.08, "s3", (0.0, -0.30, 0.08), (-0.03, -0.17, 0.086), T(0.0, SCREEN_Z - 0.004), T(-0.004, SCREEN_Z), 0, 0, None),
    ("s04", 3.80, "s4", (-0.12, -0.24, 0.10), (-0.09, -0.25, 0.095), T(-0.058, SCREEN_Z - 0.006), T(-0.058, SCREEN_Z - 0.006), 0, 0, None),
    ("s05", 4.40, "s5", (0.07, -0.20, 0.06), (0.03, -0.19, 0.08), T(0.0, SCREEN_Z - 0.004), T(0.0, SCREEN_Z), 12, 0, None),
    ("s06", 4.00, "s6", (0.0, -0.26, 0.07), (0.0, -0.24, 0.072), T(0.032, SCREEN_Z - 0.006), T(0.032, SCREEN_Z - 0.006), 0, 0, None),
    ("s07", 4.10, "s7@45-126", (0.10, -0.26, 0.09), (0.07, -0.27, 0.085), T(-0.052, SCREEN_Z - 0.006), T(-0.052, SCREEN_Z - 0.006), 0, 0, None),
    ("s08", 4.10, "s8", (-0.10, -0.23, 0.12), (-0.06, -0.24, 0.11), T(-0.038, SCREEN_Z - 0.006), T(-0.038, SCREEN_Z - 0.006), 0, 0, None),
    ("s09", 8.10, "still:s3", (0.17, 0.31, 0.11), (-0.17, 0.30, 0.10), (0.0, D2 / 2, 0.056), (0.0, D2 / 2, 0.054), 0, 0, None),
    ("s10", 4.10, "still:s3", (0.0, -0.35, 0.06), (0.0, -0.33, 0.06), T(0.046, 0.056), T(0.046, 0.056), 20, 0, None),
]
# shots whose move finishes early and then holds, so their titles land on a still device
MOVE = {"s02": 0.7, "s10": 0.55}

def ease(t):  # cubic in-out
    return 4 * t ** 3 if t < 0.5 else 1 - (-2 * t + 2) ** 3 / 2

def key_shot(sh):
    name, secs, scr, c0, c1, t0, t1, r0, r1, glow = sh
    n = int(round(secs * FPS))
    scene.frame_start, scene.frame_end = 1, n
    for f in range(1, n + 1):
        k = ease(min(1.0, (f - 1) / max(1, (n - 1) * MOVE.get(name, 1.0))))
        cam.location = Vector(c0).lerp(Vector(c1), k); cam.keyframe_insert("location", frame=f)
        target.location = Vector(t0).lerp(Vector(t1), k); target.keyframe_insert("location", frame=f)
        pivot.rotation_euler = (0, 0, math.radians(r0 + (r1 - r0) * k)); pivot.keyframe_insert("rotation_euler", frame=f)
        s = 2.0
        if glow:
            g = min(1.0, max(0.0, ((f - 1) / FPS - glow[0]) / (glow[1] - glow[0])))
            s = 2.0 * g
        emit.inputs["Strength"].default_value = s; emit.inputs["Strength"].keyframe_insert("default_value", frame=f)
    show(scr, n)
    return n

led = bpy.data.materials["led"].node_tree.nodes["Principled BSDF"]
LED = (0.86, 0.93, 1.0)
label = bpy.data.materials["label"].node_tree.nodes["Principled BSDF"]
label.inputs["Roughness"].default_value = 0.45
label.inputs["Specular IOR Level"].default_value = 0.2    # black glass, but soft enough that its reflection does not flip as the camera moves
def look(name):
    mood = name == "s09"
    led.inputs["Emission Color"].default_value = (*LED, 1)
    led.inputs["Emission Strength"].default_value = 3.0 if mood else 0.0
    # the cold open is a black screen close up, where the glass mirrors the studio lights as a big
    # diagonal glare; there the glass reflects almost nothing, so the logo is all that shows
    for n_ in bpy.data.materials["screen"].node_tree.nodes:
        if n_.type == "MATH": n_.inputs[1].default_value = 0.015 if name == "s01" else 0.12

def clear_keys():
    for o in (cam, target, pivot):
        o.animation_data_clear()
    screen_mat.node_tree.animation_data_clear()

# render settings
# Cycles always: Eevee rendered the dark grey metal as silver. 1080p even for drafts, so the screen reads
scene.render.engine = "CYCLES"
prefs = bpy.context.preferences.addons["cycles"].preferences
prefs.compute_device_type = "METAL"; prefs.get_devices()
for d in prefs.devices: d.use = True
scene.cycles.device = "GPU"
scene.cycles.samples = 12 if QUALITY == "draft" else 64
scene.cycles.use_denoising = True
scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
scene.render.fps = FPS
scene.render.film_transparent = True
# FILM_STAGE=black (default; "none" renders on transparency): a real stage instead of a flat colour added later. The device stands on a wide,
# near-black floor with a soft sheen, under a black sky, so its contact shadow, its reflection and the
# rim light are all rendered. Titles are still added by film_compose.py.
if os.environ.get("FILM_STAGE", "black") == "black":
    scene.render.film_transparent = False
    w = scene.world or bpy.data.worlds.new("world"); scene.world = w; w.use_nodes = True
    bg = w.node_tree.nodes.get("Background"); bg.inputs["Color"].default_value = (0, 0, 0, 1); bg.inputs["Strength"].default_value = 0
    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, D2 / 2, -0.0002))
    floor = bpy.context.active_object; floor.name = "stage"
    fm = bpy.data.materials.new("stage"); fm.use_nodes = True
    fb = fm.node_tree.nodes["Principled BSDF"]
    fb.inputs["Base Color"].default_value = (0.004, 0.004, 0.005, 1)
    fb.inputs["Roughness"].default_value = float(os.environ.get("FILM_STAGE_ROUGH", "0.3"))
    fb.inputs["Specular IOR Level"].default_value = float(os.environ.get("FILM_STAGE_SPEC", "0.5"))
    floor.data.materials.append(fm)
    # light linking: the studio lights light the device only, so the floor stays black and shows just
    # the device's reflection and the shade under it, instead of blowing out under lights this close
    lit = bpy.data.collections.new("lit"); scene.collection.children.link(lit)
    for o in scene.objects:
        if o.type not in ("CAMERA", "LIGHT") and o.name not in ("stage", "target", "pivot"): lit.objects.link(o)
    for l in [o for o in scene.objects if o.type == "LIGHT"]:
        l.light_linking.receiver_collection = lit
scene.render.image_settings.file_format = "PNG"; scene.render.image_settings.color_mode = "RGBA"
cam.data.lens = 50

for sh in SHOTS:
    if ONLY and sh[0] not in ONLY: continue
    clear_keys(); look(sh[0])
    n = key_shot(sh)
    os.makedirs(os.path.join(OUT, sh[0]), exist_ok=True)
    if os.environ.get("FILM_STILLS"):   # frames from the start, middle and end of each shot, to check before a full render
        for fr in sorted({max(1, int(n * x)) for x in (0.08, 0.5, 0.96)}):
            scene.frame_set(fr); scene.render.filepath = os.path.join(OUT, sh[0], "%04d.png" % fr)
            bpy.ops.render.render(write_still=True)
        continue
    scene.render.filepath = os.path.join(OUT, sh[0], "")
    bpy.ops.render.render(animation=True)
    print("rendered", sh[0], n, "frames")
