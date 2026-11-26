"""Check Jauffre's orders and Steffan's runestone lead across a playable resave."""

import argparse
from pathlib import Path

from verify_lake_arrius_passage import SHRINE, form, load_save, require, verify_events


SPIES, JAUFFRE, STEFFAN, JEARL = map(form, (0x1E729, 0x1CB98, 0x375A0, 0x36286))


def verify_checkpoint(path, expected_stages):
    state = load_save(path)
    quests = {q["quest"]: q for q in state["quests"]}
    refs = {r["key"]: r for r in state["references"]}
    player = state["player"]
    spies = quests[SPIES]
    stage = expected_stages[-1]
    require(spies["running"] and spies["stage"] == stage and spies["completed_stages"] == expected_stages,
            f"{path}: unexpected Spies progression")
    require([step["stage"] for step in spies["journal"] if step["entries"] and not step["unresolved"]]
            == expected_stages, f"{path}: missing briefing journal entry")
    require(quests[SHRINE]["stage"] == 100 and not quests[SHRINE]["running"],
            f"{path}: the book handoff did not persist")
    require(refs[JEARL]["enabled"] == (stage >= 20) and not refs[JEARL]["deleted"],
            f"{path}: Jearl's availability does not match Jauffre's orders")
    require(player["actor_values"]["health.current"] > 0, f"{path}: dead player")
    if stage >= 20:
        speaker = refs[JAUFFRE if stage == 20 else STEFFAN]
        require(speaker["custom_state"].get("native.dialogue.talked_to_player") and speaker["enabled"]
                and not speaker["deleted"], f"{path}: missing conversation with the captain")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("delivered", "orders", "lead", "restart"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--log", required=True, type=Path)
    args = parser.parse_args()
    for path, stages in ((args.delivered, [10]), (args.orders, [10, 20]),
                         (args.lead, [10, 20, 25]), (args.restart, [10, 20, 25])):
        verify_checkpoint(path, stages)
    verify_events(args.log, [
        f"actor={JAUFFRE} info={form(0x20075)} choices=1", "_00020075_2.mp3",
        f"setstage quest={SPIES} stage=20", f"enable ref={JEARL}",
        f"actor={JAUFFRE} info={form(0x20076)}", "_00020076_3.mp3",
        f"setstage quest={SPIES} stage=25", f"actor={STEFFAN} info={form(0x20077)}", "_00020077_2.mp3",
    ])
    print("Jauffre and Steffan briefings verified")
