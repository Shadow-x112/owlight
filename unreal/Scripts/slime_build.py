"""Build the slime: shell mesh (sculpted dome), core mesh, and the two materials.
Assets: /Game/Owlight/Meshes/SM_SlimeShell, SM_SlimeCore; /Game/Owlight/Materials/M_SlimeShell, M_SlimeCore.
Shell params (set per instance): Color, Clarity 0..1, Heat 0..1, Glow 0..1, Wobble.
Core params: Color, Heat, Glow.
Run: node ue_py.mjs -c "exec(open(r'<proj>/Scripts/slime_build.py',encoding='utf-8-sig').read())"
"""
import unreal, math
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
GSP = unreal.GeometryScript_Primitives; GSQ = unreal.GeometryScript_MeshQueries; GSE = unreal.GeometryScript_MeshEdits
GSL = unreal.GeometryScript_List; GSN = unreal.GeometryScript_Normals; GSA = unreal.GeometryScript_NewAssetUtils
MESHES = "/Game/Owlight/Meshes"; MATS = "/Game/Owlight/Materials"
MP = unreal.MaterialProperty

# ---- shell mesh: a 50 cm wide, 32 cm tall dome with a soft flat base and a low bulge ------------
def shell_profile(x, y, z):
    """Deform a unit sphere point into a slime. Returns (x, y, z) in unit space."""
    # flatten the bottom softly: everything below zb is pulled up toward the base plane
    zb = -0.78
    if z < zb:
        z = zb + (z - zb) * 0.12
    # horizontal bulge, strongest a little below the middle; taper toward the crown
    bulge = 1.0 + 0.16 * math.exp(-((z + 0.40) / 0.45) ** 2) - 0.06 * max(0.0, z) ** 2
    # a slight flare where it meets the ground (surface tension puddle)
    bulge += 0.06 * max(0.0, (zb + 0.20 - z) / 0.20) if z < zb + 0.20 else 0.0
    # squash
    return x * bulge, y * bulge, z * 0.86

def build_shell(radius=25.0, steps=(48, 72)):
    dm = unreal.DynamicMesh()
    opts = unreal.GeometryScriptPrimitiveOptions()
    GSP.append_sphere_lat_long(dm, opts, unreal.Transform(), radius, steps[0], steps[1])
    vl = unreal.GeometryScriptVectorList()
    vl = GSQ.get_all_vertex_positions(dm, False)[1]
    arr = GSL.convert_vector_list_to_array(vl)
    out = []
    zmin = 1e9
    for v in arr:
        x, y, z = v.x / radius, v.y / radius, v.z / radius
        x, y, z = shell_profile(x, y, z)
        out.append(unreal.Vector(x * radius, y * radius, z * radius))
        zmin = min(zmin, z * radius)
    # rest the base on z = 0
    out = [unreal.Vector(v.x, v.y, v.z - zmin) for v in out]
    nl = GSL.convert_array_to_vector_list(out)
    GSE.set_all_mesh_vertex_positions(dm, nl)
    nopts = unreal.GeometryScriptCalculateNormalsOptions(); nopts.angle_weighted = True; nopts.area_weighted = True
    GSN.recompute_normals(dm, nopts)
    return dm

def save_mesh(dm, name):
    path = f"{MESHES}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    o = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    o.enable_recompute_normals = False; o.enable_recompute_tangents = True; o.enable_nanite = False
    o.enable_collision = True; o.collision_mode = unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX
    res = GSA.create_new_static_mesh_asset_from_mesh(dm, path, o)
    sm = res[0] if isinstance(res, tuple) else res
    eal.save_asset(path)
    return sm

shell = save_mesh(build_shell(), "SM_SlimeShell")
core_dm = unreal.DynamicMesh()
GSP.append_sphere_lat_long(core_dm, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), 10.0, 24, 32)
core = save_mesh(core_dm, "SM_SlimeCore")
print("meshes:", shell.get_path_name(), core.get_path_name())

# ---- materials --------------------------------------------------------------------------------------
def new_material(name):
    path = f"{MATS}/{name}"
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path); mel.delete_all_material_expressions(m); return m
    return tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())

def ci(name):
    c = unreal.CustomInput(); c.set_editor_property("input_name", name); return c

def E(mat, cls, x, y): return mel.create_material_expression(mat, cls, x, y)
def vparam(mat, name, rgba, x, y):
    n = E(mat, unreal.MaterialExpressionVectorParameter, x, y); n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*rgba)); return n
def sparam(mat, name, v, x, y):
    n = E(mat, unreal.MaterialExpressionScalarParameter, x, y); n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v); return n
def const(mat, v, x, y):
    n = E(mat, unreal.MaterialExpressionConstant, x, y); n.set_editor_property("r", v); return n
def link(a, ao, b, bi): mel.connect_material_expressions(a, ao, b, bi)
def op(mat, cls, a, ao, b, bo, x, y):
    n = E(mat, cls, x, y); link(a, ao, n, "A"); link(b, bo, n, "B"); return n

# shell: translucent jelly. Opacity and refraction follow Clarity; Heat warms and lights the rim; Glow is the lantern.
m = new_material("M_SlimeShell")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
m.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
m.set_editor_property("two_sided", False)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property("used_with_static_lighting", False)
col = vparam(m, "Color", (0.06, 0.50, 0.16, 1), -900, -300)
clar = sparam(m, "Clarity", 0.6, -900, -100)
heat = sparam(m, "Heat", 0.0, -900, 0)
glow = sparam(m, "Glow", 0.0, -900, 100)
wob = sparam(m, "Wobble", 1.0, -900, 700)
hot = vparam(m, "HeatColor", (1.0, 0.45, 0.12, 1), -900, 200)
glowc = vparam(m, "GlowColor", (0.55, 0.95, 1.0, 1), -900, 300)
# base colour: lerp toward the heat colour as it warms
lerp_c = E(m, unreal.MaterialExpressionLinearInterpolate, -600, -250)
link(col, "", lerp_c, "A"); link(hot, "", lerp_c, "B"); h40 = op(m, unreal.MaterialExpressionMultiply, heat, "", const(m, 0.4, -750, 50), "", -700, 40); link(h40, "", lerp_c, "Alpha")
mel.connect_material_property(lerp_c, "", MP.MP_BASE_COLOR)
# opacity = 1 - Clarity*0.62 (a murky slime hides the ground; a clear one barely tints it)
one = const(m, 1.0, -700, -150)
cm = op(m, unreal.MaterialExpressionMultiply, clar, "", const(m, 0.62, -750, -120), "", -650, -120)
opac = op(m, unreal.MaterialExpressionSubtract, one, "", cm, "", -500, -140)
mel.connect_material_property(opac, "", MP.MP_OPACITY)
# fresnel rim: glossy edge that darkens the centre a touch -> reads as a volume
fres = E(m, unreal.MaterialExpressionFresnel, -700, 450); fres.set_editor_property("exponent", 3.0); fres.set_editor_property("base_reflect_fraction", 0.03)
rimc = vparam(m, "RimColor", (0.35, 1.0, 0.45, 1), -900, 450)
rim = op(m, unreal.MaterialExpressionMultiply, fres, "", rimc, "", -500, 450)
rimk = op(m, unreal.MaterialExpressionMultiply, rim, "", op(m, unreal.MaterialExpressionMultiply, clar, "", op(m, unreal.MaterialExpressionAdd, const(m, 0.15, -600, 560), "", heat, "", -500, 560), "", -420, 540), "", -350, 480)
# emissive: heat (inner warmth) + glow (lantern) + faint rim
heat_e = op(m, unreal.MaterialExpressionMultiply, hot, "", op(m, unreal.MaterialExpressionMultiply, heat, "", const(m, 1.5, -700, 320), "", -600, 320), "", -450, 300)
glow_e = op(m, unreal.MaterialExpressionMultiply, glowc, "", op(m, unreal.MaterialExpressionMultiply, glow, "", const(m, 3.0, -700, 400), "", -600, 400), "", -450, 380)
emis = op(m, unreal.MaterialExpressionAdd, op(m, unreal.MaterialExpressionAdd, heat_e, "", glow_e, "", -300, 340), "", rimk, "", -150, 400)
# emissive is in scene units; EmissiveScale = 1 / 2^exposure_bias keeps the glow relative to the scene
esc = sparam(m, "EmissiveScale", 0.0013, -300, 480)
mel.connect_material_property(op(m, unreal.MaterialExpressionMultiply, emis, "", esc, "", -50, 420), "", MP.MP_EMISSIVE_COLOR)
# murky slimes are matte and dull; clear ones are glossy
rgh = op(m, unreal.MaterialExpressionSubtract, const(m, 0.62, -500, 600), "", op(m, unreal.MaterialExpressionMultiply, clar, "", const(m, 0.52, -600, 640), "", -500, 640), "", -350, 600)
mel.connect_material_property(rgh, "", MP.MP_ROUGHNESS)
spc = op(m, unreal.MaterialExpressionAdd, const(m, 0.2, -500, 700), "", op(m, unreal.MaterialExpressionMultiply, clar, "", const(m, 0.6, -600, 740), "", -500, 740), "", -350, 700)
mel.connect_material_property(spc, "", MP.MP_SPECULAR)
# refraction: clearer slime bends light more (1.0 = none, 1.25 = strong)
ior = op(m, unreal.MaterialExpressionAdd, one, "", op(m, unreal.MaterialExpressionMultiply, clar, "", const(m, 0.22, -700, 800), "", -600, 800), "", -450, 780)
mel.connect_material_property(ior, "", MP.MP_REFRACTION)
# wobble: WPO along the normal, stronger toward the top so the base stays planted
tm = E(m, unreal.MaterialExpressionTime, -900, 900); pos = E(m, unreal.MaterialExpressionWorldPosition, -900, 980)
nrm = E(m, unreal.MaterialExpressionVertexNormalWS, -900, 1060); lpos = E(m, unreal.MaterialExpressionObjectPositionWS, -900, 1140)
cu = E(m, unreal.MaterialExpressionCustom, -600, 950)
cu.set_editor_property("code", """
float h = saturate((P.z - O.z) / 32.0);
float s = sin(T * 2.2 + P.x * 0.03 + P.y * 0.02) * 1.2 + sin(T * 3.3 + P.z * 0.05) * 0.6;
return N * s * h * W;""")
cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
cu.set_editor_property("inputs", [ci("T"), ci("P"), ci("N"), ci("O"), ci("W")])
link(tm, "", cu, "T"); link(pos, "", cu, "P"); link(nrm, "", cu, "N"); link(lpos, "", cu, "O"); link(wob, "", cu, "W")
mel.connect_material_property(cu, "", MP.MP_WORLD_POSITION_OFFSET)
mel.recompile_material(m); eal.save_asset(m.get_path_name())

# core: a small opaque heart; subsurface so it looks lit from within; Heat and Glow light it up
c = new_material("M_SlimeCore")
c.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
ccol = vparam(c, "Color", (0.03, 0.28, 0.09, 1), -700, -200)
cheat = sparam(c, "Heat", 0.0, -700, 0); cglow = sparam(c, "Glow", 0.0, -700, 80)
chot = vparam(c, "HeatColor", (1.0, 0.35, 0.08, 1), -700, 160); cglowc = vparam(c, "GlowColor", (0.6, 1.0, 1.0, 1), -700, 240)
mel.connect_material_property(ccol, "", MP.MP_BASE_COLOR)
mel.connect_material_property(ccol, "", MP.MP_SUBSURFACE_COLOR)
mel.connect_material_property(const(c, 0.55, -400, 300), "", MP.MP_OPACITY)
mel.connect_material_property(const(c, 0.35, -400, 350), "", MP.MP_ROUGHNESS)
ce = op(c, unreal.MaterialExpressionAdd,
        op(c, unreal.MaterialExpressionMultiply, chot, "", op(c, unreal.MaterialExpressionMultiply, cheat, "", const(c, 8.0, -600, 20), "", -500, 20), "", -350, 0), "",
        op(c, unreal.MaterialExpressionMultiply, cglowc, "", op(c, unreal.MaterialExpressionMultiply, cglow, "", const(c, 10.0, -600, 100), "", -500, 100), "", -350, 90), "", -200, 40)
cesc = sparam(c, "EmissiveScale", 0.0013, -200, 160)
mel.connect_material_property(op(c, unreal.MaterialExpressionMultiply, ce, "", cesc, "", -50, 60), "", MP.MP_EMISSIVE_COLOR)
mel.recompile_material(c); eal.save_asset(c.get_path_name())
print("SLIME BUILT")
