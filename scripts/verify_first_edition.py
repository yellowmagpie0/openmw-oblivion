"""Verify the peaceful First Edition handoff, optionally including the played route."""

import argparse
from pathlib import Path

from tes4_runtime_state import load_save
from verify_tar_meena_briefing import CHEST, QUEST, SHOP, book_counts, form, require, verify_events


GWINAS, PHINTIAS, BAURUS, NOTE = map(form, (0x22B02, 0x1D158, 0x32A17, 0x22B83))


def note_count(owner):
    return sum(item["count"] for item in owner["inventory"] if item["base"] == NOTE)


def verify_save(path):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == QUEST)
    require(quest["running"] and quest["stage"] == 72, "Expected the lead back to Baurus")
    require({45, 50, 60, 70, 71, 72} <= set(quest["completed_stages"]), "Missing bookstore progress")
    entries = [(j["stage"], j["entries"]) for j in quest["journal"] if j["entries"]]
    require(entries[-4:] == [(50, [0]), (60, [0]), (45, [4]), (70, [0])],
            "Unexpected journal branches for a living Gwinas handing over his book and note")
    require(all(not j["unresolved"] for j in quest["journal"] if j["stage"] >= 45),
            "A bookstore journal condition could not be evaluated")
    player = state["player"]
    require(player["cell"] == SHOP, "Player is outside First Edition")
    require(book_counts(player) == (1, 1, 1, 0) and note_count(player) == 1,
            "Player must own the first three Commentaries and one meeting note")
    refs = {r["key"]: r for r in state["references"]}
    require(book_counts(refs[CHEST])[2] == book_counts(refs[GWINAS])[2] == note_count(refs[GWINAS]) == 0,
            "The shop or Gwinas retained an item the player received")
    require(refs[BAURUS]["enabled"] and not refs[BAURUS]["deleted"]
            and refs[BAURUS]["cell"] == refs[form(0x97701)]["cell"],
            "Baurus is unavailable at the Elven Gardens rendezvous")


def verify_route(path):
    door = "M5 door activation: result=teleport ref="
    menu = "Native dialogue menu: actor="
    stage = f"setstage quest={QUEST} stage="
    verify_events(path, [
        door + form(0x49D05), "Fast travelled to Imperial City Market District", door + form(0x2C3DD),
        "Loading cell ICMarketDistrictFirstEdition", menu + PHINTIAS, stage + "50", stage + "60",
        "package-conversation info=" + form(0x22B39) + " ref=" + GWINAS,
        "package-conversation info=" + form(0x22B51) + " ref=" + PHINTIAS,
        "package-conversation info=" + form(0x22B53) + " ref=" + GWINAS,
        "onpackagedone unit=" + form(0x260C3),
        f"additem owner={GWINAS} item=" + form(0x22B06), menu + GWINAS,
        stage + "45", stage + "70", stage + "72", f"moveto ref={BAURUS} marker=" + form(0x97701),
    ])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--route-log", type=Path)
    args = parser.parse_args()
    verify_save(args.save)
    if args.route_log:
        verify_route(args.route_log)
    print("First Edition handoff verified")
