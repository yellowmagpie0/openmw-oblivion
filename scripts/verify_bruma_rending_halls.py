"""Verify the Fury Spike fight, the north door, and the party's Rending Halls checkpoint."""

import argparse
import math
from itertools import combinations
from pathlib import Path

from verify_bruma_gate_tower import PARTY, TOWER, check_party, form, require


HALLS = form(0x56199)
NORTH_DOOR = form(0x5622C)
DREMORA = (form(0x561CF), form(0x56214))


def verify(source, log, checkpoint, reload_save):
    tower = check_party(source, TOWER)
    lives = {a["actor"]: a["phase"] for a in tower["native_actor_life"]}
    require(all(lives[k] == 0 for k in DREMORA), f"{source}: expected both Dremora alive")
    events = log.read_text(encoding="utf-8", errors="replace")
    crossing = events.index(f"result=teleport ref={NORTH_DOOR}")
    for guard in PARTY:
        require(any(f"NPC Player defense: actor={guard} target={enemy} " in events[:crossing]
                    for enemy in DREMORA), f"{log}: {guard} did not defend the player in the tower")
    for path in (checkpoint, reload_save):
        state = check_party(path, HALLS)
        lives = {a["actor"]: a for a in state["native_actor_life"]}
        require(all(lives[k]["phase"] == 1 for k in DREMORA)
                and any(lives[k]["killer"] in PARTY for k in DREMORA),
                f"{path}: missing tower deaths or guard kill")
        player = state["player"]
        health = player["actor_values"]
        require(health["health.current"] == health["health.base"] + health["health.modifier"],
                f"{path}: player did not recover")
        require(math.dist(player["position"][:3], (1574, -1641, 1610)) < 150,
                f"{path}: player did not reach the foot of the ramp")
        positions = [ref["position"][:3] for ref in state["references"] if ref["key"] in PARTY]
        require(all(math.dist(a, b) > 35 for a, b in combinations([player["position"][:3], *positions], 2)),
                f"{path}: overlapping party members")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("tower", "playthrough_log", "halls", "halls_reload"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    verify(args.tower, args.playthrough_log, args.halls, args.halls_reload)
    print("Verified the tower fight, north door, and living party at the ramp before and after reload.")
