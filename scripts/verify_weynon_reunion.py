"""Check an earned reunion save and its cold resave at Weynon Priory Chapel."""
import argparse
from math import dist
from pathlib import Path

from tes4_runtime_state import load_save


def verify(save):
    state = load_save(save)
    prefix = "content:oblivion.esm:"
    chapel = prefix + "0260de"
    stages = {quest["quest"]: quest["stage"] for quest in state["quests"]}
    assert stages[prefix + "01e725"] == 100 and stages[prefix + "01e726"] == 20
    player = state["player"]
    assert player["cell"] == chapel and player["actor_values"]["health.current"] > 0
    refs = {ref["key"]: ref for ref in state["references"]}
    life = {actor["actor"]: actor["phase"] for actor in state["native_actor_life"]}
    for actor_id in ("01e745", "01cb98"):
        actor = prefix + actor_id
        assert refs[actor]["enabled"] and not refs[actor]["deleted"]
        assert refs[actor]["cell"] == chapel and life[actor] == 0
        assert dist(refs[actor]["position"][:3], player["position"][:3]) < 800
    for attacker_id in ("01ee21", "01e90c"):
        attacker = prefix + attacker_id
        assert refs[attacker]["cell"] == chapel and life[attacker] == 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("saves", type=Path, nargs="+")
    for path in parser.parse_args().saves:
        verify(path)
        print(f"{path}: Martin and Jauffre alive with the player, chapel defended, MQ03 100 and MQ04 20.")
