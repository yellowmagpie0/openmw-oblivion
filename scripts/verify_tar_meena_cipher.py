"""Verify Tar-Meena's decoded clue and the two overnight conversations."""

import argparse
from pathlib import Path

from tes4_runtime_state import load_save
from verify_tar_meena_briefing import BOOKS, LOBBY, QUEST, TAR_MEENA, book_counts, form, require, verify_events


def verify_checkpoint(path, stage):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == QUEST)
    require(quest["running"] and quest["stage"] == stage, f"Expected Path of Dawn stage {stage}")
    stages = [entry["stage"] for entry in quest["journal"] if entry["entries"] and not entry["unresolved"]]
    require(stages[-(stage - 89):] == list(range(90, stage + 1)), "Missing a cipher journal entry")
    player = state["player"]
    require(player["cell"] == LOBBY and book_counts(player) == (1, 1, 1, 1),
            "Expected the player in the Arch-Mage's Lobby with all four volumes")
    require(any(item["base"] == form(0x9588D) and item["count"] > 0 for item in player["inventory"]),
            "The Sunken Sewers key is missing")
    scholar = next(ref for ref in state["references"] if ref["key"] == TAR_MEENA)
    require(scholar["enabled"] and not scholar["deleted"] and scholar["cell"] == LOBBY,
            "Tar-Meena is unavailable")
    require(book_counts(scholar) == (1, 1, 0, 0), "Incorrect study copies")
    require(all(item.get("negative_count") for item in scholar["inventory"] if item["base"] in BOOKS),
            "Tar-Meena's study copies lost their native stock counts")


def verify_course(path):
    door = "M5 door activation: result=teleport ref="
    menu = f"Native dialogue menu: actor={TAR_MEENA} info="
    wait = "native rest hour: sleeping=false completed=24 total=24"
    verify_events(path, [
        *(door + form(ref) for ref in (0x22B85, 0x9EA58, 0x47E3C, 0x2C1F2)),
        "Fast travelled to Imperial City, The Arcane University", door + form(0x3884),
        f"setstage quest={QUEST} stage=91",
        *(f"additem owner={TAR_MEENA} item={book} count=1" for book in BOOKS[:2]),
        menu + form(0x24E6C), "_00024e6c_5.lip", wait,
        f"setstage quest={QUEST} stage=92", menu + form(0x24E6B), "_00024e6b_5.lip", wait,
        menu + form(0x22B3F), f"setstage quest={QUEST} stage=93", menu + form(0x24E81), "_00024e81_4.lip",
    ])
    log = path.read_text(encoding="utf-8", errors="replace")
    errors = [line for line in log.splitlines() if "ObScript diagnostic:" in line and "quest-result/stage=9" in line]
    require(not errors, "Cipher quest result script failed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--first-hint", type=Path)
    parser.add_argument("--restart-save", type=Path)
    parser.add_argument("--course-log", type=Path)
    args = parser.parse_args()
    verify_checkpoint(args.save, 93)
    if args.first_hint:
        verify_checkpoint(args.first_hint, 91)
    if args.restart_save:
        verify_checkpoint(args.restart_save, 93)
    if args.course_log:
        verify_course(args.course_log)
    print("Tar-Meena's cipher verified")
