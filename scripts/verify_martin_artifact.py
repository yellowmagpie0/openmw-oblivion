"""Check Martin's artifact handoff, his Bruma lead and the playable reload."""

import argparse
from pathlib import Path

from verify_azuras_star import AZURA, DAEDRA, STAR, count, fame, form, load_save, require, verify_events


HALL, MARTIN, BRUMA_GATE, WISP_PARENT = map(form, (0x313AE, 0x1E745, 0x1E72C, 0xC5402))


def verify_state(path, source, delivered):
    state = load_save(path)
    quests = {q["quest"]: q for q in state["quests"]}
    player = state["player"]
    require(player["cell"] == HALL and player["actor_values"]["health.current"] > 0,
            f"{path}: expected the Knight in the Great Hall")
    require(count(player, STAR) == (0 if delivered else 1), f"{path}: incorrect number of Stars")
    require(quests[AZURA]["stage"] == 100 and fame(state) == fame(source)
            and state["native_crime"] == source["native_crime"], f"{path}: unexpected quest, Fame or crime change")
    if delivered:
        quest = quests[DAEDRA]
        require(quest["stage"] == 100 and not quest["running"] and quest["completed_stages"][-2:] == [30, 100],
                f"{path}: Blood of the Daedra did not finish")
        require(any(j["stage"] == 30 and j["entries"] and not j["unresolved"] for j in quest["journal"]),
                f"{path}: missing artifact handoff entry")
        gate = quests[BRUMA_GATE]
        require(gate["running"] and gate["stage"] == 10 and gate["journal"][-1] ==
                {"stage": 10, "entries": [0], "unresolved": False}, f"{path}: missing direction to Jauffre")
        require(any(r["key"] == WISP_PARENT and not r["enabled"] for r in state["references"]),
                f"{path}: quest wisps still enabled")
        require({form(0x26D6B), form(0x26EF4)} <= set(state["spoken_dialogue_infos"]),
                f"{path}: missing Martin's acceptance or follow-up")
    else:
        require(quests[DAEDRA]["stage"] == 20 and quests[DAEDRA]["running"], f"{path}: handoff already occurred")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "before_martin", "delivered", "reloaded", "travel_log", "dialogue_log"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    source = load_save(args.source)
    require(source["player"]["cell"] == form(0x457F) and count(source["player"], STAR) == 1,
            "Expected the Azura's Star shrine checkpoint")
    verify_state(args.before_martin, source, False)
    for path in (args.delivered, args.reloaded):
        verify_state(path, source, True)
    verify_events(args.travel_log, ["Fast travelled to Cloud Ruler Temple", f"result=teleport ref={form(0x31494)}"])
    verify_events(args.dialogue_log, [
        f"actor={MARTIN} info={form(0x26D6D)} choices=2", "_00026d6d_2.mp3",
        f"removeitem owner={source['player']['reference']} item={form(STAR)} count=1",
        f"setstage quest={DAEDRA} stage=30", f"setstage quest={DAEDRA} stage=100", f"disable ref={WISP_PARENT}",
        "_00026d6b_1.mp3", f"setstage quest={BRUMA_GATE} stage=10", "_00026ef4_1.mp3",
    ])
    print("Verified temple arrival, Martin's two choices, the single Star removal, Bruma Gate and cold reload.")
