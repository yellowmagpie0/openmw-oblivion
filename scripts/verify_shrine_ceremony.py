"""Check the altar, release and Paradise portal against saves and gameplay events."""

import argparse
from math import dist
from pathlib import Path

from verify_lake_arrius_passage import SHRINE, form, load_save, require, verify_events


JEELIUS, ALTAR, MANKAR, SANCTUARY, PARADISE = map(form, (0x24E3B, 0x24E43, 0x3392D, 0x338EB, 0x1FB1E))


def verify_save(path, released):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == SHRINE)
    require(quest["running"] and quest["stage"] == (51 if released else 50)
            and {45, 48, 49, 50} <= set(quest["completed_stages"]), "Ceremony stages are incomplete")
    refs = {r["key"]: r for r in state["references"]}
    require(state["player"]["cell"] == SANCTUARY and refs[MANKAR]["cell"] == PARADISE, "Portal traversal failed")
    require(not refs[form(0x1FB1F)]["enabled"], "Paradise portal exit remained enabled")
    prisoner = refs[JEELIUS]
    ai = next(a for a in state["actor_ai"] if a["actor"] == JEELIUS)
    require(prisoner["enabled"] and not prisoner["deleted"]
            and prisoner["custom_state"].get("obscript.unconscious") is (not released), "Incorrect captive state")
    if released:
        require(ai["package"] != form(0x24EB8) and ai["interruption_reason"] != "target-unresolved",
                "Jeelius failed to start his escape")
    else:
        require(ai["package"] == form(0x24EB8) and ai["target"] == ALTAR and prisoner["cell"] == SANCTUARY
                and dist(prisoner["position"][:3], refs[ALTAR]["position"][:3]) < 32, "Jeelius is off the altar")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("altar", type=Path)
    parser.add_argument("released", type=Path)
    parser.add_argument("--altar-restart", type=Path)
    parser.add_argument("--released-restart", type=Path)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    verify_save(args.altar, False)
    verify_save(args.released, True)
    if args.altar_restart:
        verify_save(args.altar_restart, False)
    if args.released_restart:
        verify_save(args.released_restart, True)
    if args.log:
        verify_events(args.log, [
            f"setstage quest={SHRINE} stage=45", f"setunconscious actor={JEELIUS} value=true",
            "group=speech_handsinair played=true", f"setstage quest={SHRINE} stage=48",
            f"result=teleport ref={form(0xC9EC9)}", f"setstage quest={SHRINE} stage=49",
            f"setstage quest={SHRINE} stage=50", "mq06_mq06rumayes_00024e9b_1.mp3",
            "messagebox text=Do you want to release the prisoner?", f"setunconscious actor={JEELIUS} value=false",
            f"setstage quest={SHRINE} stage=51", f"Native NPC furniture: actor={JEELIUS} phase=0",
        ])
    print("Shrine ceremony and release verified")
