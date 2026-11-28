"""Verify the Sigil Stone activation, the Bruma party's return and the playable reload."""

import argparse
import math
import re
from pathlib import Path

from verify_bruma_gate_tower import BRUMA_GATE, GUARDS, PARTY, check_party, form, load_save, require
from verify_bruma_sigil_keeper import CHAMBER
from verify_martin_artifact import fame


def verify(before, closure_log, returned, reloaded):
    source = check_party(before, CHAMBER, stage=40)
    require(math.dist(source["player"]["position"][:3], (1, 32, 339)) < 200,
            f"{before}: expected the player at the Sigil Stone")
    events = closure_log.read_text(encoding="utf-8", errors="replace")
    award = re.search(r"additem owner=dynamic:player:0+1 list=content:oblivion.esm:05e1d9 "
                      r"item=(content:oblivion.esm:[0-9a-f]+) count=1", events)
    require(award and "Error in frame:" not in events, f"{closure_log}: missing award or frame error")
    require(f"gate-collapse ref={form(0x26EBA)} played=true" in events[award.end():],
            f"{closure_log}: missing exterior collapse after award")
    stone = award.group(1)

    def stones(state):
        return sum(i["count"] for i in state["player"]["inventory"] if i["base"] == stone)

    for path in (returned, reloaded):
        state = load_save(path)
        player = state["player"]
        refs = {r["key"]: r for r in state["references"]}
        life = {a["actor"]: a["phase"] for a in state["native_actor_life"]}
        quest = next(q for q in state["quests"] if q["quest"] == BRUMA_GATE)
        require(quest["running"] and quest["stage"] == 50 and quest["journal"][-1] ==
                {"stage": 50, "entries": [1], "unresolved": False}, f"{path}: missing Burd objective")
        require(stones(state) == stones(source) + 1 and fame(state) == fame(source) + 2,
                f"{path}: incorrect Sigil Stone or Fame award")
        gate = refs[form(0x26EBA)]
        require(gate["custom_state"].get("obscript.destroyed") and player["cell"] == form(0x454C)
                and player["actor_values"]["health.current"] > 0, f"{path}: gate or player state incorrect")
        for key in (*PARTY, *GUARDS[2:]):
            actor = refs[key]
            require(actor["enabled"] and not actor["deleted"] and life.get(key) == 0
                    and actor["cell"] in (form(0x454C), form(0x452D))
                    and math.dist(gate["position"][:3], actor["position"][:3]) < 6000,
                    f"{path}: guard missing from the Bruma gate area: {key}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("before", "closure_log", "returned", "reloaded"):
        parser.add_argument(name, type=Path)
    verify(**vars(parser.parse_args()))
    print("Verified the stone, collapse, guards, stage 50 and rewards before and after reload.")
