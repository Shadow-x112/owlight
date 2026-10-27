# Start PIE, walk the player around with scripted input, and take screenshots along the way.
# PW options: 'shots' = [(name, seconds_from_start)], 'path' = [((fx, fy), seconds), ...] movement
# segments (fx, fy = camera-relative forward/right in -1..1), 'end' seconds.
import unreal
PW = globals().get("PW", {})
PATH = PW.get("path", [((1, 0), 1.5), ((0, 1), 1.5), ((-1, 0), 1.5), ((0, -1), 1.0), ((0, 0), 1.0)])
SHOTS = PW.get("shots", [("walk_0", 0.5), ("walk_1", 2.0), ("walk_2", 3.6), ("walk_3", 5.2)])
END = float(PW.get("end", 7.0))
RES = PW.get("res", (1920, 1080))

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"t": 0.0, "shots": list(SHOTS), "started": False, "handle": None, "log": []}

def pawn():
    w = UES.get_game_world()
    if not w: return None
    return unreal.GameplayStatics.get_player_pawn(w, 0)

def tick(dt):
    if not LES.is_in_play_in_editor():
        if state["started"]:
            unreal.unregister_slate_post_tick_callback(state["handle"])
            unreal.log("PIE WALK DONE " + " | ".join(state["log"]))
        return
    state["started"] = True
    state["t"] += dt
    p = pawn()
    if p:
        t = 0.0; move = (0, 0)
        for (f, dur) in PATH:
            if state["t"] < t + dur: move = f; break
            t += dur
        yaw = float(p.get_editor_property("CameraYaw"))
        fwd = unreal.Rotator(0, 0, yaw).get_forward_vector(); right = unreal.Rotator(0, 0, yaw).get_right_vector()
        if move[0]: p.add_movement_input(fwd, float(move[0]), False)
        if move[1]: p.add_movement_input(right, float(move[1]), False)
    if state["shots"] and state["t"] >= state["shots"][0][1]:
        name, _ = state["shots"].pop(0)
        unreal.AutomationLibrary.take_high_res_screenshot(RES[0], RES[1], name + ".png")
        loc = p.get_actor_location() if p else None
        state["log"].append(f"{name} t={state['t']:.1f} pawn={'%.0f,%.0f,%.0f' % (loc.x, loc.y, loc.z) if loc else 'none'}")
    if state["t"] >= END:
        LES.editor_request_end_play()

state["handle"] = unreal.register_slate_post_tick_callback(tick)
LES.editor_request_begin_play()
print("PIE WALK STARTED")
