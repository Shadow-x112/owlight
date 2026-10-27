"""Game-quality stills via Movie Render Queue (full Lumen GI + reflections, TSR warm-up, works with the editor unfocused).
MRQ={'shots':[(name,(x,y,z),(pitch,yaw),fov), ...], 'level':'/Game/Owlight/Maps/StyleProof', 'res':(1920,1080), 'warm':48}
Builds /Game/Owlight/Cine/LS_Shots: one camera-cut section per shot, 1 output frame each after `warm` warm-up frames.
Output: Saved/Screenshots/MRQ/<name>.png (renamed from the per-shot frame files). Asynchronous: poll for the files.
Each shot = its own LevelSequence + job (keeps naming simple)."""
import unreal, os
P = globals().get("MRQ", {})
LVL = P.get("level", "/Game/Owlight/Maps/StyleProof"); RES = P.get("res", (1920, 1080)); WARM = P.get("warm", 48)
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Screenshots", "MRQ")
os.makedirs(OUT, exist_ok=True)
eal = unreal.EditorAssetLibrary; tools = unreal.AssetToolsHelpers.get_asset_tools()
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); A = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split(".")[0] != LVL: LES.load_level(LVL)
for a in A.get_all_level_actors():
    if a.get_actor_label().startswith("MRQ_Cam_"): A.destroy_actor(a)
q = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem).get_queue()
for j in list(q.get_jobs()): q.delete_job(j)
FPS = 24
for (name, loc, rot, fov) in P["shots"]:
    cam = A.spawn_actor_from_class(unreal.CineCameraActor, unreal.Vector(*loc), unreal.Rotator(roll=0, pitch=rot[0], yaw=rot[1]))
    cam.set_actor_label(f"MRQ_Cam_{name}"); cam.set_folder_path("StyleProof/Cams")
    cc = cam.get_cine_camera_component()
    fb = cc.get_editor_property("filmback"); fb.set_editor_property("sensor_width", 36.0); fb.set_editor_property("sensor_height", 36.0 * RES[1] / RES[0])
    cc.set_editor_property("filmback", fb)
    import math
    cc.set_editor_property("current_focal_length", 18.0 / math.tan(math.radians(fov) / 2))
    fs = cc.get_editor_property("focus_settings"); fs.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE); cc.set_editor_property("focus_settings", fs)
    sp = f"/Game/Owlight/Cine/LS_{name}"
    if eal.does_asset_exist(sp): eal.delete_asset(sp)
    seq = tools.create_asset(f"LS_{name}", "/Game/Owlight/Cine", unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(FPS, 1)); seq.set_playback_start(0); seq.set_playback_end(1)
    b = seq.add_possessable(cam)
    ct = seq.add_track(unreal.MovieSceneCameraCutTrack); sec = ct.add_section(); sec.set_range(0, 1)
    bid = unreal.MovieSceneObjectBindingID(); bid.set_editor_property("guid", b.get_id()); sec.set_camera_binding_id(bid)
    eal.save_asset(sp)
    job = q.allocate_new_job(unreal.MoviePipelineExecutorJob)
    job.set_editor_property("map", unreal.SoftObjectPath(LVL)); job.set_editor_property("sequence", unreal.SoftObjectPath(sp)); job.job_name = name
    cfg = job.get_configuration()
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
    out = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    out.output_directory = unreal.DirectoryPath(OUT); out.file_name_format = name; out.output_resolution = unreal.IntPoint(*RES)
    out.set_editor_property("override_existing_output", True)
    aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    aa.set_editor_property("engine_warm_up_count", WARM); aa.set_editor_property("render_warm_up_count", WARM)
    aa.set_editor_property("render_warm_up_frames", True)
    aa.set_editor_property("spatial_sample_count", P.get("spatial", 1)); aa.set_editor_property("temporal_sample_count", P.get("temporal", 1))
    gs = cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
    gs.set_editor_property("cinematic_quality_settings", True); gs.set_editor_property("texture_streaming", unreal.MoviePipelineTextureStreamingMethod.DISABLED)
LES.save_current_level()
ex = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem).render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
print("MRQ STARTED", [s[0] for s in P["shots"]], "->", OUT)
