"""Check the Bruma gate road, a restored turret fireball and the Fury Spike party."""

import argparse
import math
from itertools import combinations
from pathlib import Path

from verify_bruma_gate_entry import BRUMA_GATE, BURD, ENTRANCE, GUARDS, form, load_save, require


PARTY = (BURD, *GUARDS[:2])
BRIDGE = form(0x54F21)
TOWER = form(0x5619B)
GATE_ENEMIES = tuple(map(form, (0x6D707, 0x6D708, 0x6D70A, 0x6D70B)))


def check_party(path, cell, stage=35):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == BRUMA_GATE)
    player = state["player"]
    require(quest["running"] and quest["stage"] == stage and player["cell"] == cell,
            f"{path}: incorrect quest or player location")
    refs = {r["key"]: r for r in state["references"]}
    lives = {a["actor"]: a["phase"] for a in state["native_actor_life"]}
    for key in (*PARTY, *GUARDS[2:]):
        ref = refs[key]
        require(ref["enabled"] and not ref["deleted"] and lives.get(key) == 0,
                f"{path}: guard missing or down: {key}")
        if key in PARTY:
            require(math.dist(player["position"][:3], ref["position"][:3]) < 1200,
                    f"{path}: {key} is too far from the party")
            if cell != BRIDGE:
                require(ref["cell"] == cell, f"{path}: {key} is in a different cell")
        else:
            require(ref["cell"] == form(0x452D), f"{path}: rear guard left Bruma")
    if cell != ENTRANCE:
        require(all(lives.get(k) == 1 for k in GATE_ENEMIES), f"{path}: bridge encounter incomplete")
    if cell == TOWER:
        for a, b in combinations(PARTY, 2):
            require(math.dist(refs[a]["position"][:3], refs[b]["position"][:3]) > 35,
                    f"{path}: overlapping guards {a} and {b}")
        health = player["actor_values"]
        require(health["health.current"] == health["health.base"] + health["health.modifier"],
                f"{path}: player did not recover")
    return state


def check_flight(path, cold_log):
    flights = load_save(path)["native_magic_projectiles"]
    shot = next(p for p in flights if p["caster"] == form(0xD3AFD) and p["source"] == form(0xA97DF))
    require(shot["model"].lower().endswith("fireball.nif") and shot["magnitude"] == 6
            and 0 < shot["distance"] < shot["maximum_distance"], f"{path}: turret fireball did not travel")
    require(f"magic projectile impact: id={shot['id']} " in cold_log.read_text(encoding="utf-8", errors="replace"),
            f"{cold_log}: saved fireball did not reach impact")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("entrance", "bridge", "flight", "flight_reload_log", "tower", "tower_reload"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    check_party(args.entrance, ENTRANCE)
    check_party(args.bridge, BRIDGE)
    check_flight(args.flight, args.flight_reload_log)
    for save in (args.tower, args.tower_reload):
        check_party(save, TOWER)
    print("Verified the bridge, turret flight and impact, and the living party on both tower saves.")
