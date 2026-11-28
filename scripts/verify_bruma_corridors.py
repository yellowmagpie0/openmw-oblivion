"""Verify the southern route through the Corridors of Dark Salvation and the Fury Spike return."""

import argparse
from pathlib import Path

from verify_bruma_gate_tower import TOWER, check_party, form, require
from verify_bruma_upper_tower import CORRIDORS, check_landing


LOWER_CHURL = form(0x562A3)
UPPER_CHURL = form(0xC6DAC)


def verify(entrance, middle, upper, log, balcony, reloaded):
    stops = (
        (entrance, CORRIDORS, (1700, 149, -672), 400, (0, 0)),
        (middle, CORRIDORS, (1957, -1136, 98), 650, (1, 0)),
        (upper, CORRIDORS, (1628, 122, 864), 650, (1, 1)),
        (balcony, TOWER, (-663, 319, 5216), 650, (1, 1)),
        (reloaded, TOWER, (-663, 319, 5216), 650, (1, 1)),
    )
    for path, cell, center, radius, expected in stops:
        state = check_party(path, cell)
        check_landing(state, path, cell, center, radius, defenders_dead=True)
        lives = {a["actor"]: a["phase"] for a in state["native_actor_life"]}
        require((lives.get(LOWER_CHURL), lives.get(UPPER_CHURL)) == expected,
                f"{path}: unexpected Dremora encounter state")
    events = log.read_text(encoding="utf-8", errors="replace")
    door = f"result=teleport ref={form(0x562AB)} destination=FormId:0x105619b"
    require(door in events, f"{log}: missing upper corridor door crossing")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("entrance", "middle", "upper", "log", "balcony", "reloaded"):
        parser.add_argument(name, type=Path)
    verify(**vars(parser.parse_args()))
    print("Verified the two ramps, Dremora encounters and the living party on the high balcony after reload.")
