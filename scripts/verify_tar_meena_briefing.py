"""Check Tar-Meena's handoff save and optionally the route and conversation logs."""

import argparse
from pathlib import Path

from tes4_runtime_state import load_save


def form(number):
    return f"content:oblivion.esm:{number:06x}"


QUEST, LOBBY, TAR_MEENA = map(form, (0x1E727, 0x49CF9, 0x34EBC))
CHEST, SHOP = map(form, (0x22B09, 0x2C16E))
BOOKS = tuple(map(form, (0x22B04, 0x22B05, 0x22B06, 0x22B07)))


def book_counts(owner):
    return tuple(sum(i["count"] for i in owner["inventory"] if i["base"] == book) for book in BOOKS)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_save(path):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == QUEST)
    require(quest["running"] and quest["stage"] == 40, "Expected the First Edition lead")
    entries = [(j["stage"], j["entries"], j["unresolved"]) for j in quest["journal"]]
    require(entries[-2:] == [(36, [0], False), (40, [0, 1], False)], "Missing Tar-Meena journal entries")
    refs = {r["key"]: r for r in state["references"]}
    require(state["player"]["cell"] == LOBBY, "Player did not finish in the Arch-Mage's Lobby")
    for key, cell in ((TAR_MEENA, LOBBY), (CHEST, SHOP)):
        ref = refs[key]
        require(ref["cell"] == cell and ref["enabled"] and not ref["deleted"], f"Unavailable reference {key}")
    require(book_counts(state["player"]) == (1, 1, 0, 0), "Incorrect player Commentaries")
    require(book_counts(refs[CHEST])[2] == 1, "First Edition must hold one copy of Volume 3")


def verify_events(path, events):
    log = path.read_text(encoding="utf-8", errors="replace")
    offset = 0
    for event in events:
        pos = log.find(event, offset)
        require(pos >= 0, f"Missing or out-of-order event: {event}")
        offset = pos + len(event)
    require("Error in frame:" not in log and "Failed to load saved game:" not in log, "Game reported an error")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--route-log", type=Path)
    parser.add_argument("--dialogue-log", type=Path)
    args = parser.parse_args()
    verify_save(args.save)
    if args.route_log:
        door = "M5 door activation: result=teleport ref="
        verify_events(args.route_log, [
            door + form(0x47F2F),
            door + form(0x2C210),
            "Fast travelled to Imperial City, The Arcane University",
            door + form(0x3884),
            "Loading cell ICArcaneUniversityArchMagesTowerLobby",
        ])
    if args.dialogue_log:
        menu = f"Native dialogue menu: actor={TAR_MEENA} info="
        responses = [menu + form(info) for info in (0xC099D, 0x24E7F, 0x22B41, 0x22B42)]
        responses.extend((f"setstage quest={QUEST} stage=40", menu + form(0x22B43), "_00022b43_4.lip"))
        verify_events(args.dialogue_log, responses)
    print("Tar-Meena's briefing verified")
