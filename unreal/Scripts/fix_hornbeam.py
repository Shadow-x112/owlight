# Rebuild materials for the Hornbeam trees copied from 21112 (their PVE master didn't come along).
import unreal
EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary
ATH = unreal.AssetToolsHelpers.get_asset_tools()
T = '/Game/Megaplant_Library/Tree_Hornbeam/Textures/'
MATS = '/Game/Megaplant_Library/Tree_Hornbeam/Materials'
MP = unreal.MaterialProperty

def material(name):
    path = f'{MATS}/{name}'
    if EAL.does_asset_exist(path):
        m = EAL.load_asset(path); MEL.delete_all_material_expressions(m)
    else:
        m = ATH.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    return m

def tex(m, path, x, y, sampler):
    n = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, x, y)
    n.set_editor_property('texture', EAL.load_asset(path)); n.set_editor_property('sampler_type', sampler)
    return n

# --- bark: C + NAH (normal xy in RG, AO in B, height in A)
nah = EAL.load_asset(T + 'T_Hornbeam_01_Bark_NAH')
nah.set_editor_property('srgb', False); nah.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
EAL.save_asset(nah.get_path_name())
bark = material('M_Hornbeam_Bark')
c = tex(bark, T + 'T_Hornbeam_01_Bark_C', -600, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
p = tex(bark, T + 'T_Hornbeam_01_Bark_NAH', -600, 100, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
cu = MEL.create_material_expression(bark, unreal.MaterialExpressionCustom, -300, 100)
cu.set_editor_property('code', 'float2 xy = P.rg * 2.0 - 1.0; return float3(xy, sqrt(saturate(1.0 - dot(xy, xy))));')
cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
ci = unreal.CustomInput(); ci.set_editor_property('input_name', 'P'); cu.set_editor_property('inputs', [ci])
MEL.connect_material_expressions(p, '', cu, 'P')
MEL.connect_material_property(c, 'RGB', MP.MP_BASE_COLOR)
MEL.connect_material_property(cu, '', MP.MP_NORMAL)
MEL.connect_material_property(p, 'B', MP.MP_AMBIENT_OCCLUSION)
r = MEL.create_material_expression(bark, unreal.MaterialExpressionConstant, -300, 400); r.set_editor_property('r', 0.85)
MEL.connect_material_property(r, '', MP.MP_ROUGHNESS)
MEL.recompile_material(bark); EAL.save_asset(bark.get_path_name())

# --- foliage: CA (color + alpha), NT (normal + translucency?) -> two-sided foliage, masked
fol = material('M_Hornbeam_Foliage')
fol.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
fol.set_editor_property('two_sided', True)
fol.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
ca = tex(fol, T + 'T_Hornbeam_01_Foliage_CA', -600, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
nt = EAL.load_asset(T + 'T_Hornbeam_01_Foliage_NT')
nt.set_editor_property('srgb', False); nt.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
EAL.save_asset(nt.get_path_name())
n = tex(fol, T + 'T_Hornbeam_01_Foliage_NT', -600, 150, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
cu2 = MEL.create_material_expression(fol, unreal.MaterialExpressionCustom, -300, 150)
cu2.set_editor_property('code', 'float2 xy = P.rg * 2.0 - 1.0; return float3(xy, sqrt(saturate(1.0 - dot(xy, xy))));')
cu2.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
ci2 = unreal.CustomInput(); ci2.set_editor_property('input_name', 'P'); cu2.set_editor_property('inputs', [ci2])
MEL.connect_material_expressions(n, '', cu2, 'P')
# autumn tint: warm the leaf colour a little
tint = MEL.create_material_expression(fol, unreal.MaterialExpressionVectorParameter, -600, -400)
tint.set_editor_property('parameter_name', 'Tint'); tint.set_editor_property('default_value', unreal.LinearColor(1.15, 0.95, 0.6, 1))
mul = MEL.create_material_expression(fol, unreal.MaterialExpressionMultiply, -350, -250)
MEL.connect_material_expressions(ca, 'RGB', mul, 'A'); MEL.connect_material_expressions(tint, '', mul, 'B')
MEL.connect_material_property(mul, '', MP.MP_BASE_COLOR)
MEL.connect_material_property(ca, 'A', MP.MP_OPACITY_MASK)
MEL.connect_material_property(cu2, '', MP.MP_NORMAL)
ss = MEL.create_material_expression(fol, unreal.MaterialExpressionMultiply, -150, -100)
sst = MEL.create_material_expression(fol, unreal.MaterialExpressionConstant3Vector, -350, -50); sst.set_editor_property('constant', unreal.LinearColor(0.9, 0.8, 0.35, 1))
MEL.connect_material_expressions(mul, '', ss, 'A'); MEL.connect_material_expressions(sst, '', ss, 'B')
MEL.connect_material_property(ss, '', MP.MP_SUBSURFACE_COLOR)
r2 = MEL.create_material_expression(fol, unreal.MaterialExpressionConstant, -300, 400); r2.set_editor_property('r', 0.6)
MEL.connect_material_property(r2, '', MP.MP_ROUGHNESS)
MEL.recompile_material(fol); EAL.save_asset(fol.get_path_name())

for name, parent in (('MI_Tree_Hornbeam_01_Bark', bark), ('MI_Tree_Hornbeam_01_Foliage', fol)):
    mi = EAL.load_asset(f'{MATS}/{name}')
    MEL.set_material_instance_parent(mi, parent); EAL.save_asset(mi.get_path_name())
print('hornbeam materials rebuilt')
