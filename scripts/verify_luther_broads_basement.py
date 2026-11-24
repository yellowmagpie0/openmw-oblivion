"""Check the playable basement handoff and, optionally, its combat save and play log."""

import argparse
from pathlib import Path

from tes4_runtime_state import load_save


def form(local):
    return f"content:oblivion.esm:{local}"


ASTAV, BAURUS, BASEMENT = map(form, ("022b0c", "032a17", "047ca3"))
BOOK, POWER, QUEST = map(form, ("022b04", "022b37", "01e727"))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(path, fighting=False):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == QUEST)
    refs = {row["key"]: row for row in state["references"]}
    life = {row["actor"]: row for row in state["native_actor_life"]}
    require(state["player"]["cell"] == BASEMENT, "Player has not entered the basement")
    for actor in (ASTAV, BAURUS):
        require(refs[actor]["cell"] == BASEMENT and refs[actor]["enabled"] and not refs[actor]["deleted"],
                f"Missing actor in basement: {actor}")
    require(life[BAURUS]["phase"] == 0, "Baurus did not survive")
    gear = [i for i in refs[ASTAV]["inventory"] if i.get("bound_source")]
    timer = refs[ASTAV]["custom_state"].get("native.bound_power.remaining_seconds")
    if fighting:
        require(quest["stage"] == 31 and life[ASTAV]["phase"] == 0, "Missing live ambush")
        require(timer is not None and 0 < timer <= 1000, "Missing combat power lifetime")
        require(len(gear) == 3 and all(i["bound_source"]["spell"] == POWER and i["equipped_slots"] for i in gear),
                "Astav has not equipped his bound armor, helmet and mace")
        return
    require(quest["running"] and quest["stage"] == 36, "Missing lead to Tar-Meena")
    entries = [j["stage"] for j in quest["journal"] if j["entries"] == [0] and not j["unresolved"]]
    require(entries[-4:] == [32, 34, 35, 36], "Missing basement journal entries")
    require(life[ASTAV]["phase"] == 1 and life[ASTAV]["killer"] == BAURUS, "Missing Baurus's kill")
    require(not gear and timer is None, "Bound equipment survived Astav's death")
    require(not any(i["base"] == BOOK for i in refs[ASTAV]["inventory"]), "Book still on corpse")
    books = [i for i in state["player"]["inventory"] if i["base"] == BOOK]
    require(len(books) == 1 and books[0]["count"] == 1, "Missing unique looted Commentaries")


def verify_log(path):
    log = path.read_text(encoding="utf-8", errors="replace")
    events = [
        "Loading cell ICElvenGardensLutherBroadsBoardingHouse",
        "M5 door activation: result=teleport ref=" + form("047cc5"),
        "Loading cell ICElvenGardensLutherBroadsBasement",
        f"setstage quest={QUEST} stage=31",
        f"setstage quest={QUEST} stage=32",
        f"actor={ASTAV} killer={BAURUS} handled=true",
        f"setstage quest={QUEST} stage=34",
        "info=" + form("022b1a") + " choices=",
        "M7 inventory drop event: item=" + BOOK,
        f"setstage quest={QUEST} stage=35",
        "info=" + form("022b1d") + " choices=",
        f"setstage quest={QUEST} stage=36",
        "info=" + form("022b23") + " choices=",
    ]
    cursor = 0
    for event in events:
        cursor = log.index(event, cursor) + len(event)
    require("_00022b23_3.lip" in log, "Final voiced lead did not finish")
    require("Error in frame:" not in log and "Failed to load saved game:" not in log, "Runtime error")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--fight-save", type=Path)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    verify(args.save)
    if args.fight_save:
        verify(args.fight_save, fighting=True)
    if args.log:
        verify_log(args.log)
    print("Baurus's basement encounter verified")
