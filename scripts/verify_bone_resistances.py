"""Audit original Canosel Undead Bone Resistances ownership courses."""
import argparse
import hashlib
import json
from pathlib import Path
import tes4_runtime_state as runtime

ACTOR = "content:oblivion.esm:06c2e4"
SPELL = "content:oblivion.esm:02b532"
EFFECTS = [("RSDI", 63, 100), ("RSPO", 67, 100), ("RSFR", 62, 70),
           ("WABR", 55, 1), ("RSPA", 66, 100)]

def read(path):
    return json.loads(path.read_text())

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def actor(state):
    matches = [entry for entry in state["native_actor_values"] if entry["actor"] == ACTOR]
    assert len(matches) == 1
    assert matches[0]["base"] == "content:oblivion.esm:024ddb"
    return matches[0]

def check_ability(state):
    values = actor(state)
    assert values["passive_abilities"] == [{"spell": SPELL, "spell_type": 4, "effects": [
        [i, int.from_bytes(code.encode("ascii"), "little"), av, magnitude, magnitude]
        for i, (code, av, magnitude) in enumerate(EFFECTS)]}]
    for code, av, magnitude in EFFECTS:
        assert values["values"][av] == [0, magnitude, None, None], (code, values["values"][av])
    return values["passive_abilities"]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("fresh", "restart", "legacy", "continuation"))
    parser.add_argument("--sanitized", action="store_true")
    args = parser.parse_args(); course = args.course
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
    # Schema96 added nullable inventory-script ownership. Preserve every item
    # field while representing the genuine older source's absence explicitly.
    inventory = [{**item, "script_owner": item.get("script_owner")}
                 for item in before["player"]["inventory"]]
    assert inventory == after["player"]["inventory"]
    assert after["player"]["actor_values"]["health.current"] > 0
    events = [json.loads(line) for line in (course / "events.jsonl").read_text().splitlines()]
    loads = [event for event in events if event["event"] == "load-complete"]
    saves = [event for event in events if event["event"] == "save-complete"]
    assert len(loads) == len(saves) == 1
    epoch = read(course / "process-epoch.json")
    assert loads[0]["pid"] == saves[0]["pid"] == epoch["pid"]
    assert loads[0]["save_sha256"] == sha(source) and saves[0]["save_sha256"] == sha(target)
    assert read(course / saves[0]["live"]) == after
    loaded = read(course / loads[0]["live"])
    if args.phase == "fresh":
        assert not any(entry["actor"] == ACTOR for entry in before["native_actor_values"])
        assert not any(entry["actor"] == ACTOR for entry in loaded["native_actor_values"])
        check_ability(after)
    elif args.phase in ("restart", "continuation"):
        assert check_ability(before) == check_ability(loaded) == check_ability(after)
    else:
        for state in (before, loaded, after):
            values = actor(state)
            assert values["passive_abilities"] is None
            for _, av, _ in EFFECTS:
                assert values["values"][av] == [0, None, None, None]
        assert before["player"]["actor_values"]["health.current"] == after["player"]["actor_values"]["health.current"]
    if args.sanitized:
        env = read(course / "observed-sanitizer-environment.json")["environment"]
        assert "halt_on_error=1" in env["ASAN_OPTIONS"] and "halt_on_error=1" in env["UBSAN_OPTIONS"]
        libraries = read(course / "loaded-libraries.json")
        assert any("libasan" in path for path in libraries) and any("libubsan" in path for path in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  source_sha256=sha(source), save_sha256=sha(target), binary_sha256=epoch["exe_sha256"],
                  actor=ACTOR, ability=actor(after)["passive_abilities"],
                  player_health=after["player"]["actor_values"]["health.current"],
                  scope="Actual original innate grant/ownership and cold-load state; no hostile Paralysis cast certified.")
    (course / "bone-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))

if __name__ == "__main__":
    main()
