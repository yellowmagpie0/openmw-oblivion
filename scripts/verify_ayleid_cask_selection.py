"""Audit real Canosel cask interactions using their saved native inventory."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime


CASK = "content:oblivion.esm:04bb85"
BASE = "content:oblivion.esm:03767e"
CELL = "content:oblivion.esm:0463f5"
LISTS = {
    "014931", "0cb214", "032d7a", "032d6f", "06feba", "03e4c7", "06febb", "0a7541"
}


def read_json(path):
    return json.loads(path.read_text())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stock(items):
    counts = Counter()
    for item in items:
        assert item["count"] > 0
        counts[item["base"]] += item["count"]
    return counts


def stack_metadata(items, excluded):
    counts = Counter()
    for item in items:
        if item["base"] not in excluded:
            shape = {key: value for key, value in item.items() if key != "count"}
            counts[json.dumps(shape, sort_keys=True)] += item["count"]
    return counts


def cask(state):
    result = next(ref for ref in state["references"] if ref["key"] == CASK)
    assert result["base"] == BASE and result["cell"] == CELL
    assert result["enabled"] and not result["deleted"] and result["owner"] is None
    assert result["lock_level"] == 0 and not result["custom_state"]["locked"]
    return result


def audit(root, phase, entry):
    driver = read_json(root / "driver-process.json")
    source = Path(driver["input_save"])
    output = root / "userdata/saves/Bendu_Olo/Quicksave.omwsave"
    source_hash, output_hash = digest(source), digest(output)
    assert driver["captured"] and driver["input_unchanged"] and driver["manifest_input_unchanged"]
    assert driver["exit_code"] == 0 and source_hash == driver["input_sha256"]
    assert read_json(root / "scenario.json")["passed"]
    before, after = runtime.load_save(source), runtime.load_save(output)
    assert before["schema_version"] == after["schema_version"] == 98
    assert runtime.encode_payload(after) == runtime._find_runtime_record(output.read_bytes())[3]

    events = [json.loads(line) for line in (root / "events.jsonl").read_text().splitlines()]
    loaded = next(event for event in events if event["event"] == "load-complete")
    saved = [event for event in events if event["event"] == "save-complete"][-1]
    epoch = read_json(root / "process-epoch.json")
    assert loaded["pid"] == saved["pid"] == epoch["pid"]
    assert loaded["save_sha256"] == source_hash and saved["save_sha256"] == output_hash
    assert read_json(root / saved["live"]) == after
    loaded_state = read_json(root / loaded["live"])
    assert cask(loaded_state) == cask(before)
    assert loaded_state["player"]["inventory"] == before["player"]["inventory"]

    old, new = cask(before), cask(after)
    old_items, new_items = stock(old["inventory"]), stock(new["inventory"])
    player_before, player_after = before["player"]["inventory"], after["player"]["inventory"]
    player_old, player_new = stock(player_before), stock(player_after)
    added, removed = player_new - player_old, player_old - player_new
    log = (root / "process.log").read_text()
    opened = f"kind=loot result=opened ref={CASK} "
    looted = f"kind=loot result=looted ref={CASK} "
    resolution = f"M13 container lists resolved: ref={CASK} "
    authored = Counter({f"content:oblivion.esm:{form}": 1 for form in LISTS})
    assert old["position"] == new["position"]
    assert after["native_death_counts"] == before["native_death_counts"]
    assert after["next_dynamic_serial"] == before["next_dynamic_serial"]
    health = after["player"]["actor_values"]["health.current"]
    assert health == before["player"]["actor_values"]["health.current"] and health > 0
    assert new["custom_state"]["opened"]

    if phase == "baseline":
        assert old_items == authored and not new_items
        assert added and not removed and log.count(looted) == 1
    elif phase == "open-close":
        assert old_items == authored and new_items and not (authored & new_items)
        assert player_before == player_after and log.count(opened) == 1
        assert log.count(resolution) == 1 and looted not in log
    else:
        assert old["custom_state"]["opened"] and not (authored & (old_items + new_items))
        assert opened in log and looted not in log and resolution not in log
        assert old["custom_state"] == new["custom_state"]
        if phase == "select":
            assert entry and old_items[entry] > 0
            assert added == Counter({entry: old_items[entry]}) and not removed
            assert new_items == old_items - added and new_items
        elif phase == "deposit":
            assert entry and removed == Counter({entry: 1}) and not added
            assert new_items == old_items + removed
        elif phase == "take-all":
            assert old_items and added == old_items and not removed and not new_items
        elif phase == "empty":
            assert not old_items and not new_items and player_before == player_after
        elif phase == "reload":
            loads = [event for event in events if event["event"] == "load-complete"]
            saves = [event for event in events if event["event"] == "save-complete"]
            assert len(loads) == len(saves) == 2
            assert loads[0]["sequence"] < saves[0]["sequence"] < loads[1]["sequence"] < saves[1]["sequence"]
            assert loads[1]["pid"] == saves[0]["pid"] == epoch["pid"]
            assert loads[1]["save_sha256"] == saves[0]["save_sha256"]
            for event in (saves[0], loads[1]):
                observation = read_json(root / event["live"])
                assert cask(observation) == old and observation["player"]["inventory"] == player_before
            assert old_items and old_items == new_items and player_before == player_after
            assert log.count(opened) == 3 and "VilverinContinue same process totals 1/30/1" in log
        assert player_old + old_items == player_new + new_items
        assert stack_metadata(old["inventory"], set(added) | set(removed)) == stack_metadata(
            new["inventory"], set(added) | set(removed)
        )
    changed = set(added) | set(removed)
    assert stack_metadata(player_before, changed) == stack_metadata(player_after, changed)
    return dict(
        passed=True, phase=phase, source_sha256=source_hash, save_sha256=output_hash,
        exe_sha256=epoch["exe_sha256"], process=epoch["pid"], health=health,
        cask_before=dict(old_items), cask_after=dict(new_items),
        player_added=dict(added), player_removed=dict(removed),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("baseline", "open-close", "deposit", "select", "take-all", "empty", "reload"))
    parser.add_argument("--entry", help="Native FormKey of the selected entry")
    args = parser.parse_args()
    result = audit(args.course, args.phase, args.entry)
    (args.course / "audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
