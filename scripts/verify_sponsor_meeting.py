"""Verify the gallery branch of the Sponsor meeting in the Elven Garden Sewers."""

import argparse
from pathlib import Path

from tes4_runtime_state import load_save
from verify_baurus_lower_hall import BAURUS, verify_events
from verify_tar_meena_briefing import BOOKS, QUEST, book_counts, form, require


ROOM, RAVEN, GUARD_ONE, GUARD_TWO = map(form, (0xABB2, 0x22CCC, 0x22EF7, 0x22EF8))
CULTISTS = (RAVEN, GUARD_ONE, GUARD_TWO)
ROBE, RAVEN_POWER = form(0x24DE2), form(0x97C1F)


def verify(path, moment):
    state = load_save(path)
    player = state["player"]
    quest = next(q for q in state["quests"] if q["quest"] == QUEST)
    refs = {r["key"]: r for r in state["references"]}
    life = {r["actor"]: r for r in state["native_actor_life"]}
    require(quest["running"] and player["cell"] == ROOM and player["actor_values"]["health.current"] > 0,
            "Expected a living player in the meeting room")
    require(life[BAURUS]["phase"] == 0 and refs[BAURUS]["enabled"] and not refs[BAURUS]["deleted"],
            "Baurus must survive the meeting")
    require(any(j["stage"] == 78 and j["entries"] and not j["unresolved"] for j in quest["journal"]),
            "Expected the journal choice to cover Baurus")
    if moment == "gallery":
        require(quest["stage"] in (78, 79, 80), "Expected the meeting before the alarm")
        for key in CULTISTS:
            require(any(i["base"] == ROBE and i["equipped_slots"] == 0x3C for i in refs[key]["inventory"]),
                    f"Missing Mythic Dawn robes on {key}")
        return
    if moment == "ambush":
        require(quest["stage"] == 85 and life[RAVEN]["phase"] == 0, "Expected Raven fighting")
        bound = [i for i in refs[RAVEN]["inventory"] if i.get("bound_source")]
        require(0 < refs[RAVEN]["custom_state"].get("native.bound_power.remaining_seconds", 0) <= 1000,
                "Missing lifetime for Raven's bound gear")
        require({(i["bound_source"]["spell"], i["equipped_slots"]) for i in bound}
                == {(RAVEN_POWER, slots) for slots in (0x3C, 3, 0x10000)}, "Missing bound armor, helm or sword")
        return
    require(quest["stage"] == 90 and any(j["stage"] == 90 and j["entries"] and not j["unresolved"]
            for j in quest["journal"]), "Missing the journal entry for the fourth volume")
    require(book_counts(player) == (1, 1, 1, 1), "Expected one copy of each Commentary")
    require(any(i["base"] == form(0x9588D) and i["count"] > 0 for i in player["inventory"]),
            "Missing the Sunken Sewers key from the handoff")
    for key in CULTISTS:
        require(life[key]["phase"] == 1 and refs[key]["cell"] == ROOM, f"Missing defeated cultist {key}")
        require(not any(i.get("bound_source") for i in refs[key]["inventory"]),
                f"Temporary equipment survived the death of {key}")
    require(not any(i["base"] == BOOKS[3] for i in refs[RAVEN]["inventory"]), "Raven still has the fourth book")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path, nargs="+")
    parser.add_argument("--gallery-save", type=Path)
    parser.add_argument("--ambush-save", type=Path)
    parser.add_argument("--route-log", type=Path, nargs="+")
    parser.add_argument("--restart-log", type=Path)
    args = parser.parse_args()
    for path in args.save:
        verify(path, "handoff")
    if args.gallery_save:
        verify(args.gallery_save, "gallery")
    if args.ambush_save:
        verify(args.ambush_save, "ambush")
    if args.route_log:
        verify_events(args.route_log, [
            "setstage quest=" + QUEST + " stage=78",
            "M5 door activation: result=teleport ref=" + form(0x22B88),
            "Loading cell ImperialSewersElvenGardens02",
            "Native NPC furniture: actor=" + BAURUS + " phase=3",
            "setstage quest=" + QUEST + " stage=79",
            "setstage quest=" + QUEST + " stage=80",
            "sayto topic=" + form(0x22CC6) + " info=" + form(0x22CF6),
            "setstage quest=" + QUEST + " stage=85",
            "Native NPC furniture: actor=" + BAURUS + " phase=0",
            "sayto topic=" + form(0xD2) + " info=" + form(0x65555),
            "M7 inventory add event: item=" + BOOKS[3],
            "setstage quest=" + QUEST + " stage=90",
        ])
    if args.restart_log:
        verify_events([args.restart_log], [
            "Loading cell ImperialSewersElvenGardens02",
            "M5 door activation: result=teleport ref=" + form(0x22B85),
            "Loading cell ImperialSewersElvenGardens01",
        ])
    print("Sponsor meeting verified")
