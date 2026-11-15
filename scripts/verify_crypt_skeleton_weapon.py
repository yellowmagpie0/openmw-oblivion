"""Audit original Canosel Skeleton06c2e4 owned-melee-weapon courses."""

import argparse
import hashlib
import json
import re
from pathlib import Path

import tes4_runtime_state as runtime

SKELETON = "content:oblivion.esm:06c2e4"
BASE = "content:oblivion.esm:024ddb"
AXE = "content:oblivion.esm:000d81"
RIGHT_HAND = 1 << 16


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def actor(state):
    matches = [ref for ref in state["references"] if ref["key"] == SKELETON]
    assert len(matches) == 1 and matches[0]["base"] == BASE
    return matches[0]


def axe(state):
    matches = [item for item in actor(state)["inventory"] if item["base"] == AXE]
    assert len(matches) == 1 and matches[0]["count"] == 1
    return matches[0]


def without_equipment_and_wear(item):
    return {key: value for key, value in item.items() if key not in ("equipped_slots", "condition")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("baseline", "equip", "restart"))
    parser.add_argument("--sanitized", action="store_true")
    args = parser.parse_args()
    course = args.course
    driver = read(course / "driver-process.json")
    assert driver["exit_code"] == 0 and driver["captured"]
    assert driver["input_unchanged"] and driver["manifest_input_unchanged"]
    assert read(course / "scenario.json")["passed"]
    source = Path(driver["input_save"])
    target = course / "userdata/saves/Bendu_Olo/Quicksave.omwsave"
    assert sha(source) == driver["input_sha256"]
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after["schema_version"] == 99
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    assert before["player"]["inventory"] == after["player"]["inventory"]
    assert after["player"]["actor_values"]["health.current"] > 0
    b, a = axe(before), axe(after)
    assert without_equipment_and_wear(b) == without_equipment_and_wear(a)
    assert 0 <= a["condition"] <= b["condition"]
    unrelated = lambda state: [item for item in actor(state)["inventory"] if item["base"] != AXE]
    assert unrelated(before) == unrelated(after)
    log = (course / "process.log").read_text()
    equip_count = log.count("M15 combat melee weapon equipped: actor=" + SKELETON)
    armed_attack = re.search(r"CryptSkeleton active lower=onehand\w*attack\w*", log)
    if args.phase == "baseline":
        assert b["equipped_slots"] == a["equipped_slots"] == 0
        assert equip_count == 0 and "active lower=handtohandattack" in log
    else:
        assert a["equipped_slots"] == RIGHT_HAND and armed_attack
        if args.phase == "equip":
            assert b["equipped_slots"] == 0 and equip_count == 1
        else:
            assert b["equipped_slots"] == RIGHT_HAND and equip_count == 0
    events = [json.loads(line) for line in (course / "events.jsonl").read_text().splitlines()]
    loads = [event for event in events if event["event"] == "load-complete"]
    saves = [event for event in events if event["event"] == "save-complete"]
    assert len(loads) == len(saves) == 1
    epoch = read(course / "process-epoch.json")
    assert loads[0]["pid"] == saves[0]["pid"] == epoch["pid"]
    assert loads[0]["save_sha256"] == sha(source) and saves[0]["save_sha256"] == sha(target)
    assert read(course / saves[0]["live"]) == after
    loaded = read(course / loads[0]["live"])
    assert axe(loaded)["condition"] == b["condition"]
    if args.phase == "restart":
        assert axe(loaded)["equipped_slots"] == RIGHT_HAND
    life = [entry for entry in after["native_actor_life"] if entry["actor"] == SKELETON]
    assert len(life) == 1 and life[0]["phase"] == 0
    if args.sanitized:
        env = read(course / "observed-sanitizer-environment.json")["environment"]
        assert "halt_on_error=1" in env["ASAN_OPTIONS"]
        assert "halt_on_error=1" in env["UBSAN_OPTIONS"]
        libraries = read(course / "loaded-libraries.json")
        assert any("libasan" in path for path in libraries)
        assert any("libubsan" in path for path in libraries)
    result = {
        "passed": True,
        "phase": args.phase,
        "sanitized": args.sanitized,
        "input_sha256": sha(source),
        "save_sha256": sha(target),
        "binary_sha256": epoch["exe_sha256"],
        "actor": SKELETON,
        "weapon": AXE,
        "before_axe": b,
        "loaded_axe": axe(loaded),
        "after_axe": a,
        "auto_equip_count": equip_count,
        "armed_attack_observed": bool(armed_attack),
        "player_health_before": before["player"]["actor_values"]["health.current"],
        "player_health_after": after["player"]["actor_values"]["health.current"],
        "scope": "Equipment/animation/owned-save course; intrinsic-resistance/contact-damage support is separate.",
    }
    (course / "weapon-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
