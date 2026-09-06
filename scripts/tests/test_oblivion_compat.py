#!/usr/bin/env python3

import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from pathlib import Path


SOURCE = Path(__file__).resolve().parents[2]
SCRIPT = SOURCE / "scripts" / "oblivion_compat.py"
SPEC = importlib.util.spec_from_file_location("oblivion_compat", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)
REFERENCE_SCRIPT = SOURCE / "scripts" / "obscript_reference.py"
REFERENCE_SPEC = importlib.util.spec_from_file_location("obscript_reference", REFERENCE_SCRIPT)
REFERENCE = importlib.util.module_from_spec(REFERENCE_SPEC)
assert REFERENCE_SPEC.loader is not None
sys.modules[REFERENCE_SPEC.name] = REFERENCE
REFERENCE_SPEC.loader.exec_module(REFERENCE)


class OblivionCompatTests(unittest.TestCase):
    def test_m11_equipment_matrix_tracks_slots_assets_and_sex_fallbacks(self):
        dump = "\n".join(
            (
                "  Record: RACE",
                "  Id: 0x1",
                "  EditorId: Imperial",
                "  RaceFlags: 1",
                "  Record: ARMO",
                "  Id: 0x2",
                "  EditorId: SharedCuirass",
                r"  ModelMale: Armor\Shared\Cuirass.NIF",
                "  ModelFemale: ",
                "  BipedSlots: 4",
                "  Record: CLOT",
                "  Id: 0x3",
                "  EditorId: FemaleDress",
                "  ModelMale: ",
                r"  ModelFemale: Clothes\Dress\Dress.NIF",
                "  BipedSlots: 12",
            )
        )
        races = MODULE._parse_m11_races(dump, "Oblivion.esm")
        self.assertEqual([(race["id"], race["editor_id"]) for race in races], [("0x1", "Imperial")])
        equipment = MODULE._parse_m11_equipment(dump, "Oblivion.esm")
        references, matrix = MODULE._resolve_m11_equipment_assets(
            equipment,
            {"meshes/armor/shared/cuirass.nif", "meshes/clothes/dress/dress.nif"},
        )
        self.assertEqual([record["slot_names"] for record in matrix], [["upper_body"], ["upper_body", "lower_body"]])
        self.assertTrue(all(reference["passed"] for reference in references))
        self.assertTrue(all(record["passed"] and record["male"] == record["female"] for record in matrix))

    def test_m10_asset_paths_match_runtime_prefix_extension_and_directory_rules(self):
        self.assertEqual(
            MODULE.m10_resource_candidates(r"Sky\sun.tga", "textures", ".dds"),
            ["textures/sky/sun.dds", "textures/sky/sun.tga", "textures/sun.dds", "textures/sun.tga"],
        )
        self.assertEqual(
            MODULE.m10_resource_candidates(r"sound\fx\door.wav", "sound", ".mp3"),
            ["sound/fx/door.mp3", "sound/fx/door.wav", "sound/door.mp3", "sound/door.wav"],
        )
        records = MODULE.parse_m10_record_assets(
            "\n".join(
                (
                    "  Record: SOUN",
                    "  Id: 0x10",
                    "  EditorId: Wind",
                    "  SoundFile: fx\\ambient\\wind\\",
                    "  Record: WTHR",
                    "  Id: 0x11",
                    r"  LowerCloudTexture: sky\clouds.tga",
                )
            ),
            "Oblivion.esm",
        )
        resolved = MODULE.resolve_m10_record_assets(
            records,
            {"sound/fx/ambient/wind/wind_01.wav", "sound/fx/ambient/wind/wind_02.wav", "textures/sky/clouds.dds"},
        )
        self.assertEqual(resolved[0]["resolved"], [
            "sound/fx/ambient/wind/wind_01.wav", "sound/fx/ambient/wind/wind_02.wav"
        ])
        self.assertEqual(resolved[1]["resolved"], ["textures/sky/clouds.dds"])
        self.assertTrue(all(item["passed"] for item in resolved))

    def test_m10_asset_count_lock_rejects_inventory_drift(self):
        report = {
            "official_content": [{"name": "Oblivion.esm", "size": 1, "sha256": "x"}],
            "summary": {
                "archive_count": 1,
                "vfs_entry_count": 2,
                "record_counts": {"WTHR": 1},
                "reference_count": 1,
                "directory_reference_count": 0,
                "resolved_file_count": 1,
                "reviewed_missing_count": 0,
                "missing_reference_fingerprint": "sha256:missing",
                "inventory_counts": {"videos": 1},
                "inventory_fingerprint": "sha256:inventory",
                "reference_fingerprint": "sha256:references",
            },
        }
        count_lock = MODULE.m10_count_lock_from_report(report)
        self.assertTrue(MODULE.validate_m10_asset_count_lock(report, count_lock)["passed"])
        count_lock["expected"]["vfs_entry_count"] += 1
        self.assertFalse(MODULE.validate_m10_asset_count_lock(report, count_lock)["passed"])

    def test_m10_asset_exceptions_are_exact_and_count_locked(self):
        missing = [{"plugin": "Oblivion.esm", "record": "SOUN", "editor_id": "WPNHitBladeX", "field": "SoundFile"}]
        rules = {
            "allowed": [{
                "plugin": "Oblivion.esm", "record": "SOUN", "editor_id_pattern": "WPNHit.*X",
                "field": "SoundFile", "expected_count": 1,
            }]
        }
        self.assertTrue(MODULE.validate_m10_asset_exceptions(missing, rules)["passed"])
        rules["allowed"][0]["expected_count"] = 2
        self.assertFalse(MODULE.validate_m10_asset_exceptions(missing, rules)["passed"])

    def test_independent_obscript_frontend_is_whitespace_stable(self):
        compact = (
            "scn Test\nshort count\nbegin GameMode\nset count to 1 + 2 * 3\n"
            "if GetDisabled == 0\nEnable\nendif\nend\n"
        )
        spaced = (
            "\r\nscn Test ; comment\r\n short count\r\nbegin GameMode\r\n"
            "set count to (1 + (2 * 3))\r\nif GetDisabled==0\r\n Enable\r\nendif\r\nend\r\n"
        )
        first = REFERENCE.Parser(REFERENCE.tokenize(compact)).parse()
        second = REFERENCE.Parser(REFERENCE.tokenize(spaced)).parse()
        self.assertEqual(REFERENCE.canonical_script(first), REFERENCE.canonical_script(second))
        self.assertEqual(REFERENCE.fingerprint(REFERENCE.canonical_script(first)), "fnv1a64:19eaf376f5f36ea7")
        unit_id = "content:memory.esm:000001@memory.esm/object/unit=0"
        first_ir = REFERENCE.canonical_program(REFERENCE.Emitter(unit_id, first).emit())
        second_ir = REFERENCE.canonical_program(REFERENCE.Emitter(unit_id, second).emit())
        self.assertEqual(first_ir, second_ir)
        self.assertEqual(REFERENCE.fingerprint(first_ir), "fnv1a64:94e1c309fb0c1d26")

    def test_m6_validator_rejects_duplicate_unit_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            data = Path(temporary)
            plugins = []
            for name in MODULE.OFFICIAL_PLUGIN_ORDER:
                path = data / name
                path.write_bytes(name.encode("ascii"))
                plugins.append(
                    {
                        "name": name,
                        "size": path.stat().st_size,
                        "sha256": MODULE.sha256(path),
                        "units": 0,
                        "source": 0,
                        "compiled": 0,
                        "contexts": {},
                    }
                )
            aggregate = {
                "unit_count": 0,
                "source_count": 0,
                "compiled_count": 0,
                "source_only_count": 0,
                "compiled_only_count": 0,
                "source_payload_bytes": 0,
                "compiled_payload_bytes": 0,
                "reference_count": 0,
                "corpus_fingerprint": "fnv1a64:test",
                "coverage_entries": 0,
                "contexts": {},
                "scda": {"decoded": 0, "header_size_matches": 0, "structure_matches": 0,
                         "instruction_count": 0},
            }
            report = dict(aggregate)
            report.update({"frontend_failures": 0, "cache_entries": 0, "coverage": [], "units": [],
                           "scda": aggregate["scda"]})
            lock = {"aggregate": aggregate, "plugins": plugins}
            self.assertTrue(MODULE.validate_m6_report(report, lock, data)["passed"])
            duplicate = {
                "id": "duplicate", "plugin": "Oblivion.esm", "context": "object",
                "source": "", "source_payload_fingerprint": "x", "source_fingerprint": "x",
                "compiled_payload_fingerprint": None, "ast_fingerprint": "a", "program_fingerprint": "p",
                "reference_fingerprint": "r", "cache_stable": True, "diagnostics": [],
            }
            report["units"] = [duplicate, dict(duplicate)]
            self.assertFalse(MODULE.validate_m6_report(report, lock, data)["passed"])

    def test_log_checker_reports_unallowed_errors(self):
        result = MODULE.check_log_text("ok\nError: missing thing\nstill running\n")
        self.assertFalse(result["passed"])
        self.assertEqual(result["findings"][0]["line"], 2)

    def test_log_checker_honours_allowlist(self):
        result = MODULE.check_log_text(
            "Error: expected baseline gap\n",
            allow_patterns=[r"expected baseline gap"],
        )
        self.assertTrue(result["passed"])

    def test_placeholder_expansion_is_strict(self):
        self.assertEqual(MODULE.expand_value(["{one}"], {"one": "1"}), ["1"])
        with self.assertRaises(ValueError):
            MODULE.expand_value("{missing}", {})

    def test_m14_manifest_rejects_direct_mutation_and_validates_event_matchers(self):
        manifest = {
            "schema_version": 1,
            "name": "m14-test",
            "command": [sys.executable, "-c", "pass"],
            "m14": {"event_file": "ai-events.jsonl", "required_events": [{"event": "phase"}]},
            "actions": [{"type": "m14_assert_events", "required": [{"event": "phase"}]}],
        }
        MODULE.validate_scenario_manifest(manifest)
        manifest["actions"].append({"type": "move_actor"})
        with self.assertRaises(ValueError):
            MODULE.validate_scenario_manifest(manifest)

    def test_m14_event_stream_checker_is_structured_and_exact(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "ai-events.jsonl").write_text(
                '{"event":"phase","actor":"content:oblivion.esm:000001","to":2}\n',
                encoding="utf-8",
            )
            result = MODULE._validate_m14_events(
                {
                    "m14": {
                        "event_file": "ai-events.jsonl",
                        "required_events": [{"event": "phase", "to": 2}],
                        "forbidden_events": [{"event": "teleport"}],
                        "minimum_event_count": 1,
                    }
                },
                output,
            )
            self.assertTrue(result["passed"])
            self.assertEqual(result["event_types"], {"phase": 1})

    def test_m14_actor_event_requirements_cannot_be_satisfied_by_another_actor(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "ai-events.jsonl").write_text(
                "\n".join(
                    (
                        '{"event":"selection","actor":"actor:a"}',
                        '{"event":"phase","actor":"actor:b","to":2}',
                        '{"event":"route","actor":"actor:b"}',
                    )
                ) + "\n",
                encoding="utf-8",
            )
            result = MODULE._validate_m14_events(
                {
                    "m14": {
                        "event_file": "ai-events.jsonl",
                        "required_events": [{"event": "route"}],
                        "actor_event_requirements": [
                            {
                                "actor": "actor:a",
                                "required_events": [{"event": "route"}],
                                "minimum_event_counts": {"phase": 1},
                            }
                        ],
                    }
                },
                output,
            )
            self.assertFalse(result["passed"])
            self.assertNotIn("actor:b", result["actors"])
            self.assertEqual(result["actors"]["actor:a"]["event_types"], {"selection": 1})
            self.assertTrue(any("actor:a" in failure and "missing required event" in failure
                                for failure in result["failures"]))

    def test_m14_actor_event_requirements_enforce_scoped_order_counts_and_reasons(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "ai-events.jsonl").write_text(
                "\n".join(
                    (
                        '{"event":"selection","actor":"actor:a"}',
                        '{"event":"phase","actor":"actor:b","reason":"bounded-repath-exhausted"}',
                        '{"event":"phase","actor":"actor:a","to":1}',
                        '{"event":"route","actor":"actor:a"}',
                    )
                ) + "\n",
                encoding="utf-8",
            )
            requirement = {
                "actor": "actor:a",
                "required_event_order": [
                    {"event": "selection"}, {"event": "phase", "to": 1}, {"event": "route"}
                ],
                "minimum_event_counts": {"route": 1},
                "maximum_event_counts": {"phase": 1},
                "forbidden_reason_substrings": ["bounded-repath-exhausted"],
            }
            result = MODULE._validate_m14_events(
                {"m14": {"event_file": "ai-events.jsonl", "actor_event_requirements": [requirement]}},
                output,
            )
            self.assertTrue(result["passed"])
            self.assertTrue(result["actors"]["actor:a"]["passed"])

            requirement["actor"] = "actor:b"
            result = MODULE._validate_m14_events(
                {"m14": {"event_file": "ai-events.jsonl", "actor_event_requirements": [requirement]}},
                output,
            )
            self.assertFalse(result["passed"])
            self.assertTrue(any("forbidden reason" in failure for failure in result["failures"]))

    def test_m14_manifest_rejects_malformed_actor_event_requirements(self):
        manifest = {
            "schema_version": 1,
            "name": "m14-actor-events",
            "command": [sys.executable, "-c", "pass"],
            "m14": {"event_file": "ai-events.jsonl", "actor_event_requirements": [{"required_events": []}]},
        }
        with self.assertRaisesRegex(ValueError, "stable actor key"):
            MODULE.validate_scenario_manifest(manifest)
        manifest["m14"]["actor_event_requirements"] = [
            {"actor": "actor:a", "maximum_event_counts": {"phase": -1}}
        ]
        with self.assertRaisesRegex(ValueError, "non-negative integers"):
            MODULE.validate_scenario_manifest(manifest)
        manifest["m14"]["actor_event_requirements"] = [{"actor": "actor:a"}, {"actor": "actor:a"}]
        with self.assertRaisesRegex(ValueError, "duplicates actor"):
            MODULE.validate_scenario_manifest(manifest)

    def test_m14_detection_requirements_are_scoped_to_named_pair_and_outcomes(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "ai-events.jsonl").write_text(
                "\n".join(
                    (
                        '{"event":"detection","observer":"actor:guard","target":"actor:a",'
                        '"line_of_sight":false,"detected":false,"score":0}',
                        '{"event":"detection","observer":"actor:guard","target":"actor:b",'
                        '"line_of_sight":true,"detected":true,"score":20}',
                    )
                ) + "\n",
                encoding="utf-8",
            )
            requirement = {
                "observer": "actor:guard",
                "target": "actor:a",
                "minimum_event_count": 2,
                "maximum_event_count": 4,
                "required_outcomes": [
                    {"line_of_sight": False, "detected": False, "score": 0},
                    {"line_of_sight": True, "detected": True},
                ],
            }
            result = MODULE._validate_m14_events(
                {"m14": {"event_file": "ai-events.jsonl", "detection_requirements": [requirement]}},
                output,
            )
            self.assertFalse(result["passed"])
            pair = result["detections"]["actor:guard -> actor:a"]
            self.assertEqual(pair["events"], 1)
            self.assertTrue(any("missing required outcome" in failure for failure in pair["failures"]))

            requirement.update({"target": "actor:b", "minimum_event_count": 1})
            requirement["required_outcomes"] = [{"line_of_sight": True, "detected": True}]
            result = MODULE._validate_m14_events(
                {"m14": {"event_file": "ai-events.jsonl", "detection_requirements": [requirement]}},
                output,
            )
            self.assertTrue(result["passed"])

    def test_m14_manifest_rejects_malformed_detection_requirements(self):
        manifest = {
            "schema_version": 1,
            "name": "m14-detections",
            "command": [sys.executable, "-c", "pass"],
            "m14": {
                "event_file": "ai-events.jsonl",
                "detection_requirements": [{"observer": "actor:a", "required_outcomes": []}],
            },
        }
        with self.assertRaisesRegex(ValueError, "stable target key"):
            MODULE.validate_scenario_manifest(manifest)
        manifest["m14"]["detection_requirements"] = [
            {"observer": "actor:a", "target": "actor:b", "required_outcomes": "visible"}
        ]
        with self.assertRaisesRegex(ValueError, "required_outcomes"):
            MODULE.validate_scenario_manifest(manifest)
        manifest["m14"]["detection_requirements"] = [
            {"observer": "actor:a", "target": "actor:b", "required_outcomes": [{"detected": 1}]}
        ]
        with self.assertRaisesRegex(ValueError, "detected must be boolean"):
            MODULE.validate_scenario_manifest(manifest)
        requirement = {"observer": "actor:a", "target": "actor:b", "required_outcomes": []}
        manifest["m14"]["detection_requirements"] = [requirement, dict(requirement)]
        with self.assertRaisesRegex(ValueError, "duplicates observer/target pair"):
            MODULE.validate_scenario_manifest(manifest)

    def test_m14_actor_state_requirements_are_scoped_and_exact(self):
        states = [
            {
                "actor": "actor:a",
                "phase": 6,
                "target": "actor:b",
                "has_destination": True,
                "interruption_reason": "",
            },
            {
                "actor": "actor:b",
                "phase": 12,
                "target": "null",
                "has_destination": True,
                "interruption_reason": "bounded-repath-exhausted",
            },
        ]
        requirements = [
            {
                "actor": "actor:a",
                "expected": {"phase": 6, "target": "actor:b"},
                "forbidden_values": {"phase": [12]},
                "required_truthy": ["has_destination"],
                "forbidden_interruption_substrings": ["repath-exhausted"],
            }
        ]
        failures, summaries = MODULE._validate_m14_actor_states(states, requirements)
        self.assertEqual(failures, [])
        self.assertTrue(summaries["actor:a"]["passed"])

        requirements[0]["actor"] = "actor:b"
        failures, summaries = MODULE._validate_m14_actor_states(states, requirements)
        self.assertFalse(summaries["actor:b"]["passed"])
        self.assertTrue(any("forbidden value" in failure for failure in failures))
        self.assertTrue(any("forbidden interruption" in failure for failure in failures))

    def test_m14_manifest_rejects_malformed_actor_state_requirements(self):
        manifest = {
            "schema_version": 1,
            "name": "m14-actor-state",
            "command": [sys.executable, "-c", "pass"],
            "m14": {"event_file": "ai-events.jsonl"},
            "actions": [{"type": "m14_checkpoint", "actor_state_requirements": [{"expected": {}}]}],
        }
        with self.assertRaisesRegex(ValueError, "stable actor key"):
            MODULE.validate_scenario_manifest(manifest)
        manifest["actions"][0]["actor_state_requirements"] = [
            {"actor": "actor:a", "forbidden_values": {"phase": 12}}
        ]
        with self.assertRaisesRegex(ValueError, "value lists"):
            MODULE.validate_scenario_manifest(manifest)

    def test_m14_checkpoint_applies_actor_state_requirements(self):
        state = {
            "clock": {"day": 27, "hour": 1.0},
            "actor_ai": [{"actor": "actor:a", "phase": 12, "interruption_reason": "stalled"}],
        }
        validation = {
            "passed": True,
            "failures": [],
            "actor_count": 1,
            "companion_count": 0,
            "mount_count": 0,
            "detection_vector_count": 0,
        }
        action = {
            "type": "m14_checkpoint",
            "name": "actor-state",
            "actor_state_requirements": [
                {"actor": "actor:a", "forbidden_values": {"phase": [12]}}
            ],
        }
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(MODULE, "_single_save", return_value=Path(temporary) / "save.omwsave"), \
                mock.patch.object(MODULE.tes4_state, "load_save", return_value=state), \
                mock.patch.object(MODULE, "validate_m14_runtime_state", return_value=validation):
            result = MODULE._run_action(action, environment={}, output=Path(temporary))
            self.assertFalse(result["passed"])
            self.assertFalse(result["actor_requirements"]["actor:a"]["passed"])
            self.assertTrue(any("forbidden value" in failure for failure in result["failures"]))

    def test_single_save_can_select_named_slot_among_multiple_saves(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            saves = output / "userdata" / "saves" / "player"
            saves.mkdir(parents=True)
            (saves / "Autosave.omwsave").write_bytes(b"autosave")
            quicksave = saves / "Quicksave.omwsave"
            quicksave.write_bytes(b"quicksave")

            self.assertEqual(MODULE._single_save(output, "quicksave"), quicksave)
            with self.assertRaisesRegex(RuntimeError, "found 2"):
                MODULE._single_save(output)

    def test_m14_actor_distance_uses_named_saved_actors(self):
        state = {
            "actor_ai": [
                {"actor": "actor:a", "cell": "cell:one", "last_valid_position": [0.0, 0.0, 0.0]},
                {"actor": "actor:b", "cell": "cell:one", "last_valid_position": [300.0, 400.0, 0.0]},
            ]
        }
        action = {
            "type": "m14_actor_distance",
            "checkpoint": "checkpoints/distance.json",
            "actor": "actor:a",
            "target": "actor:b",
            "minimum_distance": 499.0,
            "maximum_distance": 501.0,
            "expected_same_cell": True,
        }
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            checkpoint = output / "checkpoints" / "distance.json"
            checkpoint.parent.mkdir()
            checkpoint.write_text(json.dumps({"runtime_state": state}), encoding="utf-8")
            result = MODULE._run_action(action, environment={}, output=output)
            self.assertTrue(result["passed"])
            self.assertEqual(result["distance"], 500.0)

            action["minimum_distance"] = 501.0
            result = MODULE._run_action(action, environment={}, output=output)
            self.assertFalse(result["passed"])
            self.assertTrue(any("outside" in failure for failure in result["failures"]))

    def test_m14_actor_distance_manifest_requires_bounded_numeric_thresholds(self):
        manifest = {
            "schema_version": 1,
            "name": "m14-distance",
            "command": [sys.executable, "-c", "pass"],
            "m14": {"event_file": "ai-events.jsonl"},
            "actions": [{"type": "m14_actor_distance", "actor": "actor:a", "target": "actor:b"}],
        }
        with self.assertRaisesRegex(ValueError, "requires a minimum_distance or maximum_distance"):
            MODULE.validate_scenario_manifest(manifest)
        manifest["actions"][0].update({"minimum_distance": 10, "maximum_distance": 5})
        with self.assertRaisesRegex(ValueError, "exceeds"):
            MODULE.validate_scenario_manifest(manifest)

    def test_m14_actor_state_delta_compares_named_checkpoint_states(self):
        before = {
            "actor_ai": [{"actor": "actor:a", "cell": "cell:one", "package": "package:one",
                          "route_generation": 2, "last_valid_position": [0.0, 0.0, 0.0]}]
        }
        after = {
            "actor_ai": [{"actor": "actor:a", "cell": "cell:one", "package": "package:two",
                          "route_generation": 4, "last_valid_position": [3.0, 4.0, 0.0]}]
        }
        action = {
            "type": "m14_actor_state_delta",
            "actor": "actor:a",
            "before_checkpoint": "checkpoints/before.json",
            "after_checkpoint": "checkpoints/after.json",
            "minimum_position_delta": 4.9,
            "maximum_position_delta": 5.1,
            "required_changed_fields": ["package"],
            "minimum_field_increases": {"route_generation": 1},
            "expected_same_cell": True,
        }
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "checkpoints").mkdir()
            (output / "checkpoints" / "before.json").write_text(
                json.dumps({"runtime_state": before}), encoding="utf-8")
            (output / "checkpoints" / "after.json").write_text(
                json.dumps({"runtime_state": after}), encoding="utf-8")
            result = MODULE._run_action(action, environment={}, output=output)
            self.assertTrue(result["passed"])
            self.assertEqual(result["position_delta"], 5.0)
            self.assertEqual(result["field_increases"], {"route_generation": 2.0})

            action["minimum_position_delta"] = 6.0
            result = MODULE._run_action(action, environment={}, output=output)
            self.assertFalse(result["passed"])
            self.assertTrue(any("position delta" in failure for failure in result["failures"]))

    def test_m14_actor_state_delta_manifest_requires_a_typed_assertion(self):
        manifest = {
            "schema_version": 1,
            "name": "m14-state-delta",
            "command": [sys.executable, "-c", "pass"],
            "m14": {"event_file": "ai-events.jsonl"},
            "actions": [{
                "type": "m14_actor_state_delta", "actor": "actor:a",
                "before_checkpoint": "before.json", "after_checkpoint": "after.json",
            }],
        }
        with self.assertRaisesRegex(ValueError, "at least one comparison assertion"):
            MODULE.validate_scenario_manifest(manifest)
        manifest["actions"][0]["minimum_field_increases"] = {"route_generation": 0}
        with self.assertRaisesRegex(ValueError, "positive numbers"):
            MODULE.validate_scenario_manifest(manifest)

    def test_m14_state_validator_rejects_occluded_detection_and_direct_markers(self):
        state = {"schema_version": 5, "ai_rng_state": 1, "actor_ai": [], "path_points": [], "companions": [],
                 "mounts": [], "detection_vectors": [{"score": 12, "line_of_sight": False, "detected": True}]}
        result = MODULE.validate_m14_runtime_state(state)
        self.assertFalse(result["passed"])
        self.assertTrue(any("occluded" in failure for failure in result["failures"]))
        state["detection_vectors"][0]["detected"] = False
        state["direct_teleport"] = True
        self.assertFalse(MODULE.validate_m14_runtime_state(state)["passed"])

    def test_m14_event_stream_checker_enforces_order_and_repeat_bounds(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "ai-events.jsonl").write_text(
                "\n".join(
                    (
                        '{"event":"selection","actor":"content:oblivion.esm:000001"}',
                        '{"event":"phase","actor":"content:oblivion.esm:000001","from":0,"to":1}',
                        '{"event":"phase","actor":"content:oblivion.esm:000001","from":0,"to":1}',
                    )
                ) + "\n",
                encoding="utf-8",
            )
            result = MODULE._validate_m14_events(
                {
                    "m14": {
                        "event_file": "ai-events.jsonl",
                        "required_event_order": [{"event": "selection"}, {"event": "phase", "to": 1}],
                        "maximum_repeated_events": {"phase": 1},
                    }
                },
                output,
            )
            self.assertFalse(result["passed"])
            self.assertTrue(any("repeated" in failure for failure in result["failures"]))

    def test_m14_diagnostic_counts_include_suppressed_occurrences_and_reload_epochs(self):
        key = "route-blocked|actor=actor:a|reason=obstruction"
        sample = {"event": "route-blocked", "actor": "actor:a", "reason": "obstruction"}
        events = [
            dict(sample, diagnostic_count=1),
            dict(sample, diagnostic_count=32),
            {"event": "diagnostic-summary", "key": key, "count": 37},
            dict(sample, diagnostic_count=1),
            {"event": "diagnostic-summary", "key": key, "count": 3},
            {"event": "selection", "actor": "actor:b"},
        ]
        self.assertEqual(MODULE._m14_event_counts(events), {"route-blocked": 40, "selection": 1})
        self.assertEqual(MODULE._m14_event_counts(events, "actor:a"), {"route-blocked": 40})
        self.assertEqual(MODULE._m14_event_counts(events, "actor:b"), {"selection": 1})
        failures, summaries = MODULE._validate_m14_actor_events(events, [{
            "actor": "actor:a", "maximum_event_count": 39,
            "maximum_event_counts": {"route-blocked": 39},
            "required_event_order": [{"event": "route-blocked"}] * 4,
        }])
        self.assertEqual(summaries["actor:a"]["events"], 40)
        self.assertTrue(any("occurred 40" in failure for failure in failures))
        self.assertTrue(any("event order" in failure for failure in failures))
        self.assertTrue(any("event count 40" in failure for failure in failures))

    def test_m14_diagnostic_counts_support_legacy_summaries(self):
        events = [{"event": "route-blocked", "actor": "actor:a", "reason": "blocked"}] * 7
        events.append({"event": "diagnostic-summary",
                       "key": "route-blocked|actor=actor:a|reason=blocked", "count": 1000})
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "ai-events.jsonl").write_text(
                "\n".join(json.dumps(event) for event in events) + "\n", encoding="utf-8")
            result = MODULE._validate_m14_events({"m14": {
                "maximum_event_count": 500, "maximum_route_blocked": 500,
                "maximum_no_progress_events": 500, "maximum_repeated_events": {"route-blocked": 500},
            }}, output)
        self.assertEqual(result["events"], 1000)
        self.assertEqual(result["event_lines"], 8)
        self.assertEqual(len(result["failures"]), 4)

    def test_m14_diagnostic_counts_reject_malformed_or_decreasing_totals(self):
        for count in (True, 0, -1, 1.5, "32", None):
            with self.subTest(count=count), self.assertRaises(ValueError):
                MODULE._m14_event_counts([{"event": "route-blocked", "diagnostic_count": count}])
        with self.assertRaises(ValueError):
            MODULE._m14_event_counts([{"event": "diagnostic-summary", "count": 4}])
        with self.assertRaises(ValueError):
            MODULE._m14_event_counts([
                {"event": "route-blocked", "diagnostic_count": 32},
                {"event": "route-blocked", "diagnostic_count": 1},
            ])

    def test_m14_final_diagnostic_validation_requires_closing_summaries(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "ai-events.jsonl").write_text(
                '{"event":"route-blocked","diagnostic_count":32}\n', encoding="utf-8")
            manifest = {"m14": {"maximum_route_blocked": 32}}
            self.assertTrue(MODULE._validate_m14_events(manifest, output)["passed"])
            result = MODULE._validate_m14_events(manifest, output, final=True)
            self.assertFalse(result["passed"])
            self.assertIn("missing closing summaries", result["failures"][0])
            (output / "ai-events.jsonl").write_text('{"event":', encoding="utf-8")
            self.assertFalse(MODULE._validate_m14_events(manifest, output, final=True)["passed"])

    def test_m14_phase_repeat_budget_cannot_be_hidden_by_other_actors_or_diagnostics(self):
        phase = {"event": "phase", "actor": "actor:a", "from": 1, "to": 2}
        events = [phase, {"event": "phase", "actor": "actor:b", "from": 2, "to": 3},
                  {"event": "detection", "actor": "actor:a"}, phase]
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            path = output / "ai-events.jsonl"
            manifest = {"m14": {"maximum_phase_repeat": 1}}
            path.write_text("\n".join(json.dumps(event) for event in events), encoding="utf-8")
            self.assertFalse(MODULE._validate_m14_events(manifest, output)["passed"])
            for boundary in ({"event": "selection", "actor": "actor:a"},
                             {"event": "phase", "actor": "actor:a", "from": 2, "to": 1}):
                with self.subTest(boundary=boundary):
                    path.write_text("\n".join(json.dumps(event) for event in [phase, boundary, phase]),
                                    encoding="utf-8")
                    self.assertTrue(MODULE._validate_m14_events(manifest, output)["passed"])

    def test_m14_state_validator_rejects_wrong_collection_and_numeric_types(self):
        state = {
            "schema_version": 5,
            "ai_rng_state": 1,
            "actor_ai": {},
            "path_points": [],
            "companions": [],
            "mounts": [],
            "detection_vectors": [],
        }
        result = MODULE.validate_m14_runtime_state(state)
        self.assertFalse(result["passed"])
        self.assertTrue(any("actor_ai" in failure for failure in result["failures"]))
        state["actor_ai"] = [{"actor": [], "source": "base"}]
        result = MODULE.validate_m14_runtime_state(state)
        self.assertFalse(result["passed"])
        self.assertTrue(any("not a string" in failure or "not an integer" in failure for failure in result["failures"]))

    def test_m14_typed_controls_reject_ambiguous_clock_and_bad_obstruction(self):
        manifest = {
            "schema_version": 1,
            "name": "m14-control-test",
            "command": [sys.executable, "-c", "pass"],
            "m14": {"event_file": "ai-events.jsonl"},
            "actions": [{"type": "m14_advance_clock", "hours": 1, "game_hour": 4}],
        }
        with self.assertRaises(ValueError):
            MODULE.validate_scenario_manifest(manifest)
        manifest["actions"] = [{"type": "m14_obstruction", "reference": "../door", "operation": "add"}]
        with self.assertRaises(ValueError):
            MODULE.validate_scenario_manifest(manifest)

    def test_test_log_summary_detects_incomplete_tests(self):
        result = MODULE.summarize_test_log(
            "TEST_START\t1\tpasses\nTEST_OK\t1\tpasses\nTEST_START\t2\tincomplete\n"
        )
        self.assertEqual(result["started"], 2)
        self.assertEqual(result["passed"], 1)
        self.assertEqual(result["incomplete_tests"], ["2\tincomplete"])

    def test_m3_gate_requires_every_check(self):
        tests = {"unit": {"passed": True}}
        scenarios = {"interior": {"passed": True}, "exterior": {"passed": True}}
        regression = {"passed_gate": True}
        self.assertTrue(MODULE.m3_acceptance_passed(tests, scenarios, regression))
        scenarios["exterior"]["passed"] = False
        self.assertFalse(MODULE.m3_acceptance_passed(tests, scenarios, regression))

    def test_scenario_runner_and_atomic_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "result"
            manifest = SOURCE / "scripts" / "data" / "oblivion_compat" / "self_test_scenario.json"
            result = MODULE.run_scenario(
                manifest,
                output,
                {"source": str(SOURCE), "python": sys.executable},
            )
            self.assertTrue(result["passed"])
            persisted = json.loads((output / "scenario.json").read_text(encoding="utf-8"))
            self.assertEqual(persisted["name"], "scenario-runner-self-test")

    def test_scenario_timeout_bounds_actions_and_process_wait(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "timeout.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "name": "bounded-timeout",
                        "command": [sys.executable, "-c", "import time; time.sleep(10)"],
                        "timeout_seconds": 0.2,
                        "actions": [{"type": "sleep", "seconds": 10}],
                    }
                ),
                encoding="utf-8",
            )
            result = MODULE.run_scenario(manifest, root / "output", {})
            self.assertTrue(result["timed_out"])
            self.assertFalse(result["passed"])
            self.assertLess(result["duration_seconds"], 2.0)
            self.assertTrue(result["actions"][0]["deadline_exceeded"])

    def test_scenario_rejects_unreviewed_error_lines(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "errors.json"
            value = {
                "schema_version": 1,
                "name": "reviewed-errors",
                "command": [sys.executable, "-c", "print('[00:00:00 E] reviewed resource')"],
                "reviewed_error_log": ["different resource"],
            }
            manifest.write_text(json.dumps(value), encoding="utf-8")
            result = MODULE.run_scenario(manifest, root / "unreviewed", {})
            self.assertFalse(result["passed"])
            self.assertEqual(len(result["unreviewed_error_log_findings"]), 1)

            value["reviewed_error_log"] = ["reviewed resource"]
            manifest.write_text(json.dumps(value), encoding="utf-8")
            result = MODULE.run_scenario(manifest, root / "reviewed", {})
            self.assertTrue(result["passed"])
            self.assertEqual(result["unreviewed_error_log_findings"], [])

    def test_wait_log_action_observes_process_readiness(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "process.log").write_text("Starting a new game\n", encoding="utf-8")
            result = MODULE._run_action(
                {"type": "wait_log", "pattern": "Starting a new game", "timeout_seconds": 0.1},
                environment={},
                output=output,
            )
            self.assertTrue(result["passed"])

    def test_m14_named_actor_event_budgets_are_strict_by_default(self):
        actor = "content:oblivion.esm:000123"
        events = [{"event": "selection", "actor": actor} for _ in range(25)]
        events.append({"event": "route-blocked", "actor": actor, "reason": "blocked"})
        failures, _ = MODULE._validate_m14_actor_events(events, [{"actor": actor}])
        self.assertTrue(any("selection" in failure for failure in failures))
        self.assertTrue(any("route-blocked" in failure for failure in failures))

        requirements = [{
            "actor": actor,
            "maximum_event_counts": {"selection": 25, "route-blocked": 1},
        }]
        failures, _ = MODULE._validate_m14_actor_events(events, requirements)
        self.assertEqual(failures, [])

    def test_scenario_generated_paths_cannot_escape_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "escape.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "name": "escape",
                        "command": [sys.executable, "-c", "pass"],
                        "files": [{"path": "../escape", "content": "bad"}],
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                MODULE.run_scenario(manifest, root / "output", {})

    def test_file_assertion_checks_binary_tags_and_stays_below_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            artifact = output / "saves" / "test.omwsave"
            artifact.parent.mkdir()
            artifact.write_bytes(b"header-GPRO-T4VR-T4ST-OMW4STATE")
            result = MODULE._run_action(
                {
                    "type": "assert_file",
                    "path_glob": "saves/*.omwsave",
                    "expected_count": 1,
                    "minimum_size": 16,
                    "contains_ascii": ["GPRO", "T4VR", "T4ST", "OMW4STATE"],
                },
                environment={},
                output=output,
            )
            self.assertTrue(result["passed"])
            with self.assertRaises(ValueError):
                MODULE._run_action(
                    {"type": "assert_file", "path_glob": "../*.omwsave"},
                    environment={},
                    output=output,
                )

    def test_normal_input_actions_have_deterministic_xdotool_commands(self):
        cases = [
            ({"type": "key_down", "value": "w"}, ["/bin/true", "keydown", "w"]),
            ({"type": "key_up", "value": "w"}, ["/bin/true", "keyup", "w"]),
            ({"type": "mouse_move", "x": -4, "y": 12}, ["/bin/true", "mousemove_relative", "--", "-4", "12"]),
            ({"type": "mouse_move_absolute", "x": 330, "y": 568}, ["/bin/true", "mousemove", "330", "568"]),
            ({"type": "mouse_click", "button": 2}, ["/bin/true", "click", "2"]),
            ({"type": "mouse_down", "button": 2}, ["/bin/true", "mousedown", "2"]),
            ({"type": "mouse_up", "button": 2}, ["/bin/true", "mouseup", "2"]),
            (
                {"type": "focus_window", "name": "Oblivion"},
                ["/bin/true", "search", "--onlyvisible", "--name", "Oblivion", "windowfocus", "--sync", "%@"],
            ),
        ]
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            MODULE.shutil, "which", return_value="/bin/true"
        ):
            for action, expected in cases:
                with self.subTest(action=action):
                    result = MODULE._run_action(action, environment={}, output=Path(temporary))
                    self.assertTrue(result["passed"])
                    self.assertEqual(result["command"], expected)

    def test_held_input_actions_preserve_pollable_key_states(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            MODULE.shutil, "which", return_value="/bin/true"
        ), mock.patch.object(MODULE.time, "sleep"):
            key = MODULE._run_action(
                {"type": "key_held", "value": "grave"}, environment={}, output=Path(temporary)
            )
            no_op = MODULE._run_action(
                {"type": "key_hold", "value": "KP_6", "seconds": 0},
                environment={},
                output=Path(temporary),
            )
            held = MODULE._run_action(
                {"type": "key_hold", "value": "w", "seconds": 1.5},
                environment={},
                output=Path(temporary),
            )
            typed = MODULE._run_action(
                {"type": "type_held", "value": "a .-_", "hold_seconds": 0.01, "pause_seconds": 0},
                environment={},
                output=Path(temporary),
            )
            self.assertTrue(key["passed"])
            self.assertEqual(
                key["commands"],
                [["/bin/true", "keydown", "grave"], ["/bin/true", "keyup", "grave"]],
            )
            self.assertEqual(no_op["commands"], [])
            self.assertEqual(
                held["commands"],
                [["/bin/true", "keydown", "w"], ["/bin/true", "keyup", "w"]],
            )
            self.assertTrue(typed["passed"])
            self.assertEqual(
                [command[-1] for command in typed["commands"][::2]],
                ["a", "space", "period", "minus", "underscore"],
            )

    def test_virtual_gamepad_actions_emit_xbox_button_and_axis_events(self):
        self.assertEqual(
            [MODULE.VirtualGamepad.BUTTONS[name] for name in ("back", "start", "guide", "leftstick", "rightstick")],
            [314, 315, 316, 317, 318],
        )
        pad = mock.Mock()
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            MODULE, "_VIRTUAL_GAMEPAD", pad
        ), mock.patch.object(MODULE.time, "sleep"):
            button = MODULE._run_action(
                {"type": "gamepad_button", "value": "a", "hold_seconds": 0},
                environment={},
                output=Path(temporary),
            )
            axis = MODULE._run_action(
                {"type": "gamepad_axis", "axis": "left_y", "value": -1},
                environment={},
                output=Path(temporary),
            )
            trigger = MODULE._run_action(
                {"type": "gamepad_axis", "axis": "left_trigger", "value": 0.5},
                environment={},
                output=Path(temporary),
            )
        self.assertTrue(button["passed"] and axis["passed"] and trigger["passed"])
        self.assertEqual(
            pad.event.call_args_list,
            [
                mock.call(MODULE.VirtualGamepad.EV_KEY, MODULE.VirtualGamepad.BUTTONS["a"], 1),
                mock.call(MODULE.VirtualGamepad.EV_KEY, MODULE.VirtualGamepad.BUTTONS["a"], 0),
                mock.call(MODULE.VirtualGamepad.EV_ABS, MODULE.VirtualGamepad.AXES["left_y"], -32767),
                mock.call(MODULE.VirtualGamepad.EV_ABS, MODULE.VirtualGamepad.AXES["left_trigger"], 16384),
            ],
        )

    def test_tes4_runtime_state_codec_mutates_every_family_and_preserves_other_save_bytes(self):
        state = {
            "schema_version": 1,
            "profile": "oblivion",
            "next_dynamic_serial": 2,
            "content": [{"plugin": "oblivion.esm", "fingerprint": "sha256:test"}],
            "clock": {"year": 433, "month": 0, "day": 1, "hour": 3.5, "time_scale": 30.0},
            "player": {
                "reference": "dynamic:player:0000000000000001",
                "cell": "content:oblivion.esm:000001",
                "position": [0.0] * 6,
                "actor_values": {
                    "health.base": 50.0,
                    "health.modifier": 0.0,
                    "health.current": 50.0,
                    "magicka.current": 40.0,
                    "fatigue.current": 30.0,
                },
                "inventory": [],
            },
            "globals": {"content:oblivion.esm:000010": 1},
            "references": [
                {
                    "key": f"content:oblivion.esm:{index:06x}",
                    "base": f"content:oblivion.esm:{index + 16:06x}",
                    "cell": "content:oblivion.esm:000001",
                    "enabled": True,
                    "deleted": False,
                    "position": [0.0] * 6,
                    "owner": None,
                    "lock_level": 0,
                    "inventory": [],
                    "custom_state": {
                        "count": 1,
                        "scale": 1.0,
                        "record_type": 1,
                        "locked": index == 0x102,
                    },
                }
                for index in (0x100, 0x101, 0x102)
            ],
        }
        payload = MODULE.tes4_state.encode_payload(state)
        self.assertEqual(MODULE.tes4_state.decode_payload(payload), state)
        body = b"VERS" + (4).to_bytes(4, "little") + (1).to_bytes(4, "little")
        body += b"DATA" + len(payload).to_bytes(4, "little") + payload
        record = b"T4ST" + len(body).to_bytes(4, "little") + b"\0" * 8 + body
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source.omwsave"
            rewritten = Path(temporary) / "rewritten.omwsave"
            source.write_bytes(record + b"TAIL" + (0).to_bytes(4, "little") + b"\0" * 8)
            loaded = MODULE.tes4_state.load_save(source)
            mutated = MODULE.tes4_state.mutate_for_acceptance(loaded, "unit")
            MODULE.tes4_state.write_save(source, rewritten, mutated)
            self.assertEqual(MODULE.tes4_state.load_save(rewritten), mutated)
            self.assertTrue(rewritten.read_bytes().endswith(b"TAIL" + (0).to_bytes(4, "little") + b"\0" * 8))
            self.assertEqual(mutated["clock"]["time_scale"], 0.0)
            self.assertTrue(mutated["references"][1]["deleted"])
            self.assertEqual(mutated["references"][0]["custom_state"]["m4_probe"], "unit")
            self.assertTrue(mutated["references"][0]["custom_state"]["locked"])
            self.assertFalse(mutated["references"][2]["custom_state"]["locked"])

    def test_m5_state_validator_checks_native_interaction_effects(self):
        state = {
            "player": {
                "cell": "content:oblivion.esm:01fbb9",
                "position": [0.0] * 6,
                "inventory": [{"base": "content:oblivion.esm:023f6e", "count": 1}],
            },
            "references": [
                {
                    "key": "content:oblivion.esm:01fc0f",
                    "deleted": True,
                    "position": [0.0] * 6,
                    "inventory": [],
                    "custom_state": {"taken": True},
                }
            ],
        }
        self.assertTrue(MODULE.validate_m5_runtime_state("take", state)["passed"])
        state["references"][0]["deleted"] = False
        result = MODULE.validate_m5_runtime_state("take", state)
        self.assertFalse(result["passed"])
        self.assertIn("loose item", result["failures"][0])

        state["references"] = [
            {
                "key": "content:oblivion.esm:0564e9",
                "deleted": False,
                "position": [0.0] * 6,
                "inventory": [],
                "custom_state": {"ownership_checked": True},
            }
        ]
        self.assertTrue(MODULE.validate_m5_runtime_state("owned", state)["passed"])
        state["references"][0]["deleted"] = True
        self.assertFalse(MODULE.validate_m5_runtime_state("owned", state)["passed"])

    def test_m13_state_validator_checks_exact_categories_metadata_and_slots(self):
        counts = {
            "017829": 24, "0105e3": 1, "01c6d1": 1, "0243d9": 1, "0888be": 1,
            "0229ad": 1, "03368c": 2, "092d8a": 1, "02cf9f": 1, "00000f": 500,
            "00000c": 2, "098496": 1, "041fa5": 1, "023d67": 1, "000c0c": 1,
            "00000a": 5,
        }
        slots = {"017829": 1 << 17, "01c6d1": 1 << 2, "02cf9f": 1 << 18, "000c0c": 1 << 16}
        inventory = []
        for local, count in counts.items():
            inventory.append({
                "base": f"content:oblivion.esm:{local}",
                "count": count,
                "condition": {"01c6d1": 300, "000c0c": 140}.get(local, -1),
                "charge": -1.0,
                "equipped_slots": slots.get(local, 0),
                "hotkey": -1,
                "owner": "null",
                "remaining_usage_time": 900.0 if local == "02cf9f" else -1.0,
            })
        state = {"schema_version": 4, "player": {"inventory": inventory}}
        self.assertTrue(MODULE.validate_m13_runtime_state(state)["passed"])

        inventory[0]["count"] -= 1
        inventory[5]["equipped_slots"] = 1 << 2
        result = MODULE.validate_m13_runtime_state(state)
        self.assertFalse(result["passed"])
        self.assertTrue(any("count=" in failure for failure in result["failures"]))
        self.assertTrue(any("equipped_slots=" in failure for failure in result["failures"]))

    def test_form_graph_validator_accepts_only_reviewed_stable_edges(self):
        report = {
            "key_count": 3,
            "revision_count": 4,
            "reference_count": 2,
            "fingerprint": "fnv1a64:test",
            "restart_stable": True,
            "runtime_reorder_stable": True,
            "enable_parent_cycles": [],
            "unresolved": [
                {
                    "source": "content:oblivion.esm:000100",
                    "target": "content:oblivion.esm:000014",
                    "plugin": "oblivion.esm",
                    "record": "SCPT",
                    "subrecord": "SCRO",
                    "reason": "missing",
                }
            ],
        }
        allowlist = {
            "allowed": [
                {
                    "target": "content:oblivion.esm:000014",
                    "reason": "missing",
                    "expected_count": 1,
                    "description": "PlayerRef is engine-reserved",
                }
            ]
        }
        result = MODULE.validate_form_graph_report(report, allowlist)
        self.assertTrue(result["passed"])
        self.assertEqual(result["reviewed_exception_count"], 1)

    def test_form_graph_validator_rejects_new_edges_and_changed_counts(self):
        report = {
            "restart_stable": True,
            "runtime_reorder_stable": True,
            "enable_parent_cycles": [],
            "unresolved": [{"target": "content:test.esp:000001", "reason": "missing"}],
        }
        result = MODULE.validate_form_graph_report(
            report,
            {
                "allowed": [
                    {
                        "target": "content:oblivion.esm:000014",
                        "expected_count": 2,
                    }
                ]
            },
        )
        self.assertFalse(result["passed"])
        self.assertEqual(len(result["unreviewed"]), 1)
        self.assertEqual(len(result["stale_or_changed_rules"]), 1)

    @unittest.skipUnless(shutil.which("compare") and shutil.which("magick"), "ImageMagick is unavailable")
    def test_identical_image_passes(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = Path(temporary) / "image.png"
            subprocess.run(["magick", "-size", "16x16", "xc:#204060", str(image)], check=True)
            result = MODULE.compare_images(image, image)
            self.assertTrue(result["passed"])
            self.assertEqual(result["changed_ratio"], 0.0)

    @unittest.skipUnless(shutil.which("compare") and shutil.which("magick"), "ImageMagick is unavailable")
    def test_different_image_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            first = Path(temporary) / "first.png"
            second = Path(temporary) / "second.png"
            subprocess.run(["magick", "-size", "16x16", "xc:black", str(first)], check=True)
            subprocess.run(["magick", "-size", "16x16", "xc:white", str(second)], check=True)
            result = MODULE.compare_images(first, second)
            self.assertFalse(result["passed"])
            self.assertGreater(result["changed_ratio"], 0.99)

    @unittest.skipUnless(shutil.which("magick"), "ImageMagick is unavailable")
    def test_black_image_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = Path(temporary) / "black.png"
            subprocess.run(["magick", "-size", "16x16", "xc:black", str(image)], check=True)
            result = MODULE.inspect_image(image)
            self.assertFalse(result["passed"])
            self.assertEqual(result["mean"], 0.0)


if __name__ == "__main__":
    unittest.main()
