"""Audit real Canosel Gate049bbd input, inventory, lock and restart courses."""

import argparse
import hashlib
import json
import math
from pathlib import Path

import tes4_runtime_state as runtime


GATE = "content:oblivion.esm:049bbd"
BASE = "content:oblivion.esm:0228c8"
PICK = "content:oblivion.esm:00000a"


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def gate(state):
    matches = [ref for ref in state["references"] if ref["key"] == GATE]
    assert len(matches) == 1 and matches[0]["base"] == BASE
    return matches[0]


def picks(state):
    return sum(item["count"] for item in state["player"]["inventory"] if item["base"] == PICK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("baseline", "cancel", "attempt", "passage"))
    parser.add_argument("--sanitized", action="store_true")
    parser.add_argument("--require-failure", action="store_true")
    args = parser.parse_args()
    course = args.course.resolve()
    driver = read(course / "driver-process.json")
    assert driver["exit_code"] == 0 and driver["captured"]
    assert driver["input_unchanged"] and driver["manifest_input_unchanged"]
    scenario = read(course / "scenario.json")
    assert scenario["passed"] and not scenario["timed_out"]
    source = Path(driver["input_save"])
    target = course / "userdata/saves/Bendu_Olo/Quicksave.omwsave"
    source_hash, target_hash = sha(source), sha(target)
    assert source_hash == driver["input_sha256"]
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after["schema_version"] == 99
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    events = [json.loads(line) for line in (course / "events.jsonl").read_text().splitlines()]
    loads = [event for event in events if event["event"] == "load-complete"]
    saves = [event for event in events if event["event"] == "save-complete"]
    assert len(loads) == len(saves) == 1
    load, save = loads[0], saves[0]
    epoch = read(course / "process-epoch.json")
    assert load["pid"] == save["pid"] == epoch["pid"]
    assert load["save_sha256"] == source_hash and save["save_sha256"] == target_hash
    assert read(course / save["live"]) == after
    loaded = read(course / load["live"])
    assert gate(loaded) == gate(before)
    assert loaded["player"]["inventory"] == before["player"]["inventory"]
    assert before["player"]["cell"] == after["player"]["cell"] == "content:oblivion.esm:0463f5"
    health = before["player"]["actor_values"]["health.current"]
    assert health > 0 and health == after["player"]["actor_values"]["health.current"]
    unrelated = lambda state: [item for item in state["player"]["inventory"] if item["base"] != PICK]
    assert unrelated(before) == unrelated(after)
    b, a = gate(before), gate(after)
    log = (course / "process.log").read_text()
    token = lambda result: f"kind=door result={result} ref={GATE}"
    failed = log.count(token("lockpick_failed"))
    prompts = log.count(token("lockpick_prompt"))
    cancelled = log.count(token("lockpick_cancelled"))
    animated = f"M5 door activation: result=animated ref={GATE}"
    if args.phase in ("baseline", "cancel"):
        assert b["lock_level"] == a["lock_level"] == 7
        assert b["custom_state"]["locked"] and a["custom_state"]["locked"]
        assert before["player"]["inventory"] == after["player"]["inventory"]
        assert picks(before) == picks(after) == 7
        assert failed == 0 and animated not in log
        assert "lockpick_auto_attempts" not in a["custom_state"]
        if args.phase == "baseline":
            assert a == b and prompts == cancelled == 0
            assert log.count(f"M5 door activation: result=locked ref={GATE}") == 1
        else:
            assert prompts == cancelled == 1
            expected = dict(b, custom_state=dict(b["custom_state"], lock_checked=True))
            assert a == expected
    elif args.phase == "attempt":
        assert b["lock_level"] == 7 and b["custom_state"]["locked"]
        assert a["lock_level"] == 0 and a["custom_state"]["locked"] is False
        assert a["custom_state"]["unlocked_with_lockpick"]
        assert prompts == failed + 1 and cancelled == 0 and log.count(animated) == 1
        attempts = a["custom_state"]["lockpick_auto_attempts"] - b["custom_state"].get("lockpick_auto_attempts", 0)
        assert attempts == failed + 1
        assert picks(before) == 7 and picks(after) == 7 - failed
        chance = a["custom_state"]["lockpick_auto_chance"]
        assert math.isfinite(chance) and 0 <= chance <= 100
        assert a["custom_state"]["obscript.animation_group"] == "open"
        assert a["custom_state"]["obscript.animation_progress"] == 1
        if args.require_failure:
            assert failed > 0
        if failed:
            assert a["custom_state"]["lockpick_broken"]
            capture = read(course / "failed-prompt-capture.json")
            assert capture["exit_code"] == 0 and capture["process"] == epoch
            assert (course / "paused-gate-after-failed-attempt.png").is_file()
    else:
        assert b == a and a["lock_level"] == 0 and a["custom_state"]["locked"] is False
        assert a["custom_state"]["obscript.animation_group"] == "open"
        assert a["custom_state"]["obscript.animation_progress"] == 1
        assert before["player"]["inventory"] == after["player"]["inventory"]
        assert failed == prompts == cancelled == 0 and animated not in log
        assert "VilverinContinue gate passage reached" in log
        initial, final = before["player"]["position"], after["player"]["position"]
        assert initial[1] > -10496 and final[1] < -10532
        assert abs(final[0] + 5312) < 55 and abs(final[2] - initial[2]) < 2
    if args.sanitized:
        observed = read(course / "observed-sanitizer-environment.json")
        assert observed["process"] == epoch
        assert observed["environment"]["ASAN_OPTIONS"] == "detect_leaks=0:halt_on_error=1"
        assert observed["environment"]["UBSAN_OPTIONS"] == "halt_on_error=1:print_stacktrace=1"
        libraries = read(course / "loaded-libraries.json")
        assert any(Path(path).name.startswith("libasan.") for path in libraries)
        assert any(Path(path).name.startswith("libubsan.") for path in libraries)
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized, process=epoch,
                  source_sha256=source_hash, save_sha256=target_hash,
                  gate_before=b, gate_after=a, picks_before=picks(before), picks_after=picks(after),
                  failed_attempts=failed, health=health,
                  position_before=before["player"]["position"], position_after=after["player"]["position"],
                  verifier_sha256=sha(__file__))
    (course / "lock-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
