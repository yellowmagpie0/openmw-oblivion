"""Check the Sigil Keeper, key transfer, chamber door and Bruma Gate journal transition."""

import argparse
from pathlib import Path

from verify_bruma_gate_tower import BRUMA_GATE, PARTY, TOWER, check_party, form, require
from verify_bruma_upper_tower import check_landing


KEEPER = form(0x9504B)
SIGIL_KEY = form(0x9504C)
DOOR = form(0x5622A)
CHAMBER = form(0x5619A)


def key_count(inventory):
    return sum(item["count"] for item in inventory if item["base"] == SIGIL_KEY)


def verify(source, key_taken, log, chamber, reload_save, reload_log):
    for path, cell, stage, player_has_key in (
        (source, TOWER, 35, False),
        (key_taken, TOWER, 35, True),
        (chamber, CHAMBER, 40, True),
        (reload_save, CHAMBER, 40, True),
    ):
        state = check_party(path, cell, stage)
        refs = {ref["key"]: ref for ref in state["references"]}
        life = next(a for a in state["native_actor_life"] if a["actor"] == KEEPER)
        require(life["phase"] == int(player_has_key), f"{path}: unexpected Sigil Keeper life state")
        require(key_count(refs[KEEPER]["inventory"]) == int(not player_has_key)
                and key_count(state["player"]["inventory"]) == int(player_has_key),
                f"{path}: missing or duplicate Sigil Key")
        require(refs[DOOR]["custom_state"]["locked"] == (cell == TOWER), f"{path}: wrong chamber door lock state")
        if cell == CHAMBER:
            check_landing(state, path, CHAMBER, (-1131, 422, -1056), 450, defenders_dead=True)

    events = log.read_text(encoding="utf-8", errors="replace")
    crossing = f"result=teleport ref={DOOR} destination=FormId:0x105619a"
    before, door, after = events.partition(crossing)
    require(door and f"setstage quest={BRUMA_GATE} stage=40" in after, f"{log}: missing chamber transition")
    for guard in PARTY:
        require(f"NPC Player defense: actor={guard} target={KEEPER}" in before, f"{log}: missing guard defense")
    climate = "M10 climate ClimateSigil:"
    restored = reload_log.read_text(encoding="utf-8", errors="replace")
    require(climate in after and climate in restored, "The chamber climate must be applied on entry and reload")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "key_taken", "log", "chamber", "reload_save", "reload_log"):
        parser.add_argument(name, type=Path)
    verify(**vars(parser.parse_args()))
    print("Verified the Sigil Key, door unlock, stage 40 and the party inside the chamber after reload.")
