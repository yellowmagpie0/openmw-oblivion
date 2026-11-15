"""Audit native Athletics on actual shore-to-Vilverin travel and grounded controls."""

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
    parser.add_argument("phase", choices=("baseline", "run", "idle", "walk", "restart"))
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
    assert b[8]["base"] == a[8]["base"] == 30
    assert all(b[i] == a[i] for i in range(27) if i != 8)
    actor = lambda state: next(v for v in state["native_actor_values"] if v["actor"] == PLAYER)
    bp, ap = actor(before), actor(after)
    assert bp["values"][13] == ap["values"][13] == [30., 0., 0., 0.]
    assert bp["passive_abilities"] == ap["passive_abilities"]
    for state in (before, after):
        life = next(v for v in state["native_actor_life"] if v["actor"] == PLAYER)
        assert life["phase"] == 0
        values = actor(state)["values"][8]
        assert sum(v or 0 for v in values) > 0
    items = lambda state: sorted((i["base"], i["count"], str(i.get("script_owner")))
                                 for i in state["player"]["inventory"])
    assert items(before) == items(after)
    assert before["player"]["actor_values"]["pc_misc_stat.27"] == after["player"]["actor_values"]["pc_misc_stat.27"]
    log = (course / "process.log").read_text()
    assert log.count("Writing saved game") == 1
    pre = log.split("Writing saved game", 1)[0]
    samples = re.findall(r"Athletics sample run=(\w+) swim=(\w+) rt=([\d.eE+-]+) st=([\d.eE+-]+) forward=([\d.eE+-]+)", pre)
    assert samples
    uses = re.findall(r"M16 Athletics use: usage=(\d+) seconds=([\d.eE+-]+) experience=([\d.eE+-]+)", pre)
    seconds = [0., 0.]
    for usage, elapsed, _ in uses:
        assert usage in ("0", "1") and math.isfinite(float(elapsed)) and float(elapsed) > 0
        seconds[int(usage)] += float(elapsed)
    if args.phase in ("baseline", "run"):
        assert re.search(r"(?:VilverinSack|AthleticsRoute) arrived missing=0", pre)
        position = after["player"]["position"]
        assert math.dist(position[:3], [52084.32421875, 87443.8125, 332.48126220703125]) < 250
        assert max(float(s[2]) for s in samples) > 1 and max(float(s[3]) for s in samples) > 1
    if args.phase == "run":
        assert seconds[0] > 1 and seconds[1] > 1
        # Independent winning SKIL03e rates; trace records the actual input
        # durations admitted by the controller, not an original-game oracle.
        expected = b[8]["experience"] + .03 * seconds[0] + .04 * seconds[1]
        assert math.isclose(a[8]["experience"], expected, abs_tol=.002), (a[8], expected, seconds)
        assert math.isclose(float(uses[-1][2]), a[8]["experience"], abs_tol=.00003)
    else:
        assert not uses and a[8] == b[8]
    distance = math.dist(before["player"]["position"][:2], after["player"]["position"][:2])
    if args.phase == "walk":
        moving = [s for s in samples if float(s[4]) != 0]
        assert moving and all(s[0] == s[1] == "false" for s in moving) and distance > 10
    if args.phase in ("idle", "restart"):
        assert all(float(s[4]) == 0 for s in samples) and distance < 10
    assert "M15 melee hit committed: actor=" + PLAYER not in pre
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
                  athletics_before=b[8], athletics_after=a[8], committed_run_seconds=seconds[0],
                  committed_swim_seconds=seconds[1], observed_run_intent_seconds=max(float(s[2]) for s in samples),
                  observed_swim_intent_seconds=max(float(s[3]) for s in samples),
                  horizontal_displacement=distance, endpoint=after["player"]["position"][:3])
    (course / "athletics-use-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
