"""Check the Rending Halls ascent, blade activation and Fury Spike balcony reload."""

import argparse
import math
from pathlib import Path

from verify_bruma_gate_tower import load_save
from verify_bruma_rending_halls import HALLS, PARTY, TOWER, check_party, form, require


HALL_ENEMIES = tuple(map(form, (0x5626F, 0x56271, 0x56272, 0x56274)))
BLADE = form(0x56251)
TRIGGER = form(0x165C0A)


def check_landing(path, cell, landing):
    state = check_party(path, cell)
    positions = [state["player"]["position"]]
    positions.extend(r["position"] for r in state["references"] if r["key"] in PARTY)
    require(all(math.dist(p[:3], landing) < 800 for p in positions),
            f"{path}: party did not reach the landing")
    lives = {a["actor"]: a["phase"] for a in state["native_actor_life"]}
    require(all(lives.get(key) == 1 for key in HALL_ENEMIES), f"{path}: hall encounter incomplete")


def verify(source, approach_log, before_blade, crossing_log, swinging_blade, upper_exit, balcony, reload_save):
    start = check_party(source, HALLS)
    require(math.dist(start["player"]["position"][:3], (1574, -1641, 1610)) < 150,
            f"{source}: expected the foot of the first ramp")
    approach = approach_log.read_text(encoding="utf-8", errors="replace")
    require(f"result=animated ref={form(0x56269)}" in approach, f"{approach_log}: missing lower gate")
    require(f"trap impact: reference={BLADE}" in approach, f"{approach_log}: missing test of blade damage")
    landing = check_party(before_blade, HALLS)
    require(math.dist(landing["player"]["position"][:3], (-2028, -1200, 3048)) < 100,
            f"{before_blade}: expected the upper corridor approach")
    events = crossing_log.read_text(encoding="utf-8", errors="replace")
    expected = (
        f"activate-script ref={BLADE} activator={TRIGGER}",
        f"result=teleport ref={form(0x56280)}",
    )
    offset = 0
    for event in expected:
        at = events.find(event, offset)
        require(at >= 0, f"{crossing_log}: missing {event}")
        offset = at + len(event)
    require(f"playgroup ref={BLADE} group=unequip played=true" in events.lower(),
            f"{crossing_log}: blade animation did not start")
    crossing = load_save(swinging_blade)
    animation = next(r["custom_state"] for r in crossing["references"] if r["key"] == BLADE)
    require(animation["obscript.animation_group"] == "unequip" and animation["obscript.animation_playing"]
            and 0 < animation["obscript.animation_progress"] < 1,
            f"{swinging_blade}: missing mid-swing animation state")
    check_landing(upper_exit, HALLS, (498, 210, 3178))
    for path in (balcony, reload_save):
        check_landing(path, TOWER, (878, 161, 2355))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "approach_log", "before_blade", "crossing_log", "swinging_blade",
                 "upper_exit", "balcony", "reload_save"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    verify(**vars(args))
    print("Verified both ramps, the blade, the upper exit and the living party on both balcony saves.")
