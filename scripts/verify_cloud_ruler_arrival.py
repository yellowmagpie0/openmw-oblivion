"""Check an earned MQ04 arrival save and optionally the observed overland escort."""

import argparse
import re
from collections import deque
from math import dist
from pathlib import Path

from tes4_runtime_state import load_save


def form(local_id):
    return f"content:oblivion.esm:{local_id}"


def verify_save(path):
    state = load_save(path)
    player = state["player"]
    quest = next(q for q in state["quests"] if q["quest"] == form("01e726"))
    assert quest["running"] and {20, 30, 35}.issubset(quest["completed_stages"])
    refs = {r["key"]: r for r in state["references"]}
    gate = refs[form("037b4b")]
    assert player["cell"] == gate["cell"] == form("0044ad")
    assert player["actor_values"]["health.current"] > 0
    assert dist(player["position"][:3], gate["position"][:3]) < 1000
    life = {a["actor"]: a["phase"] for a in state["native_actor_life"]}
    distances = []
    for local_id, radius in (("01e745", 1000), ("01cb98", 1250)):
        key = form(local_id)
        actor = refs[key]
        assert sum(r["key"] == key for r in state["references"]) == 1
        assert actor["cell"] == player["cell"] and actor["enabled"] and not actor["deleted"] and life[key] == 0
        distance = dist(actor["position"][:3], gate["position"][:3])
        assert distance < radius, (local_id, distance)
        distances.append(round(distance))
    for local_id in ("037b4b", "031494", "000db6", "000bd9"):
        assert refs[form(local_id)]["custom_state"]["locked"] is False, local_id
    print(f"{path}: MQ04 stage {quest['stage']}, gate unlocked, Martin {distances[0]} and "
          f"Jauffre {distances[1]} units from the main gate.")


def verify_road(path):
    # The pilot reports physical positions on the game's one-second clock.
    # Require both companions throughout the road, including the first seam.
    sample = re.compile(r"Temple sample i=(\d+) player=([-\d,]+) martin=([-\d,]+),(\w+) jauffre=([-\d,]+),(\w+)")
    milestones = set()
    origin = (-56199, 88353, 13567)
    previous = None
    travelled = [0., 0.]
    high_water = [0., 0.]
    moving = [deque(maxlen=12), deque(maxlen=12)]
    left_house = used_exit = outside_together = arrived = False
    missing_at_exit = 0
    with path.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if arrived and "setstage quest=content:oblivion.esm:01e726 stage=40" in line:
                # The reception now sends both actors up the stairs while the player can stay at the gate.
                break
            if "setstage quest=content:oblivion.esm:01e726 stage=35" in line:
                arrived = True
            if "Temple activate house exit" in line:
                used_exit = True
            if "Temple sample" in line and "=away" in line:
                missing_at_exit += 1
                assert not outside_together and "cell=WeynonPrioryExterior" in line and missing_at_exit <= 12, line
            match = sample.search(line)
            if not match:
                continue
            if int(match[1]) < 2:
                left_house |= "cell=WeynonPrioryHouse" in line
                continue
            player, martin, jauffre = (tuple(map(int, match[i].split(","))) for i in (2, 3, 5))
            if not outside_together:
                assert used_exit and "cell=WeynonPrioryExterior" in line, line
                assert max(dist(origin, actor) for actor in (player, martin, jauffre)) < 900, line
                outside_together = True
            for i, (actor, leader, animation) in enumerate(((martin, player, match[4]), (jauffre, martin, match[6]))):
                high_water[i] = max(high_water[i], dist(player, actor))
                if previous:
                    step = dist(actor, previous[i])
                    assert step < 2500, (match[1], actor, previous[i])
                    travelled[i] += step
                history = moving[i]
                if not arrived and dist(actor, leader) > 750 and animation.startswith(("run", "walk")):
                    history.append(actor)
                    if len(history) == history.maxlen:
                        assert max(dist(history[0], position) for position in history) > 64, (actor, animation)
                else:
                    history.clear()
            if dist(player, origin) > 1500 and max(dist(player, martin), dist(player, jauffre)) < 1750:
                milestones.add(int(match[1]))
            previous = martin, jauffre
    assert left_house and outside_together and arrived, "Expected both companions at the house exit and the gate"
    assert set(range(4, 102)).issubset(milestones), sorted(set(range(4, 102)) - milestones)
    assert min(travelled) > 140000, travelled
    print(f"{path}: both actors followed across the overland route. Maximum observed distances "
          f"from the player were {high_water[0]:.0f} and {high_water[1]:.0f} units.")


def verify_map(path):
    travelled = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "Fast travelled to Cloud Ruler Temple" in line:
            travelled = True
            continue
        if travelled and "Temple sample" in line:
            positions = re.findall(r"(?:player|martin|jauffre)=(-?\d+,-?\d+,-?\d+)", line)
            assert len(positions) == 3, line
            player, martin, jauffre = (tuple(map(int, p.split(","))) for p in positions)
            marker = (13391, 160255, 31610)
            assert max(dist(p, marker) for p in (player, martin, jauffre)) < 600, line
            assert min(dist(player, martin), dist(player, jauffre), dist(martin, jauffre)) > 32, line
            print(f"{path}: player, Martin and Jauffre landed separately at the temple marker.")
            return
    raise AssertionError(f"{path}: missing native fast travel or party positions after landing")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("saves", nargs="+", type=Path)
    parser.add_argument("--road-log", type=Path)
    parser.add_argument("--map-log", type=Path)
    args = parser.parse_args()
    if args.road_log:
        verify_road(args.road_log)
    if args.map_log:
        verify_map(args.map_log)
    for save in args.saves:
        verify_save(save)
