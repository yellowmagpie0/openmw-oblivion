"""Bounded normal-input feedback loops must stop and release their controls."""
import copy
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from test_oblivion_compat import MODULE


class ScenarioRepeatTests(unittest.TestCase):
    def action(self, **changes):
        return dict(type="repeat_until_log", pattern="route done", maximum_iterations=2,
                    timeout_seconds=10, actions=[dict(type="key_hold", value="w", seconds=.5)],
                    cleanup_actions=[dict(type="key_up", value="w")], **changes)

    def run_loop(self, output, action, callback):
        replay = mock.Mock()
        replay.action.side_effect = callback
        result = MODULE._run_action(action, environment={}, output=output, offscreen_input=replay)
        return result, replay

    def test_existing_completion_sends_only_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "process.log").write_text("route done\n")
            result, replay = self.run_loop(output, self.action(), lambda a: dict(type=a["type"], passed=True))
            self.assertTrue(result["passed"])
            self.assertEqual(result["iterations"], [])
            replay.action.assert_called_once_with(dict(type="key_up", value="w"))

    def test_final_iteration_completion_and_nested_input_receipts(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            count = 0

            def deliver(action):
                nonlocal count
                count += 1
                with (output / "process.log").open("a") as log:
                    log.write("route " if count == 1 else "done\n")
                return dict(type=action["type"], passed=True, first_input_sequence=count)

            result, replay = self.run_loop(output, self.action(), deliver)
            self.assertTrue(result["passed"])
            self.assertEqual(len(result["iterations"]), 2)
            self.assertEqual(result["iterations"][1][0]["first_input_sequence"], 2)
            self.assertEqual(replay.action.call_count, 3)

    def test_fresh_gate_does_not_skip_inputs_for_old_completion(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            log = output / "process.log"
            log.write_text("route done\n")

            def deliver(action):
                with log.open("a") as stream:
                    stream.write("route done\n")
                return dict(type=action["type"], passed=True)

            result, _ = self.run_loop(output, self.action(after_start=True), deliver)
            self.assertTrue(result["passed"])
            self.assertEqual(len(result["iterations"]), 1)
            self.assertEqual(result["start_offset"], len("route done\n"))

    def test_iteration_bound_fails_without_completion_and_releases_input(self):
        with tempfile.TemporaryDirectory() as directory:
            result, replay = self.run_loop(Path(directory), self.action(), lambda a: dict(type=a["type"], passed=True))
            self.assertFalse(result["passed"])
            self.assertEqual(result["reason"], "maximum_iterations")
            self.assertEqual(len(result["iterations"]), 2)
            self.assertEqual(replay.action.call_args.args[0], dict(type="key_up", value="w"))

    def test_failed_observation_stops_body_and_runs_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            action = self.action()
            action["actions"].insert(0, dict(type="wait_log", pattern="missing", timeout_seconds=.002))
            result, replay = self.run_loop(Path(directory), action, lambda a: dict(type=a["type"], passed=True))
            self.assertFalse(result["passed"])
            self.assertEqual(result["reason"], "child_failed")
            self.assertEqual(len(result["iterations"][0]), 1)
            replay.action.assert_called_once_with(dict(type="key_up", value="w"))

    def test_delivery_exception_still_releases_input(self):
        with tempfile.TemporaryDirectory() as directory:
            calls = []

            def deliver(action):
                calls.append(action["type"])
                if action["type"] == "key_hold":
                    raise RuntimeError("receipt missing")
                return dict(type=action["type"], passed=True)

            with self.assertRaisesRegex(RuntimeError, "receipt missing"):
                self.run_loop(Path(directory), self.action(), deliver)
            self.assertEqual(calls, ["key_hold", "key_up"])

    def test_late_completion_does_not_override_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            now = [0.0]

            def deliver(action):
                now[0] = 20.0
                (output / "process.log").write_text("route done\n")
                return dict(type=action["type"], passed=True)

            with mock.patch.object(MODULE.time, "monotonic", side_effect=lambda: now[0]):
                result, replay = self.run_loop(output, self.action(), deliver)
            self.assertFalse(result["passed"])
            self.assertEqual(result["reason"], "timeout")
            self.assertEqual(replay.action.call_count, 2)

    def test_cleanup_failure_prevents_success(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "process.log").write_text("route done\n")
            result, _ = self.run_loop(output, self.action(), lambda a: dict(type=a["type"], passed=False))
            self.assertFalse(result["passed"])
            self.assertFalse(result["cleanup_actions"][0]["passed"])

    def test_preflight_checks_bounds_scope_and_nested_controls(self):
        base = dict(schema_version=1, name="normal input feedback", command=["openmw"],
                    sdl_xvfb_input=True, xvfb=True, actions=[self.action()])
        MODULE.validate_scenario_manifest(base)
        for field, value in [("maximum_iterations", 0), ("maximum_iterations", True),
                             ("timeout_seconds", 0), ("timeout_seconds", float("nan")),
                             ("pattern", "["), ("after_start", 1), ("path", "../outside.log"),
                             ("path", ""), ("actions", []), ("cleanup_actions", None),
                             ("actions", [dict(type="key_hold", value="shift", seconds=.5)]),
                             ("actions", [dict(type="command", command=["SetHealth", "100"])]),
                             ("actions", [self.action()])]:
            manifest = copy.deepcopy(base)
            manifest["actions"][0][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                MODULE.validate_scenario_manifest(manifest)
        with self.assertRaisesRegex(ValueError, "isolated SDL"):
            MODULE.validate_scenario_manifest(dict(base, sdl_xvfb_input=False, xvfb=False))


if __name__ == "__main__":
    unittest.main()
