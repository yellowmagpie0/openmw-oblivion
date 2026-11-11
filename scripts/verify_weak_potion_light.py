"""Inspect the earned potion replay's live state, save payload and fixed view."""

import argparse
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageStat

import tes4_runtime_state as runtime


PLAYER = "dynamic:player:0000000000000001"
POTION = "content:oblivion.esm:09847f"
LIGHT = "content:oblivion.esm:0052b4"
ROI = (90, 45, 520, 260)


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def bottle_count(state):
    return sum(i["count"] for i in state["player"]["inventory"] if i["base"] == POTION)


def effects(state):
    player = next(a for a in state["native_actor_values"] if a["actor"] == PLAYER)
    return player.get("potion_light_effects", [])


def unchanged_inventory(state):
    return [i for i in state["player"]["inventory"] if i["base"] != POTION]


def mean_rgb(path):
    with Image.open(path) as image:
        assert image.size == (640, 360)
        return [round(x, 3) for x in ImageStat.Stat(image.convert("RGB").crop(ROI)).mean]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("phase", choices=("use", "move", "expire", "empty"))
    parser.add_argument("--dark-view", type=Path, required=True,
                        help="Rendered dark view matching this course's camera and position")
    args = parser.parse_args()
    course, phase = args.course, args.phase
    driver = read(course / "driver-process.json")
    assert driver["captured"] and driver["input_unchanged"] and driver["manifest_input_unchanged"]
    assert driver["exit_code"] == 0 and read(course / "scenario.json")["passed"]
    source = Path(driver["input_save"])
    target = course / "userdata/saves/Bendu_Olo/Quicksave.omwsave"
    source_hash, target_hash = sha(source), sha(target)
    assert source_hash == driver["input_sha256"]
    before, after = runtime.load_save(source), runtime.load_save(target)
    assert after["schema_version"] == 99
    assert runtime.encode_payload(after) == runtime._find_runtime_record(target.read_bytes())[3]
    events = [json.loads(line) for line in (course / "events.jsonl").read_text().splitlines()]
    load = next(event for event in events if event["event"] == "load-complete")
    save = next(event for event in events if event["event"] == "save-complete")
    epoch = read(course / "process-epoch.json")
    assert load["pid"] == save["pid"] == epoch["pid"]
    assert load["save_sha256"] == source_hash and save["save_sha256"] == target_hash
    assert read(course / save["live"]) == after
    loaded = read(course / load["live"])
    assert bottle_count(loaded) == bottle_count(before) and effects(loaded) == effects(before)
    assert unchanged_inventory(before) == unchanged_inventory(after)
    assert bottle_count(after) == 0
    assert before["player"]["cell"] == after["player"]["cell"]
    distance = math.dist(before["player"]["position"][:3], after["player"]["position"][:3])
    if phase == "move":
        assert 80 < distance < 140
        assert abs(before["player"]["position"][2] - after["player"]["position"][2]) < 1
        assert abs(after["player"]["position"][3]) < .01
        assert before["player"]["position"][4:] == after["player"]["position"][4:]
    else:
        assert distance < 1
        assert before["player"]["position"][3:] == after["player"]["position"][3:]
    assert before["player"]["actor_values"]["health.current"] == after["player"]["actor_values"]["health.current"]
    log = (course / "process.log").read_text()
    applied = "M16 potion Light applied: potion=" + POTION
    expired = "M16 potion Light expired: potion=" + POTION
    dark = mean_rgb(args.dark_view)
    pictures = {"baseline_dark": dark}
    if phase == "use":
        assert bottle_count(before) == 1 and not effects(before)
        (effect,) = effects(after)
        assert {k: effect[k] for k in ("potion", "light", "magnitude", "duration")} == {
            "potion": POTION, "light": LIGHT, "magnitude": 20, "duration": 60,
        }
        assert 0 <= effect["elapsed"] < 10
        assert log.count(applied) == 1 and expired not in log
        pictures["active"] = mean_rgb(course / "fixed-view-after-potion.png")
        assert pictures["active"][1] - dark[1] > 5
    elif phase == "move":
        assert bottle_count(before) == 0
        (initial,) = effects(before)
        (final,) = effects(after)
        assert {k: initial[k] for k in initial if k != "elapsed"} == {
            k: final[k] for k in final if k != "elapsed"}
        assert 3 < final["elapsed"] - initial["elapsed"] < 15
        assert applied not in log and expired not in log
        pictures["moved_dark"] = mean_rgb(args.dark_view)
        pictures["moved_active"] = mean_rgb(course / "pool-backstep-active.png")
        assert pictures["moved_active"][1] - pictures["moved_dark"][1] > 5
    else:
        assert bottle_count(before) == 0 and not effects(after)
        assert applied not in log
        pictures["loaded"] = mean_rgb(course / f"fixed-view-after-load-{phase}.png")
        if phase == "expire":
            assert len(effects(before)) == 1 and log.count(expired) == 1
            pictures["expired"] = mean_rgb(course / "fixed-view-after-expiry.png")
            assert pictures["loaded"][1] - pictures["expired"][1] > 5
            assert abs(pictures["expired"][1] - dark[1]) < 3
        else:
            assert not effects(before) and expired not in log
            assert abs(pictures["loaded"][1] - dark[1]) < 3
    result = dict(passed=True, phase=phase, source_sha256=source_hash, save_sha256=target_hash,
                  process=epoch, effects_before=effects(before), effects_after=effects(after),
                  potion_before=bottle_count(before), potion_after=bottle_count(after), mean_rgb=pictures)
    (course / "audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
