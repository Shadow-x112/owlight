# PIE test of the live sim: a SlimeActor runs a day on fast time, fed charcoal, and we watch it
# turn into a soot slime. ST options: 'time_scale' (sim seconds per real second), 'end' (real s),
# 'feed' (food id), 'feed_every' (real s), 'shots' (real s list).
import unreal
ST = globals().get("ST", {})
TIME_SCALE = float(ST.get("time_scale", 3600.0))     # 1 real second = 1 sim hour
END = float(ST.get("end", 40.0))
FOOD = ST.get("feed", "charcoal"); FEED_EVERY = float(ST.get("feed_every", 4.0))
SHOTS = list(ST.get("shots", [2.0, 14.0, 26.0, 38.0]))

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"t": 0.0, "next_feed": 1.0, "handle": None, "log": [], "last_type": None, "started": False, "n": 0}

def slime_actor():
    w = UES.get_game_world()
    if not w: return None
    acts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SlimeActor)
    return acts[0] if acts else None

def tick(dt):
    if not LES.is_in_play_in_editor():
        if state["started"]:
            unreal.unregister_slate_post_tick_callback(state["handle"])
            unreal.log("SLIME DAY DONE\n" + "\n".join(state["log"]))
        return
    state["started"] = True
    state["t"] += dt
    a = slime_actor()
    if not a:
        return
    comp = a.slime
    if state["t"] < 0.5:
        comp.set_editor_property("time_scale", TIME_SCALE)
    if state["t"] >= state["next_feed"]:
        ok = comp.feed(FOOD); state["next_feed"] += FEED_EVERY
        state["log"].append(f"t={state['t']:.1f}s age={comp.get_age_hours():.1f}h fed {FOOD} ok={ok} type={comp.get_type_id()} "
                            f"den={comp.get_hidden_value(unreal.SlimeProperty.DENSITY):.2f} cla={comp.get_hidden_value(unreal.SlimeProperty.CLARITY):.2f}")
    ty = str(comp.get_type_id())
    if ty != state["last_type"]:
        state["log"].append(f"t={state['t']:.1f}s age={comp.get_age_hours():.1f}h TYPE {state['last_type']} -> {ty}")
        state["last_type"] = ty
    if SHOTS and state["t"] >= SHOTS[0]:
        SHOTS.pop(0); state["n"] += 1
        unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, f"slimeday_{state['n']}.png")
    if state["t"] >= END:
        state["log"].append("journal: " + " || ".join(str(s) for s in comp.get_journal()[-8:]))
        LES.editor_request_end_play()

state["handle"] = unreal.register_slate_post_tick_callback(tick)
LES.editor_request_begin_play()
print("SLIME DAY STARTED")
