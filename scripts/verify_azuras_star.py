"""Verify the walk back from Gutted Mine, Azura's reward and its cold resave."""

import argparse
from pathlib import Path

from verify_azura_offering import AZURA, DAEDRA
from verify_gutted_mine import LAMPS
from verify_noon_inscription import count, form, load_save, require, verify_events


STAR = 0x193


def fame(state):
    return next(a["reputation"]["fame"] for a in state["native_actor_values"]
                if a["actor"] == state["player"]["reference"])


def check(path, source):
    state = load_save(path)
    player = state["player"]
    require(player["cell"] == form(0x457F) and count(player, STAR) == 1, f"{path}: expected one Star at the shrine")
    require(player["actor_values"]["health.current"] > 0, f"{path}: player did not survive")
    require(state["native_crime"] == source["native_crime"], f"{path}: crime changed")
    require(fame(state) == fame(source) + 1, f"{path}: incorrect Fame")
    require(player["actor_values"]["pc_misc_stat.19"] == source["player"]["actor_values"].get("pc_misc_stat.19", 0) + 1,
            f"{path}: incorrect number of artifacts found")
    quests = {q["quest"]: q for q in state["quests"]}
    for quest, stage in ((AZURA, 100), (DAEDRA, 20)):
        require(quests[quest]["stage"] == stage and quests[quest]["journal"][-1] ==
                {"stage": stage, "entries": [0], "unresolved": False}, f"{path}: incorrect journal for {quest}")
    refs = {r["key"]: r for r in state["references"]}
    require(all(refs[lamp]["enabled"] and not refs[lamp]["deleted"] for lamp in LAMPS), f"{path}: missing memorial")
    require(any(a["base"] == form(0x1EBF7) and a["essential"] is False for a in state["native_actor_bases"]),
            f"{path}: Mels Maryon's essential flag did not clear")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "reward", "reloaded", "road_log", "reward_log"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    source = load_save(args.source)
    stages = {q["quest"]: q["stage"] for q in source["quests"]}
    require(source["player"]["cell"] == form(0x15C40) and count(source["player"], STAR) == 0
            and stages[AZURA] == 30 and stages[DAEDRA] == 15, "Expected the cleared-mine checkpoint")
    verify_events(args.road_log, [f"result=teleport ref={form(0x15C4B)}", "Arrius arrived point 99803 131009",
                                 "Arrius arrived point 111650 138060"])
    verify_events(args.reward_log, ["disableplayercontrols movement=true", "_0002c6c5_1.mp3", "_0002c6c6_1.mp3",
                                   "enableplayercontrols movement=true", f"setstage quest={AZURA} stage=100",
                                   f"setstage quest={DAEDRA} stage=20",
                                   f"additem owner={source['player']['reference']} item={form(STAR)} count=1"])
    for path in (args.reward, args.reloaded):
        check(path, source)
    print("Verified the mine exit, shrine audience, artifact, both journal updates, Fame and cold reload.")
