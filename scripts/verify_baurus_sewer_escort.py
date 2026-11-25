"""Check the bookstore-to-sewer escort and the state needed to resume it."""

import argparse
from math import dist
from pathlib import Path

from tes4_runtime_state import load_save
from verify_first_edition import BAURUS, note_count
from verify_tar_meena_briefing import QUEST, book_counts, form, require, verify_events


SEWER = form(0xABB3)
ENTRANCE = form(0x22CEA)
GO_TO_WHEEL = form(0x9589D)


def verify_save(path):
    state = load_save(path)
    player = state["player"]
    quest = next(q for q in state["quests"] if q["quest"] == QUEST)
    require(quest["running"] and quest["stage"] == 75 and player["cell"] == SEWER,
            "Expected the active escort inside the Elven Garden Sewers")
    require([(entry["entries"], entry["unresolved"]) for entry in quest["journal"] if entry["stage"] == 75]
            == [([0], False)], "Expected one journal instruction to follow Baurus")
    require(book_counts(player) == (1, 1, 1, 0) and note_count(player) == 1,
            "The player must retain the three Commentaries and the Sponsor's note")
    baurus = next(r for r in state["references"] if r["key"] == BAURUS)
    ai = next(a for a in state["actor_ai"] if a["actor"] == BAURUS)
    life = next(a for a in state["native_actor_life"] if a["actor"] == BAURUS)
    require(baurus["enabled"] and not baurus["deleted"] and life["phase"] == 0, "Baurus is unavailable")
    require(ai["cell"] == baurus["cell"] == SEWER and ai["package"] == GO_TO_WHEEL
            and ai["target"] == player["reference"] and ai["last_transition_door"] == ENTRANCE
            and ai["phase"] == 2 and not ai["interruption_reason"],
            "Baurus must have led the player through the grate and still be escorting them")
    require(dist(baurus["position"][:3], player["position"][:3]) < 550,
            "Baurus is outside the escort range at the handoff")
    return player["position"][:3], baurus["position"][:3]


def verify_route(path):
    door = "M5 door activation: result=teleport ref="
    verify_events(path, [
        door + form(0x2C246), door + form(0x2C394),
        "Native dialogue menu: actor=" + BAURUS + " info=" + form(0x22B8B),
        "setstage quest=" + QUEST + " stage=75",
        "Native dialogue menu: actor=" + BAURUS + " info=" + form(0x22CF0),
        door + ENTRANCE, "Loading cell ImperialSewersElvenGardens01",
    ])


def verify_wait(before_path, after_path):
    samples = []
    for path in (before_path, after_path):
        state = load_save(path)
        require(state["player"]["cell"] != SEWER, "The player must still be outside")
        actor = next(a for a in state["actor_ai"] if a["actor"] == BAURUS)
        require(actor["cell"] == SEWER and actor["last_transition_door"] == ENTRANCE
                and actor["package"] == GO_TO_WHEEL and not actor["interruption_reason"],
                "Baurus lost the escort while waiting on the far side")
        samples.append((state["clock"], actor["last_valid_position"][:3]))
    require(samples[0][0] != samples[1][0] and dist(samples[0][1], samples[1][1]) < 2,
            "Baurus should wait at the landing while game time advances")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--route-log", type=Path)
    parser.add_argument("--outside", nargs=2, type=Path, metavar=("BEFORE", "AFTER"))
    parser.add_argument("--after-walking", type=Path, help="Later save from walking forward after a restart")
    args = parser.parse_args()
    player, leader = verify_save(args.save)
    if args.route_log:
        verify_route(args.route_log)
    if args.outside:
        verify_wait(*args.outside)
    if args.after_walking:
        next_player, next_leader = verify_save(args.after_walking)
        require(dist(player, next_player) > 64 and dist(leader, next_leader) > 64,
                "Both the player and Baurus must advance after resuming the escort")
    print("Baurus sewer escort verified")
