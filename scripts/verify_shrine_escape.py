"""Verify taking the Xarxes, escaping Lake Arrius and bringing Jeelius outside alive."""

import argparse
import math
from pathlib import Path

from verify_lake_arrius_passage import SHRINE, count, form, load_save, require, verify_events


def verify_escape(path):
    state = load_save(path)
    player = state["player"]
    shrine = next(q for q in state["quests"] if q["quest"] == SHRINE)
    require(shrine["running"] and shrine["stage"] == 70
            and {51, 60, 70}.issubset(shrine["completed_stages"]), "Missing rescue, theft or escape stage")
    lake = form(0x5D54)
    require(player["cell"] == lake and player["actor_values"]["health.current"] > 0
            and count(player, 0x5599) == 1, "Player did not escape with the Xarxes")
    refs = {r["key"]: r for r in state["references"]}
    book = refs[form(0x1E746)]["custom_state"]
    require(book["read"] and book["taken"] and book["count"] == 0, "Book still on the lectern")
    require(state["quest_book_flags"].get(form(0x1F14C)) is False, "Shrine key still flagged as a quest item")
    jeelius = form(0x24E3B)
    priest = refs[jeelius]
    life = next(actor for actor in state["native_actor_life"] if actor["actor"] == jeelius)
    require(priest["enabled"] and not priest["deleted"] and priest["cell"] == lake and life["phase"] == 0
            and priest["custom_state"].get("obscript.unconscious") is False, "Jeelius did not escape alive")
    movement = next(actor for actor in state["actor_ai"] if actor["actor"] == jeelius)
    require(movement["last_transition_door"] == form(0x1E900) and movement["cell"] == lake
            and movement["destination_cell"] == form(0x153B8) and not movement["interruption_reason"],
            "Jeelius lost his onward route after crossing the cave door")
    wall = refs[form(0x9087A)]["custom_state"]
    require(wall.get("obscript.animation_group") == "forward" and wall.get("obscript.animation_progress") == 1,
            "The secret passage did not stay open")
    return movement["last_valid_position"][:3]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--restart-save", type=Path)
    parser.add_argument("--take-log", type=Path)
    parser.add_argument("--escape-log", type=Path)
    args = parser.parse_args()
    position = verify_escape(args.save)
    if args.restart_save:
        require(math.dist(position, verify_escape(args.restart_save)) >= 80,
                "Jeelius did not continue down the trail after reloading")
    if args.take_log:
        verify_events(args.take_log, [
            "M13 native book page: base=FormId:0x1005599 first=1 pages=2",
            f"setstage quest={SHRINE} stage=60", f"playgroup ref={form(0x94EB1)} group=forward played=true",
            f"playgroup ref={form(0x94C34)} group=unequip played=true", f"setquestobject base={form(0x1F14C)} flag=0",
            f"kind=take result=taken ref={form(0x1E746)}", f"playgroup ref={form(0x94EB3)} group=forward played=true",
            f"result=teleport ref={form(0x22C36)}"])
    if args.escape_log:
        verify_events(args.escape_log, [
            f"result=teleport ref={form(0x22DCE)}", f"playgroup ref={form(0x9087A)} group=forward played=true",
            f"result=teleport ref={form(0x1E900)}", f"setstage quest={SHRINE} stage=70"])
    print("Shrine escape verified")
