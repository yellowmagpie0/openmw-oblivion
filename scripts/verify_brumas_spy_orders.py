"""Check Burd's authorization, the basement search, and a stock resave."""

import argparse
from pathlib import Path

from verify_lake_arrius_passage import count, form, load_save, require
from verify_spies_briefing import SPIES


BURD, BARRACKS, BASEMENT, HATCH, ORDERS = map(form, (0x36264, 0x2DB9A, 0x1E6A3, 0x1E6EA, 0x6719D))


def verify(path, stage):
    state = load_save(path)
    player = state["player"]
    quest = next(row for row in state["quests"] if row["quest"] == SPIES)
    refs = {r["key"]: r for r in state["references"]}
    require(quest["running"] and quest["stage"] == stage, f"{path}: unexpected Spies stage")
    require(count(player, 0x2007F) == 1 and count(player, 0x35E50) >= 1, f"{path}: missing keys")
    require(count(player, 0xC026) == int(stage == 80), f"{path}: unexpected number of copies of Jearl's Orders")
    rep = next(a["reputation"] for a in state["native_actor_values"] if a["actor"] == player["reference"])
    require(rep["bounty_accumulator"] == 0, f"{path}: bounty during authorized search")
    if stage >= 70:
        require({30, 31, 50, 70} <= set(quest["completed_stages"])
                and refs[BURD]["custom_state"].get("native.dialogue.talked_to_player"),
                f"{path}: missing spy deaths or Burd's authorization")
    if stage == 70:
        require(player["cell"] == BARRACKS, f"{path}: missing barracks conversation")
    if stage == 80:
        require(player["cell"] == BASEMENT and refs[HATCH]["lock_level"] <= 0,
                f"{path}: basement inaccessible")
        require(refs[ORDERS]["deleted"] and refs[ORDERS]["custom_state"].get("read")
                and refs[ORDERS]["custom_state"]["count"] == 0, f"{path}: missing read or duplicate world scroll")
        journal = next(step for step in quest["journal"] if step["stage"] == 80)
        require(journal["entries"] and not journal["unresolved"], f"{path}: missing lead to Jauffre")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("runestone", "authorized", "orders", "restart"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    for path, stage in ((args.runestone, 45), (args.authorized, 70), (args.orders, 80), (args.restart, 80)):
        verify(path, stage)
    print("Burd's authorization, the orders, and fresh-process persistence verified")
