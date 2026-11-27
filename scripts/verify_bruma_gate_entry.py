"""Verify Burd's speech, the guard detail and the party on the Oblivion side."""

import argparse
import math
from itertools import combinations
from pathlib import Path

from verify_bruma_gate_meeting import BRUMA_GATE, BURD, GATE, GUARDS, form, load_save, require, verify_events


LINES = (0x46303, 0x46304, 0x463AA, 0x463AB, 0x463BB, 0x463C1)
REPLIES = (0x46302, 0x46305, 0x46306)
ENTRANCE = form(0x54C28)


def check(path, stage):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == BRUMA_GATE)
    require(quest["running"] and quest["stage"] == stage, f"{path}: unexpected Bruma Gate stage")
    script = next(s for s in state["script_instances"] if s["context"] == BRUMA_GATE
                  and s["unit"].startswith(form(0x3A840) + "@"))
    inside, speech, dead = (local["value"] for local in script["locals"])
    refs = {r["key"]: r for r in state["references"]}
    pending = refs[BURD]["custom_state"].get("obscript.conversation")
    if stage == 32:
        require(pending and inside == 0 and speech == 2, f"{path}: missing mid-speech continuation")
    else:
        require(not pending and speech == (5 if stage == 35 else 0), f"{path}: unexpected speech history")
    require(dead == 0 and state["player"]["actor_values"]["health.current"] > 0, f"{path}: casualties")
    if stage != 35:
        return
    health = state["player"]["actor_values"]
    require(health["health.current"] == health["health.base"] + health["health.modifier"],
            f"{path}: player did not recover before continuing")
    require(quest["completed_stages"][-4:] == [31, 32, 33, 35] and inside == 1
            and state["player"]["cell"] == ENTRANCE, f"{path}: gate transition incomplete")
    require({form(i) for i in (*LINES, *REPLIES)} <= set(state["spoken_dialogue_infos"]),
            f"{path}: missing lines from Burd or his men")
    for ref in (BURD, *GUARDS):
        actor = refs[ref]
        require(actor["enabled"] and not actor["deleted"], f"{path}: missing {ref}")
        if ref in (BURD, *GUARDS[:2]):
            require(actor["cell"] == ENTRANCE and math.dist(actor["position"][:3], state["player"]["position"][:3]) < 1600,
                    f"{path}: {ref} did not join the player")
        else:
            require(actor["cell"] == form(0x452D), f"{path}: {ref} left the gate defenses")
    for a, b in combinations((BURD, *GUARDS[:2]), 2):
        require(math.dist(refs[a]["position"][:3], refs[b]["position"][:3]) > 35,
                f"{path}: {a} and {b} overlap at the entrance")
    require(all(a["phase"] == 0 for a in state["native_actor_life"] if a["actor"] in (BURD, *GUARDS)),
            f"{path}: a member of the guard detail is down")
    spawns = {r["key"] for r in refs.values() if r["custom_state"].get("native.spawn.requested_base") == form(0x3ECF1)
              and r["custom_state"].get("native.spawn.source") == form(0x613C3)}
    require(spawns and any(s["context"] in spawns and s["unit"].startswith(form(0xA8EE) + "@")
                           and s["on_load_fired"] for s in state["script_instances"]),
            f"{path}: missing gate spawn or its leveled-list script")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "mid_speech", "party", "reloaded", "course_log", "speech_reload_log"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    for path, stage in ((args.source, 30), (args.mid_speech, 32), (args.party, 35), (args.reloaded, 35)):
        check(path, stage)
    for log, lines in ((args.course_log, LINES), (args.speech_reload_log, LINES[2:])):
        verify_events(log, [f"_{i:08x}_1.mp3 lip=" for i in lines] + [f"setstage quest={BRUMA_GATE} stage=33"]
                      + [f"_{i:08x}_1.mp3" for i in REPLIES] + [f"requested={form(0x3ECF1)}",
                         f"result=teleport ref={GATE}", f"setstage quest={BRUMA_GATE} stage=35"])
    print("Verified the speech and its restart, the three replies, gate spawns and the reunited party on reload.")
