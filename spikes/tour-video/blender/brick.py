# A model of the TrimUI Brick Hammer for Bramley's launch film, built from proportions measured on
# TrimUI's orthographic views (front 437 x 662 px). X is across, Z up, -Y toward the viewer.
# Not shipped. Run:  Blender -b -P brick.py -- <screen.png> <out-prefix> [white|dark|grey]
import bpy, bmesh, math, sys, os
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SCREEN_IMG = argv[0] if argv else ""
OUT = argv[1] if len(argv) > 1 else "/tmp/brick"
FINISH = argv[2] if len(argv) > 2 else "dark"     # dark | silver
LED = (0.86, 0.93, 1.0)                          # the mood light: cool white (Howard, 2026-10-30)
INTER = os.environ.get("BRICK_INTER", "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/res/Inter-Bold.ttf")   # BRICK_INTER points at another copy if the repo's cannot be read
FONTS = os.environ.get("BRICK_FONTS", "")   # a folder with Avenir-Light.ttf and Avenir-Medium.ttf (the back's lettering)
def font_file(name, fallback):
    f = os.path.join(FONTS, name)
    return f if FONTS and os.path.exists(f) else INTER.replace("Bold", fallback)

W = 0.072                       # metres; only the proportions matter
H = W * 1.515
D1 = W * 0.1667                 # thickness at the screen (TrimUI side views, overlaid row by row)
D2 = W * 0.2781                 # thickness at the grip (TrimUI side views, overlaid row by row)
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

BODY_HEX = {"dark": ("64656a", "64656a"), "silver": ("c9cacd", "bfc0c3")}[FINISH]
M_BODY = mat("anodised", srgb(BODY_HEX[0]), metal=1.0, rough=0.36)
M_BACK = mat("back", srgb(BODY_HEX[1]), metal=1.0, rough=0.44)
M_BLACK = mat("button", srgb("141416"), rough=0.62)
M_PILL = mat("pill", srgb("b9babd"), rough=0.4)
M_GLASS = mat("glass", srgb("060607"), rough=0.08, coat=0.6)
M_HOLE = mat("hole", srgb("0a0a0a"), rough=0.8)
M_LETTER = mat("letter", srgb("38383c"), rough=0.5)   # letters cut into the black dome, read by their edges (TrimUI photo 14)
M_ENGRAVE = mat("engrave", srgb("3f4045"), metal=1.0, rough=0.62)
M_TEAL = mat("teal", srgb("44b3a5"), rough=0.4)   # the power key on Howard's unit (IMG_8505); lit by the scene, no glow
M_LABEL = mat("label", (0.0, 0.0, 0.0), rough=0.03)
M_LABEL_TEXT = mat("labeltext", srgb("c8c8cc"), rough=0.5)
MOOD = os.environ.get("BRICK_MOOD", "off") == "on"   # the mood light: off by default, as in Howard's photos
M_LED = mat("led", LED, emit=LED, strength=4.0) if MOOD else mat("led", srgb("a9abb0"), rough=0.5)
M_STRIP = mat("strip", LED, emit=LED, strength=4.0) if MOOD else mat("strip", srgb("9ea2b6"), rough=0.45)   # frosted, faintly lavender when off (IMG_8508)  # frosted when off (photos IMG_8457-8459)
M_USB = mat("usb", srgb("6e6f74"), metal=1.0, rough=0.42)   # the USB-C piece's ribbed part: the grip's metal, a shade lighter (Howard's straight-on photo)
M_PILL_LIT = mat("pill_lit", (0.85, 0.95, 1.0), rough=0.3, emit=LED, strength=3.0) if MOOD else mat("pill_lit", srgb("7e7f83"), rough=0.45)   # mid grey (TrimUI photo 14)
M_ICON = mat("icon", srgb("222224"), rough=0.55)   # dark marks on the black buttons (TrimUI photo 14)

def sparkle(m, lo=0.22, hi=0.50):
    """The fine sparkle of the Hammer's finish: tiny flecks vary the roughness and the normal."""
    nt = m.node_tree; n = nt.nodes; bsdf = n["Principled BSDF"]
    tc = n.new("ShaderNodeTexCoord")
    noise = n.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value = 9000; noise.inputs["Detail"].default_value = 2
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    mr = n.new("ShaderNodeMapRange"); mr.inputs["To Min"].default_value = lo; mr.inputs["To Max"].default_value = hi
    nt.links.new(noise.outputs["Fac"], mr.inputs["Value"]); nt.links.new(mr.outputs["Result"], bsdf.inputs["Roughness"])
    bump = n.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.05
    nt.links.new(noise.outputs["Fac"], bump.inputs["Height"]); nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
sparkle(M_BODY, 0.34, 0.58); sparkle(M_BACK, 0.34, 0.58)   # anodised: soft reflections
M_FACE = mat("facebutton", srgb("111113"), rough=0.32)   # the face buttons: near black with a soft gloss (TrimUI photo 14)
M_FACE_CUT = M_FACE.copy(); M_FACE_CUT.name = "face_cut"   # the same black, so a cut label can be told apart and shaded flat
M_DISH = mat("dish", srgb("121214"), rough=0.5)
M_SHOULDER = mat("shoulder", srgb("222225"), rough=0.45)   # dark grey, a little lighter than the face buttons (TrimUI photo 14)
M_SHOULDER.node_tree.nodes["Principled BSDF"].inputs["Specular IOR Level"].default_value = 0.12
M_FACE.node_tree.nodes["Principled BSDF"].inputs["Specular IOR Level"].default_value = 0.3
M_PORT = mat("port", srgb("3a3b3f"), metal=1.0, rough=0.35)   # the port's steel lining, in shadow
M_LABEL_BTN = mat("btnlabel", srgb("2a2a2e"), rough=0.45)
M_SCREW = mat("screw", srgb("c4c5c8"), metal=1.0, rough=0.25)
M_ETCH = mat("etch", srgb("8a8b90"), metal=1.0, rough=0.55)   # light, thin markings on the grip

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
    dim = n.new("ShaderNodeMath"); dim.operation = "MULTIPLY"; dim.inputs[1].default_value = 0.35   # the glass reflects softly
    nt.links.new(fres.outputs[0], dim.inputs[0]); nt.links.new(dim.outputs[0], mix.inputs[0]); nt.links.new(emit.outputs[0], mix.inputs[1])
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

def slab(name, pts, y0, y1, material, bevel=0.0006, segs=3, back_only=False, back_zmax=None, back_skip_x=None):
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
    back_edges = set()
    if back_only:
        bm.edges.ensure_lookup_table()
        back = set(vb)
        # the edges along the top (above back_zmax) stay sharp; a small round there broke the booleans
        def weight(e):   # the sides and bottom round fully; the top edge and the USB-C tab stay crisp (partial weights broke the booleans)
            if back_skip_x and all(back_skip_x[0] <= v.co.x <= back_skip_x[1] for v in e.verts): return 0.0
            if back_zmax is None or any(v.co.z < back_zmax for v in e.verts): return 1.0
            return 0.0   # the top edge stays crisp (rounding it rolled the ledge into a slope)
        back_edges = {e.index: weight(e) for e in bm.edges if all(v in back for v in e.verts)}
    bm.to_mesh(me); bm.free()
    if back_only:
        a = me.attributes.new("bevel_weight_edge", "FLOAT", "EDGE")
        for i in range(len(me.edges)): a.data[i].value = back_edges.get(i, 0.0)
    ob = bpy.data.objects.new(name, me); scene.collection.objects.link(ob)
    ob.data.materials.append(material)
    if bevel:
        m = ob.modifiers.new("bevel", "BEVEL"); m.width = bevel; m.segments = segs
        if back_only: m.limit_method = "WEIGHT"; m.use_clamp_overlap = False   # short corner edges must not shrink the round
        else: m.limit_method = "ANGLE"; m.angle_limit = math.radians(40)
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

def dome(x, z, r, y_face, height, material):
    """A shallow convex cap on a round button's face."""
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=r, location=(x, y_face, z))
    ob = bpy.context.object; ob.scale = (1, height / r, 1); ob.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    # keep only the front half
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="DESELECT"); bpy.ops.object.mode_set(mode="OBJECT")
    for v in ob.data.vertices: v.select = v.co.y > 0.00001
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.delete(type="VERT"); bpy.ops.object.mode_set(mode="OBJECT")
    return ob

def text(s, x, z, size, material, y, rot_z=0.0, depth=0.00008, align="LEFT", face="front"):
    bpy.ops.object.text_add(location=(x, y, z))
    ob = bpy.context.object; ob.data.body = s; ob.data.size = size; ob.data.extrude = depth
    ob.data.font = bpy.data.fonts.load(INTER); ob.data.align_x = align; ob.data.align_y = "CENTER"
    ob.rotation_euler = (math.radians(90), 0, rot_z) if face == "front" else (math.radians(90), 0, math.radians(180))
    ob.data.materials.append(material)
    return ob

def apply_mods(ob):
    bpy.ops.object.select_all(action="DESELECT"); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    for m_ in list(ob.modifiers): bpy.ops.object.modifier_apply(modifier=m_.name)
    ob.select_set(False)

def reshade(ob, angle=30):
    """After a cut: drop the old custom normals and smooth only across the rounded edges, so flat faces stay flat."""
    bpy.ops.object.select_all(action="DESELECT"); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.mesh.customdata_custom_splitnormals_clear(); bpy.ops.object.shade_auto_smooth(angle=math.radians(angle))
    ob.select_set(False)

def flat_cuts(ob):
    """Shade a cut label's floor and walls flat: smoothing them with the curved top left blobs at sharp corners."""
    ix = [i for i, m in enumerate(ob.data.materials) if m and m.name.startswith("face_cut")]
    for p_ in ob.data.polygons:
        if p_.material_index in ix: p_.use_smooth = False

def text_mesh(*a, **k):
    """text() turned into a closed mesh, ready to cut with."""
    t_ = text(*a, **k)
    if all(ch in "AXYVWKLMNTIEFHZ" for ch in a[0]): t_.data.resolution_u = 1   # straight letters: no curve handles to overshoot into loops (the A's crossbar corners)
    bpy.ops.object.select_all(action="DESELECT"); t_.select_set(True); bpy.context.view_layer.objects.active = t_
    bpy.ops.object.convert(target="MESH")
    # tidy the outline: the font's tight curve points left tiny self-crossing loops (the A's crossbar corners)
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=0.00003); bpy.ops.mesh.dissolve_degenerate(threshold=0.00001)
    bpy.ops.object.mode_set(mode="OBJECT"); t_.select_set(False)
    return t_

def solid_button(name, x, z, r, y0, y1, dome_r, dome_h, material, bevel):
    """A round button as one closed solid: the cylinder and its domed top joined, so a label can be cut into it."""
    b_ = cyl(name, x, z, r, y0, y1, material, bevel=bevel); apply_mods(b_)
    d_ = dome(x, z, dome_r, y0 + 0.0001, dome_h + 0.0001, material)   # its base sunk inside the cylinder: a shared face spoiled the cuts
    bpy.ops.object.select_all(action="DESELECT"); d_.select_set(True); bpy.context.view_layer.objects.active = d_
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT"); bpy.ops.mesh.fill_holes(sides=0)
    bpy.ops.object.mode_set(mode="OBJECT"); bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    m_ = b_.modifiers.new("dome", "BOOLEAN"); m_.operation = "UNION"; m_.object = d_; m_.solver = "EXACT"
    bpy.context.view_layer.objects.active = b_; bpy.ops.object.modifier_apply(modifier="dome")
    bpy.data.objects.remove(d_, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT"); b_.select_set(True); bpy.context.view_layer.objects.active = b_
    bpy.ops.object.shade_auto_smooth(angle=math.radians(40)); b_.select_set(False)
    return b_

def box(name, cx, cy, cz, sx, sy, sz, material):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz)); b_ = bpy.context.object; b_.name = name
    b_.scale = (sx, sy, sz); bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    b_.data.materials.append(material)
    return b_

# body: the upper slab and the grip behind its lower half
x0, x1 = -W / 2, W / 2
body = slab("body", outline(x0, 0, x1, H, R_TOP, R_TOP, R_BOT, R_BOT, 24), 0, D1, M_BODY, bevel=0)
bm_ = body.data; bw = bm_.attributes.new("bevel_weight_edge", "FLOAT", "EDGE")
for e in bm_.edges:   # front edges 0.6 mm, back edges 1.2 mm (under the top corners' 1.4 mm radius) (photos 6 and 8), swept round the corners
    v0, v1 = bm_.vertices[e.vertices[0]].co, bm_.vertices[e.vertices[1]].co
    if abs(v0.y) < 1e-6 and abs(v1.y) < 1e-6: bw.data[e.index].value = 0.6   # TrimUI side view: about 0.7 mm round on the front edges
    elif v0.y > D1 - 1e-6 and v1.y > D1 - 1e-6: bw.data[e.index].value = 1.0
bb_ = body.modifiers.new("round", "BEVEL"); bb_.width = 0.0012; bb_.segments = 10; bb_.limit_method = "WEIGHT"
bb_.use_clamp_overlap = False; bb_.miter_outer = "MITER_ARC"
bpy.context.view_layer.objects.active = body; bpy.ops.object.modifier_apply(modifier="round")
# --- the back: layout from TrimUI's flat views, details from Howard's photo (2026-10-30) ---
# Howard's photo looks down at the back from above and behind, so it shows top surfaces too: the
# light pipe on the top edge, the tops of the shoulder buttons with their names, and the ledge where
# the grip steps out, which carries frosted light strips and the USB-C port facing up. Each surface
# is foreshortened differently, so pv() maps a height on the photo (a fraction of 885 px from its
# top, y = 205 in the 787 x 1400 view) to the device, surface by surface.
def bu(u): return fx(1 - u)
def pv(v):
    img = 205 + v * 885
    if img <= 600: return max(0.0, (img - 235) / 912.5)            # upper back plate
    if img >= 715: return 0.454 + (img - 715) * 0.546 / 375        # grip panel
    return 0.40 + (img - 600) / 115 * 0.054                         # the ledge, seen from above
LEDGE = fz(0.454)                      # the grip's top: the ledge
PANEL_TOP = LEDGE
BTN_H = H * 0.062                      # the shoulder buttons stand this high on the ledge
LED_Y0, LED_Y1 = D2 - 0.0024, D2 - 0.0004   # frosted strips along the ledge's back edge
LED_H = 0.002                          # Howard's IMG_8508: each cap stands on a frosted light strip about 2 mm tall
UB0, UB1 = bu(0.586), bu(0.394)        # the USB-C piece, between R2 and L2
UTOP = LEDGE + 0.053 * W               # TrimUI photo 14: the tab rises about half the buttons' height
_pts = outline(x0, 0, x1, LEDGE, 0.0003, 0.0003, R_BOT, R_BOT)
_k = 15                                # outline(): after the top-left corner's 15 points comes the top edge
_tab = [(UB0, LEDGE), (UB0, UTOP - 0.0006), (UB0 + 0.0006, UTOP), (UB1 - 0.0006, UTOP), (UB1, UTOP - 0.0006), (UB1, LEDGE)]
# The grip is built from its cross-sections, not rounded afterwards: the intersection of its outline seen
# from behind (rounded bottom corners), its section across the width (the back corners rounded over the
# sides, IMG_8459 and TrimUI's bottom view, about 0.045 W) and its section down the side (the back
# rounded into the bottom). The top edge at the shoulder row stays crisp, so nothing has to blend there.
RG = 0.0045                            # a tight edge where the grip meets its sides and bottom (TrimUI photo 14; Howard: the 5.5 mm curve read as a chrome band)
def prism(name, pts, axis, a0, a1):
    """A 2D outline extruded along an axis: 'z' takes (x, y) points, 'x' takes (y, z) points."""
    me_ = bpy.data.meshes.new(name); bm_ = bmesh.new()
    mk = (lambda p, a: (p[0], p[1], a)) if axis == "z" else (lambda p, a: (a, p[0], p[1]))
    lo = [bm_.verts.new(mk(p, a0)) for p in pts]; hi = [bm_.verts.new(mk(p, a1)) for p in pts]
    bm_.faces.new(lo[::-1]); bm_.faces.new(hi)
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm_.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm_.normal_update(); bmesh.ops.recalc_face_normals(bm_, faces=bm_.faces); bm_.to_mesh(me_); bm_.free()
    o_ = bpy.data.objects.new(name, me_); scene.collection.objects.link(o_); o_.data.materials.append(M_BACK)
    return o_
def intersect(a, b):
    m = a.modifiers.new("x", "BOOLEAN"); m.operation = "INTERSECT"; m.object = b; m.solver = "EXACT"
    bpy.context.view_layer.objects.active = a; bpy.ops.object.modifier_apply(modifier="x")
    bpy.data.objects.remove(b, do_unlink=True)
GY0 = D1 - 0.0020
# one bevel swept round the whole back edge, so the side round and the bottom round meet in a smooth
# corner like the machined part, not a crease; the top edge stays crisp under the shoulder row
# Built taller than the ledge and rounded evenly on every back edge, then cut flat at the ledge: a round
# that stopped at a crisp top edge folded over itself near the top corners (Howard: shards at the corners,
# a dark crease down the sides).
GRIP_OVER = RG + 0.004
grip = slab("grip", outline(x0, 0, x1, LEDGE + GRIP_OVER, 0.0003, 0.0003, R_BOT, R_BOT, 24), GY0, D2, M_BACK, bevel=0)
gm = grip.data
gw = gm.attributes.new("bevel_weight_edge", "FLOAT", "EDGE")
for e in gm.edges:
    v0, v1 = gm.vertices[e.vertices[0]].co, gm.vertices[e.vertices[1]].co
    if v0.y > D2 - 1e-6 and v1.y > D2 - 1e-6: gw.data[e.index].value = 1.0
gb = grip.modifiers.new("round", "BEVEL"); gb.width = RG; gb.segments = 18; gb.limit_method = "WEIGHT"
gb.use_clamp_overlap = False; gb.miter_outer = "MITER_ARC"   # clamping shrank the round to the short corner edges
bpy.context.view_layer.objects.active = grip; bpy.ops.object.modifier_apply(modifier="round")
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, (GY0 + D2) / 2, LEDGE + 0.05))
top_cut = bpy.context.object; top_cut.scale = (W * 1.5, 0.05, 0.1)
m = grip.modifiers.new("cut", "BOOLEAN"); m.operation = "DIFFERENCE"; m.object = top_cut; m.solver = "EXACT"
bpy.context.view_layer.objects.active = grip; bpy.ops.object.modifier_apply(modifier="cut")
bpy.data.objects.remove(top_cut, do_unlink=True)
# the USB-C tab rises from the grip's top as part of the same piece
# silver on Howard's unit (IMG_8505-8506), with grooves across its back
M_TAB = mat("tab_silver", srgb("b4b6ba"), metal=1.0, rough=0.32)
tab = slab("tab", outline(UB0, LEDGE, UB1, UTOP, 0.0006, 0.0006, 0, 0, 6), GY0, D2, M_TAB, bevel=0)
m = grip.modifiers.new("tab", "BOOLEAN"); m.operation = "UNION"; m.object = tab; m.solver = "EXACT"; m.material_mode = "TRANSFER"
bpy.context.view_layer.objects.active = grip; bpy.ops.object.modifier_apply(modifier="tab")
bpy.data.objects.remove(tab, do_unlink=True)
# shoulder buttons standing on the ledge, R1 R2 | USB-C | L2 L1 as seen from behind (Howard's close-up):
# R1 0-23.6 %, R2 24-38.8 %, USB-C 39.4-58.6 %, L2 59-73.3 %, L1 73.8-100 % of the width; R1 and L1 reach
# the edges, lower and with a rounded lip; R2 and L2 stand a little taller; names on top
def prism_z(name, pts_xy, z0, z1, material):
    """An outline in x and y, extruded from z0 up to z1."""
    me_ = bpy.data.meshes.new(name); bm = bmesh.new()
    lo = [bm.verts.new((x, y, z0)) for x, y in pts_xy]; hi = [bm.verts.new((x, y, z1)) for x, y in pts_xy]
    bm.faces.new(lo[::-1]); bm.faces.new(hi)
    for i in range(len(pts_xy)):
        j = (i + 1) % len(pts_xy); bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm.normal_update(); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me_); bm.free()
    o_ = bpy.data.objects.new(name, me_); scene.collection.objects.link(o_); o_.data.materials.append(material)
    return o_
def shoulder(name, x0_, x1_, base, h, outer_sign, material):
    """A shoulder button as two parts (Howard's close-ups, TrimUI's top view and back-buttons render): a
    cap whose top rolls over the back in a large radius and ends in a lip that overhangs the body below,
    and the narrower body under it. R1 and L1 (outer_sign +1 or -1) round off at their outer end in
    plan like a bullet, the cap's lip running round that end too. Returns the cap, for carving."""
    y0, yb, lip = D1 + 0.00005, D2 - 0.0002, 0.0   # against the body, no slot (TrimUI side views)     # one block, no lip (TrimUI photo 14)
    r_end = W * 0.03 if outer_sign else 0.0008                   # the outer end in plan
    def plan(inset):
        xa, xb = x0_, x1_
        if outer_sign > 0: xb -= inset
        if outer_sign < 0: xa += inset
        rb = max(0.0005, r_end - inset)
        # outline() corners: (left, front), (right, front), (right, back), (left, back) in x and y here
        rlf, rrf = 0.0010, 0.0010   # rounder in plan (TrimUI top view)
        rrb = rb if outer_sign > 0 else 0.0006
        rlb = rb if outer_sign < 0 else 0.0006
        if not outer_sign: return outline(xa, y0, xb, yb + lip - inset, 0, 0, 0, 0, 1)   # square in plan; the bevel rounds every edge alike
        return outline(xa, y0, xb, yb + lip - inset, rlb, rrb, rrf, rlf, 14)
    z_lip = base + 0.0002 if outer_sign else base                # upright blocks stand on their light strips (IMG_8508)
    body = prism_z(name + "_body", plan(lip), base, z_lip + 0.0004, material) if outer_sign else None
    if not outer_sign:
        # TrimUI photo 14 and the side views: a block whose top rolls over the back in a broad round and
        # over the front in a small one; its back face is flat down to the grip. Drawn as that side
        # profile and pushed across the button's width.
        prof_ = outline(y0, z_lip, yb + lip, base + h, 0.0010, 0.0044, 0.00005, 0.00005, 16)   # the broad roll over the back (TrimUI side views; Howard's photos IMG_8505-8507)
        cap = prism(name, prof_, "x", max(x0_, -W / 2 + 0.00012), min(x1_, W / 2 - 0.00012)); cap.data.materials.clear(); cap.data.materials.append(material)   # kept 0.12 mm inside the side face: a shared face flickered black
    else:
        # R1 / L1: seen from behind, the cap curls down over the outer end. CURL is traced from Howard's
        # close-up of R1: u runs from the tip (0) to the inner end (1), v is how far the lip has dropped,
        # as a share of the back face's height under it. The tip overhangs the body by TIP_OUT.
        CURL = [(0.0, 0.62), (0.02, 0.48), (0.06, 0.33), (0.12, 0.24), (0.30, 0.12), (0.60, 0.04), (1.0, 0.0)]
        def drop(u):
            for (ua, va), (ub, vb) in zip(CURL, CURL[1:]):
                if u <= ub: return va + (vb - va) * (u - ua) / (ub - ua)
            return 0.0
        tc, top = base + h - z_lip, base + h
        face = z_lip - base
        xi, xo = (x0_, x1_) if outer_sign > 0 else (x1_, x0_)
        wid = abs(x1_ - x0_)
        us = [i / 60 for i in range(61)]
        us = sorted(set(us + [0.005, 0.01, 0.02, 0.03]))
        xo -= outer_sign * 0.0003      # the outer end stays just inside the device's side (Howard: no overhang)
        wid = abs(xo - xi)
        flare = lambda u: 0.0
        upper = [(xo - outer_sign * u * wid + flare(u), top - drop(u) * face) for u in us]
        lower = [(xo - outer_sign * u * wid + flare(u), top - tc - drop(u) * face) for u in us]
        prof = upper[::-1] + lower        # from the inner end along the top to the tip, back along the lip
        if outer_sign < 0: prof = prof[::-1]
        me_ = bpy.data.meshes.new(name); bm = bmesh.new()
        F = [bm.verts.new((x, y0, z)) for x, z in prof]; B = [bm.verts.new((x, yb + lip, z)) for x, z in prof]
        bm.faces.new(F); bm.faces.new(B[::-1])
        for i in range(len(prof)):
            j = (i + 1) % len(prof); bm.faces.new((F[i], F[j], B[j], B[i]))
        bm.normal_update(); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me_); bm.free()
        cap = bpy.data.objects.new(name, me_); scene.collection.objects.link(cap); cap.data.materials.append(material)
        TIP_OUT = 0.05
        xs = xo - outer_sign * TIP_OUT * wid
        # the body stops short of the curl
        for v in body.data.vertices:
            if (v.co.x - xs) * outer_sign > 0: v.co.x = xs
    bv = cap.modifiers.new("roll", "BEVEL"); bv.width = 0.0009 if not outer_sign else 0.0026; bv.segments = 6 if not outer_sign else 12; bv.use_clamp_overlap = bool(outer_sign); bv.limit_method = "ANGLE"; bv.angle_limit = math.radians(50)
    bv.profile = 0.5
    if body:
        bb = body.modifiers.new("bevel", "BEVEL"); bb.width = 0.0005; bb.segments = 4; bb.limit_method = "ANGLE"; bb.angle_limit = math.radians(50)
    return cap, (y0 + yb) / 2 - 0.0002
for (a, b, lab, outer) in [(0.0, 0.236, "R1", True), (0.240, 0.388, "R2", False), (0.590, 0.733, "L2", False), (0.738, 1.0, "L1", True)]:
    u0, u1 = bu(b), bu(a)
    sign = 0   # TrimUI photo 14: all four are upright blocks; R1 and L1 no longer curl over their outer ends
    h = W * (0.108 if outer else 0.118) + 0.0007 - LED_H   # the strip below takes 2 mm of the height, so the row stays as tall; TrimUI side views: the row stands 0.7 mm taller than photo 14 read; TrimUI photo 14, measured from the grip's top: R1/L1 0.108 W, R2/L2 0.118 W
    btn, yc = shoulder("shoulder_" + lab, u0, u1, LEDGE + LED_H, h, sign, M_SHOULDER)
    # the frosted light strip under the cap (IMG_8508): a little narrower than the cap, set back from its face
    _xa, _xb = min(u0, u1), max(u0, u1)
    _xa = max(_xa, -W / 2 + 0.0015) + 0.0010; _xb = min(_xb, W / 2 - 0.0015) - 0.0010
    slab("led_strip", outline(_xa, LEDGE - 0.0005, _xb, LEDGE + LED_H + 0.0003, 0.0002, 0.0002, 0.0001, 0.0001, 3),
         D1 + 0.0005, D2 - 0.0007, M_STRIP, bevel=0.00015, segs=2)
    if outer:   # R1 and L1 follow the grip's side round at their outer back corner; cut on the clean block, before its round
        rr_ = RG - 0.0002
        trim = prism_z("trim", outline(x0, D1 - 0.002, x1, D2 - 0.0002, rr_, rr_, 0.00005, 0.00005, 18), LEDGE - 0.01, LEDGE + 0.03, M_SHOULDER)
        rl = btn.modifiers["roll"]; btn.modifiers.remove(rl)
        intersect(btn, trim)
        # seen from behind, the outer top corner rounds down over the end (Howard's photos IMG_8506-8507)
        xo_ = W / 2 if u1 > 0 else -W / 2
        xl_, xr_ = (min(u0, u1) - 0.001, xo_) if xo_ > 0 else (xo_, max(u0, u1) + 0.001)
        ro_ = 0.0032
        end_ = slab("end", outline(xl_, LEDGE - 0.01, xr_, LEDGE + LED_H + h, ro_ if xo_ < 0 else 0.00005, ro_ if xo_ > 0 else 0.00005, 0.00005, 0.00005, 32),
                    D1 - 0.003, D2 + 0.001, M_SHOULDER, bevel=0)
        intersect(btn, end_)
        bv = btn.modifiers.new("roll", "BEVEL"); bv.width = 0.0009; bv.segments = 6; bv.limit_method = "ANGLE"; bv.angle_limit = math.radians(50); bv.profile = 0.5
    # the name carved into the cap's flat top, on its inner part
    lx = (u0 + u1) / 2 if not outer else ((u0 + u1) / 2 - sign * (u1 - u0) * 0.10)
    lz = LEDGE + LED_H + h - 0.00015
    t = text(lab, lx, lz, W * 0.092, M_SHOULDER, yc, align="CENTER", depth=0.0004)
    t.data.font = bpy.data.fonts.load(font_file("Avenir-Heavy.ttf", "Bold"))
    t.rotation_euler = (0, 0, math.radians(180)); t.location = (lx, yc, lz)
    bpy.context.view_layer.objects.active = t; t.select_set(True); bpy.ops.object.convert(target="MESH")
    bpy.context.view_layer.objects.active = btn
    bpy.ops.object.modifier_apply(modifier="roll")
    m = btn.modifiers.new("carve", "BOOLEAN"); m.operation = "DIFFERENCE"; m.object = t; m.solver = "EXACT"; m.use_self = True
    bpy.ops.object.modifier_apply(modifier="carve")
    bpy.data.objects.remove(t, do_unlink=True)
    for o_ in [x_ for x_ in (btn, bpy.data.objects.get("shoulder_" + lab + "_body")) if x_]:  # smooth only across the rounded edges
        bpy.ops.object.select_all(action="DESELECT"); o_.select_set(True); bpy.context.view_layer.objects.active = o_
        bpy.ops.object.shade_auto_smooth(angle=math.radians(55))   # smooth across the rounded ends too; 35 left them faceted
    bpy.ops.object.select_all(action="DESELECT")
# USB-C, read in 3D from TrimUI's top view, its back-buttons close-up, photo 8 and Howard's notes: one
# metal piece between R2 and L2, lower than the buttons, its back face grooved (the ribbed plate is the
# same piece), and the port a real hole cut down into its top, lined in steel, with the tongue inside.
def cut(target, cutter, self_=False):
    _n0 = len(target.data.polygons)
    m = target.modifiers.new("cut_" + cutter.name, "BOOLEAN"); m.operation = "DIFFERENCE"; m.object = cutter
    m.solver = "EXACT"; m.material_mode = "TRANSFER"; m.use_self = self_   # self: joined cutters that overlap one another
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=m.name)
    if os.environ.get("BRICK_DBG"): print("CUT", target.name, cutter.name, _n0, "->", len(target.data.polygons))
    bpy.data.objects.remove(cutter, do_unlink=True)
def cut_all(target, cutters):
    """One boolean for many cutters: they are joined first."""
    bpy.ops.object.select_all(action="DESELECT")
    for c in cutters: c.select_set(True)
    bpy.context.view_layer.objects.active = cutters[0]; bpy.ops.object.join()
    cut(target, bpy.context.view_layer.objects.active, self_=True)
# a fine bright edge along the grip's top and round the USB-C tab (TrimUI photo 14), laid on the back face
M_EDGE = mat("edge", srgb("c2c3c7"), metal=1.0, rough=0.28)
_e = 0.00028
for (xa, za, xb, zb) in [(x0 + RG, LEDGE - _e, UB0, LEDGE), (UB1, LEDGE - _e, x1 - RG, LEDGE)]:
    slab("grip_edge", outline(min(xa, xb), min(za, zb), max(xa, xb), max(za, zb), 0, 0, 0, 0, 1), D2 - 0.00002, D2 + 0.00003, M_EDGE, bevel=0)
# four grooves across the tab's back, the lowest at the joint with the grip (IMG_8506)
cut_all(grip, [slab("tab_groove", outline(UB0 + 0.0005, zg - 0.00007, UB1 - 0.0005, zg + 0.00007, 0.00003, 0.00003, 0.00003, 0.00003, 1), D2 - 0.00015, D2 + 0.001, M_TAB, bevel=0)
               for zg in [UTOP - f * (UTOP - LEDGE) for f in (0.32, 0.57, 0.78)] + [LEDGE + 0.00009]])
# the port: a rounded slot cut 3 mm down into the top; its walls take the steel of the cutter
ucx, ucy = (UB0 + UB1) / 2, (D1 + D2) / 2 + 0.0004
cut(grip, prism_z("port", outline(ucx - 0.0044, ucy - 0.0013, ucx + 0.0044, ucy + 0.0013, 0.0013, 0.0013, 0.0013, 0.0013, 12),
                  UTOP - 0.0030, UTOP + 0.001, M_PORT))
bpy.context.view_layer.objects.active = grip; grip.select_set(True)
bpy.ops.object.shade_auto_smooth(angle=math.radians(35)); grip.select_set(False)
prism_z("port_floor", outline(ucx - 0.0043, ucy - 0.0012, ucx + 0.0043, ucy + 0.0012, 0.0012, 0.0012, 0.0012, 0.0012, 12),
        UTOP - 0.0030, UTOP - 0.0028, M_HOLE)
prism_z("port_tongue", outline(ucx - 0.0033, ucy - 0.00035, ucx + 0.0033, ucy + 0.00035, 0.0002, 0.0002, 0.0002, 0.0002, 4),
        UTOP - 0.0029, UTOP - 0.0006, M_HOLE)
# the grip panel: "BRICK HAMMER" spaced wide, raised ribs, the marks in a band, four screw holes
# the grip panel, measured on Howard's straight-on photo in units of the width W, down from its top:
# the name at 0.12, Torx screws at the corners, rounded grooves every 0.02 from 0.165, and an engraved
# house-like outline in the middle: a roof 0.24 wide rising to 0.19, two posts down from 0.26
# the name is cut into the grip (Howard: countersunk, not raised)
# measured on IMG_8457: centred 0.10 W below the grip's top, 0.34 W wide, capitals 0.023 W tall
# cut in the grip's own metal, so it reads by its edges catching light, not as dark paint; letters broad
t = text("BRICK HAMMER", bu(0.507), 0.713 * W, W * 0.046, M_BACK, D2, face="back", align="CENTER", depth=0.00026)   # IMG_8506 flattened
t.data.font = bpy.data.fonts.load(font_file("Avenir-Medium.ttf", "Bold")); t.data.space_character = 1.15
t.scale = (1.18, 1.0, 1.0)
bpy.ops.object.select_all(action="DESELECT"); t.select_set(True); bpy.context.view_layer.objects.active = t
bpy.ops.object.convert(target="MESH")
cut(grip, t, self_=True)   # letters' outlines can overlap in some copies of the font
# The grip's back, measured on Howard's IMG_8506 flattened onto the model through its four screw holes
# (u across as seen from behind, z up from the bottom, both in W). Fine cut lines, a blank band where the
# text and marks are printed, and the TrimUI honeycomb set shallowly into the lines; nothing is cut in the band.
M_CUT = mat("cut", srgb(BODY_HEX[1]), metal=1.0, rough=0.12)
M_CHAMFER = M_BACK   # the honeycomb's slope has the back's own finish; it shows only by catching the light
LW_, LD_ = 0.0025 * W, 0.00024                         # line width and depth: deeper than the honeycomb, so the lines run on through it
UPPER_Z = [0.655 * W - i * 0.0277 * W for i in range(12)]   # twelve lines from 0.655 to 0.350
LOWER_Z = [0.245 * W, 0.203 * W, 0.162 * W]                  # three below the band
BAND_LO, BAND_HI = 0.255 * W, 0.341 * W                      # the blank band for the print
grooves = []
def cutline(name, pts, w_):
    """A straight cut of width w_ along pts[0] -> pts[1], in the back view's x and z."""
    (xa, za), (xb, zb) = pts; dx, dz = xb - xa, zb - za; L = math.hypot(dx, dz); nx, nz = -dz / L * w_ / 2, dx / L * w_ / 2
    grooves.append(slab(name, [(xa + nx, za + nz), (xb + nx, zb + nz), (xb - nx, zb - nz), (xa - nx, za - nz)], D2 - LD_, D2 + 0.001, M_CUT, bevel=0))
for i, z in enumerate(UPPER_Z + LOWER_Z):
    ua, ub = (0.100, 0.900) if i < 2 else (0.072, 0.930)     # the top two stop short, between the screw holes
    cutline(f"groove{i}", [(bu(ua), z), (bu(ub), z)], LW_)
# the honeycomb: three pointed-top hexagons, side 0.121 W, spaced apart; the top one at (0.508, 0.513),
# the lower two at (0.378, 0.292) and (0.638, 0.292), centred in the band, where they are not cut
HEX_S, HEX_D, HEX_C = 0.121 * W, 0.00004, 0.0007   # gentler (Howard)   # side, depth, and the chamfer's width across the face (IMG_8508)
_hc = [(bu(0.508), 0.513 * W), (bu(0.378), 0.292 * W), (bu(0.638), 0.292 * W)]
def hexagon(cx, cz, r):
    return [(cx + r * math.cos(math.radians(90 + 60 * k)), cz + r * math.sin(math.radians(90 + 60 * k))) for k in range(6)]
def hex_cutter(cx, cz):
    """A shallow hexagonal pocket whose walls are a wide chamfer, its outer edge on the hexagon; the band is
    taken out of it so nothing is cut where the print goes."""
    me_ = bpy.data.meshes.new("logo_hex"); bm_ = bmesh.new()
    k_ = HEX_C / HEX_D / math.cos(math.radians(30))
    r_floor = HEX_S - HEX_C / math.cos(math.radians(30))
    lo = [bm_.verts.new((x, D2 - HEX_D, z)) for x, z in hexagon(cx, cz, r_floor)]
    hi = [bm_.verts.new((x, D2 + 0.0004, z)) for x, z in hexagon(cx, cz, r_floor + k_ * (HEX_D + 0.0004))]
    bm_.faces.new(lo[::-1]); bm_.faces.new(hi)
    walls = [bm_.faces.new((lo[i], lo[(i + 1) % 6], hi[(i + 1) % 6], hi[i])) for i in range(6)]
    bmesh.ops.recalc_face_normals(bm_, faces=bm_.faces)
    for f in walls: f.material_index = 1
    bm_.to_mesh(me_); bm_.free()
    c_ = bpy.data.objects.new("logo_hex", me_); scene.collection.objects.link(c_)
    c_.data.materials.append(M_BACK); c_.data.materials.append(M_CHAMFER)
    band = slab("band", outline(x0 - 0.01, BAND_LO, x1 + 0.01, BAND_HI, 0.0001, 0.0001, 0.0001, 0.0001, 1), D2 - 0.002, D2 + 0.002, M_BACK, bevel=0)
    cut(c_, band)
    return c_
# a crisp groove, like the grip lines, along each pocket's inner edge where the slope meets the floor (IMG_8508),
# left out across the band
def clip_band(pa, pb):
    """The parts of segment pa-pb outside the band, as a list of segments."""
    (xa, za), (xb, zb) = pa, pb
    if abs(zb - za) < 1e-12:
        return [] if BAND_LO < za < BAND_HI else [(pa, pb)]
    pt = lambda t: (xa + (xb - xa) * t, za + (zb - za) * t)
    t_lo, t_hi = sorted(((BAND_LO - za) / (zb - za), (BAND_HI - za) / (zb - za)))
    out = []
    if t_lo > 0: out.append((pa, pt(min(t_lo, 1))))
    if t_hi < 1: out.append((pt(max(t_hi, 0)), pb))
    return [(p, q) for p, q in out if math.hypot(q[0] - p[0], q[1] - p[1]) > 0.0004]   # no slivers: they broke the boolean
cut_all(grip, grooves)   # the straight lines first (cut after the pockets, they failed), then the pockets, then their outlines
cut_all(grip, [hex_cutter(cx_, cz_) for cx_, cz_ in _hc])
grooves = []
for n_, (cx_, cz_) in enumerate(_hc):
    v_ = hexagon(cx_, cz_, HEX_S - HEX_C / math.cos(math.radians(30)) + LW_ * 0.3)
    for k in range(6):
        (xa, za), (xb, zb) = v_[k], v_[(k + 1) % 6]
        ex, ez = (xb - xa) * 0.01, (zb - za) * 0.01
        for j_, (p_, q_) in enumerate(clip_band((xa - ex, za - ez), (xb + ex, zb + ez))):
            cutline(f"hexline{n_}_{k}_{j_}", [p_, q_], LW_ * 0.3)   # finer than the grip lines (IMG_8508)
for g_ in grooves: cut(grip, g_)   # one at a time: joined, their crossing corners broke the boolean
def groove_line(name, pts, w_=0.00014, depth=0.00018):
    """A fine cut along a polyline: each segment a thin box pushed into the grip's back face."""
    parts = []
    for i, ((xa, za), (xb, zb)) in enumerate(zip(pts, pts[1:])):
        dx, dz = xb - xa, zb - za; L = math.hypot(dx, dz); nx, nz = -dz / L * w_, dx / L * w_
        ex, ez = dx / L * w_, dz / L * w_          # a little past each end, so the corners close
        quad = [(xa - ex + nx, za - ez + nz), (xb + ex + nx, zb + ez + nz), (xb + ex - nx, zb + ez - nz), (xa - ex - nx, za - ez - nz)]
        me_ = bpy.data.meshes.new(f"{name}{i}"); bm = bmesh.new()
        F = [bm.verts.new((x, D2 - depth, z)) for x, z in quad]; B = [bm.verts.new((x, D2 + 0.001, z)) for x, z in quad]
        bm.faces.new(F); bm.faces.new(B[::-1])
        for j in range(4):
            k = (j + 1) % 4; bm.faces.new((F[j], F[k], B[k], B[j]))
        bm.normal_update(); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me_); bm.free()
        o_ = bpy.data.objects.new(f"{name}{i}", me_); scene.collection.objects.link(o_); parts.append(o_)
    return parts
# Torx screws at the four corners of the panel, silver and nearly flush (TrimUI photo 14: 0.069 W in from
# the side, 0.176 W below the grip's top and 0.122 W above its bottom, 0.042 W across)
for (sx, zc) in [(-1, LEDGE - 0.176 * W), (1, LEDGE - 0.176 * W), (-1, 0.122 * W), (1, 0.122 * W)]:
    xc = sx * (W / 2 - 0.069 * W)
    cyl("grip_screw", xc, zc, W * 0.021, D2 - 0.0002, D2 + 0.00006, M_SCREW, bevel=0.00004)   # flush (TrimUI side views)
    for a_ in (0, 60, 120):   # the Torx star, three dark slots across the head
        ca, sa = math.cos(math.radians(a_)), math.sin(math.radians(a_)); L_, w_ = W * 0.012, W * 0.0035
        pts = [(xc + ca * L_ - sa * w_, zc + sa * L_ + ca * w_), (xc - ca * L_ - sa * w_, zc - sa * L_ + ca * w_),
               (xc - ca * L_ + sa * w_, zc - sa * L_ - ca * w_), (xc + ca * L_ + sa * w_, zc + sa * L_ - ca * w_)]
        me_ = bpy.data.meshes.new("torx"); me_.from_pydata([(x, D2 + 0.00007, z) for x, z in pts], [], [(0, 1, 2, 3)])
        o_ = bpy.data.objects.new("torx", me_); scene.collection.objects.link(o_); o_.data.materials.append(M_HOLE)
# the printed marks low on the right as seen from behind (TrimUI photo 14): FC, CE and two fine lines of text
M_PRINT = mat("print", (0.002, 0.002, 0.002), rough=0.85)   # black print, matte
M_PRINT.node_tree.nodes["Principled BSDF"].inputs["Specular IOR Level"].default_value = 0.0   # ink: no sheen, black from every side
def flat(name, pts, y, material):
    """A printed shape: one flat face just proud of the grip's back."""
    me_ = bpy.data.meshes.new(name); me_.from_pydata([(x, y, z) for x, z in pts], [], [tuple(range(len(pts)))])
    o_ = bpy.data.objects.new(name, me_); scene.collection.objects.link(o_); o_.data.materials.append(material); return o_
def stroke(name, a, b, w_, y, material):
    (xa, za), (xb, zb) = a, b; L = math.hypot(xb - xa, zb - za); nx, nz = -(zb - za) / L * w_ / 2, (xb - xa) / L * w_ / 2
    return flat(name, [(xa + nx, za + nz), (xb + nx, zb + nz), (xb - nx, zb - nz), (xa - nx, za - nz)], y, material)
YP, ZP = D2 + 0.00004, 0.308 * W       # IMG_8506 flattened: the print's row 0.308 W above the bottom, in the blank band
# Measured on IMG_8506 flattened: three left-aligned lines from 0.077 across, the longest reaching 0.559
_pl = []
for k, line in enumerate(("MODEL: TG3040 . TRIMUI BRICK \u00b7 DC 5V/3000mA \u00b7 DESIGN BY",
                          "TRIMUI . MADE IN CHINA . Built in rechargeable li-po battery \u00b7",
                          "Only can use certified charger. The battery may explode in the fire.")):
    t = text(line, bu(0.077), ZP + 0.018 * W - k * 0.018 * W, W * 0.0172, M_PRINT, YP, face="back", align="LEFT", depth=0.00002)
    _pf = os.environ.get("BRICK_PRINT_FONT")   # the unit's print is a wide geometric face like Montserrat; Inter when none is given
    t.data.font = bpy.data.fonts.load(_pf if _pf and os.path.exists(_pf) else INTER.replace("Bold", "Regular")); _pl.append(t)
# no stretching: the longest line is brought to the measured width (0.482 W) by letter spacing alone
for _ in range(3):
    bpy.context.view_layer.update()
    _wide = max(t.dimensions.x for t in _pl)
    for t in _pl: t.data.space_character *= (0.482 * W) / _wide
def ring_arc(name, cx_, cz_, ro, ri, open_deg, n=28):
    """An open ring on the back, its gap facing the viewer's right (seen from behind, +x is on the left)."""
    a0, a1 = open_deg, 360 - open_deg
    out_ = [(cx_ - math.cos(math.radians(a0 + (a1 - a0) * q / n)) * ro, cz_ + math.sin(math.radians(a0 + (a1 - a0) * q / n)) * ro) for q in range(n + 1)]
    in_ = [(cx_ - math.cos(math.radians(a1 - (a1 - a0) * q / n)) * ri, cz_ + math.sin(math.radians(a1 - (a1 - a0) * q / n)) * ri) for q in range(n + 1)]
    return flat(name, out_ + in_, YP, M_PRINT)
# the FCC mark: a bold F, then a C holding a smaller c (TrimUI photo 14), 0.05 W tall
fx_, fh, fw = -0.097 * W, 0.025 * W, 0.0092 * W   # IMG_8506: FC spans 0.589 to 0.661
stroke("fc_f", (fx_, ZP - fh), (fx_, ZP + fh), fw, YP, M_PRINT)
stroke("fc_f", (fx_ + fw / 2, ZP + fh - fw / 2), (fx_ - 0.021 * W, ZP + fh - fw / 2), fw, YP, M_PRINT)
stroke("fc_f", (fx_ + fw / 2, ZP + 0.001 * W), (fx_ - 0.017 * W, ZP + 0.001 * W), fw * 0.9, YP, M_PRINT)
ring_arc("fc_c", -0.143 * W, ZP, 0.025 * W, 0.0168 * W, 52)
ring_arc("fc_c", -0.143 * W, ZP, 0.0122 * W, 0.0062 * W, 58)
# the CE mark: two open rings, the E's with a bar that stops short of its gap
ce_r, ce_t = 0.023 * W, 0.0082 * W
for cxk in (-0.211 * W, -0.252 * W):   # IMG_8506: C centred 0.706, E 0.746
    ring_arc("ce", cxk, ZP, ce_r, ce_r - ce_t, 62)
stroke("ce_bar", (-0.252 * W + ce_r - ce_t * 0.6, ZP), (-0.252 * W - ce_r * 0.45, ZP), ce_t * 0.85, YP, M_PRINT)
# the recycling mark: three bold arrows chasing round a rounded triangle; each runs along one side,
# bends round the corner and ends in a head on the next side
def disc(name, c_, r_, y, material, n=20):
    return flat(name, [(c_[0] + math.cos(2 * math.pi * q / n) * r_, c_[1] + math.sin(2 * math.pi * q / n) * r_) for q in range(n)], y, material)
def polystroke(name, pts, w_, y, material):
    for pa, pb in zip(pts, pts[1:]): stroke(name, pa, pb, w_, y, material)
    for pj in pts[1:-1]: disc(name, pj, w_ / 2, y, material)
rc, rr_ = Vector((-0.325 * W, ZP - 0.004 * W)), 0.031 * W   # IMG_8506: 0.794 to 0.851
cor = [rc + Vector((math.cos(math.radians(90 + j * 120)), math.sin(math.radians(90 + j * 120)))) * rr_ for j in range(3)]
wr = 0.0112 * W                                    # bold bands, as on TrimUI's mark
for j in range(3):
    # each arrow folds round one corner: its tail on one side, its head on the next, a gap before the next tail
    A, B, Cn = cor[j], cor[(j + 1) % 3], cor[(j + 2) % 3]
    d1 = (B - A).normalized(); d2 = (Cn - B).normalized(); L = (B - A).length
    start = A + d1 * 0.50 * L; bend0 = B - d1 * 0.14 * L; bend1 = B + d2 * 0.14 * L
    end = B + d2 * 0.20 * L
    polystroke("recycle", [tuple(start), tuple(bend0), tuple(B * 0.82 + rc * 0.18), tuple(bend1), tuple(end)], wr, YP, M_PRINT)
    n2 = Vector((-d2.y, d2.x))
    flat("recycle_tip", [tuple(end + n2 * wr * 1.25), tuple(end + d2 * wr * 1.45), tuple(end - n2 * wr * 1.25)], YP, M_PRINT)
# the crossed-out bin
bx, bw, bh, lw = -0.401 * W, 0.030 * W, 0.040 * W, 0.0035 * W
body_ = [(bx - bw / 2, ZP + bh / 2 - 0.006 * W), (bx + bw / 2, ZP + bh / 2 - 0.006 * W), (bx + bw * 0.38, ZP - bh / 2), (bx - bw * 0.38, ZP - bh / 2)]
for q in range(4): stroke("bin", body_[q], body_[(q + 1) % 4], lw, YP, M_PRINT)
stroke("bin_lid", (bx - bw * 0.62, ZP + bh / 2), (bx + bw * 0.62, ZP + bh / 2), lw, YP, M_PRINT)
stroke("bin_x1", (bx - bw * 0.7, ZP + bh * 0.62), (bx + bw * 0.7, ZP - bh * 0.62), lw, YP, M_PRINT)
stroke("bin_x2", (bx + bw * 0.7, ZP + bh * 0.62), (bx - bw * 0.7, ZP - bh * 0.62), lw, YP, M_PRINT)
stroke("bin_bar", (bx + bw * 0.62, ZP - bh * 0.78), (bx - bw * 0.62, ZP - bh * 0.78), lw * 1.8, YP, M_PRINT)   # the bar under it
# upper back: two silver Torx screws, the three-dot mark
for u in (0.09, 0.91):
    cyl("screw", bu(u), fz(0.042), W * 0.02, D1 - 0.0001, D1 + 0.00006, M_SCREW, bevel=0.00004)   # flush (TrimUI side views)
    xc, zc = bu(u), fz(0.042)
    for a_ in (0, 60, 120):   # a Torx star like the grip's screws (TrimUI photo 14)
        ca, sa = math.cos(math.radians(a_)), math.sin(math.radians(a_)); L_, w_ = W * 0.011, W * 0.0033
        pts = [(xc + ca * L_ - sa * w_, zc + sa * L_ + ca * w_), (xc - ca * L_ - sa * w_, zc - sa * L_ + ca * w_),
               (xc - ca * L_ + sa * w_, zc - sa * L_ - ca * w_), (xc + ca * L_ + sa * w_, zc + sa * L_ - ca * w_)]
        me_ = bpy.data.meshes.new("torx"); me_.from_pydata([(x, D1 + 0.00007, z) for x, z in pts], [], [(0, 1, 2, 3)])
        o_ = bpy.data.objects.new("torx", me_); scene.collection.objects.link(o_); o_.data.materials.append(M_HOLE)
# the TrimUI mark: three small pointed-top hexagons, one above two, as a honeycomb (TrimUI photo 14, IMG_8506)
_ms, _mg = W * 0.0105, 1.10                       # side, and spacing so a hairline of metal parts them
_mc = (0.0, fz(0.282))
for dx_, dz_ in ((0, 0.75), (-math.sqrt(3) / 2, -0.75), (math.sqrt(3) / 2, -0.75)):
    cx_, cz_ = _mc[0] + dx_ * _ms * _mg, _mc[1] + dz_ * _ms * _mg
    slab("mark", [(cx_ + _ms * math.cos(math.radians(90 + 60 * k)), cz_ + _ms * math.sin(math.radians(90 + 60 * k))) for k in range(6)],
         D1 - 0.00005, D1 + 0.00005, M_HOLE, bevel=0)
# the label: black glass from the back's top edge, its light pipe set into the top edge above it
slab("top_led", outline(bu(0.77), H - 0.0003, bu(0.23), H - 0.00002, 0.0002, 0.0002, 0.0002, 0.0002, 2),
     D1 - 0.0028, D1 - 0.0002, M_LED, bevel=0)   # flush with the top and set in from the back (TrimUI side views)
slab("label", outline(bu(0.77), fz(0.113), bu(0.23), H - 0.0003, 0.0003, 0.0003, W * 0.03, W * 0.03, 8), D1 - 0.0001, D1 + 0.00003, M_LABEL, bevel=0.0001, segs=1)   # flush with the back (TrimUI side views)
slab("label_bar", outline(bu(0.77), fz(0.030), bu(0.23), H - 0.0003, 0.0002, 0.0002, 0.0001, 0.0001, 2), D1 - 0.0001, D1 + 0.0003, mat("labelbar", srgb("c6c8cc"), metal=0.6, rough=0.35), bevel=0)   # thicker and brighter (Howard, TrimUI photo 14: about 1/7 of the label)   # the light bar along its top
t = text("BRICK HAMMER", 0, fz(0.054), W * 0.031, M_LABEL_TEXT, D1 + 0.00005, face="back", depth=0.00002, align="CENTER")
t.data.font = bpy.data.fonts.load(font_file("Avenir-Medium.ttf", "Regular")); t.data.space_character = 1.35
t = text("BRICK METAL VERSION DESIGNED BY TRIMUI", 0, fz(0.076), W * 0.012, M_LABEL_TEXT, D1 + 0.00005, face="back", depth=0.00002, align="CENTER")
t.data.font = bpy.data.fonts.load(font_file("Avenir-Medium.ttf", "Regular")); t.data.space_character = 1.15
# front: glass across the top 52 %, the display inside it
slab("glass", outline(x0 + 0.0004, fz(0.517), x1 - 0.0004, H - 0.0004, R_TOP * 0.8, R_TOP * 0.8, 0.0003, 0.0003),
     -0.0001, 0.0002, M_GLASS, bevel=0.00008, segs=2)   # near flush (TrimUI side views)
me = bpy.data.meshes.new("display")
dx0, dx1, dz0, dz1 = fx(0.022), fx(0.978), fz(0.490), fz(0.014)
me.from_pydata([(dx0, -0.00011, dz0), (dx1, -0.00011, dz0), (dx1, -0.00011, dz1), (dx0, -0.00011, dz1)], [], [(0, 1, 2, 3)])
me.uv_layers.new(); uv = me.uv_layers[0].data
for i, c in enumerate([(0, 0), (1, 0), (1, 1), (0, 1)]): uv[i].uv = c
disp = bpy.data.objects.new("display", me); scene.collection.objects.link(disp); disp.data.materials.append(M_SCREEN)

# d-pad: a cross, raised, in a shallow dark well
cx, cz = fx(0.222), fz(0.653); arm, th = W * 0.135 - 0.0005, W * 0.046 - 0.0005   # inside a thicker dark border (TrimUI views)
def rounded_poly(corners, radii, seg=10):
    """A polygon with each corner replaced by an arc tangent to its two edges (convex or concave)."""
    pts, n = [], len(corners)
    for i in range(n):
        p0, p1, p2 = Vector(corners[i - 1]), Vector(corners[i]), Vector(corners[(i + 1) % n])
        a, b = (p0 - p1).normalized(), (p2 - p1).normalized()
        half = math.acos(max(-1.0, min(1.0, a.dot(b)))) / 2
        r = radii[i]; d = r / math.tan(half)
        t0, t1 = p1 + a * d, p1 + b * d
        c = p1 + (a + b).normalized() * (r / math.sin(half))
        a0, a1 = math.atan2(t0.y - c.y, t0.x - c.x), math.atan2(t1.y - c.y, t1.x - c.x)
        da = (a1 - a0 + math.pi) % (2 * math.pi) - math.pi
        for k in range(seg + 1):
            ang = a0 + da * k / seg
            pts.append((c.x + r * math.cos(ang), c.y + r * math.sin(ang)))
    return pts
# one piece, as on the device: a cross with rounded outer corners and filleted inner corners (TrimUI's
# close-up), its top rounded over; in the middle a shallow round dish with the three-dot mark sunk in it
a_, t_ = arm, th
cross = [(t_, a_), (-t_, a_), (-t_, t_), (-a_, t_), (-a_, -t_), (-t_, -t_), (-t_, -a_), (t_, -a_), (t_, -t_), (a_, -t_), (a_, t_), (t_, t_)]
rad = [0.0011, 0.0011, 0.0006, 0.0011, 0.0011, 0.0006, 0.0011, 0.0011, 0.0006, 0.0011, 0.0011, 0.0006]
DP_H = 0.0014                       # the d-pad stands 1.4 mm proud (TrimUI photos 3 and 9); its mark is cut in (Howard, 2026-11-04)
dp = slab("dpad", [(cx + x, cz + z) for x, z in rounded_poly(cross, rad)], -DP_H, 0.0002, M_FACE, bevel=0)
# the dark gap around it follows the cross's own outline
g_ = 0.0010
gap = [(x + (g_ if x > 0 else -g_) * (abs(x) > t_ - 1e-9), z + (g_ if z > 0 else -g_) * (abs(z) > t_ - 1e-9)) for x, z in cross]
_gp = [(cx + x, cz + z) for x, z in rounded_poly(gap, [r_ + 0.0004 if r_ > 0.001 else 0.0004 for r_ in rad])]
slab("dpad_well", _gp, -0.00005, 0.0003, M_HOLE, bevel=0)
dw = dp.data.attributes.new("bevel_weight_edge", "FLOAT", "EDGE")
for e in dp.data.edges:
    v0, v1 = dp.data.vertices[e.vertices[0]].co, dp.data.vertices[e.vertices[1]].co
    if v0.y < -(DP_H - 0.00001) and v1.y < -(DP_H - 0.00001): dw.data[e.index].value = 1.0
db = dp.modifiers.new("round", "BEVEL"); db.width = 0.0005; db.segments = 6; db.limit_method = "WEIGHT"; db.use_clamp_overlap = False
bpy.context.view_layer.objects.active = dp; bpy.ops.object.modifier_apply(modifier="round")
rr, depth = t_ * 0.9, 0.00035                       # the dish: 0.8 of the arm's width across, 0.25 mm deep
R = (rr * rr + depth * depth) / (2 * depth); yb = -DP_H + depth
# a crisp round recess with a dark floor (TrimUI's front view), not a soft dish
bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=rr, depth=0.001, location=(cx, -DP_H - 0.0005 + depth, cz), rotation=(math.radians(90), 0, 0))
bpy.context.object.data.materials.append(M_DISH); cut(dp, bpy.context.object)
_floor = -DP_H + depth
cut_all(dp, [cyl("dpad_dot", cx + math.cos(math.radians(a)) * 0.00125, cz + math.sin(math.radians(a)) * 0.00125, 0.00072,
                 _floor - 0.0003, _floor + 0.00015, M_LETTER, bevel=0) for a in (90, 210, 330)])   # the mark: three small circles cut into the dish
bpy.ops.object.select_all(action="DESELECT"); dp.select_set(True); bpy.context.view_layer.objects.active = dp
bpy.ops.object.shade_auto_smooth(angle=math.radians(40)); bpy.ops.object.select_all(action="DESELECT")
# face buttons with their letters
for l, u, v in [("X", 0.778, 0.586), ("Y", 0.675, 0.654), ("A", 0.876, 0.654), ("B", 0.778, 0.722)]:
    cyl("btn_gap", fx(u), fz(v), W * 0.059, -0.00003, 0.0, M_HOLE, bevel=0)   # the dark gap round each button (TrimUI views)
    # 1.5 mm proud with sides (TrimUI photos 3 and 9); the letter cut 0.18 mm into the top (Howard, 2026-11-04)
    b_ = solid_button("btn_" + l, fx(u), fz(v), W * 0.052, -0.0011, 0.0002, W * 0.051, 0.0004, M_FACE, 0.0004)
    apply_mods(b_)
    tm_ = text_mesh(l, fx(u), fz(v), W * 0.068, M_FACE_CUT, -0.0015, depth=0.00018, align="CENTER")
    if l == "A":
        # Inter draws A as the legs plus a separate crossbar that overlaps them; filled together the overlap
        # becomes a hole. Build A's cutter instead as one outline, from the glyph's own points (units of 2048):
        # the legs and bar joined, the counter above the bar taken out. Placed over the text's own box.
        cs_ = [tm_.matrix_world @ Vector(c) for c in tm_.bound_box]
        xa_, xb_ = min(c.x for c in cs_), max(c.x for c in cs_); za_, zb_ = min(c.z for c in cs_), max(c.z for c in cs_)
        ya_, yb_ = min(c.y for c in cs_), max(c.y for c in cs_)
        bpy.data.objects.remove(tm_, do_unlink=True)
        gp = lambda g: (xa_ + (g[0] - 67) / (2038 - 67) * (xb_ - xa_), za_ + g[1] / 2048 * (zb_ - za_))
        outer_ = [(67, 0), (531, 0), (682.9, 467), (1422.4, 467), (1574, 0), (2038, 0), (1332, 2048), (774, 2048)]
        counter_ = [(792.9, 805), (1312.6, 805), (1061, 1580), (1045, 1580)]
        tm_ = slab("a_cut", [gp(g) for g in outer_], ya_, yb_, M_FACE_CUT, bevel=0)
        cut(tm_, slab("a_counter", [gp(g) for g in counter_], ya_ - 0.001, yb_ + 0.001, M_FACE_CUT, bevel=0))
    cut(b_, tm_, self_=True)
    reshade(b_); flat_cuts(b_)
# select and start
for u in (0.426, 0.572):
    slab("pill", outline(fx(u) - W * 0.048, fz(0.579) - W * 0.012, fx(u) + W * 0.048, fz(0.579) + W * 0.012,
                        W * 0.0119, W * 0.0119, W * 0.0119, W * 0.0119, 8), -0.0006, 0.0002, M_PILL_LIT, bevel=0.0003)
    g_ = W * 0.005
    _pg = outline(fx(u) - W * 0.048 - g_, fz(0.579) - W * 0.012 - g_, fx(u) + W * 0.048 + g_, fz(0.579) + W * 0.012 + g_,
                  W * 0.0119 + g_, W * 0.0119 + g_, W * 0.0119 + g_, W * 0.0119 + g_, 8)
    slab("pill_gap", _pg, -0.00003, 0.0, M_HOLE, bevel=0)
# the three round buttons along the bottom, and the speaker holes
MENU = {}
for u in (0.364, 0.503, 0.643):
    cyl("menu_gap", fx(u), fz(0.918), W * 0.045, -0.00003, 0.0, M_HOLE, bevel=0)
    MENU[u] = solid_button("menu", fx(u), fz(0.918), W * 0.039, -0.0007, 0.0002, W * 0.038, 0.0003, M_FACE, 0.0003)   # 1 mm proud (TrimUI photo 3)
# their marks, cut 0.15 mm into the tops (Howard, 2026-11-04): the three dots, a square, a play arrow
MT0, MT1 = -0.00135, -0.00085
cut_all(MENU[0.364], [cyl("m_dot", fx(0.364) + math.cos(math.radians(a)) * W * 0.011, fz(0.918) + math.sin(math.radians(a)) * W * 0.011, W * 0.0085,
                          MT0, MT1, M_FACE_CUT, bevel=0) for a in (90, 210, 330)])
cut(MENU[0.503], slab("m_sq", outline(fx(0.503) - W * 0.016, fz(0.918) - W * 0.011, fx(0.503) + W * 0.016, fz(0.918) + W * 0.011,
                     0.0002, 0.0002, 0.0002, 0.0002, 2), MT0, MT1, M_FACE_CUT, bevel=0))
c = (fx(0.643), fz(0.918)); k = W * 0.016
cut(MENU[0.643], slab("m_play", [(c[0] - k * 0.6, c[1] + k), (c[0] + k, c[1]), (c[0] - k * 0.6, c[1] - k)], MT0, MT1, M_FACE_CUT, bevel=0))
for b_ in MENU.values(): reshade(b_); flat_cuts(b_)
for su in (0.165, 0.851):
    for r, n in ((0, 8), (1, 7)):  # two staggered rows, 8 over 7 (TrimUI front view)
        for k in range(n):
            cyl("spk", fx(su) + (k - (n - 1) / 2) * W * 0.0163, fz(0.904) + r * W * 0.014, W * 0.0034,
                -0.00005, 0.0003, M_HOLE, bevel=0)
t = text("BRICK HAMMER", fx(0.029), fz(0.538), W * 0.027, M_ETCH, -0.00002)   # light, thin and spaced wide (TrimUI front view)
t.data.font = bpy.data.fonts.load(INTER.replace("Bold", "Regular")); t.data.space_character = 1.5
# sides: the power slider and teal key on the right, two keys on the left
def side_pill(x_sign, v, length, depth, material, out=0.0007, name="key", yc=None, bevel=0.00025):
    """A pill on a side face: rounded fully in its own plane (depth x length), standing out by `out`,
    centred yc back from the front (half the body's depth unless given)."""
    yc = D1 * 0.5 if yc is None else yc
    r = min(length, depth) / 2 * 0.98
    pts = outline(yc - depth / 2, fz(v) - length / 2, yc + depth / 2, fz(v) + length / 2, r, r, r, r, 10)
    # slab() draws the outline in its x and z and extrudes along y; turning it 90 degrees about z puts the
    # outline's x on the device's depth and the extrusion across the side
    ob = slab(name, pts, -out, out, material, bevel=bevel, segs=3)
    ob.rotation_euler = (0, 0, math.radians(90))  # outline x -> depth (y), extrusion -> across the side
    ob.location = (x_sign * W / 2, 0, 0)
    return ob
def side_box(x_sign, yc, zc, dy, dz, out, material, name="mark"):
    """A thin flat box on a side face, standing `out` proud of the face: marks and ridges."""
    me_ = bpy.data.meshes.new(name); xs_ = x_sign * (W / 2 + out)
    me_.from_pydata([(xs_, yc - dy / 2, zc - dz / 2), (xs_, yc + dy / 2, zc - dz / 2), (xs_, yc + dy / 2, zc + dz / 2), (xs_, yc - dy / 2, zc + dz / 2)], [], [(0, 1, 2, 3)])
    o_ = bpy.data.objects.new(name, me_); scene.collection.objects.link(o_); o_.data.materials.append(material)
    return o_
# Measured on TrimUI's side views (h11, 2026-11-04). Right side: the power switch in its slot, the knob
# down at the slot's lower end with ridges across it, then an upright frosted power key in a dark ring.
# Left side: volume + and -, slimmer pills in dark rings, a little behind the middle of the depth.
M_SLOT = mat("slot", srgb("2a2b2e"), metal=1.0, rough=0.5)
SW_Y = D1 * 0.54
side_pill(1, 0.208, H * 0.097, D1 * 0.36, M_SLOT, out=0.00004, name="slot", yc=SW_Y, bevel=0)
KV, KL, KD = 0.219, H * 0.074, D1 * 0.31
side_pill(1, KV, KL, KD, M_BODY, out=0.0004, name="switch", yc=SW_Y, bevel=0.0002)
for q in range(9):   # ridges across the knob's lower part
    side_box(1, SW_Y, fz(KV) + KL * (0.22 - q * 0.075), KD * 0.78, KL * 0.022, 0.00041, M_SLOT, name="ridge")
PW_V, PW_Y = 0.353, D1 * 0.52
side_pill(1, PW_V, H * 0.058 + 0.0006, D1 * 0.37 + 0.0006, M_HOLE, out=0.00004, name="power_ring", yc=PW_Y, bevel=0)
PWR = side_pill(1, PW_V, H * 0.058, D1 * 0.37, M_TEAL, out=0.0006, name="power", yc=PW_Y)
# its mark, cut 0.15 mm into the key (Howard, 2026-11-04): a broken ring with a bar through the gap
pk = Vector((W / 2, PW_Y, fz(PW_V))); _cx0, _cx1 = W / 2 + 0.00045, W / 2 + 0.0012
_pw = []
for q in range(9):
    a0, a1 = math.radians(129 + q * 31), math.radians(131 + (q + 1) * 31)
    r_o, r_i = D1 * 0.10, D1 * 0.071
    quad = [(math.cos(a0) * r_o, math.sin(a0) * r_o), (math.cos(a1) * r_o, math.sin(a1) * r_o), (math.cos(a1) * r_i, math.sin(a1) * r_i), (math.cos(a0) * r_i, math.sin(a0) * r_i)]
    _pw.append(prism("pwr", [(pk.y + u_, pk.z + v_) for u_, v_ in quad], "x", _cx0, _cx1))
bw_ = D1 * 0.016
_pw.append(prism("pwr_bar", [(pk.y - bw_, pk.z + D1 * 0.13), (pk.y + bw_, pk.z + D1 * 0.13), (pk.y + bw_, pk.z + D1 * 0.02), (pk.y - bw_, pk.z + D1 * 0.02)], "x", _cx0, _cx1))
for o_ in _pw: o_.data.materials.clear(); o_.data.materials.append(M_ICON)
apply_mods(PWR); cut_all(PWR, _pw); apply_mods(PWR); reshade(PWR)
VOL_Y, VOL_L, VOL_D = D1 * 0.56, H * 0.072, D1 * 0.30
for v, mark in ((0.0925, "+"), (0.193, "-")):
    side_pill(-1, v, VOL_L + 0.0006, VOL_D + 0.0006, M_HOLE, out=0.00004, name="vol_ring", yc=VOL_Y, bevel=0)
    vk = side_pill(-1, v, VOL_L, VOL_D, M_BODY, out=0.0007, name="vol", yc=VOL_Y)   # body-coloured (TrimUI views)
    # the mark: thin engraved lines, the bar across the key's width, the + with an upright stroke too
    # the mark cut 0.15 mm into the key (Howard, 2026-11-04): a bar across, and an upright for +
    _xm = -(W / 2 + 0.0007 + 0.0002 - 0.00035 / 2)
    _vm = [box("vol_mark", _xm, VOL_Y, fz(v), 0.00035, VOL_D * 0.42, 0.00025, M_ENGRAVE)]
    if mark == "+": _vm.append(box("vol_mark", _xm, VOL_Y, fz(v), 0.00035, 0.00025, VOL_D * 0.42, M_ENGRAVE))
    apply_mods(vk); cut_all(vk, _vm); apply_mods(vk); reshade(vk)
# bottom edge: card slot, USB-C, headphone jack
# bottom edge, measured on the bottom view: card slot, reset, USB-C, light, headphones
def bottom_hole(u, w_, h_, r):
    yc = D2 * 0.48
    me_ = bpy.data.meshes.new("port"); bm = bmesh.new()
    pts = outline(fx(u) - w_ / 2, yc - h_ / 2, fx(u) + w_ / 2, yc + h_ / 2, r, r, r, r, 8)
    vs = [bm.verts.new((x, z, 0.00002 - 0.00004)) for x, z in pts]  # x across, the depth as y, just below z = 0
    bm.faces.new(vs); bm.to_mesh(me_); bm.free()
    o_ = bpy.data.objects.new("port", me_); scene.collection.objects.link(o_); o_.data.materials.append(M_HOLE)
bottom_hole(0.22, W * 0.17, D2 * 0.08, 0.0002)
bottom_hole(0.368, W * 0.045, W * 0.045, W * 0.0224)
bottom_hole(0.502, W * 0.125, D2 * 0.2, D2 * 0.099)
bottom_hole(0.642, W * 0.014, W * 0.014, W * 0.0069)
bottom_hole(0.755, W * 0.07, W * 0.07, W * 0.0349)

# one shell: the grip and the body are the same metal piece (Howard), so join them into one solid and
# let no seam show where the back steps out
bpy.context.view_layer.objects.active = body
for mod in list(body.modifiers): bpy.ops.object.modifier_apply(modifier=mod.name)
for v in grip.data.vertices:   # 10 microns proud of the body's sides: their shared faces left a slot in the union
    if abs(v.co.x) > W / 2 - 1e-6: v.co.x += 0.00001 if v.co.x > 0 else -0.00001
m = body.modifiers.new("shell", "BOOLEAN"); m.operation = "UNION"; m.object = grip; m.solver = "EXACT"; m.material_mode = "TRANSFER"
bpy.ops.object.modifier_apply(modifier="shell")
bpy.data.objects.remove(grip, do_unlink=True)
# shading: faces lying flat along an axis stay flat; only the rounded parts are smooth, with sharp edges
# between the two, so the large back faces cannot pick up smeared normals from the boolean's triangles
bpy.ops.object.select_all(action="DESELECT"); body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.mesh.customdata_custom_splitnormals_clear()   # the baked weighted normals smeared across the new faces
me = body.data
for poly in me.polygons:
    n = poly.normal; poly.use_smooth = max(abs(n.x), abs(n.y), abs(n.z)) < 0.99999   # 0.999 left the round's first facets flat: a dark line along the back corners (Howard)
for e in me.edges:
    e.use_edge_sharp = False
import collections
edge_faces = collections.defaultdict(list)
for poly in me.polygons:
    for ek in poly.edge_keys: edge_faces[ek].append(poly.use_smooth)
for e in me.edges:
    f = edge_faces.get(e.key, [])
    if len(f) == 2 and f[0] != f[1]: e.use_edge_sharp = True

# the side marking, measured on Howard's photo 10 (straightened): "TRIMUI", the three-ring mark and "BRICK"
# cut shallow into the body's right side, 0.6 of its depth back from the front, reading from the bottom
# up with the letters' tops toward the front. Capitals 2.0 mm; TRIMUI from 5 mm above the bottom to 16.6 mm,
# the mark at 18.7 mm, BRICK from 21 to 30 mm.
from mathutils import Matrix
SIDE_ROT = Matrix(((0, 0, 1), (0, -1, 0), (1, 0, 0))).to_euler()   # text x -> up the side, text y -> front
SIDE_Y = D1 * 0.6
M_SIDE_INK = mat("side_ink", srgb("2c2d31"), metal=1.0, rough=0.6)
side_cuts = []
def side_word(word, mm_from_bottom, spacing):
    t = text(word, 0, 0, W * 0.054, M_SIDE_INK, 0, align="CENTER", depth=0.0005)   # the cut takes the darker fill (TrimUI side view)
    t.data.font = bpy.data.fonts.load(font_file("Avenir-Black.ttf", "Bold")); t.data.space_character = spacing
    t.data.align_y = "CENTER"
    t.rotation_euler = SIDE_ROT; t.location = (W / 2 - 0.00016, SIDE_Y, mm_from_bottom / 1000)   # cut 0.16 mm deep: read by edge light, not shadow
    bpy.ops.object.select_all(action="DESELECT"); t.select_set(True); bpy.context.view_layer.objects.active = t
    bpy.ops.object.convert(target="MESH"); side_cuts.append(t)
# TrimUI's side view puts the marking higher than Howard's photo 10: TRIMUI centred 13.1 mm up, the mark at
# 21.5 mm, BRICK centred at 28.9 mm, all filled darker than the metal
side_word("TRIMUI", 13.1, 1.15)
side_word("BRICK", 28.9, 1.05)
for a_ in (90, 210, 330):   # the mark: three filled dots
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.00052, depth=0.00032,
        location=(W / 2, SIDE_Y - math.sin(math.radians(a_)) * 0.00058, 0.0215 + math.cos(math.radians(a_)) * 0.00058),
        rotation=(0, math.radians(90), 0))
    bpy.context.object.data.materials.append(M_SIDE_INK)
    side_cuts.append(bpy.context.object)
# the parting line where the body meets the grip (Howard's photo 10): a fine groove along both sides
# and across the bottom
for sx in (-1, 1):
    side_cuts.append(prism("part", outline(D1 - 0.00006, -0.001, D1 + 0.00006, LEDGE - 0.0004, 0, 0, 0, 0, 1), "x",
                           sx * W / 2 - 0.0001, sx * W / 2 + 0.0001))
for c_ in side_cuts:
    cut(body, c_, self_=c_.name.startswith("Text"))   # letters' outlines can overlap in some copies of the font; the rest cut plainly

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
    ("top", (0, 0.45, 0.05), 1.2, 2.5),
    ("side", (-110, 0.10, 0.40), 0.5, 4),
]
for name, _, size, power in LIGHTS:
    bpy.ops.object.light_add(type="AREA"); l = bpy.context.object; l.name = "light_" + name  # own names: "key" is also a side key
    l.data.size = size; l.data.energy = power * float(os.environ.get("BRICK_LIGHT", "1"))  # BRICK_LIGHT=0.4 dims every light
def place_lights(cam_loc):
    """Lights keep their places around the camera, so each angle is lit like a product photo."""
    az = math.degrees(math.atan2(cam_loc[0], -cam_loc[1]))
    centre = Vector((0, D1, H * 0.5))
    for name, (da, h, dist), _, _ in LIGHTS:
        l = bpy.data.objects["light_" + name]; a = math.radians(az + da)
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
    "left": (-0.45, -0.12, 0.12),
    "bottom": (0.10, -0.30, -0.22),
    "backtop": (0.05, 0.30, 0.42),     # the angle of reference photo 8
    "back": (0.0, 0.46, 0.06),         # straight on
    "mine": (0.0, 0.36, 0.30),         # above and behind, as Howard's photo
    "photo6": (0.26, -0.30, -0.05),    # low at the front right, as TrimUI's close-up
    "rside": (0.48, 0.02, 0.05),       # the teal-key side, as photo 12
    "sideclose": (0.15, -0.02, -0.07),
    "cornerbl": (0.10, 0.12, -0.10),      # the grip's bottom corner from behind and below
    "cornertop": (0.12, 0.10, 0.20),      # the top corners from behind and above
    "cornerfront": (-0.12, -0.12, -0.08), # the front's bottom corner
    "sideflat": (0.20, D1 * 0.6, 0.02),   # straight at the right side, low # close on the side marking, as Howard's photo 10
    "lclose": (-0.20, -0.02, 0.12),    # close on the volume keys
    "dpadclose": (-0.05, -0.12, 0.10),  # close on the d-pad, as TrimUI's close-up
    "usbclose": (0.02, 0.10, 0.16),    # close on the USB-C, from above and behind
    "back2": (0.0, 0.30, 0.062),
    "p8459": (0.10, 0.22, 0.055),      # behind and to the teal-key side, as IMG_8459
    "stripclose": (0.05, 0.11, 0.075), # close on the strips under R1 and R2        # straight behind the shoulder row, as Howard's second photo
    "btnclose": (0.05, 0.22, 0.14),    # the shoulder row from behind and above, as Howard's close-up
}
if os.environ.get("BRICK_SAVE"):  # BRICK_SAVE=/path/brick.blend saves the scene to open in Blender, without rendering
    cam.location = SHOTS["front34"]; place_lights(cam.location)
    bpy.ops.wm.save_as_mainfile(filepath=os.environ["BRICK_SAVE"])
    sys.exit(0)
if os.environ.get("BRICK_ORTHO"):  # flat views with no perspective, to line up against TrimUI's drawings
    cam.constraints.clear(); cam.data.type = "ORTHO"; cam.data.ortho_scale = H * 1.25
    scene.render.resolution_x = scene.render.resolution_y = int(os.environ.get("BRICK_ORTHO_RES", "1400"))
    c = Vector((0, D2 / 2, H / 2)); far = 0.5
    ONLY_V = os.environ.get("BRICK_ORTHO_VIEWS")   # a comma list of views to render only those
    VIEWS = {"front": ((0, -far, 0), (90, 0, 0)), "back": ((0, far, 0), (90, 0, 180)),
             "right": ((far, 0, 0), (90, 0, 90)), "left": ((-far, 0, 0), (90, 0, -90)),
             "top": ((0, 0, far), (0, 0, 0)), "bottom": ((0, 0, -far), (180, 0, 0))}
    for name, (off, rot) in VIEWS.items():
        if ONLY_V and name not in ONLY_V.split(","): continue
        cam.location = c + Vector(off); cam.rotation_euler = tuple(math.radians(r) for r in rot)
        place_lights(cam.location if name not in ("top", "bottom") else Vector((0.001, -0.3, 0.2)))
        scene.render.filepath = f"{OUT}-ortho-{name}.png"
        bpy.ops.render.render(write_still=True)
    sys.exit(0)
ONLY = os.environ.get("BRICK_SHOTS")  # a comma list of shot names to render only those
for name, loc in SHOTS.items():
    if ONLY and name not in ONLY.split(","): continue
    cam.location = loc
    place_lights(loc)
    scene.render.filepath = f"{OUT}-{name}.png"
    bpy.ops.render.render(write_still=True)
