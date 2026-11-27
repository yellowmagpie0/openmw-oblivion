"""Verify the journey from Cloud Ruler Temple to Azura's Shrine on the level-one save."""

import argparse
import math
from pathlib import Path

from verify_lake_arrius_passage import form, load_save, require, verify_events
from verify_martins_daedric_artifact_lead import DAEDRA, SHRINE, verify_checkpoint


MELS, ALTAR, AZURA = map(form, (0x97864, 0x98206, 0x146A2))


def verify_shrine(path, briefed):
    state = load_save(path)
    refs = {r["key"]: r for r in state["references"]}
    quests = {q["quest"]: q for q in state["quests"]}
    player = state["player"]
    require(refs[SHRINE]["custom_state"].get("native.map_marker.reveal") == 3,
            f"{path}: shrine must be both revealed and discovered")
    require(player["cell"] == refs[MELS]["cell"]
            and math.dist(player["position"][:3], refs[ALTAR]["position"][:3]) < 1400,
            f"{path}: player is outside the shrine grounds")
    require(refs[MELS]["enabled"] and not refs[MELS]["deleted"]
            and bool(refs[MELS]["custom_state"].get("native.dialogue.talked_to_player")) == briefed,
            f"{path}: unexpected state of Mels Maryon")
    require(player["actor_values"]["level"] == 1 and player["actor_values"]["health.current"] > 0,
            f"{path}: unexpected player state")
    require(quests[DAEDRA]["stage"] == 15 and quests[DAEDRA]["completed_stages"] == [9, 10, 15]
            and quests[AZURA]["stage"] == 0 and not quests[AZURA]["completed_stages"],
            f"{path}: artifact or shrine quest advanced unexpectedly")
    if briefed:
        well = refs[form(0x6B140)]["custom_state"]
        require(well.get("obscript.animation_group") == "specialidle" and well.get("obscript.animation_playing")
                and well.get("obscript.animation_loop_count", 0) > 1,
                f"{path}: the nearby Ayleid well lost its looping idle")
    return player["actor_values"]["pc_misc_stat.7"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "arrival", "ready", "reloaded", "travel_log", "shrine_log"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    verify_checkpoint(args.source, True)
    discoveries = verify_shrine(args.arrival, False)
    for path in (args.ready, args.reloaded):
        require(verify_shrine(path, True) == discoveries, f"{path}: discovery counted twice")
    verify_events(args.travel_log, [
        f"result=teleport ref={form(0x31468)}", "Fast travelled to Lake Arrius Caverns",
        f"discovered ref={SHRINE} location=Azura's Shrine",
    ])
    verify_events(args.shrine_log, [
        f"discovered ref={SHRINE} location=Azura's Shrine", f"actor={MELS} info={form(0x981F1)}",
        f"actor={MELS} info={form(0x98204)}", "_00098204_2.mp3",
        f"native cast release: spell={form(0x47ADD)} applied=1", f"actor={MELS} info={form(0x1EBE9)} choices=2",
        f"actor={MELS} info={form(0x1EBE4)}", "_0001ebe4_2.mp3",
        f"event=onactivate unit={form(0x1EBE7)}@oblivion.esm/object/unit=0 instance={ALTAR}",
        "messagebox text=Azura demands a more powerful champion to begin this quest.",
    ])
    replay = args.shrine_log.read_text(encoding="utf-8", errors="replace")
    for group in ("backward", "specialidle"):
        require(replay.count(f"playgroup ref={form(0x6B140)} group={group} played=true") == 1,
                f"The shrine well did not run its {group} group exactly once during this visit")
    print("Verified shrine discovery, Mels's disposition branches, the level-one altar response and stock reload.")
