# Import the Poly Haven downloads (RawAssets/PolyHaven) into /Game/PolyHaven.
# Models -> static mesh + textures + material instances per slot. Textures -> material instances.
# Re-runnable: skips assets that already exist unless PH.get('force').
import os, re, unreal

RAW = r'C:/Users/manri/Documents/UnrealProjects/Owlight/RawAssets/PolyHaven'
ROOT = '/Game/PolyHaven'
PH = globals().get('PH', {})
FORCE = PH.get('force', False)
ONLY = PH.get('only')  # list of ids or None
SPLIT = set(PH.get('split', []))  # ids whose FBX holds several variants: import as separate meshes
NANITE = PH.get('nanite', True)

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
ATH = unreal.AssetToolsHelpers.get_asset_tools()
log = unreal.log

# ---------- master materials ----------
def master(name, masked):
    path = f'{ROOT}/Masters/{name}'
    if EAL.does_asset_exist(path) and not FORCE:
        return EAL.load_asset(path)
    if EAL.does_asset_exist(path):
        m = EAL.load_asset(path); MEL.delete_all_material_expressions(m)
    else:
        m = ATH.create_asset(name, f'{ROOT}/Masters', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', masked)
    if masked:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    def tex(pname, x, y, sampler):
        n = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        n.set_editor_property('parameter_name', pname)
        n.set_editor_property('sampler_type', sampler)
        if sampler == unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL:
            n.set_editor_property('texture', EAL.load_asset('/Engine/EngineMaterials/DefaultNormal'))
        elif sampler == unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE:
            n.set_editor_property('texture', EAL.load_asset('/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq'))
        return n
    def scalar(pname, default, x, y):
        n = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property('parameter_name', pname); n.set_editor_property('default_value', default)
        return n
    tc = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tile = scalar('Tiling', 1.0, -900, 120)
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -720, 40)
    MEL.connect_material_expressions(tc, '', mul, 'A'); MEL.connect_material_expressions(tile, '', mul, 'B')
    d = tex('BaseColor', -500, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    n = tex('Normal', -500, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    r = tex('Roughness', -500, 400, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    a = tex('AO', -500, 700, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    for s in (d, n, r, a):
        MEL.connect_material_expressions(mul, '', s, 'UVs')
    MEL.connect_material_property(d, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(n, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(r, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(a, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if masked:
        o = tex('Opacity', -500, 1000, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        MEL.connect_material_expressions(mul, '', o, 'UVs')
        MEL.connect_material_property(o, 'R', unreal.MaterialProperty.MP_OPACITY_MASK)
        # subsurface tint for leaves
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        ss = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, -300)
        tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, -500)
        tint.set_editor_property('parameter_name', 'SubsurfaceTint')
        tint.set_editor_property('default_value', unreal.LinearColor(0.6, 0.9, 0.3, 1))
        MEL.connect_material_expressions(d, 'RGB', ss, 'A'); MEL.connect_material_expressions(tint, '', ss, 'B')
        MEL.connect_material_property(ss, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    MEL.recompile_material(m); EAL.save_asset(path)
    return m

# ---------- texture import ----------
def import_textures(files, dest):
    tasks = []
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        if EAL.does_asset_exist(f'{dest}/{name}') and not FORCE:
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property('filename', f); t.set_editor_property('destination_path', dest)
        t.set_editor_property('automated', True); t.set_editor_property('save', True)
        t.set_editor_property('replace_existing', True)
        tasks.append(t)
    if tasks:
        ATH.import_asset_tasks(tasks)
    out = {}
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        tex = EAL.load_asset(f'{dest}/{name}')
        if not tex:
            log(f'  missing texture {name}'); continue
        kind = classify(name)
        if kind == 'nor':
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        elif kind in ('rough', 'ao', 'disp', 'alpha', 'mask'):
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
        EAL.save_asset(tex.get_path_name())
        out[name] = (kind, tex)
    return out

def classify(name):
    for k, pat in (('nor', r'_nor_dx_'), ('rough', r'_rough_'), ('ao', r'_ao_'), ('disp', r'_disp_'),
                   ('alpha', r'_alpha_'), ('mask', r'_mask_'), ('diff', r'_diff_')):
        if re.search(pat, name): return k
    return 'other'

def slot_prefix(name, aid):
    # 'fir_tree_01_bark_diff_2k' -> 'bark'; 'rock_07_diff_2k' -> ''
    s = name[len(aid) + 1:] if name.startswith(aid + '_') else name
    s = re.sub(r'_?(diff|nor_dx|rough|ao|disp|alpha|mask)_2k$', '', s)
    return s

def make_instance(name, dest, parent, maps):
    path = f'{dest}/{name}'
    if EAL.does_asset_exist(path):
        mi = EAL.load_asset(path)
    else:
        mi = ATH.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for kind, pname in (('diff', 'BaseColor'), ('nor', 'Normal'), ('rough', 'Roughness'), ('ao', 'AO'), ('alpha', 'Opacity')):
        if kind in maps:
            MEL.set_material_instance_texture_parameter_value(mi, pname, maps[kind])
    EAL.save_asset(path)
    return mi

# ---------- models ----------
def import_model(aid, folder, m_opaque, m_masked):
    dest = f'{ROOT}/Models/{aid}'
    fbx = next(os.path.join(folder, f) for f in os.listdir(folder) if f.lower().endswith('.fbx'))
    imgs = [os.path.join(folder, f) for f in os.listdir(folder) if f.lower().endswith(('.jpg', '.png'))]
    mesh_path = f'{dest}/SM_{aid}'
    split = aid in SPLIT
    if split and EAL.does_asset_exist(mesh_path):
        EAL.delete_asset(mesh_path)
    if not EAL.does_asset_exist(mesh_path) or FORCE:
        ui = unreal.FbxImportUI()
        ui.set_editor_property('import_mesh', True); ui.set_editor_property('import_as_skeletal', False)
        ui.set_editor_property('import_materials', False); ui.set_editor_property('import_textures', False)
        ui.set_editor_property('import_animations', False)
        smd = ui.static_mesh_import_data
        smd.set_editor_property('combine_meshes', not split)
        smd.set_editor_property('generate_lightmap_u_vs', False)
        smd.set_editor_property('auto_generate_collision', True)
        smd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
        t = unreal.AssetImportTask()
        t.set_editor_property('filename', fbx); t.set_editor_property('destination_path', dest)
        if not split: t.set_editor_property('destination_name', f'SM_{aid}')
        t.set_editor_property('automated', True); t.set_editor_property('save', True)
        t.set_editor_property('replace_existing', True); t.set_editor_property('options', ui)
        ATH.import_asset_tasks([t])
    if split:
        # keep LOD0 only (Nanite), drop the rest, and name everything SM_<id>[_variant]
        for p in EAL.list_assets(dest, recursive=False):
            full = p.split('.')[0]; name = full.rsplit('/', 1)[1]
            if not isinstance(EAL.load_asset(full), unreal.StaticMesh): continue
            mm = re.match(r'^(.*?)_LOD(\d+)$', name)
            if mm and int(mm.group(2)) > 0:
                EAL.delete_asset(full); continue
            base = mm.group(1) if mm else name
            if base.startswith(aid): base = 'SM_' + base
            if base != name:
                if EAL.does_asset_exist(f'{dest}/{base}'): EAL.delete_asset(f'{dest}/{base}')
                EAL.rename_asset(full, f'{dest}/{base}')
    meshes = [EAL.load_asset(p.split('.')[0]) for p in EAL.list_assets(dest, recursive=False) if p.rsplit('/',1)[1].startswith('SM_')] if split else [EAL.load_asset(mesh_path)]
    meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
    if not meshes:
        log(f'!! mesh import failed {aid}'); return
    texs = import_textures(imgs, f'{dest}/Textures')
    # group maps by slot prefix
    groups = {}
    for name, (kind, tex) in texs.items():
        groups.setdefault(slot_prefix(name, aid), {})[kind] = tex
    for mesh in meshes:
        assign(mesh, aid, groups, m_opaque, m_masked, dest)
    log(f'  {aid}: {len(meshes)} meshes, groups {list(groups)}')

def assign(mesh, aid, groups, m_opaque, m_masked, dest):
    mats = mesh.get_editor_property('static_materials')
    new_mats = []
    for sm in mats:
        slot = str(sm.get_editor_property('material_slot_name')).lower()
        key = next((g for g in groups if g and g in slot), None)
        if key is None:
            key = '' if '' in groups else (next(iter(groups)) if groups else None)
        if key is None:
            new_mats.append(sm); continue
        maps = groups[key]
        masked = 'alpha' in maps
        mi = make_instance(f'MI_{aid}{("_" + key) if key else ""}', dest, m_masked if masked else m_opaque, maps)
        sm.set_editor_property('material_interface', mi)
        new_mats.append(sm)
    mesh.set_editor_property('static_materials', new_mats)
    if NANITE:
        ns = mesh.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', True)
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, ns, True)
    EAL.save_asset(mesh.get_path_name())

# ---------- surface textures ----------
def import_surface(aid, folder, m_opaque):
    dest = f'{ROOT}/Surfaces/{aid}'
    imgs = [os.path.join(folder, f) for f in os.listdir(folder) if f.lower().endswith(('.jpg', '.png'))]
    texs = import_textures(imgs, dest)
    maps = {kind: tex for _, (kind, tex) in texs.items()}
    make_instance(f'MI_{aid}', dest, m_opaque, maps)
    log(f'  surface {aid}: {list(maps)}')

m_opaque = master('M_PH_Opaque', False)
m_masked = master('M_PH_Masked', True)
for kind in ('models', 'textures'):
    base = os.path.join(RAW, kind)
    for aid in sorted(os.listdir(base)):
        if ONLY and aid not in ONLY: continue
        folder = os.path.join(base, aid)
        log(f'[{kind}] {aid}')
        if kind == 'models': import_model(aid, folder, m_opaque, m_masked)
        else: import_surface(aid, folder, m_opaque)
log('PolyHaven import done')

