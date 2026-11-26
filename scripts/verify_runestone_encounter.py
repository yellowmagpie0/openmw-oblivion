"""Verify the dusk encounter, the recovered keys and an independent resave."""

import argparse
import math
from pathlib import Path

from verify_lake_arrius_passage import count, form, load_save, require
from verify_spies_briefing import JEARL, SPIES


SAVERI = form(0x36287)
STONE = (18693, 157587, 27236)
BOUND_SPELLS = {form(0x66CD6), form(0x6A8DC)}


def check_armed(state):
    player = state["player"]
    require(16 <= state["clock"]["hour"] < 21 and math.dist(player["position"][:3], STONE) < 1000,
            "The player did not use the runestone at dusk")
    owners = next(row["abilities"] for row in state["native_bound_abilities"] if row["actor"] == player["reference"])
    boon = next(entry for entry in owners if entry["spell"] in BOUND_SPELLS)
    require(0 < boon["remaining_seconds"] <= 300, "Invalid runestone duration")
    equipped = [i for i in player["inventory"] if i["equipped_slots"]
                and (i.get("bound_source") or {}).get("spell") == boon["spell"]]
    require(len(equipped) == 2, "Missing conjured weapon or gauntlets")
    return boon


def check_encounter(state):
    player = state["player"]
    quest = next(q for q in state["quests"] if q["quest"] == SPIES)
    require(player["actor_values"]["health.current"] > 0, "The player did not survive")
    reputation = next(a["reputation"] for a in state["native_actor_values"] if a["actor"] == player["reference"])
    require(reputation["bounty_accumulator"] == 0, "The encounter charged a bounty")
    require(quest["running"] and {25, 30, 31, 40, 45} <= set(quest["completed_stages"])
            and quest["stage"] == 45, "Missing a spy death or the basement key lead")
    for actor in (JEARL, SAVERI):
        life = next(a for a in state["native_actor_life"] if a["actor"] == actor)
        ref = next(r for r in state["references"] if r["key"] == actor)
        require(life["phase"] == 1 and math.dist(ref["position"][:3], STONE) < 2500,
                f"{actor} did not die near the runestone")
    require(count(player, 0x2007F) == 1 and count(player, 0x35E50) >= 1, "Missing Jearl's house or basement key")
    script = next(s for s in state["script_instances"] if s["context"] == SPIES
                  and s["unit"].startswith(form(0x1E741) + "@"))
    require(script["locals"][0]["value"] == 2, "Both spy deaths must be counted exactly once")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("armed", "encounter", "restart"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--expired", type=Path)
    args = parser.parse_args()
    boon = check_armed(load_save(args.armed))
    checkpoint = load_save(args.encounter)
    restart = load_save(args.restart)
    check_encounter(checkpoint)
    check_encounter(restart)
    checkpoint_boon = check_armed(checkpoint)
    restart_boon = check_armed(restart)
    require(0 < restart_boon["remaining_seconds"] <= checkpoint_boon["remaining_seconds"] <= boon["remaining_seconds"]
            and restart_boon["displaced_inventory"] == boon["displaced_inventory"],
            "Reload lost the remaining duration or displaced equipment")
    if args.expired:
        state = load_save(args.expired)
        check_encounter(state)
        player = state["player"]
        require(all(owner["actor"] != player["reference"] for owner in state["native_bound_abilities"])
                and all(not item.get("bound_source") for item in player["inventory"]), "Expired boon still active")
        worn = {(i["base"], i["equipped_slots"]) for i in player["inventory"] if i["equipped_slots"]}
        for item in boon["displaced_inventory"]:
            require((item["base"], item["equipped_slots"]) in worn, "Displaced equipment was not restored")
    print("Runestone equipment, both spy deaths and recovered keys verified")
