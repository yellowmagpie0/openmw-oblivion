"""Check the Fury Spike balcony climb and the party's Corridors of Dark Salvation checkpoint."""

import argparse
import math
from itertools import combinations
from pathlib import Path

from verify_bruma_gate_tower import PARTY, TOWER, check_party, form, load_save, require


CORRIDORS = form(0x56198)
UPPER_DOOR = form(0x56232)
DEFENDERS = (form(0x5620F), form(0x56211))


def check_landing(state, path, cell, landing, radius, defenders_dead):
    refs = {r["key"]: r for r in state["references"]}
    members = [state["player"], *(refs[key] for key in PARTY)]
    require(all(member["cell"] == cell for member in members), f"{path}: party outside expected cell")
    positions = [member["position"][:3] for member in members]
    require(all(math.dist(pos, landing) < radius for pos in positions), f"{path}: party outside landing")
    require(all(math.dist(a, b) > 35 for a, b in combinations(positions, 2)), f"{path}: party overlaps")
    lives = {a["actor"]: a["phase"] for a in state["native_actor_life"]}
    require(all(lives.get(key) == 0 for key in PARTY), f"{path}: guard down")
    expected_phase = 1 if defenders_dead else 0
    require(all(lives.get(actor) == expected_phase for actor in DEFENDERS), f"{path}: unexpected defender state")


def verify(balcony, upper_landing, run_log, corridors, reload_save):
    check_landing(check_party(balcony, TOWER), balcony, TOWER, (878, 161, 2355), 450, False)
    check_landing(load_save(upper_landing), upper_landing, TOWER, (-600, 390, 2987), 400, True)
    events = run_log.read_text(encoding="utf-8", errors="replace")
    before, crossing, _ = events.partition(f"result=teleport ref={UPPER_DOOR} destination=FormId:0x1056198")
    require(crossing, f"{run_log}: missing door to the Corridors of Dark Salvation")
    deaths = [line for line in before.splitlines() if "native death callback:" in line]
    for actor in DEFENDERS:
        require(any(f" actor={actor} " in line for line in deaths), f"{run_log}: missing fight with {actor}")
    for path in (corridors, reload_save):
        check_landing(check_party(path, CORRIDORS), path, CORRIDORS, (1700, 149, -672), 400, True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("balcony", "upper_landing", "run_log", "corridors", "reload_save"):
        parser.add_argument(name, type=Path)
    verify(**vars(parser.parse_args()))
    print("Verified both tower fights, the upper door, and the party at the corridor entrance before and after reload.")
