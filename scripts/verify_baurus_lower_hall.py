"""Verify the sewer wheel, the player's route and Baurus's Lower Hall handoff."""

import argparse
from math import dist
from pathlib import Path

from tes4_runtime_state import load_save
from verify_baurus_sewer_escort import BAURUS, SEWER
from verify_first_edition import note_count
from verify_tar_meena_briefing import QUEST, book_counts, form, require


WHEEL, GATE = form(0x830AE), form(0x830AC)
LOWER_HALL = (2123.490, 929.407, 5408)


def verify_save(path):
    state = load_save(path)
    player = state["player"]
    quest = next(q for q in state["quests"] if q["quest"] == QUEST)
    require(quest["running"] and quest["stage"] == 76, "Expected the meeting decision at stage 76")
    require(player["cell"] == SEWER and player["actor_values"]["health.current"] > 0,
            "The player must be alive in the Elven Garden Sewers")
    require(book_counts(player) == (1, 1, 1, 0) and note_count(player) == 1,
            "Expected the three Commentaries and the Sponsor's note")
    refs = {r["key"]: r for r in state["references"]}
    actor = refs[BAURUS]
    life = next(a for a in state["native_actor_life"] if a["actor"] == BAURUS)
    require(actor["cell"] == SEWER and actor["enabled"] and not actor["deleted"] and life["phase"] == 0,
            "Baurus must be available in the player's cell")
    require(dist(actor["position"][:3], LOWER_HALL) < 300 and dist(player["position"][:3], LOWER_HALL) < 350,
            "Both characters must have arrived at the Lower Hall rendezvous")
    for key in (WHEEL, GATE):
        pose = refs[key]["custom_state"]
        require(pose["obscript.animation_group"].lower() == "forward" and pose["obscript.animation_progress"] == 1,
                f"Missing final animation pose for {key}")
    switch = next(s for s in state["script_instances"] if s["context"] == WHEEL)
    require([slot["value"] for slot in switch["locals"]] == [1, GATE],
            "The authored wheel activation and linked gate must survive the handoff")


def verify_events(paths, events):
    events = iter(events)
    expected = next(events)
    for path in paths:
        with path.open(encoding="utf-8", errors="replace") as log:
            for line in log:
                require("Error in frame:" not in line and "Failed to load saved game:" not in line,
                        f"Game reported an error in {path}")
                if expected and expected in line:
                    expected = next(events, None)
    require(expected is None, f"Missing or out-of-order event {expected}")


def verify_route(paths):
    verify_events(paths, [
        "playgroup ref=" + WHEEL + " group=Forward played=true",
        "playgroup ref=" + GATE + " group=Forward played=true",
        "M5 door activation: result=teleport ref=" + form(0x6AF16),
        "Loading cell ImperialSewersElvenGardens03",
        "M5 door activation: result=teleport ref=" + form(0x6AF26),
        "Loading cell ImperialSewersElvenGardens01",
        "setstage quest=" + QUEST + " stage=76",
        "Native dialogue menu: actor=" + BAURUS + " info=" + form(0x22CEB) + " choices=3",
    ])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path, nargs="+")
    parser.add_argument("--route-log", nargs="+", type=Path, help="Chronological logs, including any restarts")
    parser.add_argument("--restart-log", type=Path, help="Cold restart and player activation after postponing")
    args = parser.parse_args()
    for save in args.save:
        verify_save(save)
    if args.route_log:
        verify_route(args.route_log)
    if args.restart_log:
        verify_events([args.restart_log], [
            "Loading cell ImperialSewersElvenGardens01",
            "M5 activation focus: result=object ref=" + BAURUS,
            "Native dialogue menu: actor=" + BAURUS + " info=" + form(0x22CEC) + " choices=3",
        ])
    print("Baurus Lower Hall handoff verified")
