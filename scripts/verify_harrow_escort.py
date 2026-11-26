"""Check the shrine escort from Harrow's invitation through a cold resave."""

import argparse
from collections import Counter
from pathlib import Path

from verify_lake_arrius_passage import HARROW, SHRINE, form, load_save, require, verify_events


ROBE = form(0x24DE2)
# Volume III and the Sigil Keep Key are quest items. The five tutorial
# Blades armor records have the TES4 non-playable biped flag.
PROTECTED = set(map(form, (0x22B06, 0x93EF5, 0x22F20, 0x22F65, 0x22F6F, 0x22F70, 0x22F71)))


def counts(items):
    result = Counter()
    for item in items:
        result[item["base"]] += item["count"]
    return result


def verify_save(before, path, at_shrine):
    state = load_save(path)
    refs = {r["key"]: r for r in state["references"]}
    quest = next(q for q in state["quests"] if q["quest"] == SHRINE)
    require(quest["running"] and quest["stage"] == 40 and 41 not in quest["completed_stages"],
            "Initiation branch or player cover lost")
    original = before["player"]["inventory"]
    old_harrow = next(r["inventory"] for r in before["references"] if r["key"] == HARROW)
    player = state["player"]["inventory"]
    harrow = refs[HARROW]["inventory"]
    kept = [i for i in original if i["base"] in PROTECTED]
    require(counts(player) == counts(kept) + Counter({ROBE: 1}), "Unexpected player inventory")
    require(any(i["base"] == ROBE and i["equipped_slots"] == 0x3C for i in player), "Initiate robe is not worn")
    require(counts(harrow) == counts(old_harrow) + (counts(original) - counts(kept)), "Lost or duplicated gear")
    for item in original:
        if not item.get("script_owner") or item["base"] in PROTECTED:
            continue
        carried = next(i for i in harrow if i.get("script_owner") == item["script_owner"])
        for field in ("base", "condition", "charge", "weapon_poison"):
            require(carried.get(field) == item.get(field), f"Changed {field} on {item['base']}")
        require(not carried["equipped_slots"], "Harrow equipped a surrendered item")
    require(not refs[form(0x1F14E)]["custom_state"]["locked"], "Shrine door stayed locked")
    if at_shrine:
        require(state["player"]["cell"] == refs[HARROW]["cell"] == form(0x338EB), "Escort did not reach Dagon Shrine")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("start", type=Path)
    parser.add_argument("shrine", type=Path)
    parser.add_argument("--initiate", type=Path)
    parser.add_argument("--restart", type=Path)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    start = load_save(args.start)
    verify_save(start, args.shrine, True)
    if args.initiate:
        verify_save(start, args.initiate, False)
    if args.restart:
        verify_save(start, args.restart, True)
    if args.log:
        verify_events(args.log, [f"setstage quest={SHRINE} stage=40", f"destination={HARROW}",
                                 f"result=teleport ref={form(0x90871)}", f"result=teleport ref={form(0x1F14E)}"])
    print("Harrow's gear transfer and shrine escort verified")
