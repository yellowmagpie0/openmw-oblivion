"""Check the earned Cloud Ruler briefing against the original MQ04 and MQ05 results."""

import argparse
import math
import re
from pathlib import Path

from tes4_runtime_state import load_save


def form(local_id):
    return f"content:oblivion.esm:{local_id}"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_save(path):
    state = load_save(path)
    quests = {q["quest"]: q for q in state["quests"]}
    priory = quests[form("01e726")]
    dawn = quests[form("01e727")]
    require(priory["stage"] == 100 and not priory["running"], "Weynon Priory is unfinished")
    require(dawn["stage"] == 20 and dawn["running"], "The Path of Dawn is not at the briefing")
    require(any(entry == {"stage": 20, "entries": [0], "unresolved": False} for entry in dawn["journal"]),
            "The journal lacks Jauffre's boarding-house lead")

    bases = {base["base"]: base for base in state["native_actor_bases"]}
    martin = bases[form("033907")]
    require(martin.get("full_name") == "Martin", "Martin's new name was lost")
    require(dict(martin["factions"]).get(form("0150bc")) == 1, "Martin lacks his Emperor faction rank")
    require([33, 0, 5] in martin["values"], "Martin's aggression was not restored")
    require({"from": form("029f82"), "to": form("0274ab"), "value": 0} in state["faction_reactions"],
            "The Mythic Dawn reaction toward the Blades was not reset")
    baurus = bases[form("023f2a")]
    require(baurus.get("level") == [1, True, 1, 0], "Baurus lost his player-level offset")
    require(dict(baurus["factions"]).get(form("0274ab")) == 1, "Baurus lacks his Blades rank")
    require([33, 0, 5] in baurus["values"], "Baurus's aggression did not change")

    refs = {ref["key"]: ref for ref in state["references"]}
    baurus_key, inn = form("032a17"), form("02c1be")
    for key in (baurus_key, form("022b0c")):
        require(refs[key]["cell"] == inn and refs[key]["enabled"] and not refs[key]["deleted"],
                f"Actor {key} is unavailable at Luther Broad's")
    marker = refs[form("03011b")]
    require(marker["cell"] == inn and math.dist(refs[baurus_key]["position"][:3], marker["position"][:3]) < 150,
            "Baurus did not arrive near his meeting marker")
    ai = next(actor for actor in state["actor_ai"] if actor["actor"] == baurus_key)
    require(ai["cell"] == ai["last_valid_cell"] == inn, "Baurus's AI still tracks his previous cell")
    require(state["ignore_friendly_hits"].get(baurus_key), "Baurus's result script stopped early")


def verify_log(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    steps = [
        "actor=" + form("01e745") + " info=" + form(info) for info in ("01efe0", "01efe1", "01efe2")
    ] + ["Dawn ready 01cb98"] + [
        "actor=" + form("01cb98") + " info=" + form(info) for info in ("037cea", "037cf1", "01efe3")
    ]
    position = 0
    for step in steps:
        found = text.find(step, position)
        require(found >= 0, f"Missing or out-of-order interaction {step}")
        position = found + len(step)
    samples = re.findall(r"Dawn (?:approach|ready) 01cb98 player=\(([^)]+)\)", text)
    positions = [tuple(map(float, sample.split(","))) for sample in samples]
    require(len(positions) >= 2 and math.dist(positions[0], positions[-1]) > 150,
            "The player did not walk across the courtyard")
    require("moveto ref=" + form("032a17") + " marker=" + form("03011b") in text,
            "The quest never moved Baurus")
    for line in (1, 2, 3):
        require(f"mq05_amuletofkingstopic_0001efe3_{line}.lip" in text,
                f"Missing facial animation for briefing line {line}")
    require(not re.search(r"OBSV\d+.*(?:setlevel|setactorfullname|getsitting)", text, re.IGNORECASE),
            "A required quest command failed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    verify_save(args.save)
    if args.log:
        verify_log(args.log)
    print("Verified Martin's handoff, Jauffre's journal lead and Baurus at Luther Broad's.")
