"""Audit real taken/untaken Vilverin claymore state; inspect the HUD image separately."""

import argparse
import hashlib
import json
import re
from pathlib import Path

import tes4_runtime_state as runtime

REFERENCE = "content:oblivion.esm:0c56f9"
WEAPON = "content:oblivion.esm:01c6cd"
PLAYER = "dynamic:player:0000000000000001"


def read(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("baseline", "taken", "untaken", "restart"))
    parser.add_argument("--sanitized", action="store_true")
    args = parser.parse_args()
    course = args.course
    driver = read(course / "driver-process.json")
    assert driver["exit_code"] == 0 and driver["captured"]
    assert driver["input_unchanged"] and driver["manifest_input_unchanged"]
    assert read(course / "scenario.json")["passed"]
    source = Path(driver["input_save"])
    save = course / "userdata/saves/Bendu_Olo/Quicksave.omwsave"
    assert sha(source) == driver["input_sha256"]
    before, after = runtime.load_save(source), runtime.load_save(save)
    assert after["schema_version"] == 101
    assert runtime.encode_payload(after) == runtime._find_runtime_record(save.read_bytes())[3]
    taken = args.phase != "untaken"
    for state in (before, after):
        reference = next(r for r in state["references"] if r["key"] == REFERENCE)
        assert reference["base"] == WEAPON and reference["deleted"] == taken
        assert reference["custom_state"]["count"] == (0 if taken else 1)
        if taken:
            assert reference["custom_state"]["taken"]
        owned = sum(i["count"] for i in state["player"]["inventory"] if i["base"] == WEAPON)
        assert owned == int(taken)
        life = next(a for a in state["native_actor_life"] if a["actor"] == PLAYER)
        assert life["phase"] == 0
    inventory = lambda state: sorted((i["base"], i["count"]) for i in state["player"]["inventory"])
    assert inventory(before) == inventory(after)
    log = (course / "process.log").read_text()
    samples = re.findall(r"FocusProbe sample count=(-?\d+) scene=(\d+) owned=(\d+) enabled=(\S+) valid=(\S+)", log)
    assert samples
    expected = ("-1", "0", "1", "unavailable", "false") if taken else ("1", "1", "0", "true", "true")
    assert all(sample == expected for sample in samples), samples
    assert "M5 interaction: kind=take result=taken" not in log
    assert "M15 melee hit committed: actor=" + PLAYER not in log
    events = [json.loads(line) for line in (course / "events.jsonl").read_text().splitlines()]
    loads = [event for event in events if event["event"] == "load-complete"]
    saves = [event for event in events if event["event"] == "save-complete"]
    epoch = read(course / "process-epoch.json")
    assert len(loads) == len(saves) == 1
    assert loads[0]["pid"] == saves[0]["pid"] == epoch["pid"]
    assert loads[0]["save_sha256"] == sha(source) and saves[0]["save_sha256"] == sha(save)
    assert read(course / saves[0]["live"]) == after
    if args.sanitized:
        env = read(course / "observed-sanitizer-environment.json")["environment"]
        assert "halt_on_error=1" in env["ASAN_OPTIONS"] and "halt_on_error=1" in env["UBSAN_OPTIONS"]
        libraries = read(course / "loaded-libraries.json")
        assert any("libasan" in name for name in libraries) and any("libubsan" in name for name in libraries)
    image = course / "original-claymore-world-focus.png"
    assert image.is_file()
    result = dict(
        state_audit_passed=True, phase=args.phase, sanitized=args.sanitized,
        source_sha256=sha(source), save_sha256=sha(save), binary_sha256=epoch["exe_sha256"],
        observation=dict(zip(("count", "scene", "owned", "enabled", "valid"), expected)),
        hud_image_sha256=sha(image), hud_requires_separate_review=True,
    )
    (course / "claymore-focus-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
