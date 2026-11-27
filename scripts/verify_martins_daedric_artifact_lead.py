"""Verify Martin's first ritual ingredient, the temple book and the shrine lead after reloading."""

import argparse
from pathlib import Path

from verify_jauffres_spy_report import verify_report
from verify_lake_arrius_passage import form, load_save, require, verify_events


MARTIN, SPIES, DAEDRA = map(form, (0x1E745, 0x1E729, 0x1E72A))
HERETICS, XARXES, SHRINE = map(form, (0x181CF0, 0x8B607, 0x3B363))


def verify_checkpoint(path, read):
    state = load_save(path)
    quests = {quest["quest"]: quest for quest in state["quests"]}
    refs = {ref["key"]: ref for ref in state["references"]}
    require(state["player"]["cell"] == refs[MARTIN]["cell"] == form(0x313AE), f"{path}: expected Great Hall")
    require(quests[SPIES]["stage"] == 100 and not quests[SPIES]["running"], f"{path}: Spies still running")
    quest = quests[DAEDRA]
    stages = [10, 15, 9] if read else [10, 15]
    require(quest["running"] and quest["stage"] == 15 and quest["completed_stages"] == sorted(stages)
            and [entry["stage"] for entry in quest["journal"]] == stages, f"{path}: unexpected artifact journal")
    for key in (HERETICS, XARXES):
        book = refs[key]
        require(book["enabled"] and not book["deleted"] and book["cell"] == refs[MARTIN]["cell"]
                and book["custom_state"]["count"] == 1, f"{path}: missing desk book {key}")
    require(bool(refs[HERETICS]["custom_state"].get("read")) == read, f"{path}: unexpected book state")
    require(refs.get(SHRINE, {}).get("custom_state", {}).get("native.map_marker.reveal", 0) == int(read),
            f"{path}: unexpected shrine discovery state")
    require(refs[form(0xC5402)]["enabled"], f"{path}: quest encounter enable flag was lost")
    study = next(actor for actor in state["actor_ai"] if actor["actor"] == MARTIN)
    require(study["package"] == form(0x376AC) and study["phase"] in (5, 6, 7),  # Act, Wait or Complete
            f"{path}: Martin's study package did not recover")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("reported", "briefed", "read", "reloaded", "log"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    verify_report(args.reported)
    for checkpoint, read in ((args.briefed, False), (args.read, True), (args.reloaded, True)):
        verify_checkpoint(checkpoint, read)
    voice = "M10 voice playback: sound/voice/oblivion.esm/imperial/m/"
    verify_events(args.log, [
        f"result=teleport ref={form(0xAFD)}",
        f"Native NPC furniture: actor={MARTIN} ref={form(0x376AA)}",
        f"Native NPC furniture: actor={MARTIN} phase=3",
        f"setstage quest={DAEDRA} stage=10",
        *(voice + f"mq07_greeting_0002006e_{line}.mp3" for line in (6, 4, 5, 7)),
        f"setstage quest={DAEDRA} stage=15",
        *(voice + f"mq08_mq08artifacttopic_00026efa_{line}.mp3" for line in (1, 3, 2)),
        f"event=onactivate unit={form(0x9829A)}@oblivion.esm/object/unit=0 instance={HERETICS}",
        f"showmap ref={SHRINE} flags=1", f"setstage quest={DAEDRA} stage=9",
    ])
    print("Verified Martin's seated briefing, both voiced responses, the desk book, shrine lead and stock reload.")
