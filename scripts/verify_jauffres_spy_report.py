"""Check the trip from Jearl's basement, the report to Jauffre, and a stock reload."""

import argparse
import math
from pathlib import Path

from verify_brumas_spy_orders import verify as verify_basement
from verify_lake_arrius_passage import count, form, load_save, require, verify_events
from verify_spies_briefing import JAUFFRE, SPIES


def verify_report(path):
    state = load_save(path)
    player = state["player"]
    quest = next(q for q in state["quests"] if q["quest"] == SPIES)
    require(quest["running"] and quest["stage"] == 90 and quest["completed_stages"][-2:] == [80, 90],
            f"{path}: unexpected outcome of the spy report")
    require(quest["journal"][-1] == {"stage": 90, "entries": [0, 1], "unresolved": False},
            f"{path}: missing warning to Bruma or lead to Martin")
    require(count(player, 0xC026) == 1 and state["quest_book_flags"].get(form(0xC026)) is False,
            f"{path}: the reported orders should remain as an ordinary scroll")
    jauffre = next(r for r in state["references"] if r["key"] == JAUFFRE)
    require(player["cell"] == jauffre["cell"] == form(0xADD) and jauffre["enabled"] and not jauffre["deleted"],
            f"{path}: expected Jauffre and the player in the temple's East Wing")
    for blade in state["references"]:
        if blade["key"] in (form(0x375A0), form(0x375A1)) and blade["cell"] == jauffre["cell"]:
            require(math.dist(jauffre["position"][:3], blade["position"][:3]) >= 48,
                    f"{path}: {blade['key']} is crowding Jauffre at the dining marker")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("basement", "reported", "reloaded"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--walk-log", type=Path, required=True)
    parser.add_argument("--dialogue-log", type=Path, required=True)
    args = parser.parse_args()
    verify_basement(args.basement, 80)
    for path in (args.reported, args.reloaded):
        verify_report(path)
    verify_events(args.walk_log, [f"result=teleport ref={form(ref)}" for ref in
                                  (0x1E6AB, 0x303D3, 0x1771A5, 0x31494)])
    voice = "M10 voice playback: sound/voice/oblivion.esm/breton/m/mq07_"
    verify_events(args.dialogue_log, [
        f"result=teleport ref={form(0xAFE)}", voice + "greeting_0002006b_1.mp3",
        f"actor={JAUFFRE} info={form(0x2006B)} choices=1", f"setstage quest={SPIES} stage=90",
        f"addtopic topic={form(0x3B3D8)}", f"setquestobject base={form(0xC026)} flag=0",
        voice + "mq07progressplans_00020085_1.mp3", f"actor={JAUFFRE} info={form(0x20085)}",
        voice + "mq07progressplans_00020085_2.mp3", voice + "mq07progressplans_00020085_3.mp3",
    ])
    print("Temple journey, Jauffre's voiced report, both journal entries, and stock reload verified")
