"""Owlight style proof: one slime on a forest floor at dusk, photoreal underneath, pixelated on top.
Builds /Game/Owlight/Maps/StyleProof from scratch (destroys and rebuilds every actor it owns).
Run: node ue_py.mjs -c "exec(open(r'<proj>/Scripts/style_proof.py',encoding='utf-8').read(), {'SP':{}})"
SP options: 'seed' (int), 'world_px' (float, default 4), 'slime_px' (float, default 2), 'pixel' (bool, default True)
"""
import unreal, random, math
P = globals().get("SP", {})
SEED = P.get("seed", 7); WORLD_PX = float(P.get("world_px", 4.0)); SLIME_PX = float(P.get("slime_px", 2.0)); PIXEL = P.get("pixel", True)
LVL = "/Game/Owlight/Maps/StyleProof"; MATS = "/Game/Owlight/Materials"
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); A = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
rng = random.Random(SEED)

# ---- level -------------------------------------------------------------------------------
if not eal.does_asset_exist(LVL):
    LES.new_level(LVL)
elif UES.get_editor_world().get_path_name().split(".")[0] != LVL:
    LES.load_level(LVL)
for a in A.get_all_level_actors():
    if a.get_actor_label().startswith("SP_"): A.destroy_actor(a)

def spawn(cls, loc, rot=(0, 0, 0), label=None, folder="StyleProof"):
    a = A.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    if label: a.set_actor_label("SP_" + label)
    a.set_folder_path(folder)
    return a

def mesh_actor(path, loc, rot=(0, 0, 0), scale=(1, 1, 1), label=None, mat=None):
    a = spawn(unreal.StaticMeshActor, loc, rot, label)
    c = a.static_mesh_component
    c.set_static_mesh(unreal.load_asset(path))
    c.set_world_scale3d(unreal.Vector(*scale))
    if mat: c.set_material(0, unreal.load_asset(mat))
    return a

# ---- light: dusk ---------------------------------------------------------------------------
SUN_YAW = float(P.get("sun_yaw", 205)); SUN_PITCH = float(P.get("sun_pitch", -14))
sun = spawn(unreal.DirectionalLight, (0, 0, 800), (0, SUN_PITCH, SUN_YAW), "Sun")   # low, from in front of the camera: backlights the slime
sc = sun.light_component
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_intensity(float(P.get("sun", 9.0))); sc.set_light_color(unreal.LinearColor(1.0, 0.62, 0.36))
sc.set_editor_property("atmosphere_sun_light", True)
sc.set_editor_property("light_source_angle", 1.2)
sc.set_editor_property("volumetric_scattering_intensity", 3.0)
sc.set_enable_light_shaft_bloom(True); sc.set_editor_property("bloom_scale", 0.15)
spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
skc = sky.light_component
skc.set_mobility(unreal.ComponentMobility.MOVABLE)
skc.set_editor_property("real_time_capture", True); skc.set_intensity(2.5)
fog = spawn(unreal.ExponentialHeightFog, (0, 0, -100), label="Fog")
fc = fog.component
fc.set_editor_property("fog_density", 0.018); fc.set_editor_property("fog_height_falloff", 0.6)
fc.set_volumetric_fog(True)
fc.set_editor_property("volumetric_fog_scattering_distribution", 0.4)
fc.set_editor_property("volumetric_fog_extinction_scale", 2.0)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.35, 0.28, 0.22))

if P.get('ph', True):
    exec(open(r'C:/Users/manri/Documents/UnrealProjects/Owlight/Scripts/sp_forest_ph.py', encoding='utf-8-sig').read())
else:
    # ---- ground ----------------------------------------------------------------------------------
    ground = mesh_actor("/Engine/BasicShapes/Plane", (0, 0, 0), scale=(40, 40, 1), label="Ground",
                        mat="/Game/ProceduralNaturePack/Materials/terrain_materials/dirt_ground_large")

    # ---- forest ring: trees, rocks, ferns, ivy -------------------------------------------------
    def ring(r0, r1, n):
        out = []
        for _ in range(n):
            t = rng.uniform(0, 2 * math.pi); r = rng.uniform(r0, r1)
            out.append((r * math.cos(t), r * math.sin(t)))
        return out

    for i, (x, y) in enumerate(ring(700, 1100, 5)):
        s = rng.uniform(0.9, 1.3)
        mesh_actor("/Game/ProceduralNaturePack/Meshes/trees_w_roots/large_tree_w_roots_a", (x, y, 0),
                   (0, 0, rng.uniform(0, 360)), (s, s, s), f"Tree_{i}")
    for i, (x, y) in enumerate(ring(420, 650, 4)):
        m = rng.choice(["rock_boulder_a", "rock_boulder_b1", "rock_boulder_b2", "rock_boulder_b3"])
        s = rng.uniform(0.6, 1.1)
        mesh_actor(f"/Game/ProceduralNaturePack/Meshes/rocks_boulders/{m}", (x, y, 0), (0, 0, rng.uniform(0, 360)), (s, s, s), f"Boulder_{i}")
    for i, (x, y) in enumerate(ring(260, 600, 10)):
        m = rng.choice(["rock_group_small_a1", "rock_group_small_b2", "rock_group_small_c1", "rock_group_small_a3"])
        mesh_actor(f"/Game/ProceduralNaturePack/Meshes/rocks_small/{m}", (x, y, 0), (0, 0, rng.uniform(0, 360)), (1, 1, 1), f"Rocks_{i}")
    for i, (x, y) in enumerate(ring(280, 700, 16)):
        m = rng.choice(["leaves_fern_01", "leaves_fern_02"])
        s = rng.uniform(0.8, 1.4)
        mesh_actor(f"/Game/ProceduralNaturePack/Meshes/leaves/{m}", (x, y, 0), (0, 0, rng.uniform(0, 360)), (s, s, s), f"Fern_{i}")
    for i, (x, y) in enumerate(ring(300, 650, 8)):
        v = rng.choice("ABCDEFGH")
        mesh_actor(f"/Game/Fab/Megascans/Plants/English_Ivy_xfcnebhqx/Medium/xfcnebhqx_tier_2/StaticMeshes/SM_xfcnebhqx_Var{v}",
                   (x, y, 0), (0, 0, rng.uniform(0, 360)), (1, 1, 1), f"Ivy_{i}")
    for i, (x, y) in enumerate(ring(380, 620, 3)):
        v = rng.choice("ABCDEFGH")
        mesh_actor(f"/Game/Fab/Megascans/Plants/Bigleaf_Hydrangea_vgztealha/Medium/vgztealha_tier_2/StaticMeshes/SM_vgztealha_Var{v}",
                   (x, y, 0), (0, 0, rng.uniform(0, 360)), (1, 1, 1), f"Hydrangea_{i}")
    # a few small rocks and a fern right by the slime, for scale
    mesh_actor("/Game/ProceduralNaturePack/Meshes/rocks_small/rock_group_small_b1", (90, -70, 0), (0, 0, 20), (0.7, 0.7, 0.7), "NearRock")
    mesh_actor("/Game/ProceduralNaturePack/Meshes/leaves/leaves_fern_01", (-110, 90, 0), (0, 0, 140), (0.9, 0.9, 0.9), "NearFern")


# ---- the slime (meshes + materials from Scripts/slime_build.py) --------------------------------
SL = dict(color=(0.06, 0.50, 0.16), clarity=0.6, heat=0.0, glow=0.0, density=0.5, rim=None, pos=(0, 0, 0), yaw=20)
SL.update(P.get("slime", {}))
def spawn_slime(spec, label="Slime"):
    d = float(spec["density"])
    sq = 1.0 - 0.35 * (d - 0.5) * 2          # dense: squat and wide; light: tall and narrow
    scale = unreal.Vector(1.0 / math.sqrt(sq) * 1.0, 1.0 / math.sqrt(sq) * 1.0, sq)
    px, py, pz = spec["pos"]
    shell = spawn(unreal.StaticMeshActor, (px, py, pz - 2.0 * (d - 0.5) * 2), (0, 0, spec["yaw"]), label)
    sc = shell.static_mesh_component
    sc.set_static_mesh(unreal.load_asset(f"{MESHES}/SM_SlimeShell")); sc.set_world_scale3d(scale)
    sc.set_editor_property("render_custom_depth", True); sc.set_editor_property("custom_depth_stencil_value", 1)
    sc.set_editor_property("cast_shadow", True)
    mi = sc.create_dynamic_material_instance(0, unreal.load_asset(f"{MATS}/M_SlimeShell"))
    r, g, b = spec["color"]
    mi.set_vector_parameter_value("Color", unreal.LinearColor(r, g, b, 1))
    rim = spec["rim"] or (min(1, r * 2.5 + 0.2), min(1, g * 1.8 + 0.3), min(1, b * 2.5 + 0.2))
    mi.set_vector_parameter_value("RimColor", unreal.LinearColor(*rim, 1))
    for k in ("Clarity", "Heat", "Glow"): mi.set_scalar_parameter_value(k, float(spec[k.lower()]))
    mi.set_scalar_parameter_value("EmissiveScale", 0.03 * 2.0 ** (9.6 - float(P.get("exposure", 10.5))))   # 0.3 ~ scene white at bias 9.6
    core = spawn(unreal.StaticMeshActor, (px, py, pz + 11 * sq - 3 * (d - 0.5) * 2), (0, 0, 0), label + "Core")
    cc = core.static_mesh_component
    cc.set_static_mesh(unreal.load_asset(f"{MESHES}/SM_SlimeCore"))
    cs = 0.75 + 0.5 * d
    cc.set_world_scale3d(unreal.Vector(cs * 1.1, cs * 1.1, cs * 0.85))
    cc.set_editor_property("render_custom_depth", True); cc.set_editor_property("custom_depth_stencil_value", 1)
    cm = cc.create_dynamic_material_instance(0, unreal.load_asset(f"{MATS}/M_SlimeCore"))
    cm.set_vector_parameter_value("Color", unreal.LinearColor(r * 0.5, g * 0.55, b * 0.5, 1))
    for k in ("Heat", "Glow"): cm.set_scalar_parameter_value(k, float(spec[k.lower()]))
    cm.set_scalar_parameter_value("EmissiveScale", 0.03 * 2.0 ** (9.6 - float(P.get("exposure", 10.5))))
    core.attach_to_actor(shell, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    return shell
MESHES = "/Game/Owlight/Meshes"
spawn_slime(SL)
for i, extra in enumerate(P.get("more_slimes", [])):
    s2 = dict(SL); s2.update(extra); spawn_slime(s2, f"Slime{i+2}")
def new_material(name):
    path = f"{MATS}/{name}"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path); mel.delete_all_material_expressions(mat); return mat
    return tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
def ci(name):
    c = unreal.CustomInput(); c.set_editor_property("input_name", name); return c
def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)
MP = unreal.MaterialProperty
# ---- pixel post-process ----------------------------------------------------------------------
pp = new_material("M_PixelPass")
pp.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
pp.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
wpx = expr(pp, unreal.MaterialExpressionScalarParameter, -700, -100); wpx.set_editor_property("parameter_name", "WorldPixel"); wpx.set_editor_property("default_value", WORLD_PX)
spx = expr(pp, unreal.MaterialExpressionScalarParameter, -700, 0); spx.set_editor_property("parameter_name", "SlimePixel"); spx.set_editor_property("default_value", SLIME_PX)
# SceneTexture nodes must exist in the graph for SceneTextureLookup to compile; they are not used directly.
st0 = expr(pp, unreal.MaterialExpressionSceneTexture, -700, 200); st0.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
st1 = expr(pp, unreal.MaterialExpressionSceneTexture, -700, 320); st1.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
zero = expr(pp, unreal.MaterialExpressionConstant, -500, 260); zero.set_editor_property("r", 0.0)
m0 = expr(pp, unreal.MaterialExpressionMultiply, -350, 220); mk0 = expr(pp, unreal.MaterialExpressionComponentMask, -520, 200); mk0.set_editor_property("r", True); mk0.set_editor_property("g", True); mk0.set_editor_property("b", True); mel.connect_material_expressions(st0, "Color", mk0, ""); mel.connect_material_expressions(mk0, "", m0, "A"); mel.connect_material_expressions(zero, "", m0, "B")
m1 = expr(pp, unreal.MaterialExpressionMultiply, -350, 320); mk1 = expr(pp, unreal.MaterialExpressionComponentMask, -520, 320); mk1.set_editor_property("r", True); mk1.set_editor_property("g", True); mk1.set_editor_property("b", True); mel.connect_material_expressions(st1, "Color", mk1, ""); mel.connect_material_expressions(mk1, "", m1, "A"); mel.connect_material_expressions(zero, "", m1, "B")
pc = expr(pp, unreal.MaterialExpressionCustom, -350, -60)
pc.set_editor_property("code", """
float2 vs = View.ViewSizeAndInvSize.xy;
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
// Is the pixel we are about to write mostly slime? Decide the grid from the stencil at the
// cell center so the whole cell agrees.
float2 cw = (floor(uv * vs / WorldPixel) + 0.5) * WorldPixel / vs;
float2 cs = (floor(uv * vs / SlimePixel) + 0.5) * SlimePixel / vs;
float stW = SceneTextureLookup(cw, 25, false).r;
float stS = SceneTextureLookup(cs, 25, false).r;
float2 q = (stS > 0.5 || stW > 0.5) ? cs : cw;
return SceneTextureLookup(q, 14, false).rgb;
""")
pc.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
pc.set_editor_property("inputs", [ci("WorldPixel"), ci("SlimePixel")])
mel.connect_material_expressions(wpx, "", pc, "WorldPixel"); mel.connect_material_expressions(spx, "", pc, "SlimePixel")
add = expr(pp, unreal.MaterialExpressionAdd, -150, 0); mel.connect_material_expressions(pc, "", add, "A"); mel.connect_material_expressions(m0, "", add, "B")
add2 = expr(pp, unreal.MaterialExpressionAdd, 0, 0); mel.connect_material_expressions(add, "", add2, "A"); mel.connect_material_expressions(m1, "", add2, "B")
mel.connect_material_property(add2, "", MP.MP_EMISSIVE_COLOR)
mel.recompile_material(pp); eal.save_asset(pp.get_path_name())

ppv = spawn(unreal.PostProcessVolume, (0, 0, 0), label="PostProcess")
ppv.set_editor_property("unbound", True)
s = ppv.settings
s.set_editor_property("override_auto_exposure_method", True); s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property("override_auto_exposure_bias", True); s.set_editor_property("auto_exposure_bias", float(P.get("exposure", 10.5)))
s.set_editor_property("override_dynamic_global_illumination_method", True); s.set_editor_property("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.LUMEN)
s.set_editor_property("override_reflection_method", True); s.set_editor_property("reflection_method", unreal.ReflectionMethod.LUMEN)
s.set_editor_property("override_lumen_scene_lighting_quality", True); s.set_editor_property("lumen_scene_lighting_quality", 2.0)
s.set_editor_property("override_lumen_final_gather_quality", True); s.set_editor_property("lumen_final_gather_quality", 2.0)
s.set_editor_property("override_bloom_intensity", True); s.set_editor_property("bloom_intensity", 0.15)
s.set_editor_property("override_film_grain_intensity", True); s.set_editor_property("film_grain_intensity", 0.0)
if PIXEL:
    s.set_editor_property("weighted_blendables", unreal.WeightedBlendables(array=[unreal.WeightedBlendable(weight=1.0, object=pp)]))
ppv.settings = s

LES.save_current_level()
print("STYLE PROOF BUILT", LVL, "world_px", WORLD_PX, "slime_px", SLIME_PX, "pixel", PIXEL)
