"""Verify the played route from Jauffre's briefing to Baurus's warning."""

import argparse
from pathlib import Path

from tes4_runtime_state import load_save


def form(local_id):
    return f"content:oblivion.esm:{local_id}"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_save(path):
    state = load_save(path)
    quest = next(q for q in state["quests"] if q["quest"] == form("01e727"))
    require(quest["running"] and quest["stage"] in (25, 30), "Unexpected Path of Dawn stage")
    require(any(j["stage"] == 25 and j["entries"] == [0] and not j["unresolved"]
                for j in quest["journal"]), "Missing instruction to follow Baurus's watcher")
    require(state["player"]["cell"] == form("02c1be"), "Player left the boarding house")
    require(not state.get("native_player_furniture"), "Player is still on the stool")
    refs = {r["key"]: r for r in state["references"]}
    for key in ("032a17", "022b0c"):
        actor = refs[form(key)]
        require(actor["enabled"] and not actor["deleted"], f"Actor {key} unavailable")
    require(any(item["base"] == form("022b04") and item["count"] > 0
                for item in refs[form("022b0c")]["inventory"]), "Astav lost his copy of the Commentaries")
    if quest["stage"] == 30:
        watcher = next(ai for ai in state["actor_ai"] if ai["actor"] == form("022b0c"))
        require(watcher["package"] == form("022b0d") and watcher["target"] == form("032a17"),
                "Astav is not following Baurus")


def verify_log(path):
    log = path.read_text(encoding="utf-8", errors="replace")
    start = log.index("Fast travelled to Imperial City Elven Gardens District")
    door = log.index("M5 door activation: result=teleport ref=" + form("02c3b8"), start)
    room = log.index("Loading cell ICElvenGardensLutherBroadsBoardingHouse", door)
    warning = log.index("info=" + form("022b13") + " choices=3", room)
    for actor, chair in (("032a17", "0549df"), ("022b0c", "022b0a")):
        prefix = "Native NPC furniture: actor=" + form(actor)
        require(prefix + " ref=" + form(chair) in log[room:warning], f"Missing chair for {actor}")
        require(any(line.endswith("phase=3") and prefix in line
                    for line in log[room:warning].splitlines()), f"{actor} was not seated")
    require("Native Player furniture: phase=3 ref=" + form("0549e1") in log[room:warning],
            "Conversation did not start from the adjacent stool")
    reply = log.index("info=" + form("022b16") + " choices=", warning)
    menus = [line for line in log[room:].splitlines() if "Native dialogue menu:" in line]
    require(len(menus) >= 2 and form("022b13") in menus[0] and form("022b16") in menus[1],
            "Another greeting replaced the warning before the player replied")
    for info in ("022b13", "022b16"):
        require("Native dialogue voices: info=" + form(info) + " files=1 responses=1 matched=1" in log[room:],
                f"Missing voice line for {info}")
        require(f"_00{info}_1.lip" in log[room:], f"Missing lip animation for {info}")
    require("Native NPC furniture: actor=" + form("032a17") + " phase=0" in log[reply:],
            "Baurus did not leave his stool")
    require("Native Player furniture: phase=0" in log[reply:], "Player never stood back up")
    require("Error in frame:" not in log and "Failed to load saved game:" not in log, "Runtime error")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    verify_save(args.save)
    if args.log:
        verify_log(args.log)
    print("Baurus's surveillance checkpoint verified")
