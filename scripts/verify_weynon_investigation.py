"""Verify the earned Weynon House discovery and Cloud Ruler Temple objective."""

import argparse
import re
from math import dist
from pathlib import Path

from tes4_runtime_state import load_save


def form(local_id):
    return f"content:oblivion.esm:{local_id}"


def verify(path):
    state = load_save(path)
    house = form("0260df")
    refs = {ref["key"]: ref for ref in state["references"]}
    quests = {quest["quest"]: quest for quest in state["quests"]}
    life = {actor["actor"]: actor["phase"] for actor in state["native_actor_life"]}
    player = state["player"]
    assert player["cell"] == house and player["actor_values"]["health.current"] > 0
    assert quests[form("01e725")]["stage"] == 100
    quest = quests[form("01e726")]
    assert quest["stage"] == 30 and quest["running"]
    assert {20, 30}.issubset(quest["completed_stages"])
    assert {25, 27}.intersection(quest["completed_stages"])
    assert [j for j in quest["journal"] if j["stage"] == 30] == [
        {"stage": 30, "entries": [0], "unresolved": False}
    ]
    assert {form("01ee2d"), form("01ee2e")}.issubset(state["spoken_dialogue_infos"])
    temple = refs[form("0177ab")]
    assert temple["enabled"] and not temple["deleted"]
    assert temple["custom_state"]["native.map_marker.reveal"] == 3

    chest = refs[form("0c8ca7")]
    assert chest["cell"] == house and chest["base"] == form("01e743")
    assert chest["enabled"] and not chest["deleted"] and chest["custom_state"]["opened"]
    assert not chest["inventory"]
    owners = [
        owner for owner in state["script_instances"]
        if owner["context"] == quest["quest"] and owner["unit"].startswith(form("01e6a8") + "@")
    ]
    assert len(owners) == 1 and owners[0]["locals"][0] == {"type": "number", "value": 1}

    for door_id in ("02c6e6", "02c6e7"):
        door = refs[form(door_id)]
        pose = door["custom_state"]
        assert door["cell"] == house and not pose["locked"] and pose["obscript.door_default_open"]
        assert pose["obscript.animation_group"] == "open"
        assert not pose["obscript.animation_absolute"] and pose["obscript.animation_progress"] == 1

    for actor_id in ("01e745", "01cb98"):
        actor = refs[form(actor_id)]
        assert actor["enabled"] and not actor["deleted"] and life[actor["key"]] == 0
        assert actor["cell"] == house and dist(actor["position"][:3], player["position"][:3]) < 800

    ai = {actor["actor"]: actor for actor in state["actor_ai"]}
    jauffre = ai[form("01cb98")]
    martin = ai[form("01e745")]
    assert jauffre["package"] == form("01ee32") and jauffre["target"] == martin["actor"]
    assert martin["package"] == form("064c4b") and martin["target"] == player["reference"]


def verify_crossing(log):
    # The two cabinet pivots lie at y=257 upstairs. A stage change or a
    # correctly assigned package alone doesn't prove the actor cleared them.
    outside = crossed = False
    sample = re.compile(r"Jauffre crossing x=([\d.-]+) y=([\d.-]+) z=([\d.-]+) lower=(\w+)")
    with log.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if "setstage quest=content:oblivion.esm:01e726 stage=30" in line:
                break
            match = sample.search(line)
            if not match:
                continue
            x, y, z = map(float, match.groups()[:3])
            if abs(z + 151) > 16:
                continue
            if 360 < x < 550 and y < 230:
                outside = True
            if outside and y > 330:
                crossed = True
            if crossed and dist((x, y), (322, 613)) < 100:
                if "idle" in match[4] or match[4].startswith("turn"):
                    return
    raise AssertionError(f"{log}: Jauffre must walk through the cabinet and stop running by the chest")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("saves", type=Path, nargs="+")
    parser.add_argument("--crossing-log", type=Path, help="rendered game log with Jauffre position and animation samples")
    args = parser.parse_args()
    if args.crossing_log:
        verify_crossing(args.crossing_log)
        print(f"{args.crossing_log}: Jauffre cleared the cabinet and stopped running beside the chest.")
    for save in args.saves:
        verify(save)
        print(f"{save}: chest inspected, party alive in Weynon House, MQ04 stage 30.")
