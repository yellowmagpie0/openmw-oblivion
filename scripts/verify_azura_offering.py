"""Check the Bruma skill gains, bed level-up, and Azura's accepted offering."""

import argparse
import struct
from pathlib import Path

from tes4_runtime_state import _save_records
from verify_azura_glow_dust import AZURA, DAEDRA, DUST
from verify_noon_inscription import count, form, load_save, require, verify_events


def saved_field(path, record, field):
    data = path.read_bytes()
    return next((data[start + 8:end] for _, _, tag, subs in _save_records(data) if tag == record
                 for name, start, end in subs if name == field), b"")


def level_progress(path):
    value = saved_field(path, b"PLAY", b"LPRO")
    return struct.unpack("<i", value)[0] if value else 0


def verify(path, source, level, progress, offered):
    state = load_save(path)
    player = state["player"]
    stats = player["actor_values"]
    before = source["player"]["actor_values"]
    gains = {"illusion": 1, "heavyarmor": 3, "handtohand": 1}
    for skill, increase in gains.items():
        require(stats[skill + ".base"] == before[skill + ".base"] + increase, f"{path}: {skill}")
    require(stats["level"] == level and level_progress(path) == progress, f"{path}: level progress")
    require(count(player, DUST) == int(not offered), f"{path}: incorrect glow dust count")
    require(state["native_crime"] == source["native_crime"], f"{path}: unexpected crime")
    quests = {q["quest"]: q for q in state["quests"]}
    require(quests[AZURA]["stage"] == (20 if offered else 0) and quests[DAEDRA]["stage"] == 15,
            f"{path}: incorrect quest state")
    if level == 2:
        for name, value in (("strength", 58), ("endurance", 54), ("personality", 57), ("health", 113)):
            require(stats[name + ".base"] == value, f"{path}: {name} gain lost")
    if offered:
        require(saved_field(path, b"TQST", b"ID__").rstrip(b"\0").decode() == AZURA,
                f"{path}: Azura is no longer tracked")
        refs = {r["key"]: r for r in state["references"]}
        marker = refs[form(0x1752A)]["custom_state"]
        require(marker.get("native.map_marker.reveal") == 1, f"{path}: mine should be revealed")
        require(refs[form(0x17529)]["lock_level"] <= 0, f"{path}: mine is still locked")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "trained", "leveled", "dawn", "offered", "reloaded", "shrine_log"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    source = load_save(args.source)
    require(level_progress(args.source) == 5 and count(source["player"], DUST) == 1, "Unexpected starting save")
    verify(args.trained, source, 1, 10, False)
    verify(args.leveled, source, 2, 0, False)
    dawn = load_save(args.dawn)
    require(5 <= dawn["clock"]["hour"] < 7 and count(dawn["player"], DUST) == 1, "Expected dawn at the altar")
    for path in (args.offered, args.reloaded):
        verify(path, source, 2, 0, True)
    verify_events(args.shrine_log, [
        "Do you wish to offer Glow Dust", f"item={form(DUST)} count=1",
        "disableplayercontrols movement=true", *[f"_0002c6c{i}_1.mp3" for i in range(1, 5)],
        "enableplayercontrols movement=true", f"setstage quest={AZURA} stage=20",
    ])
    print("Verified five earned major increases, level 2, Azura's four responses, mine access, and stock reload.")
