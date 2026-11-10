"""Audit actual-content bow checkpoints; never writes or repairs a save."""

import argparse
import copy
import hashlib
import json
import re
from pathlib import Path

import tes4_runtime_state as runtime


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", type=Path)
    parser.add_argument("--phase", choices=("held", "flight", "completed"), required=True)
    parser.add_argument("--release-count", type=int, choices=(0, 1))
    parser.add_argument("--target-key")
    parser.add_argument("--defeated-key")
    parser.add_argument("--allow-player-armor-wear", action="store_true")
    args = parser.parse_args()
    root = args.course
    driver = json.loads((root / "driver-process.json").read_text())
    source = Path(driver["input_save"])
    output = root / "userdata/saves/Bendu_Olo/Quicksave.omwsave"
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    assert driver["exit_code"] == 0 and driver["captured"] and driver["input_unchanged"]
    assert digest(source) == driver["input_sha256"]
    scenario = json.loads((root / "scenario.json").read_text())
    assert scenario["passed"]
    initial, final = runtime.load_save(source), runtime.load_save(output)
    assert final["schema_version"] == 96
    assert runtime.encode_payload(final) == runtime._find_runtime_record(output.read_bytes())[3]
    observed_boundaries = False
    if (root / "events.jsonl").exists():
        events = [json.loads(line) for line in (root / "events.jsonl").read_text().splitlines()]
        load = next(event for event in events if event["event"] == "load-complete")
        saved = [event for event in events if event["event"] == "save-complete"][-1]
        assert load["save_sha256"] == digest(source)
        assert saved["save_sha256"] == digest(output)
        loaded = json.loads((root / load["live"]).read_text())
        for field in ("native_bow_states", "native_player_bow_timer", "native_melee_states",
                      "physical_actions", "physical_action_owners", "native_arrow_projectiles",
                      "next_arrow_projectile"):
            if field == "native_player_bow_timer":
                assert (field in loaded) == (field in initial), field
                assert loaded.get(field) == initial.get(field), field
            else:
                assert loaded[field] == initial.get(field, 1 if field == "next_arrow_projectile" else []), field
        assert loaded["player"]["inventory"] == initial["player"]["inventory"]
        assert json.loads((root / saved["live"]).read_text()) == final
        epoch = json.loads((root / "process-epoch.json").read_text())
        assert load["pid"] == saved["pid"] == epoch["pid"]
        observed_boundaries = True
    bow_key, ammo_key = "content:oblivion.esm:047aca", "content:oblivion.esm:017829"
    before = copy.deepcopy(initial["player"]["inventory"])
    after = copy.deepcopy(final["player"]["inventory"])
    before_bow = next(item for item in before if item["base"] == bow_key)
    after_bow = next(item for item in after if item["base"] == bow_key)
    before_ammo = next(item for item in before if item["base"] == ammo_key)
    after_ammo = next(item for item in after if item["base"] == ammo_key)
    releases = args.release_count if args.release_count is not None else int(args.phase != "held")
    released = bool(releases)
    assert after_ammo["count"] == before_ammo["count"] - int(released)
    assert before_bow["equipped_slots"] == after_bow["equipped_slots"] == 65536
    assert before_ammo["equipped_slots"] == after_ammo["equipped_slots"] == 131072
    condition_before, condition_after = before_bow["condition"], after_bow["condition"]
    if released:
        assert 0 < condition_after < condition_before
    else:
        assert condition_after == condition_before
    after_bow["condition"] = condition_before
    after_ammo["count"] = before_ammo["count"]
    armor_wear = []
    if args.allow_player_armor_wear:
        for key in ("content:oblivion.esm:0661c1", "content:oblivion.esm:0661c3"):
            old = next(item for item in before if item["base"] == key)
            new = next(item for item in after if item["base"] == key)
            assert old["equipped_slots"] == new["equipped_slots"] != 0
            assert 0 <= new["condition"] <= old["condition"]
            armor_wear.append(dict(base=key, before=old["condition"], after=new["condition"]))
            new["condition"] = old["condition"]
    assert before == after, "unrelated inventory or item metadata changed"
    assert final["next_arrow_projectile"] == initial.get("next_arrow_projectile", 1) + int(released)
    bows, flights = final["native_bow_states"], final["native_arrow_projectiles"]
    if args.phase == "held":
        assert len(bows) == 1 and not bows[0]["release_committed"]
        assert bows[0]["bow_base"] == bow_key and bows[0]["ammo_base"] == ammo_key
        assert not flights
    elif args.phase == "flight":
        assert len(flights) == 1
    else:
        assert not bows and not flights
        assert final["native_player_bow_timer"] == 0
    log = (root / "process.log").read_text()
    assert log.count("M15 Player arrow released:") == int(released)
    assert "M15 native bow rejected:" not in log
    target_report = None
    if args.target_key:
        key = args.target_key
        hit_damage = [float(value) for value in re.findall(
            r"M15 Player arrow impact: id=\d+ target=" + re.escape(key) + r" damage=([\d.eE+-]+)", log)]
        assert len(hit_damage) == 1 and hit_damage[0] > 0
        def health(state):
            actor = next(item for item in state["native_actor_values"] if item["actor"] == key)
            return sum(value for value in actor["values"][8] if value is not None)
        initial_health, final_health = health(initial), health(final)
        assert abs(initial_health - final_health - hit_damage[0]) < .001
        target_before = next(item for item in initial["references"] if item["key"] == key)
        target_after = next(item for item in final["references"] if item["key"] == key)
        stock = lambda reference: sorted((item["base"], item["count"], item["owner"], item["charge"])
                                         for item in reference["inventory"])
        assert stock(target_before) == stock(target_after)
        target_report = dict(key=key, health_before=initial_health, health_after=final_health,
                             logged_damage=hit_damage[0], inventory_stock_preserved=True)
    defeated = None
    if args.defeated_key:
        key = args.defeated_key
        old = next(item for item in initial["native_actor_life"] if item["actor"] == key)
        new = next(item for item in final["native_actor_life"] if item["actor"] == key)
        assert new["phase"] == 1 and new["killer"] == "dynamic:player:0000000000000001"
        old_counts = {item["base"]: item["count"] for item in initial["native_death_counts"]}
        new_counts = {item["base"]: item["count"] for item in final["native_death_counts"]}
        if old["phase"] == 0:
            old_counts[new["base"]] = old_counts.get(new["base"], 0) + 1
            assert final["next_death_event"] == initial["next_death_event"] + 1
        else:
            assert new == old
            assert final["next_death_event"] == initial["next_death_event"]
        assert new_counts == old_counts
        defeated = dict(life=new, death_count=new_counts[new["base"]],
                        new_death=old["phase"] == 0)
    report = dict(passed=True, phase=args.phase, releases=releases, input_sha256=digest(source),
                  output_sha256=digest(output), arrows_before=before_ammo["count"],
                  arrows_after=next(x for x in final["player"]["inventory"] if x["base"] == ammo_key)["count"],
                  bow_condition_before=condition_before, bow_condition_after=condition_after,
                  unrelated_inventory_preserved=True, binary_roundtrip=True,
                  observed_load_and_save_boundaries=observed_boundaries,
                  target=target_report,
                  defeated=defeated, player_armor_wear=armor_wear,
                  pending_bows=bows, pending_flights=flights)
    (root / "bow-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
