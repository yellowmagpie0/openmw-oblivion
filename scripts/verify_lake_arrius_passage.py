"""Check the Lake Arrius journey, password encounter and playable resave."""

import argparse
from pathlib import Path

from verify_noon_inscription import CAVE, QUEST, SHRINE, book_counts, count, form, load_save, require, verify_events


GUARD, HARROW, GATE, CAVERN = map(form, (0x1F10A, 0x3391B, 0x22865, 0x1E8CD))


def verify_save(path, admitted):
    state = load_save(path)
    player = state["player"]
    quests = {q["quest"]: q for q in state["quests"]}
    refs = {r["key"]: r for r in state["references"]}
    require(quests[QUEST]["stage"] == 100 and not quests[QUEST]["running"], "The Path of Dawn restarted")
    expected = [10, 20, 30] if admitted else [10, 20]
    require(quests[SHRINE]["running"] and quests[SHRINE]["completed_stages"] == expected
            and quests[SHRINE]["stage"] == expected[-1], "Unexpected Dagon Shrine stage")
    require(player["cell"] == CAVERN and book_counts(player) == (1, 1, 1, 1)
            and count(player, 0x9588D) == 1, "Unexpected player location or belongings")
    require(refs[CAVE]["custom_state"].get("native.map_marker.reveal") == 3
            and player["actor_values"].get("pc_misc_stat.7") == 1, "Lake discovery was lost or counted twice")
    outfit = {i["base"]: i["equipped_slots"] for i in refs[GUARD]["inventory"]}
    require(outfit.get(form(0x24DE2)) == 0x3C and outfit.get(form(0x8D755)) == 2
            and count(refs[GUARD], 0x1F14C) == 1, "Incorrect doorkeeper outfit or missing key")
    if admitted:
        gate = refs[GATE]
        require(gate["lock_level"] <= 0 and gate["custom_state"].get("locked") is False
                and gate["custom_state"].get("obscript.door_default_open") is True, "Doorkeeper's gate is locked")
        require(gate["custom_state"].get("obscript.animation_group") == "open"
                and gate["custom_state"].get("obscript.animation_progress") == 1,
                "The gate did not retain the open pose")
        require(player["position"][0] > gate["position"][0] + 40, "Player did not pass through the gate")
        require(refs[HARROW]["enabled"] and not refs[HARROW]["deleted"] and refs[HARROW]["cell"] == CAVERN,
                "Harrow did not come to meet the player")
        base = next(b for b in state["native_actor_bases"] if b["base"] == form(7))
        require([form(0x29F82), 0] in base["factions"], "Missing Mythic Dawn membership")


def verify_gate(path):
    verify_events(path, ["_00036408_1.mp3", f"setstage quest={SHRINE} stage=30",
                         f"faction={form(0x29F82)} rank=0", f"addscriptpackage actor={GUARD} package={form(0x97CC4)}",
                         "_0001f10b_1.mp3", "_0001f10b_2.mp3", "_0001f10b_3.mp3",
                         f"event=onactivate unit={form(0xCB551)}", f"result=animated ref={GATE}",
                         f"activate-default ref={GATE}"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--approach-save", type=Path)
    parser.add_argument("--restart-save", type=Path)
    parser.add_argument("--journey-log", type=Path)
    parser.add_argument("--gate-log", type=Path)
    parser.add_argument("--harrow-log", type=Path)
    args = parser.parse_args()
    verify_save(args.save, True)
    if args.approach_save:
        verify_save(args.approach_save, False)
    if args.restart_save:
        verify_save(args.restart_save, True)
    if args.journey_log:
        verify_events(args.journey_log, ["Fast travelled to Cheydinhal", f"result=teleport ref={form(0x7A6A)}",
                                        f"discovered ref={CAVE}", f"result=teleport ref={form(0x1E902)}",
                                        f"setstage quest={SHRINE} stage=20"])
    if args.gate_log:
        verify_gate(args.gate_log)
    if args.harrow_log:
        verify_events(args.harrow_log, [f"actor={HARROW} info={form(0x1F146)}", "_0001f146_5.mp3"])
    print("Lake Arrius passage verified")
