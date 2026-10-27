# ---- ground + forest from Poly Haven scans (CC0) ------------------------------------------------
PH = "/Game/PolyHaven/Models"; SURF = "/Game/PolyHaven/Surfaces"
GROUND_MI = P.get("ground", "forest_floor")
gm = unreal.load_asset(f"{SURF}/{GROUND_MI}/MI_{GROUND_MI}")
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(gm, "Tiling", float(P.get("ground_tiling", 14.0)))
ground = mesh_actor("/Engine/BasicShapes/Plane", (0, 0, 0), scale=(40, 40, 1), label="Ground", mat=f"{SURF}/{GROUND_MI}/MI_{GROUND_MI}")

def variants(aid):
    return [p.split(".")[0] for p in eal.list_assets(f"{PH}/{aid}", recursive=False) if p.rsplit("/", 1)[1].startswith("SM_")]

CAM = P.get('cam', (-560, -410)); CAM_CLEAR = float(P.get('cam_clear', 420))
def ring(r0, r1, n):
    out = []
    while len(out) < n:
        t = rng.uniform(0, 2 * math.pi); r = rng.uniform(r0, r1)
        x, y = r * math.cos(t), r * math.sin(t)
        if math.hypot(x - CAM[0], y - CAM[1]) < CAM_CLEAR: continue   # keep the camera's spot open
        out.append((x, y))
    return out

def scatter(aids, r0, r1, n, label, smin=0.9, smax=1.2, tilt=0.0):
    for i, (x, y) in enumerate(ring(r0, r1, n)):
        aid = rng.choice(aids); vs = variants(aid)
        if not vs: continue
        s = rng.uniform(smin, smax)
        mesh_actor(rng.choice(vs), (x, y, 0), (rng.uniform(-tilt, tilt), rng.uniform(-tilt, tilt), rng.uniform(0, 360)), (s, s, s), f"{label}_{i}")

# canopy: the Hornbeam skeletal trees from 21112 (8-16 m), ring just outside the clearing
HORN = "/Game/Megaplant_Library/Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_"
for i, (x, y) in enumerate(ring(620, 950, 7)):
    a = spawn(unreal.SkeletalMeshActor, (x, y, 0), (0, 0, rng.uniform(0, 360)), f"Tree_{i}")
    a.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_asset(HORN + rng.choice("AABBCD")))
    s = rng.uniform(0.9, 1.2); a.skeletal_mesh_component.set_world_scale3d(unreal.Vector(s, s, s))
for i, (x, y) in enumerate(ring(1000, 1700, 16)):   # close the horizon
    a = spawn(unreal.SkeletalMeshActor, (x, y, 0), (0, 0, rng.uniform(0, 360)), f"FarTree_{i}")
    a.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_asset(HORN + rng.choice("ABCD")))
    s = rng.uniform(1.0, 1.4); a.skeletal_mesh_component.set_world_scale3d(unreal.Vector(s, s, s))
scatter(["fir_tree_01"], 1100, 1600, 4, "FarFir", 0.9, 1.2)
scatter(["tree_small_02", "fir_sapling_medium"], 520, 800, 4, "Understory", 0.8, 1.1)
# the big forms around the clearing
scatter(["rock_moss_set_01", "rock_moss_set_02"], 380, 600, 4, "MossRocks", 0.7, 0.9)
scatter(["dead_tree_trunk_02", "dead_tree_trunk"], 280, 520, 3, "Log", 1.0, 1.0)
scatter(["tree_stump_01", "tree_stump_02"], 300, 520, 2, "Stump", 1.0, 1.1)
scatter(["root_cluster_01", "root_cluster_02", "pine_roots"], 250, 500, 4, "Roots", 1.0, 1.0)
scatter(["boulder_01"], 350, 600, 2, "Boulder", 0.8, 1.1)
# undergrowth
scatter(["fern_02"], 180, 650, 26, "Fern", 0.7, 1.2, 4)
scatter(["shrub_01", "shrub_02", "shrub_03", "shrub_04"], 300, 750, 10, "Shrub", 0.8, 1.2)
scatter(["nettle_plant", "celandine_01"], 200, 650, 10, "Weed", 0.8, 1.2)
scatter(["grass_medium_01", "grass_medium_02"], 100, 650, 90, "Grass", 1.0, 1.6)
scatter(["moss_01", "bark_debris_01", "dry_branches_medium_01"], 120, 600, 30, "Litter", 0.9, 1.3)
scatter(["stone_01", "rock_07", "rock_09"], 100, 500, 18, "Pebble", 0.9, 1.6)
# right by the slime, for scale
mesh_actor(variants("fern_02")[0], (-120, 95, 0), (0, 0, 140), (0.9, 0.9, 0.9), "NearFern")
mesh_actor(variants("stone_01")[0], (80, -60, 0), (0, 0, 20), (1.4, 1.4, 1.4), "NearStone")
mesh_actor(variants("moss_01")[0], (40, 70, 0), (0, 0, 60), (1.2, 1.2, 1.2), "NearMoss")
