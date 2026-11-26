"""Check the playable Green Emperor Way checkpoint, its route and a cold resave."""

import argparse
from pathlib import Path

from tes4_runtime_state import load_save
from verify_tar_meena_briefing import BOOKS, QUEST, book_counts, form, require, verify_events


SHRINE, TOMB, CAVE = map(form, (0x1E728, 0x24E4A, 0x1E903))
BLADES_ARMOUR = (0x2391C, 0x23921, 0x23922, 0x23318, 0x23329)


def count(owner, base):
    return sum(stack["count"] for stack in owner["inventory"] if stack["base"] == form(base))


def verify_save(path, carried=(1, 1, 1, 1)):
    state = load_save(path)
    quests = {q["quest"]: q for q in state["quests"]}
    refs = {r["key"]: r for r in state["references"]}
    dawn, shrine = quests[QUEST], quests[SHRINE]
    require(not dawn["running"] and dawn["stage"] == 100
            and dawn["completed_stages"][-3:] == [93, 95, 100], "Incomplete inscription quest")
    require(dawn["journal"][-1] == {"stage": 100, "entries": [1], "unresolved": False},
            "Missing four-volume conclusion")
    require(shrine["running"] and shrine["stage"] == 10
            and shrine["journal"][-1] == {"stage": 10, "entries": [0], "unresolved": False},
            "Missing Dagon Shrine lead")
    require(state["quest_book_flags"].get(BOOKS[3]) is False, "Volume IV remains a quest item")
    player = state["player"]
    require(player["cell"] == form(0x1C29E) and book_counts(player) == carried
            and count(player, 0x9588D) == 1, "Incorrect player location or carried items")
    require(refs[CAVE]["enabled"] and not refs[CAVE]["deleted"]
            and refs[CAVE]["custom_state"].get("native.map_marker.reveal") == 1,
            "The unvisited Lake Arrius marker was lost")
    require(all(refs[form(i)]["enabled"] and not refs[form(i)]["deleted"] for i in (0x3391B, 0x3392D)),
            "Shrine participants unavailable")
    require(count(refs[form(0x1F10A)], 0x1F14C) == 1, "Missing doorkeeper key")
    require(all(count(refs[form(0x32A17)], i) == 1 for i in BLADES_ARMOUR), "Unexpected Baurus equipment")
    reputation = next(a["reputation"] for a in state["native_actor_values"] if a["actor"] == player["reference"])
    require(reputation["fame"] == 5, "Unexpected Fame after completing The Path of Dawn")
    return state


def verify_book_round_trip(dropped, recovered):
    before = verify_save(dropped, (1, 1, 1, 0))
    after = verify_save(recovered)
    loose = [r for r in before["references"] if r["base"] == BOOKS[3] and not r["deleted"]
             and r["dropper"] == before["player"]["reference"]]
    require(len(loose) == 1 and loose[0]["custom_state"]["count"] == 1, "Missing dropped book")
    key = loose[0]["key"]
    taken = next(r for r in after["references"] if r["key"] == key)
    require(taken["deleted"] and taken["custom_state"]["count"] == 0, "Duplicate world copy after pickup")
    owner = next(i["script_owner"] for i in after["player"]["inventory"] if i["base"] == BOOKS[3])
    require(owner and owner.startswith("dynamic:inventory-script:"), "Missing inventory script owner")
    for state, expected in ((before, key), (after, owner)):
        contexts = [s["context"] for s in state["script_instances"] if s["unit"].startswith(form(0x22EFB) + "@")]
        require(contexts == [expected], "Volume IV's script was lost or duplicated")


def verify_course(log):
    activation = f"event=onactivate unit={form(0x24DCB)}"
    events = [f"result=teleport ref={form(0x49D05)}", "Fast travelled to Imperial City Palace",
              activation, "native rest hour", f"{TOMB} group=forward played=true", activation,
              f"setstage quest={QUEST} stage=95", f"setstage quest={QUEST} stage=100",
              f"setquestobject base={BOOKS[3]} flag=0", f"setstage quest={SHRINE} stage=10",
              f"showmap ref={CAVE} flags=1", f"enable ref={form(0x3392D)}", f"enable ref={form(0x3391B)}",
              f"additem owner={form(0x1F10A)} item={form(0x1F14C)} count=1"]
    events.extend(f"additem owner={form(0x32A17)} item={form(base)} count=1" for base in BLADES_ARMOUR)
    verify_events(log, events)
    require(log.read_text(encoding="utf-8", errors="replace").count(f"setstage quest={QUEST} stage=95") == 1,
            "Repeated inscription result")


def verify_approach(afternoon, glowing):
    for path, lit in ((afternoon, False), (glowing, True)):
        state = load_save(path)
        hour = state["clock"]["hour"]
        dawn = next(q for q in state["quests"] if q["quest"] == QUEST)
        tomb = next(r for r in state["references"] if r["key"] == TOMB)
        require(dawn["running"] and dawn["stage"] == 93, "Quest advanced before reading the stone")
        require((11.8 < hour < 12.5) == lit, "Incorrect inscription window")
        require((tomb["custom_state"].get("obscript.animation_group") == "forward") == lit,
                "Incorrect inscription animation")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--course-log", type=Path)
    parser.add_argument("--approach", nargs=2, type=Path, metavar=("AFTERNOON", "GLOWING"))
    parser.add_argument("--restart-save", type=Path)
    parser.add_argument("--book-round-trip", nargs=2, type=Path, metavar=("DROPPED", "RECOVERED"))
    args = parser.parse_args()
    verify_save(args.save)
    if args.course_log:
        verify_course(args.course_log)
    if args.approach:
        verify_approach(*args.approach)
    if args.restart_save:
        verify_save(args.restart_save)
    if args.book_round_trip:
        verify_book_round_trip(*args.book_round_trip)
    print("Noon inscription and Lake Arrius lead verified")
