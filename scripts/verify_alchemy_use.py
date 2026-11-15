"""Audit earned Bonemeal Alchemy use, real food exclusion and cold persistence."""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blade_use import shared

PLAYER = "dynamic:player:0000000000000001"
BONE = "content:oblivion.esm:01ebff"
FOOD = "content:oblivion.esm:033681"


def read(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("baseline", "bonemeal", "food", "restart"))
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
    b, a = shared(source), shared(save)
    assert after["schema_version"] == 101
    assert runtime.encode_payload(after) == runtime._find_runtime_record(save.read_bytes())[3]
    assert b[16]["base"] == a[16]["base"] == 5
    assert all(b[i] == a[i] for i in range(27) if i != 16)
    expected = b[16]["experience"] + (.5 if args.phase == "bonemeal" else 0)
    assert math.isclose(a[16]["experience"], expected, abs_tol=.00001), (b[16], a[16])
    actor = lambda state: next(v for v in state["native_actor_values"] if v["actor"] == PLAYER)
    bp, ap = actor(before), actor(after)
    assert bp["values"][19] == ap["values"][19] == [5., 0., 0., 0.]
    assert bp["passive_abilities"] == ap["passive_abilities"]
    assert bp["values"][:10] == ap["values"][:10] and bp["values"][11:] == ap["values"][11:]
    for state in (before, after):
        assert next(v for v in state["native_actor_life"] if v["actor"] == PLAYER)["phase"] == 0
        assert math.isclose(sum(v or 0 for v in actor(state)["values"][8]), 110, abs_tol=.01)
    consumed = None if args.phase == "restart" else FOOD if args.phase == "food" else BONE
    items = lambda state, omit: sorted(json.dumps(i, sort_keys=True) for i in state["player"]["inventory"]
                                       if i["count"] and i["base"] != omit)
    assert items(before, consumed) == items(after, consumed)
    count = lambda state, base: sum(i["count"] for i in state["player"]["inventory"] if i["base"] == base)
    if consumed:
        assert count(before, consumed) == 1 and count(after, consumed) == 0
    assert count(before, BONE) == count(after, BONE) + (consumed == BONE)
    assert count(before, FOOD) == count(after, FOOD) + (consumed == FOOD)
    log = (course / "process.log").read_text()
    assert log.count("Writing saved game") == 1
    pre = log.split("Writing saved game", 1)[0]
    uses = re.findall(r"M16 Alchemy ingredient use: ingredient=(\S+)", pre)
    eaten = re.findall(r"TES4 ingredient consumed: ingredient=(\S+) magnitude=([\d.eE+-]+) duration=([\d.eE+-]+)", pre)
    assert uses == ([BONE] if args.phase == "bonemeal" else [])
    assert eaten == ([] if not consumed else [(consumed, "5", "1")] if consumed == FOOD
                     else [(consumed, "-1", "3")])
    samples = re.findall(r"ingredient sample count=(\d+) rice=1 fatigue=([\d.eE+-]+) health=110 alchemy=([\d.eE+-]+)", pre)
    assert samples
    # Lua progress is normalized; raw PLAY and the process-bound native export
    # independently establish the exact persisted experience below.
    assert (float(samples[-1][2]) > 0) == (a[16]["experience"] > 0)
    # Native effects retain their existing ownership and timing; this chunk
    # changes skill publication only. A completed cold load may finish a timer.
    effects = ap.get("ingredient_restoration", [])
    if args.phase == "restart":
        assert not effects and "Alchemy effect window completed" in pre
    elif consumed:
        assert len(effects) == 1
        effect = effects[0]
        assert effect["ingredient"] == consumed and 0 <= effect["elapsed"] < effect.get("duration", 1.)
        assert (effect.get("magnitude", 5.), effect.get("duration", 1.)) == ((5., 1.) if consumed == FOOD else (-1., 3.))
    assert after["player"]["actor_values"].get("skill_advances.19", 0) == 0
    events = [json.loads(line) for line in (course / "events.jsonl").read_text().splitlines()]
    loads = [e for e in events if e["event"] == "load-complete"]
    saves = [e for e in events if e["event"] == "save-complete"]
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
    result = dict(passed=True, phase=args.phase, sanitized=args.sanitized,
                  source_sha256=sha(source), save_sha256=sha(save), binary_sha256=epoch["exe_sha256"],
                  alchemy_before=b[16], alchemy_after=a[16], athletics_retained=a[8], acrobatics_retained=a[20],
                  consumed=eaten, uses=uses, remaining_effects=effects)
    (course / "alchemy-use-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
