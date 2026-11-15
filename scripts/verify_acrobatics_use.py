"""Audit a completed real Vilverin entrance jump and native Acrobatics persistence."""

import argparse
import hashlib
import json
import math
import re
from pathlib import Path

import tes4_runtime_state as runtime
from verify_blade_use import shared

PLAYER = "dynamic:player:0000000000000001"


def read(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("baseline", "jump", "idle", "restart"))
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
    assert b[20]["base"] == a[20]["base"] == 5
    assert all(b[i] == a[i] for i in range(27) if i != 20)
    actor = lambda state: next(v for v in state["native_actor_values"] if v["actor"] == PLAYER)
    bp, ap = actor(before), actor(after)
    assert bp["values"][26] == ap["values"][26] == [5., 0., 0., 0.]
    assert bp["passive_abilities"] == ap["passive_abilities"]
    for state in (before, after):
        assert next(v for v in state["native_actor_life"] if v["actor"] == PLAYER)["phase"] == 0
        assert math.isclose(sum(v or 0 for v in actor(state)["values"][8]), 110, abs_tol=.01)
    items = lambda state: sorted((i["base"], i["count"], str(i.get("script_owner")))
                                 for i in state["player"]["inventory"])
    assert items(before) == items(after)
    log = (course / "process.log").read_text()
    assert log.count("Writing saved game") == 1
    pre = log.split("Writing saved game", 1)[0]
    jumps = re.findall(r"M12 player movement: mode=jump input=keyboard-mouse vertical=([\d.eE+-]+) fatigue=([\d.eE+-]+)->([\d.eE+-]+)", pre)
    uses = re.findall(r"M16 Acrobatics jump use: experience=([\d.eE+-]+)", pre)
    samples = re.findall(r"AcroProbe sample ground=(\w+) z=([\d.eE+-]+) floor=([\d.eE+-]+) peak=([\d.eE+-]+)", pre)
    assert samples
    height = max(float(s[3]) - float(s[2]) for s in samples)
    if args.phase in ("baseline", "jump"):
        assert len(jumps) == 1 and "AcroProbe airborne" in pre and "AcroProbe landed" in pre
        # The legacy vertical field is the request value. Actual rise and
        # grounded landing are observed independently from the live world.
        assert float(jumps[0][0]) > 0 and float(jumps[0][1]) > float(jumps[0][2])
        assert height > 5 and samples[-1][0] == "true"
    else:
        assert not jumps and not uses and height < 1
    expected = b[20]["experience"] + (.3 if args.phase == "jump" else 0)
    assert math.isclose(a[20]["experience"], expected, abs_tol=.00001), (b[20], a[20])
    if args.phase == "jump":
        assert len(uses) == 1 and math.isclose(float(uses[0]), a[20]["experience"], abs_tol=.00001)
    else:
        assert not uses
    assert "M16 Athletics use:" not in pre
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
                  acrobatics_before=b[20], acrobatics_after=a[20], athletics_retained=a[8],
                  jump_count=len(jumps), observed_rise=height,
                  fatigue_debits=[float(j[1])-float(j[2]) for j in jumps])
    (course / "acrobatics-use-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
