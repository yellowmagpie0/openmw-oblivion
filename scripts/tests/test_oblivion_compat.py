#!/usr/bin/env python3

import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest

from pathlib import Path


SOURCE = Path(__file__).resolve().parents[2]
SCRIPT = SOURCE / "scripts" / "oblivion_compat.py"
SPEC = importlib.util.spec_from_file_location("oblivion_compat", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class OblivionCompatTests(unittest.TestCase):
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
