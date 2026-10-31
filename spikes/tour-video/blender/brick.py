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
INTER = "/Users/howard/Documents/GitHub/brick-hammer-launcher/workspace/all/launcher/res/Inter-Bold.ttf"
FONTS = os.environ.get("BRICK_FONTS", "")   # a folder with Avenir-Light.ttf and Avenir-Medium.ttf (the back's lettering)
def font_file(name, fallback):
    f = os.path.join(FONTS, name)
    return f if FONTS and os.path.exists(f) else INTER.replace("Bold", fallback)

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

BODY_HEX = {"dark": ("64656a", "64656a"), "silver": ("c9cacd", "bfc0c3")}[FINISH]
M_BODY = mat("anodised", srgb(BODY_HEX[0]), metal=1.0, rough=0.36)
M_BACK = mat("back", srgb(BODY_HEX[1]), metal=1.0, rough=0.44)
M_BLACK = mat("button", srgb("141416"), rough=0.62)
M_PILL = mat("pill", srgb("b9babd"), rough=0.4)
M_GLASS = mat("glass", srgb("060607"), rough=0.08, coat=0.6)
M_HOLE = mat("hole", srgb("0a0a0a"), rough=0.8)
M_LETTER = mat("letter", srgb("4a4a4d"), rough=0.5)
M_ENGRAVE = mat("engrave", srgb("3f4045"), metal=1.0, rough=0.62)
M_TEAL = mat("teal", srgb("5fd6c8"), rough=0.35, emit=srgb("5fd6c8"), strength=0.4)
M_LABEL = mat("label", (0.0, 0.0, 0.0), rough=0.03)
M_LABEL_TEXT = mat("labeltext", srgb("c8c8cc"), rough=0.5)
M_LED = mat("led", LED, emit=LED, strength=4.0)
M_PILL_LIT = mat("pill_lit", (0.85, 0.95, 1.0), rough=0.3, emit=LED, strength=3.0)
M_ICON = mat("icon", srgb("3c3c3f"), rough=0.5)

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
sparkle(M_BODY); sparkle(M_BACK)
M_FACE = mat("facebutton", srgb("1e1e21"), rough=0.4)   # the face buttons: dark grey, a soft sheen (photo 6)
M_SHOULDER = mat("shoulder", srgb("0e0e10"), rough=0.7)   # matte black rubber, little sheen
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

def slab(name, pts, y0, y1, material, bevel=0.0006, segs=3, back_only=False):
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
        back_edges = {e.index for e in bm.edges if all(v in back for v in e.verts)}
    bm.to_mesh(me); bm.free()
    if back_only:
        a = me.attributes.new("bevel_weight_edge", "FLOAT", "EDGE")
        for i in range(len(me.edges)): a.data[i].value = 1.0 if i in back_edges else 0.0
    ob = bpy.data.objects.new(name, me); scene.collection.objects.link(ob)
    ob.data.materials.append(material)
    if bevel:
        m = ob.modifiers.new("bevel", "BEVEL"); m.width = bevel; m.segments = segs
        if back_only: m.limit_method = "WEIGHT"
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

# body: the upper slab and the grip behind its lower half
x0, x1 = -W / 2, W / 2
body = slab("body", outline(x0, 0, x1, H, R_TOP, R_TOP, R_BOT, R_BOT), 0, D1, M_BODY, bevel=0.0007, segs=1)
body.modifiers["bevel"].angle_limit = math.radians(60)
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
grip = slab("grip", outline(x0, 0, x1, LEDGE, 0.0025, 0.0025, R_BOT, R_BOT), D1 - 0.0005, D2, M_BACK, bevel=0.0038, segs=10, back_only=True)
bpy.context.view_layer.objects.active = grip
for mod in list(grip.modifiers): bpy.ops.object.modifier_apply(modifier=mod.name)   # baked, so the USB-C piece can join it
# shoulder buttons standing on the ledge, R1 R2 | USB-C | L2 L1 as seen from behind (Howard's close-up):
# R1 0-23.6 %, R2 24-38.8 %, USB-C 39.4-58.6 %, L2 59-73.3 %, L1 73.8-100 % of the width; R1 and L1 reach
# the edges, lower and with a rounded lip; R2 and L2 stand a little taller; names on top
LED_H = 0.0028                         # the frosted strip under each button, seen on the back
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
    y0, yb, lip = D1 + 0.0003, D2 - 0.0002, 0.0008
    r_end = W * 0.03 if outer_sign else 0.0008                   # the outer end in plan
    def plan(inset):
        xa, xb = x0_, x1_
        if outer_sign > 0: xb -= inset
        if outer_sign < 0: xa += inset
        rb = max(0.0005, r_end - inset)
        # outline() corners: (left, front), (right, front), (right, back), (left, back) in x and y here
        rlf, rrf = 0.0004, 0.0004
        rrb = rb if outer_sign > 0 else 0.0006
        rlb = rb if outer_sign < 0 else 0.0006
        return outline(xa, y0, xb, yb + lip - inset, rlb, rrb, rrf, rlf, 14)
    z_lip = base + h * 0.58
    body = prism_z(name + "_body", plan(lip), base, z_lip + 0.0004, material)
    if not outer_sign:
        cap = prism_z(name, plan(0), z_lip, base + h, material)
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
        flare = lambda u: outer_sign * 0.0007 * max(0.0, 1 - u / 0.12) ** 2   # the tip flares out past the side
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
    bv = cap.modifiers.new("roll", "BEVEL"); bv.width = 0.0019; bv.segments = 10; bv.limit_method = "ANGLE"; bv.angle_limit = math.radians(50)
    bv.profile = 0.55
    bb = body.modifiers.new("bevel", "BEVEL"); bb.width = 0.0005; bb.segments = 4; bb.limit_method = "ANGLE"; bb.angle_limit = math.radians(50)
    return cap, (y0 + yb) / 2 - 0.0002
for (a, b, lab, outer) in [(0.0, 0.236, "R1", True), (0.240, 0.388, "R2", False), (0.590, 0.733, "L2", False), (0.738, 1.0, "L1", True)]:
    u0, u1 = bu(b), bu(a)
    sign = 0 if not outer else (1 if lab == "R1" else -1)
    h = H * (0.080 if outer else 0.084)
    btn, yc = shoulder("shoulder_" + lab, u0, u1, LEDGE + LED_H, h, sign, M_SHOULDER)
    # the name carved into the cap's flat top, on its inner part
    lx = (u0 + u1) / 2 if not outer else ((u0 + u1) / 2 - sign * (u1 - u0) * 0.10)
    lz = LEDGE + LED_H + h - 0.00015
    t = text(lab, lx, lz, W * 0.06, M_SHOULDER, yc, align="CENTER", depth=0.0003)
    t.data.font = bpy.data.fonts.load(font_file("Avenir-Medium.ttf", "SemiBold"))
    t.rotation_euler = (0, 0, math.radians(180)); t.location = (lx, yc, lz)
    bpy.context.view_layer.objects.active = t; t.select_set(True); bpy.ops.object.convert(target="MESH")
    bpy.context.view_layer.objects.active = btn
    bpy.ops.object.modifier_apply(modifier="roll")
    m = btn.modifiers.new("carve", "BOOLEAN"); m.operation = "DIFFERENCE"; m.object = t; m.solver = "EXACT"
    bpy.ops.object.modifier_apply(modifier="carve")
    bpy.data.objects.remove(t, do_unlink=True)
    for o_ in (btn, bpy.data.objects["shoulder_" + lab + "_body"]):  # smooth only across the rounded edges
        bpy.ops.object.select_all(action="DESELECT"); o_.select_set(True); bpy.context.view_layer.objects.active = o_
        bpy.ops.object.shade_auto_smooth(angle=math.radians(35))
    bpy.ops.object.select_all(action="DESELECT")
    slab("led_" + lab, outline(u0 + 0.0004, LEDGE - 0.0002, u1 - 0.0004, LEDGE + LED_H, 0.0002, 0.0002, 0.0002, 0.0002, 2),
         D1 + 0.0006, D2 + 0.0001, M_LED, bevel=0.0002, segs=1)
# USB-C, read in 3D from TrimUI's top view, its back-buttons close-up, photo 8 and Howard's notes: one
# metal piece between R2 and L2, lower than the buttons, its back face grooved (the ribbed plate is the
# same piece), and the port a real hole cut down into its top, lined in steel, with the tongue inside.
UB0, UB1 = bu(0.586), bu(0.394)
UTOP = LEDGE + LED_H + 0.0030
def cut(target, cutter):
    m = target.modifiers.new("cut_" + cutter.name, "BOOLEAN"); m.operation = "DIFFERENCE"; m.object = cutter
    m.solver = "EXACT"; m.material_mode = "TRANSFER"
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.data.objects.remove(cutter, do_unlink=True)
block = slab("usb_block", outline(UB0, LEDGE - 0.002, UB1, UTOP, 0.0006, 0.0006, 0, 0, 6),
             D1 + 0.0003, D2, M_BACK, bevel=0)
# grooves across its back face, from the grip's top up to just under the port
for k in range(7):
    z = LEDGE + 0.00045 + k * 0.00052
    g = slab("groove", outline(UB0 - 0.001, z, UB1 + 0.001, z + 0.00022, 0, 0, 0, 0, 1), D2 - 0.00012, D2 + 0.001, M_BODY, bevel=0)
    cut(block, g)
# the port: a rounded slot cut 3 mm down into the top; its walls take the steel of the cutter
ucx, ucy = (UB0 + UB1) / 2, (D1 + D2) / 2 + 0.0004
cut(block, prism_z("port", outline(ucx - 0.0044, ucy - 0.0013, ucx + 0.0044, ucy + 0.0013, 0.0013, 0.0013, 0.0013, 0.0013, 12),
                   UTOP - 0.0030, UTOP + 0.001, M_PORT))
# the USB-C piece is part of the grip's housing: one solid, the grip's back running up into it
m = grip.modifiers.new("join_usb", "BOOLEAN"); m.operation = "UNION"; m.object = block; m.solver = "EXACT"; m.material_mode = "TRANSFER"
bpy.context.view_layer.objects.active = grip; bpy.ops.object.modifier_apply(modifier="join_usb")
bpy.data.objects.remove(block, do_unlink=True)
bpy.context.view_layer.objects.active = grip; grip.select_set(True)
bpy.ops.object.shade_auto_smooth(angle=math.radians(35)); grip.select_set(False)
prism_z("port_floor", outline(ucx - 0.0043, ucy - 0.0012, ucx + 0.0043, ucy + 0.0012, 0.0012, 0.0012, 0.0012, 0.0012, 12),
        UTOP - 0.0030, UTOP - 0.0028, M_HOLE)
prism_z("port_tongue", outline(ucx - 0.0033, ucy - 0.00035, ucx + 0.0033, ucy + 0.00035, 0.0002, 0.0002, 0.0002, 0.0002, 4),
        UTOP - 0.0029, UTOP - 0.0006, M_HOLE)
# the grip panel: "BRICK HAMMER" spaced wide, raised ribs, the marks in a band, four screw holes
t = text("BRICK HAMMER", 0, fz(pv(0.635)), W * 0.034, M_ETCH, D2 + 0.00006, face="back", align="CENTER")
t.data.font = bpy.data.fonts.load(font_file("Avenir-Light.ttf", "Regular")); t.data.space_character = 2.1
for i in range(18):
    v = 0.675 + i * 0.0155
    slab(f"rib{i}", outline(bu(0.905), fz(pv(v + 0.0085)), bu(0.095), fz(pv(v)), 0.0003, 0.0003, 0.0003, 0.0003, 2),
         D2 - 0.0001, D2 + 0.00028, M_BACK, bevel=0.00012, segs=2)
# the zigzag engraved across the grip, seen only in the grooves between the ribs (Howard's photo)
def zigzag(v_top, v_bot, period, name):
    me_ = bpy.data.meshes.new(name); verts, faces = [], []
    z_top, z_bot = fz(pv(v_top)), fz(pv(v_bot)); x = bu(0.89); up = True; w_ = 0.00012
    while x < bu(0.11) - 0.0001:
        x2 = min(x + period / 2, bu(0.11))
        za, zb = (z_bot, z_top) if up else (z_top, z_bot)
        dx, dz = x2 - x, zb - za; L = math.hypot(dx, dz); nx, nz = -dz / L * w_, dx / L * w_
        i = len(verts)
        for (px, pz) in ((x + nx, za + nz), (x2 + nx, zb + nz), (x2 - nx, zb - nz), (x - nx, za - nz)):
            verts.append((px, D2 + 0.00003, pz))
        faces.append((i, i + 1, i + 2, i + 3)); x = x2; up = not up
    me_.from_pydata(verts, [], faces)
    o_ = bpy.data.objects.new(name, me_); scene.collection.objects.link(o_); o_.data.materials.append(M_ETCH)
zigzag(0.712, 0.772, W * 0.24, "zigzag_upper")
zigzag(0.912, 0.945, W * 0.24, "zigzag_lower")
for (u, v) in [(0.075, 0.676), (0.925, 0.676), (0.085, 0.962), (0.915, 0.962)]:
    cyl("hole", bu(u), fz(pv(v)), W * 0.024, D2 - 0.0004, D2 + 0.00005, M_HOLE, bevel=0)
# upper back: two silver Phillips screws, the three-dot mark
for u in (0.09, 0.91):
    cyl("screw", bu(u), fz(pv(0.09)), W * 0.02, D1 - 0.0001, D1 + 0.00022, M_SCREW, bevel=0.00012)
    for horiz in (True, False):
        w_, h_ = (W * 0.022, W * 0.004) if horiz else (W * 0.004, W * 0.022)
        slab("slot", outline(bu(u) - w_ / 2, fz(pv(0.09)) - h_ / 2, bu(u) + w_ / 2, fz(pv(0.09)) + h_ / 2, 0.00005, 0.00005, 0.00005, 0.00005, 1),
             D1 + 0.00018, D1 + 0.00024, M_HOLE, bevel=0)
for a in (90, 210, 330):
    cyl("mark", math.cos(math.radians(a)) * W * 0.014, fz(pv(0.37)) + math.sin(math.radians(a)) * W * 0.014, W * 0.011,
        D1 - 0.00005, D1 + 0.00005, M_HOLE, bevel=0)
# the label: black glass from the back's top edge, its light pipe set into the top edge above it
slab("top_led", outline(bu(0.77), H - 0.0003, bu(0.23), H + 0.00015, 0.0002, 0.0002, 0.0002, 0.0002, 2),
     D1 - 0.0028, D1 + 0.0004, M_LED, bevel=0)
slab("label", outline(bu(0.77), fz(pv(0.175)), bu(0.23), H - 0.0003, 0.0003, 0.0003, W * 0.03, W * 0.03, 8), D1 - 0.0001, D1 + 0.0004, M_LABEL, bevel=0.0001, segs=1)
t = text("BRICK HAMMER", 0, fz(pv(0.095)), W * 0.05, M_LABEL_TEXT, D1 + 0.00042, face="back", align="CENTER")
t.data.font = bpy.data.fonts.load(font_file("Avenir-Medium.ttf", "Regular")); t.data.space_character = 1.45
t = text("BRICK METAL VERSION DESIGNED BY TRIMUI", 0, fz(pv(0.135)), W * 0.016, M_LABEL_TEXT, D1 + 0.00042, face="back", align="CENTER")
t.data.font = bpy.data.fonts.load(font_file("Avenir-Medium.ttf", "Regular")); t.data.space_character = 1.15
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
slab("dpad_h", outline(cx - arm, cz - th, cx + arm, cz + th, 0.0009, 0.0009, 0.0009, 0.0009, 4), -0.0016, 0.0002, M_FACE, bevel=0.0004)
slab("dpad_v", outline(cx - th, cz - arm, cx + th, cz + arm, 0.0009, 0.0009, 0.0009, 0.0009, 4), -0.0016, 0.0002, M_FACE, bevel=0.0004)
for a in (90, 210, 330):  # the three-dot mark in the middle
    cyl("dot", cx + math.cos(math.radians(a)) * W * 0.014, cz + math.sin(math.radians(a)) * W * 0.014, W * 0.011,
        -0.00168, -0.00158, M_LETTER, bevel=0.00003)
# face buttons with their letters
for l, u, v in [("X", 0.778, 0.586), ("Y", 0.675, 0.654), ("A", 0.876, 0.654), ("B", 0.778, 0.722)]:
    cyl("btn_" + l, fx(u), fz(v), W * 0.05, -0.0013, 0.0002, M_FACE, bevel=0.0004)
    dome(fx(u), fz(v), W * 0.049, -0.0013, 0.0006, M_FACE)
    text(l, fx(u), fz(v), W * 0.05, M_LETTER, -0.00192, align="CENTER")
# select and start
for u in (0.426, 0.572):
    slab("pill", outline(fx(u) - W * 0.048, fz(0.579) - W * 0.012, fx(u) + W * 0.048, fz(0.579) + W * 0.012,
                        W * 0.0119, W * 0.0119, W * 0.0119, W * 0.0119, 8), -0.0008, 0.0002, M_PILL_LIT, bevel=0.0003)
# the three round buttons along the bottom, and the speaker holes
for u in (0.364, 0.503, 0.643):
    cyl("menu", fx(u), fz(0.918), W * 0.039, -0.0009, 0.0002, M_FACE, bevel=0.0003)
    dome(fx(u), fz(0.918), W * 0.038, -0.0009, 0.0004, M_FACE)
# their marks: the three dots, a square, a play arrow
for a in (90, 210, 330):
    cyl("m_dot", fx(0.364) + math.cos(math.radians(a)) * W * 0.011, fz(0.918) + math.sin(math.radians(a)) * W * 0.011, W * 0.0085,
        -0.00138, -0.00128, M_ICON, bevel=0)
slab("m_sq", outline(fx(0.503) - W * 0.016, fz(0.918) - W * 0.011, fx(0.503) + W * 0.016, fz(0.918) + W * 0.011,
                     0.0002, 0.0002, 0.0002, 0.0002, 2), -0.00138, -0.00128, M_ICON, bevel=0)
tri = bpy.data.meshes.new("m_play"); c = (fx(0.643), fz(0.918)); k = W * 0.016
tri.from_pydata([(c[0] - k * 0.6, -0.00135, c[1] - k), (c[0] - k * 0.6, -0.00135, c[1] + k), (c[0] + k, -0.00135, c[1])], [], [(0, 1, 2)])
o = bpy.data.objects.new("m_play", tri); scene.collection.objects.link(o); o.data.materials.append(M_ICON)
for su in (0.165, 0.851):
    for r, n in ((0, 6), (1, 5)):  # two staggered rows, 6 over 5 (photo 6)
        for k in range(n):
            cyl("spk", fx(su) + (k - (n - 1) / 2) * W * 0.015, fz(0.898) + r * W * 0.012, W * 0.003,
                -0.00005, 0.0003, M_HOLE, bevel=0)
text("BRICK HAMMER", fx(0.035), fz(0.545), W * 0.03, M_ENGRAVE, -0.00002)
# sides: the power slider and teal key on the right, two keys on the left
def side_pill(x_sign, v, length, depth, material, out=0.0007, name="key"):
    """A pill on a side face: rounded fully in its own plane (depth x length), standing out by `out`."""
    r = min(length, depth) / 2 * 0.98
    pts = outline(D1 * 0.5 - depth / 2, fz(v) - length / 2, D1 * 0.5 + depth / 2, fz(v) + length / 2, r, r, r, r, 10)
    # slab() draws the outline in its x and z and extrudes along y; turning it 90 degrees about z puts the
    # outline's x on the device's depth and the extrusion across the side
    ob = slab(name, pts, -out, out, material, bevel=0.00025, segs=3)
    ob.rotation_euler = (0, 0, math.radians(90))  # outline x -> depth (y), extrusion -> across the side
    ob.location = (x_sign * W / 2, 0, 0)
    return ob
# right side: the power switch in a recessed slot, then the teal key; left side: volume + and -
side_pill(1, 0.21, H * 0.095, D1 * 0.42, M_HOLE, out=0.00004, name="slot")
side_pill(1, 0.19, H * 0.040, D1 * 0.30, M_FACE, out=0.0005, name="switch")
side_pill(1, 0.355, H * 0.065, D1 * 0.42, M_TEAL, out=0.0007, name="teal")
for v, mark in ((0.09, "+"), (0.195, "-")):
    side_pill(-1, v, H * 0.076, D1 * 0.42, M_FACE, out=0.0007, name="vol")
    t = text(mark, -W / 2 - 0.00075, fz(v), W * 0.04, M_ICON, D1 * 0.5, align="CENTER")
    t.rotation_euler = (math.radians(90), 0, math.radians(-90)); t.location = (-W / 2 - 0.00075, D1 * 0.5, fz(v))
# "TRIMUI", the three-dot mark as three discs (no font needed), "BRICK", running up the right side
YS = D1 + 0.0016  # on the grip's flat side, ahead of its rounded back edge
for word, v in (("TRIMUI", 0.89), ("BRICK", 0.755)):
    t = text(word, W / 2 + 0.00002, fz(v), W * 0.03, M_ENGRAVE, YS, align="CENTER")
    t.rotation_euler = (math.radians(90), math.radians(-90), math.radians(90)); t.location = (W / 2 + 0.00002, YS, fz(v))
for a in (0, 120, 240):
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=W * 0.0035, depth=0.00004,
        location=(W / 2 + 0.00002, YS + math.cos(math.radians(a)) * W * 0.0045, fz(0.822) + math.sin(math.radians(a)) * W * 0.0045),
        rotation=(0, math.radians(90), 0))
    bpy.context.object.data.materials.append(M_ENGRAVE)
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
    l.data.size = size; l.data.energy = power
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
    "lclose": (-0.20, -0.02, 0.12),    # close on the volume keys
    "usbclose": (0.02, 0.10, 0.16),    # close on the USB-C, from above and behind
    "btnclose": (0.05, 0.22, 0.14),    # the shoulder row from behind and above, as Howard's close-up
}
if os.environ.get("BRICK_SAVE"):  # BRICK_SAVE=/path/brick.blend saves the scene to open in Blender, without rendering
    cam.location = SHOTS["front34"]; place_lights(cam.location)
    bpy.ops.wm.save_as_mainfile(filepath=os.environ["BRICK_SAVE"])
    sys.exit(0)
if os.environ.get("BRICK_ORTHO"):  # flat views with no perspective, to line up against TrimUI's drawings
    cam.constraints.clear(); cam.data.type = "ORTHO"; cam.data.ortho_scale = H * 1.25
    scene.render.resolution_x = scene.render.resolution_y = 1400
    c = Vector((0, D2 / 2, H / 2)); far = 0.5
    VIEWS = {"front": ((0, -far, 0), (90, 0, 0)), "back": ((0, far, 0), (90, 0, 180)),
             "right": ((far, 0, 0), (90, 0, 90)), "left": ((-far, 0, 0), (90, 0, -90)),
             "top": ((0, 0, far), (0, 0, 0)), "bottom": ((0, 0, -far), (180, 0, 0))}
    for name, (off, rot) in VIEWS.items():
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
