"""Verify the Gutted Mine encounter and a cold reload of its continuation."""

import argparse
from pathlib import Path

from verify_azura_offering import AZURA, DAEDRA, saved_field
from verify_noon_inscription import form, load_save, require


BRETHREN = {form(0x98207 + i): form(0x98254 + i) for i in range(5)}
LAMPS = tuple(map(form, (0x2C4E5, 0x2C418, 0x2C417, 0x2C416, 0x2C414, 0x187B70)))


def check(path, cleared, crime):
    state = load_save(path)
    quests = {q["quest"]: q for q in state["quests"]}
    require(quests[AZURA]["stage"] == (30 if cleared else 20) and quests[DAEDRA]["stage"] == 15,
            f"{path}: unexpected quest stages")
    script = next(s for s in state["script_instances"] if s["context"] == AZURA and "/quest/" in s["unit"])
    require(script["locals"][0]["value"] == (5 if cleared else 0), f"{path}: incorrect vampire death total")
    counts = {d["base"]: d["count"] for d in state["native_death_counts"]}
    for base in BRETHREN.values():
        require(counts.get(base, 0) == int(cleared), f"{path}: incorrect death count for {base}")
    require(state["player"]["actor_values"]["health.current"] > 0, f"{path}: player did not survive")
    require(saved_field(path, b"TQST", b"ID__").rstrip(b"\0").decode() == AZURA,
            f"{path}: expected Azura to remain tracked")
    require(state["native_crime"] == crime, f"{path}: crime state changed")
    if cleared:
        require(state["player"]["cell"] == form(0x15C40), f"{path}: expected the player in Gutted Mine")
        health = state["player"]["actor_values"]
        require(health["health.current"] == health["health.base"], f"{path}: rest did not restore full health")
        lives = {entry["actor"]: entry for entry in state["native_actor_life"]}
        for ref, base in BRETHREN.items():
            require(lives[ref]["base"] == base and lives[ref]["phase"] == 1, f"{path}: {ref} did not stay dead")
        refs = {r["key"]: r for r in state["references"]}
        require(all(refs[lamp]["enabled"] and not refs[lamp]["deleted"] for lamp in LAMPS),
                f"{path}: Azura's memorial lights were not enabled")
        require(any(j["stage"] == 30 and j["entries"] and not j["unresolved"] for j in quests[AZURA]["journal"]),
                f"{path}: missing return-to-Azura journal entry")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "cleared", "reloaded"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    crime = load_save(args.source)["native_crime"]
    check(args.source, False, crime)
    for path in (args.cleared, args.reloaded):
        check(path, True, crime)
    print("Verified all five deaths, Azura's journal and memorial lights, and the cold reload.")
