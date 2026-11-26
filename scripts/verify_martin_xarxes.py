"""Check the Cloud Ruler Temple book handoff and a fresh-process resave."""

import argparse
from pathlib import Path

from verify_lake_arrius_passage import SHRINE, count, form, load_save, require, verify_events


SPIES, MARTIN, BOOK, DESK_BOOK = map(form, (0x1E729, 0x1E745, 0x5599, 0x8B607))


def verify_save(path, delivered):
    state = load_save(path)
    player = state["player"]
    refs = {ref["key"]: ref for ref in state["references"]}
    quests = {quest["quest"]: quest for quest in state["quests"]}
    require(player["cell"] == form(0xADD) and player["actor_values"]["health.current"] > 0,
            "Player did not reach the east wing alive")
    require(refs[MARTIN]["cell"] == player["cell"] and refs[MARTIN]["enabled"] and not refs[MARTIN]["deleted"],
            "Martin is missing from the handoff")
    require(count(player, 0x5599) == (0 if delivered else 1), "Incorrect carried Xarxes count")
    require(refs[DESK_BOOK]["cell"] == form(0x313AE) and refs[DESK_BOOK]["enabled"] == delivered
            and not refs[DESK_BOOK]["deleted"], "The Great Hall's copy has the wrong state")
    require(quests[SHRINE]["stage"] == (100 if delivered else 70) and quests[SHRINE]["running"] != delivered
            and quests[SPIES]["stage"] == (10 if delivered else 0) and quests[SPIES]["running"] == delivered,
            "Unexpected Dagon Shrine or Spies state")
    jeelius = form(0x24E3B)
    require(refs[jeelius]["enabled"] and not refs[jeelius]["deleted"]
            and next(actor["phase"] for actor in state["native_actor_life"] if actor["actor"] == jeelius) == 0,
            "Jeelius did not survive the handoff")
    return state


def check_transfer(before, after):
    owner = next(stack["script_owner"] for stack in before["player"]["inventory"] if stack["base"] == BOOK)
    require(owner.startswith("dynamic:inventory-script:")
            and any(instance["context"] == owner for instance in before["script_instances"])
            and all(instance["context"] != owner for instance in after["script_instances"]),
            "The removed book's script instance was not retired")
    def fame(state):
        return next(a["reputation"]["fame"] for a in state["native_actor_values"]
                    if a["actor"] == state["player"]["reference"])
    require(fame(after) == fame(before) + 1, "Quest completion did not award Fame exactly once")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("delivered", type=Path)
    parser.add_argument("--restart", type=Path)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    before = verify_save(args.before, False)
    check_transfer(before, verify_save(args.delivered, True))
    if args.restart:
        check_transfer(before, verify_save(args.restart, True))
    if args.log:
        verify_events(args.log, [
            "Fast travelled to Cloud Ruler Temple", f"result=teleport ref={form(0x31494)}",
            f"result=teleport ref={form(0xAFE)}", "_0008b604_2.mp3",
            f"removeitem owner=dynamic:player:0000000000000001 item={BOOK} count=1",
            "_0008b618_2.mp3", f"setstage quest={SHRINE} stage=100", f"setstage quest={SPIES} stage=10",
            f"enable ref={DESK_BOOK}", "_0008b619_4.mp3"])
    print("Martin's Xarxes handoff verified")
