"""Verify the original Kvatch-to-Weynon map journey, using two ordinary saves."""
import argparse
from math import dist
from pathlib import Path

from tes4_runtime_state import load_save


def check_party(save, cell, marker):
    state = load_save(save)
    player = state["player"]
    martin = "content:oblivion.esm:01e745"
    ref = next(ref for ref in state["references"] if ref["key"] == martin)
    ai = next(actor for actor in state["actor_ai"] if actor["actor"] == martin)
    assert ref["enabled"] and not ref["deleted"]
    assert ref["cell"] == ai["cell"] == player["cell"] == cell
    assert dist(ref["position"][:3], player["position"][:3]) < 800
    assert dist(player["position"][:3], marker) < 800
    assert ai["package"] == "content:oblivion.esm:01e8d8"
    assert ai["target"] == "dynamic:player:0000000000000001"
    assert player["actor_values"]["health.current"] > 0
    return state


def verify(departure, arrival):
    before = check_party(departure, "content:oblivion.esm:014766", (-144882, -15885, 11464))
    after = check_party(arrival, "content:oblivion.esm:00f32b", (-56998, 85907, 13311))
    def date(state):
        return tuple(state["clock"][key] for key in ("year", "month", "day", "hour"))
    assert date(after) > date(before)
    stages = {quest["quest"]: quest["stage"] for quest in after["quests"]}
    assert stages["content:oblivion.esm:01e726"] >= 7
    print("Martin and the living player travelled together. Time advanced and the priory encounter started.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("departure", type=Path)
    parser.add_argument("arrival", type=Path)
    args = parser.parse_args()
    verify(args.departure, args.arrival)
