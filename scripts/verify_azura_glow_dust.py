"""Verify the Bruma guild route for obtaining Azura's offering."""

import argparse
from pathlib import Path

from verify_azuras_shrine_arrival import AZURA, DAEDRA
from verify_noon_inscription import count, form, load_save, require, verify_events


GUILD, JOIN, JEANNE, ACCEPT = map(form, (0x22296, 0x2CD10, 0xBF55, 0x2D286))
DUST, PLACED_DUST = 0x1EBE8, form(0x53B7D)
GUILD_HALL = form(0x260CD)


def verify_checkpoint(path, starting_crime):
    state = load_save(path)
    player = state["player"]
    quests = {q["quest"]: q for q in state["quests"]}
    dust = next(r for r in state["references"] if r["key"] == PLACED_DUST)
    factions = next(b["factions"] for b in state["native_actor_bases"] if b["base"] == form(7))
    require(player["cell"] == GUILD_HALL and player["actor_values"]["level"] == 1,
            f"{path}: expected the level-one player in the guild hall")
    require(count(player, DUST) == 1 and dust["deleted"] and dust["custom_state"]["count"] == 0,
            f"{path}: the single placed sample did not transfer to the player")
    require([GUILD, 0] in factions and quests[JOIN]["stage"] == 50
            and count(player, 0x2229C) == 1 and count(player, 0x26D8B) == 1,
            f"{path}: missing membership, key, or guild charter")
    require(quests[DAEDRA]["stage"] == 15 and quests[AZURA]["stage"] == 0,
            f"{path}: unexpected shrine or main quest stage")
    require(state["native_crime"] == starting_crime, f"{path}: crime state changed during the guild visit")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("source", "checkpoint", "reloaded", "log"):
        parser.add_argument(arg, type=Path)
    args = parser.parse_args()
    source = load_save(args.source)
    require(count(source["player"], DUST) == 0, "Starting save already has glow dust")
    for path in (args.checkpoint, args.reloaded):
        verify_checkpoint(path, source["native_crime"])
    verify_events(args.log, [
        "Fast travelled to Castle Bruma", f"result=teleport ref={form(0x30409)}",
        f"actor={JEANNE} info={form(0x2D27B)} choices=2", f"faction={GUILD} rank=0",
        f"setstage quest={JOIN} stage=50", f"actor={JEANNE} info={ACCEPT}", "_0002d286_3.mp3",
        f"result=teleport ref={form(0x49186)}", f"result=taken ref={PLACED_DUST} base={form(DUST)}",
        f"native bed rest: ref={form(0x4FA41)} opened=true", "sleeping=true completed=1 total=1",
        f"result=teleport ref={form(0x4D4A3)}",
    ])
    lines = args.log.read_text(encoding="utf-8", errors="replace").splitlines()
    require(not any("ObScript diagnostic:" in line and f"unit={ACCEPT}@" in line for line in lines),
            "Jeanne's enrollment result script reported an error")
    print("Verified Bruma guild enrollment, glow dust pickup, rest, and playable reload.")
