"""Check Jauffre's briefing, Burd's two choices and the playable gate checkpoint."""

import argparse
import math
from pathlib import Path

from verify_martin_artifact import AZURA, BRUMA_GATE, DAEDRA, HALL, STAR, count, fame, form, load_save, require, verify_events


BURD, GATE, GATE_PARENT = map(form, (0x36264, 0x26EBA, 0x613C3))
GUARDS = tuple(map(form, (0x3A841, 0x3A842, 0x4631B, 0x46324, 0x46390)))


def verify_save(state, path, stage, source_crime, source_fame):
    quests = {q["quest"]: q for q in state["quests"]}
    quest = quests[BRUMA_GATE]
    expected = [s for s in (10, 20, 30) if s <= stage]
    require(quest["running"] and quest["stage"] == stage and quest["completed_stages"] == expected
            and quest["journal"] == [{"stage": s, "entries": [0], "unresolved": False} for s in expected],
            f"{path}: incorrect Bruma Gate journal")
    player = state["player"]
    require(quests[DAEDRA]["stage"] == quests[AZURA]["stage"] == 100 and count(player, STAR) == 0
            and player["actor_values"]["health.current"] > 0 and fame(state) == source_fame
            and state["native_crime"] == source_crime, f"{path}: unexpected player or quest state")
    if stage <= 20:
        require(player["cell"] == HALL, f"{path}: expected the Great Hall")
    if stage >= 20:
        refs = {r["key"]: r for r in state["references"]}
        require(all(refs[key]["enabled"] and not refs[key]["deleted"] for key in (GATE_PARENT, BURD, *GUARDS)),
                f"{path}: missing gate or guard detail")
        require(form(0x26EFB) in state["spoken_dialogue_infos"], f"{path}: Jauffre did not give directions")
    if stage == 30:
        require(all(entry["phase"] == 0 for entry in state["native_actor_life"]
                    if entry["actor"] in (BURD, *GUARDS)), f"{path}: a member of the guard detail is down")
        require(refs[GATE]["enabled"] and not refs[GATE]["deleted"]
                and math.dist(player["position"][:3], refs[GATE]["position"][:3]) < 5000
                and min(math.dist(player["position"][:3], refs[g]["position"][:3]) for g in GUARDS) < 900,
                f"{path}: player did not reach the gate approach")
        require({form(0x26EE9), form(0x3A854)} <= set(state["spoken_dialogue_infos"]),
                f"{path}: missing Burd's greeting or decision to wait")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "assigned", "gate", "reloaded", "route_log"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    source = load_save(args.source)
    crime, reputation = source["native_crime"], fame(source)
    verify_save(source, args.source, 10, crime, reputation)
    del source
    for path, stage in ((args.assigned, 20), (args.gate, 30), (args.reloaded, 30)):
        verify_save(load_save(path), path, stage, crime, reputation)
    verify_events(args.route_log, [
        "01cb98 info=" + form(0x26EA7) + " choices=1", "_00026ea7_4.mp3",
        f"setstage quest={BRUMA_GATE} stage=20", f"enable ref={GATE_PARENT}",
        "_00026efb_1.mp3", f"result=teleport ref={form(0x31468)}",
        "Arrius arrived point 13391 160255", "Arrius arrived point 28373 152953",
        f"setstage quest={BRUMA_GATE} stage=30", f"actor={BURD} info={form(0x26EE9)} choices=2",
        "_00026ee9_2.mp3", f"actor={BURD} info={form(0x3A854)} choices=0 goodbye=1",
        f"setweather weather={form(0x836D5)} override=true applied=true", "Arrius arrived point 30120 145740",
        f"actor={BURD} info={form(0x3A844)} choices=2",
    ])
    verify_events(args.route_log, [f"ref={GATE} group=specialidle played=true"])
    require(not any("ObScript diagnostic:" in line and form(0x981BF) in line
                    for line in args.route_log.read_text(encoding="utf-8", errors="replace").splitlines()),
            "The gate script stopped during the approach")
    print("Verified the briefing, road descent, guard detail, Burd's greeting and cold reload at the gate.")
