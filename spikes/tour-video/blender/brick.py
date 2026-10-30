# A model of the TrimUI Brick Hammer for Bramley's launch film, built from proportions measured on
# TrimUI's orthographic views (front 437 x 662 px). X is across, Z up, -Y toward the viewer.
# Not shipped. Run:  Blender -b -P brick.py -- <screen.png> <out-prefix> [white|dark|grey]
import bpy, bmesh, math, sys, os
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SCREEN_IMG = argv[0] if argv else ""
OUT = argv[1] if len(argv) > 1 else "/tmp/brick"
BG = argv[2] if len(argv) > 2 else "grey"
INTER = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/res/Inter-Bold.ttf"

W = 0.072                       # metres; only the proportions matter
H = W * 1.515
D1 = W * 0.151                  # thickness at the screen
D2 = W * 0.254                  # thickness at the grip
GRIP_TOP = H * (1 - 0.454)      # the step on the back
R_TOP, R_BOT = W * 0.020, W * 0.070

def fx(u): return (u - 0.5) * W           # front fraction across -> x
def fz(v): return H * (1 - v)             # front fraction down -> z

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

def mat(name, color, metal=0.0, rough=0.5, coat=0.0, emit=None, strength=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if coat: b.inputs["Coat Weight"].default_value = coat
    if emit:
        b.inputs["Emission Color"].default_value = (*emit, 1)
        b.inputs["Emission Strength"].default_value = strength
    return m

def srgb(h):
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple((x / 12.92) if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)

M_BODY = mat("anodised", srgb("6b6c70"), metal=1.0, rough=0.36)
M_BACK = mat("back", srgb("626367"), metal=1.0, rough=0.44)
M_BLACK = mat("button", srgb("1c1c1e"), rough=0.45)
M_PILL = mat("pill", srgb("b9babd"), rough=0.4)
M_GLASS = mat("glass", srgb("060607"), rough=0.08, coat=0.6)
M_HOLE = mat("hole", srgb("0a0a0a"), rough=0.8)
M_LETTER = mat("letter", srgb("4a4a4d"), rough=0.5)
M_ENGRAVE = mat("engrave", srgb("2e2f32"), metal=1.0, rough=0.7)
M_TEAL = mat("teal", srgb("5fd6c8"), rough=0.35, emit=srgb("5fd6c8"), strength=0.4)
M_LABEL = mat("label", srgb("0c0c0d"), rough=0.25)
M_LABEL_TEXT = mat("labeltext", srgb("c8c8cc"), rough=0.5)
M_LED = mat("led", srgb("bfe3ff"), emit=srgb("9fd2ff"), strength=6.0)

def screen_mat():
    m = bpy.data.materials.new("screen"); m.use_nodes = True
    nt = m.node_tree; n = nt.nodes; n.clear()
    out = n.new("ShaderNodeOutputMaterial")
    emit = n.new("ShaderNodeEmission"); emit.inputs["Strength"].default_value = 1.6
    gloss = n.new("ShaderNodeBsdfGlossy"); gloss.inputs["Roughness"].default_value = 0.03
    gloss.inputs["Color"].default_value = (1, 1, 1, 1)
    fres = n.new("ShaderNodeFresnel"); fres.inputs["IOR"].default_value = 1.5
    mix = n.new("ShaderNodeMixShader")
    if SCREEN_IMG and os.path.exists(SCREEN_IMG):
        tex = n.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(SCREEN_IMG)
        tex.extension = "EXTEND"
        nt.links.new(tex.outputs["Color"], emit.inputs["Color"])
    nt.links.new(fres.outputs[0], mix.inputs[0]); nt.links.new(emit.outputs[0], mix.inputs[1])
    nt.links.new(gloss.outputs[0], mix.inputs[2]); nt.links.new(mix.outputs[0], out.inputs[0])
    return m
M_SCREEN = screen_mat()

def outline(x0, z0, x1, z1, rtl, rtr, rbr, rbl, seg=14):
    """A rounded rectangle in XZ, corners top-left, top-right, bottom-right, bottom-left."""
    pts = []
    for (cx, cz, r, a0) in [(x0 + rtl, z1 - rtl, rtl, 90), (x1 - rtr, z1 - rtr, rtr, 0),
                            (x1 - rbr, z0 + rbr, rbr, -90), (x0 + rbl, z0 + rbl, rbl, -180)]:
        for i in range(seg + 1):
            a = math.radians(a0 + 90 - 90 * i / seg) if False else math.radians(a0 + 90 * (1 - i / seg)) if False else None
        # corners walked clockwise
    pts = []
    corners = [(x0 + rtl, z1 - rtl, rtl, 180, 90), (x1 - rtr, z1 - rtr, rtr, 90, 0),
               (x1 - rbr, z0 + rbr, rbr, 0, -90), (x0 + rbl, z0 + rbl, rbl, -90, -180)]
    for cx, cz, r, a0, a1 in corners:
        for i in range(seg + 1):
            a = math.radians(a0 + (a1 - a0) * i / seg)
            pts.append((cx + r * math.cos(a), cz + r * math.sin(a)))
    return pts

def slab(name, pts, y0, y1, material, bevel=0.0006, segs=3):
    """The outline extruded from y0 (front) to y1 (back)."""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vf = [bm.verts.new((x, y0, z)) for x, z in pts]
    vb = [bm.verts.new((x, y1, z)) for x, z in pts]
    bm.faces.new(vf[::-1]); bm.faces.new(vb)
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((vf[i], vf[j], vb[j], vb[i]))
    bm.normal_update(); bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); scene.collection.objects.link(ob)
    ob.data.materials.append(material)
    if bevel:
        m = ob.modifiers.new("bevel", "BEVEL"); m.width = bevel; m.segments = segs; m.limit_method = "ANGLE"
        m.angle_limit = math.radians(40)
    for p in ob.data.polygons: p.use_smooth = True
    ob.modifiers.new("ws", "WEIGHTED_NORMAL") if bevel else None
    return ob

def cyl(name, x, z, r, y0, y1, material, bevel=0.0004):
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=r, depth=abs(y1 - y0), location=(x, (y0 + y1) / 2, z),
                                        rotation=(math.radians(90), 0, 0))
    ob = bpy.context.object; ob.name = name; ob.data.materials.append(material)
    m = ob.modifiers.new("bevel", "BEVEL"); m.width = bevel; m.segments = 3
    bpy.ops.object.shade_smooth()
    return ob

def text(s, x, z, size, material, y, rot_z=0.0, depth=0.00008, align="LEFT", face="front"):
    bpy.ops.object.text_add(location=(x, y, z))
    ob = bpy.context.object; ob.data.body = s; ob.data.size = size; ob.data.extrude = depth
    ob.data.font = bpy.data.fonts.load(INTER); ob.data.align_x = align; ob.data.align_y = "CENTER"
    ob.rotation_euler = (math.radians(90), 0, rot_z) if face == "front" else (math.radians(90), 0, math.radians(180))
    ob.data.materials.append(material)
    return ob

# body: the upper slab and the grip behind its lower half
x0, x1 = -W / 2, W / 2
slab("body", outline(x0, 0, x1, H, R_TOP, R_TOP, R_BOT, R_BOT), 0, D1, M_BODY, bevel=0.0009, segs=4)
slab("grip", outline(x0, 0, x1, GRIP_TOP, 0.0012, 0.0012, R_BOT, R_BOT), D1 - 0.0005, D2, M_BACK, bevel=0.0012, segs=4)
# ribs across the grip's back
for i in range(14):
    z = GRIP_TOP * 0.12 + i * GRIP_TOP * 0.055
    slab(f"rib{i}", outline(x0 + W * 0.09, z, x1 - W * 0.09, z + GRIP_TOP * 0.018, 0.0003, 0.0003, 0.0003, 0.0003, 3),
         D2 - 0.0001, D2 + 0.00045, M_BACK, bevel=0.00015, segs=1)
# screws on the back's upper plate and the grip
for (u, v, yb) in [(0.08, 0.04, D1), (0.92, 0.04, D1), (0.08, 0.93, D2), (0.92, 0.93, D2)]:
    cyl("screw", fx(u), fz(v), W * 0.022, yb - 0.0001, yb + 0.0003, M_LETTER, bevel=0.0002)
# the black label with its light edge on the upper back (photo 7)
slab("label", outline(fx(0.22), fz(0.17), fx(0.78), fz(0.07), 0.001, 0.001, 0.001, 0.001, 4), D1 - 0.0001, D1 + 0.00025, M_LABEL, bevel=0.0001, segs=1)
text("BRICK HAMMER", 0, fz(0.12), W * 0.05, M_LABEL_TEXT, D1 + 0.00027, face="back", align="CENTER")
# shoulder buttons on the step, and the light under them
for (u0, u1) in [(0.02, 0.24), (0.26, 0.42), (0.58, 0.74), (0.76, 0.98)]:
    slab("shoulder", outline(fx(u0), GRIP_TOP - 0.0005, fx(u1), GRIP_TOP + H * 0.035, 0.0008, 0.0008, 0.0004, 0.0004, 4),
         D1 + 0.0004, D2 - 0.0008, M_BLACK, bevel=0.0005, segs=3)
slab("shoulder_led", outline(fx(0.03), GRIP_TOP - 0.0006, fx(0.97), GRIP_TOP, 0.0002, 0.0002, 0.0002, 0.0002, 2),
     D1 + 0.0002, D2 - 0.0004, M_LED, bevel=0)
# the light bar on the top edge (photos 2 and 8)
slab("top_led", outline(fx(0.22), H - 0.0001, fx(0.78), H + 0.00035, 0.0002, 0.0002, 0.0002, 0.0002, 2),
     D1 * 0.45, D1 * 0.85, M_LED, bevel=0)

# front: glass across the top 52 %, the display inside it
slab("glass", outline(x0 + 0.0004, fz(0.517), x1 - 0.0004, H - 0.0004, R_TOP * 0.8, R_TOP * 0.8, 0.0003, 0.0003),
     -0.00025, 0.0002, M_GLASS, bevel=0.00015, segs=2)
me = bpy.data.meshes.new("display")
dx0, dx1, dz0, dz1 = fx(0.022), fx(0.978), fz(0.490), fz(0.014)
me.from_pydata([(dx0, -0.00026, dz0), (dx1, -0.00026, dz0), (dx1, -0.00026, dz1), (dx0, -0.00026, dz1)], [], [(0, 1, 2, 3)])
me.uv_layers.new(); uv = me.uv_layers[0].data
for i, c in enumerate([(0, 0), (1, 0), (1, 1), (0, 1)]): uv[i].uv = c
disp = bpy.data.objects.new("display", me); scene.collection.objects.link(disp); disp.data.materials.append(M_SCREEN)

# d-pad: a cross, raised, in a shallow dark well
cx, cz = fx(0.222), fz(0.653); arm, th = W * 0.135, W * 0.046
slab("dpad_well", outline(cx - arm - 0.0008, cz - th - 0.0008, cx + arm + 0.0008, cz + th + 0.0008, 0.0012, 0.0012, 0.0012, 0.0012, 4),
     -0.00005, 0.0003, M_HOLE, bevel=0.0002, segs=1)
slab("dpad_h", outline(cx - arm, cz - th, cx + arm, cz + th, 0.0009, 0.0009, 0.0009, 0.0009, 4), -0.0016, 0.0002, M_BLACK, bevel=0.0004)
slab("dpad_v", outline(cx - th, cz - arm, cx + th, cz + arm, 0.0009, 0.0009, 0.0009, 0.0009, 4), -0.0016, 0.0002, M_BLACK, bevel=0.0004)
for a in (90, 210, 330):  # the three-dot mark in the middle
    cyl("dot", cx + math.cos(math.radians(a)) * W * 0.014, cz + math.sin(math.radians(a)) * W * 0.014, W * 0.011,
        -0.00168, -0.00158, M_LETTER, bevel=0.00003)
# face buttons with their letters
for l, u, v in [("X", 0.778, 0.586), ("Y", 0.675, 0.654), ("A", 0.876, 0.654), ("B", 0.778, 0.722)]:
    cyl("btn_" + l, fx(u), fz(v), W * 0.05, -0.0017, 0.0002, M_BLACK, bevel=0.0005)
    text(l, fx(u), fz(v), W * 0.055, M_LETTER, -0.00172, align="CENTER")
# select and start
for u in (0.426, 0.572):
    slab("pill", outline(fx(u) - W * 0.048, fz(0.579) - W * 0.012, fx(u) + W * 0.048, fz(0.579) + W * 0.012,
                        W * 0.0119, W * 0.0119, W * 0.0119, W * 0.0119, 8), -0.0008, 0.0002, M_PILL, bevel=0.0003)
# the three round buttons along the bottom, and the speaker holes
for u in (0.364, 0.503, 0.643):
    cyl("menu", fx(u), fz(0.918), W * 0.039, -0.0012, 0.0002, M_BLACK, bevel=0.0004)
for su in (0.165, 0.851):
    for r in range(2):
        for k in range(8):
            cyl("spk", fx(su) + (k - 3.5) * W * 0.0125 + (r * W * 0.006), fz(0.900) + r * W * 0.012, W * 0.0028,
                -0.00005, 0.0003, M_HOLE, bevel=0)
text("BRICK HAMMER", fx(0.035), fz(0.545), W * 0.03, M_ENGRAVE, -0.00002)
# sides: the power slider and teal key on the right, two keys on the left
def side_key(x_sign, v, length, mat_):
    x = x_sign * (W / 2)
    slab("key", outline(-0.0012, fz(v) - length / 2, 0.0012, fz(v) + length / 2, 0.0012, 0.0012, 0.0012, 0.0012, 6),
         D1 * 0.25, D1 * 0.75, mat_, bevel=0.0003).location.x = x
for v, L in ((0.215, H * 0.09), (0.33, H * 0.06)):
    side_key(1, v, L, M_BLACK if v < 0.3 else M_TEAL)
for v in (0.62, 0.70):
    side_key(-1, v, H * 0.055, M_BLACK)
# bottom edge: card slot, USB-C, headphone jack
for (u, w_, h_) in [(0.2, W * 0.16, D1 * 0.12), (0.5, W * 0.12, D1 * 0.28), (0.8, W * 0.05, W * 0.05)]:
    slab("port", outline(fx(u) - w_ / 2, -0.0002, fx(u) + w_ / 2, 0.0004, 0.0003, 0.0003, 0.0003, 0.0003, 2),
         D2 * 0.5 - h_ / 2, D2 * 0.5 + h_ / 2, M_HOLE, bevel=0)

# light, world and camera
world = bpy.data.worlds.new("w"); scene.world = world; world.use_nodes = True
# a soft, even studio around the device for reflections; the backdrop itself is added afterwards
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.35, 0.35, 0.37, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.12
def area(name, loc, size, power, rot):
    bpy.ops.object.light_add(type="AREA", location=loc, rotation=rot); l = bpy.context.object
    l.data.size = size; l.data.energy = power; l.name = name
LIGHTS = [  # (name, offset from the camera's side: azimuth degrees, height, distance), size, power
    ("key", (-40, 0.30, 0.42), 0.5, 10),
    ("fill", (55, 0.05, 0.40), 0.6, 3),
    ("rim", (160, 0.22, 0.38), 0.3, 10),
    ("top", (0, 0.45, 0.05), 0.9, 6),
    ("side", (-110, 0.10, 0.40), 0.5, 4),
]
for name, _, size, power in LIGHTS:
    bpy.ops.object.light_add(type="AREA"); l = bpy.context.object; l.name = name
    l.data.size = size; l.data.energy = power
def place_lights(cam_loc):
    """Lights keep their places around the camera, so each angle is lit like a product photo."""
    az = math.degrees(math.atan2(cam_loc[0], -cam_loc[1]))
    centre = Vector((0, D1, H * 0.5))
    for name, (da, h, dist), _, _ in LIGHTS:
        l = bpy.data.objects[name]; a = math.radians(az + da)
        l.location = centre + Vector((math.sin(a) * dist, -math.cos(a) * dist, h))
        l.rotation_euler = (centre - l.location).to_track_quat("-Z", "Y").to_euler()

scene.render.engine = "CYCLES"
prefs = bpy.context.preferences.addons["cycles"].preferences
try:
    prefs.compute_device_type = "METAL"; prefs.get_devices()
    for d in prefs.devices: d.use = True
    scene.cycles.device = "GPU"
except Exception as e:
    print("GPU not available:", e)
scene.cycles.samples = 96; scene.cycles.use_denoising = True
scene.render.resolution_x, scene.render.resolution_y = 1280, 960
scene.view_settings.view_transform = "AgX"; scene.view_settings.look = "AgX - Medium High Contrast"
scene.render.film_transparent = True

bpy.ops.object.camera_add(); cam = bpy.context.object; scene.camera = cam; cam.data.lens = 85
target = bpy.data.objects.new("target", None); scene.collection.objects.link(target); target.location = (0, D1, H * 0.5)
tc = cam.constraints.new("TRACK_TO"); tc.target = target; tc.track_axis = "TRACK_NEGATIVE_Z"; tc.up_axis = "UP_Y"
SHOTS = {
    "front34": (-0.17, -0.42, 0.20),
    "back34": (0.26, 0.36, 0.16),
    "front": (0.0, -0.48, H * 0.5),
}
for name, loc in SHOTS.items():
    cam.location = loc
    place_lights(loc)
    scene.render.filepath = f"{OUT}-{name}.png"
    bpy.ops.render.render(write_still=True)
