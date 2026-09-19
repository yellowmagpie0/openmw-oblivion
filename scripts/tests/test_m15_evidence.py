import copy
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import oblivion_compat as compat
import tes4_m15_evidence as m15


PLAYER = "dynamic:player:0000000000000001"
VICTIM = "content:fixture.esm:000010"
OTHER = "content:fixture.esm:000011"


def manifest():
    return {
        "schema_version": 1, "name": "m15-contract", "command": ["{openmw}"],
        "files": [{"path": "config/openmw.cfg", "content": ""},
                  {"path": "config/settings.cfg", "content": ""}],
        "m15": {
            "version": 1, "kind": "synthetic", "seed": 15, "audio": False, "event_file": "events.jsonl",
            "inputs": [{"path": "fixture.esm", "sha256": "a" * 64, "role": "content"}],
            "cases": [{"id": "hit", "actor": PLAYER, "target": VICTIM, "action_id": "swing-1",
                       "events": ["input", "contact", "damage"], "before": "before", "after": "after",
                       "deltas": [{"actor": VICTIM, "path": ["health"], "event_field": "health", "expected": -10}]}],
            "artifacts": [{"path": "capture.png", "kind": "image"},
                          {"path": "snapshots/after.json", "kind": "snapshot"}],
        },
        "actions": [
            {"type": "m15_snapshot", "phase": "setup", "name": "before", "save": "userdata/quick.omwsave"},
            {"type": "key", "phase": "exercise", "value": "w"},
            {"type": "m15_snapshot", "phase": "observe", "name": "after", "save": "userdata/quick.omwsave"},
            {"type": "screenshot", "phase": "observe", "name": "capture.png"},
        ],
    }


def evidence():
    events = []
    for index, kind in enumerate(["run-start", "input", "contact", "damage", "run-end"], 1):
        event = {"run_id": "fresh", "epoch": 1, "pid": 42, "sequence": index, "tick": index, "event": kind}
        if kind not in ("run-start", "run-end"):
            event.update(actor=PLAYER, target=VICTIM, action_id="swing-1", cause="normal-input", result=kind)
        events.append(event)
    events[3]["deltas"] = {"health": -10}
    events[-1].update(event_count=4, error_count=0, unsupported_count=0, pending_count=0)
    state = {"player": {"reference": PLAYER, "actor_values": {"health": 100}},
             "references": [{"key": VICTIM, "health": 100}, {"key": OTHER, "health": 100}]}
    snapshots = {
        "before": {"run_id": "fresh", "epoch": 1, "ordinal": 0, "event_sequence": 1, "state": copy.deepcopy(state)},
        "after": {"run_id": "fresh", "epoch": 1, "ordinal": 1, "event_sequence": 4, "state": copy.deepcopy(state)},
    }
    snapshots["after"]["state"]["references"][0]["health"] = 90
    return events, snapshots


class M15ManifestTests(unittest.TestCase):
    def test_empty_m15_block_is_not_silently_ignored(self):
        with self.assertRaisesRegex(ValueError, "M15"):
            compat.validate_scenario_manifest({
                "schema_version": 1, "name": "empty-m15",
                "command": ["{openmw}"], "m15": {},
            })

    def test_well_typed_contract_is_accepted(self):
        compat.validate_scenario_manifest(manifest())

    def test_schema_rejects_nested_unknown_fields_wrong_types_and_nonfinite_values(self):
        mutations = [
            lambda x: x["m15"].update(seed=True),
            lambda x: x["m15"].update(seed=0),
            lambda x: x["m15"].update(audio="yes"),
            lambda x: x["m15"].update(version=2),
            lambda x: x["m15"].update(kind="campaign-ish"),
            lambda x: x["m15"].update(cases=[]),
            lambda x: x["m15"]["cases"][0].update(typo="discard me"),
            lambda x: x["m15"]["cases"][0]["deltas"][0].update(expected=float("nan")),
            lambda x: x["m15"]["cases"][0]["deltas"][0].update(tolerance=-1),
            lambda x: x["m15"]["cases"][0]["deltas"][0].update(path=[]),
            lambda x: x["m15"]["cases"][0]["deltas"][0].update(actor=OTHER),
            lambda x: x["m15"]["cases"][0].update(target="oblivion.esm:10"),
            lambda x: x["actions"][1].update(command=["sh", "-c", "true"]),
        ]
        for mutation in mutations:
            value = manifest()
            mutation(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                compat.validate_scenario_manifest(value)

    def test_console_generic_commands_and_hidden_input_are_rejected(self):
        for kind in ("type", "type_held", "command", "m14_console", "m14_advance_clock", "m14_obstruction"):
            value = manifest()
            value["actions"][1] = {"type": kind, "phase": "exercise", "value": "player.kill"}
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                compat.validate_scenario_manifest(value)
        for key in ("grave", "quoteleft", "ctrl+grave", "F12", "w Return", "0x29"):
            value = manifest()
            value["actions"][1]["value"] = key
            with self.subTest(key=key), self.assertRaises(ValueError):
                compat.validate_scenario_manifest(value)

    def test_setup_cannot_be_reentered_and_observe_cannot_send_input(self):
        for phase in ("setup", "observe"):
            value = manifest()
            value["actions"].append({"type": "key", "phase": phase, "value": "w"})
            with self.subTest(phase=phase), self.assertRaises(ValueError):
                compat.validate_scenario_manifest(value)

    def test_injection_environment_and_fake_evidence_files_are_rejected(self):
        for key in ("OPENMW_OBSCRIPT_EVENTS", "OPENMW_M15_EVENTS", "LD_PRELOAD", "PYTHONPATH"):
            value = manifest()
            value["environment"] = {key: "fake-success"}
            with self.subTest(key=key), self.assertRaises(ValueError):
                compat.validate_scenario_manifest(value)
        for path in ("events.jsonl", "snapshots/after.json", "config/input_v3.xml", "../fake", "/tmp/fake"):
            value = manifest()
            value["files"].append({"path": path, "content": "fake"})
            with self.subTest(path=path), self.assertRaises(ValueError):
                compat.validate_scenario_manifest(value)

    def test_duplicate_cases_snapshots_and_reserved_paths_are_rejected(self):
        for mutate in (
            lambda v: v["m15"]["cases"].append(copy.deepcopy(v["m15"]["cases"][0])),
            lambda v: v["actions"].append(copy.deepcopy(v["actions"][2])),
            lambda v: v["m15"].update(event_file="process.log"),
            lambda v: v["m15"].update(event_file="../stale.jsonl"),
            lambda v: v["m15"]["cases"][0].update(after="missing"),
        ):
            value = manifest()
            mutate(value)
            with self.assertRaises(ValueError):
                compat.validate_scenario_manifest(value)

    def test_audio_case_cannot_use_no_sound_or_timeout_as_success(self):
        value = manifest()
        value["m15"]["audio"] = True
        value["command"].append("--no-sound=1")
        with self.assertRaisesRegex(ValueError, "sound"):
            compat.validate_scenario_manifest(value)
        value = manifest()
        value["expected_exit"] = "timeout"
        with self.assertRaises(ValueError):
            compat.validate_scenario_manifest(value)


class M15CausalEvidenceTests(unittest.TestCase):
    def test_canonical_subset_preserves_metadata_types_and_requires_fields(self):
        events, snapshots = evidence()
        case = manifest()["m15"]["cases"][0]
        metadata = {"owner": PLAYER, "stolen": True, "items": [{"count": 1, "charge": None}]}
        snapshots["after"]["state"]["references"][0]["inventory"] = copy.deepcopy(metadata)
        case["assertions"] = [{"actor": VICTIM, "path": ["inventory"], "snapshot": "after",
                               "mode": "exact", "expected": metadata}]
        self.assertTrue(self.evaluate(events, snapshots, [case]))
        for changed in ({**metadata, "stolen": 1}, {**metadata, "extra": 0},
                        {**metadata, "items": [{"count": 1}]}, {}, None):
            with self.subTest(changed=changed):
                snapshots["after"]["state"]["references"][0]["inventory"] = changed
                self.assertFalse(self.evaluate(events, snapshots, [case]))
        case["assertions"][0].update(path=["absent"], expected=None)
        self.assertFalse(self.evaluate(events, snapshots, [case]))

    def test_numeric_subset_requires_explicit_finite_tolerance(self):
        events, snapshots = evidence()
        case = manifest()["m15"]["cases"][0]
        assertion = {"actor": VICTIM, "path": ["health"], "snapshot": "after",
                     "mode": "numeric", "expected": 90.1, "tolerance": 0.2}
        case["assertions"] = [assertion]
        self.assertTrue(self.evaluate(events, snapshots, [case]))
        for value in (0, -1, float("nan"), float("inf"), True):
            assertion["tolerance"] = value
            self.assertFalse(self.evaluate(events, snapshots, [case]))

    def test_subset_schema_rejects_exclusions_and_unscoped_or_duplicate_assertions(self):
        source = manifest()
        assertion = {"actor": VICTIM, "path": ["health"], "snapshot": "after",
                     "mode": "numeric", "expected": 90, "tolerance": 0}
        source["m15"]["cases"][0]["assertions"] = [assertion]
        compat.validate_scenario_manifest(source)
        for change in ({"exclude": ["owner"]}, {"actor": OTHER}, {"snapshot": "missing"},
                       {"mode": "exact"}, {"tolerance": None}):
            candidate = copy.deepcopy(source)
            candidate["m15"]["cases"][0]["assertions"][0].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                compat.validate_scenario_manifest(candidate)
        source["m15"]["cases"][0]["assertions"].append(copy.deepcopy(assertion))
        with self.assertRaises(ValueError):
            compat.validate_scenario_manifest(source)

    def evaluate(self, events, snapshots, cases=None):
        return m15.evaluate_cases(cases or manifest()["m15"]["cases"], events, snapshots, "fresh", 1)[0]["passed"]

    def test_matching_causality_and_independent_state_delta_pass(self):
        events, snapshots = evidence()
        m15.validate_events(events, "fresh", 1, 42)
        self.assertTrue(self.evaluate(events, snapshots))

    def test_missing_wrong_actor_target_action_order_and_duplicate_contacts_fail(self):
        for change in ("empty", "actor", "target", "action_id", "order", "duplicate"):
            events, snapshots = evidence()
            if change == "empty":
                events = []
            elif change in ("actor", "target", "action_id"):
                events[2][change] = "wrong"
            elif change == "order":
                events[1], events[2] = events[2], events[1]
            else:
                events.insert(3, copy.deepcopy(events[2]))
            with self.subTest(change=change):
                self.assertFalse(self.evaluate(events, snapshots))

    def test_incident_projectile_and_transaction_identity_are_scoped(self):
        for name in ("incident_id", "projectile_id", "transaction_id"):
            events, snapshots = evidence()
            case = copy.deepcopy(manifest()["m15"]["cases"])
            case[0][name] = "correct-1"
            for event in events[1:-1]:
                event[name] = "correct-1"
            self.assertTrue(self.evaluate(events, snapshots, case))
            events[2][name] = "wrong-1"
            self.assertFalse(self.evaluate(events, snapshots, case))

    def test_unchanged_wrong_victim_and_missing_state_fail_even_with_correct_telemetry(self):
        for change in ("unchanged", "other-victim", "missing", "nonfinite", "boolean", "duplicate"):
            events, snapshots = evidence()
            actors = snapshots["after"]["state"]["references"]
            if change == "unchanged":
                actors[0]["health"] = 100
            elif change == "other-victim":
                actors[0]["health"] = 100
                actors[1]["health"] = 90
            elif change == "missing":
                del actors[0]["health"]
            elif change == "nonfinite":
                actors[0]["health"] = float("nan")
            elif change == "boolean":
                actors[0]["health"] = True
            else:
                actors.append(copy.deepcopy(actors[0]))
            with self.subTest(change=change):
                self.assertFalse(self.evaluate(events, snapshots))

    def test_wrong_telemetry_and_stale_or_reversed_snapshots_fail(self):
        for change in ("telemetry", "run_id", "epoch", "ordinal", "event_sequence"):
            events, snapshots = evidence()
            if change == "telemetry":
                events[3]["deltas"]["health"] = -999
            else:
                snapshots["after"][change] = {"run_id": "old", "epoch": 2, "ordinal": 0, "event_sequence": 2}[change]
            with self.subTest(change=change):
                self.assertFalse(self.evaluate(events, snapshots))

    def test_harmless_zero_delta_control_requires_real_actor_and_matching_zero_telemetry(self):
        events, snapshots = evidence()
        snapshots["after"]["state"] = copy.deepcopy(snapshots["before"]["state"])
        events[3]["deltas"]["health"] = 0
        cases = manifest()["m15"]["cases"]
        cases[0]["deltas"][0]["expected"] = 0
        self.assertTrue(self.evaluate(events, snapshots, cases))
        snapshots["after"]["state"]["references"] = []
        self.assertFalse(self.evaluate(events, snapshots, cases))

    def test_stream_freshness_complete_summary_monotonicity_and_nonzero_errors(self):
        for change in ("old-run", "pid", "epoch", "bool-epoch", "sequence", "tick", "summary", "count",
                       "pending_count", "unsupported_count", "error_count", "missing-count", "cause", "result"):
            events, _ = evidence()
            if change == "old-run":
                for event in events:
                    event["run_id"] = "previous"
            elif change in ("pid", "epoch"):
                events[2][change] += 1
            elif change == "bool-epoch":
                events[2]["epoch"] = True
            elif change == "sequence":
                events[2]["sequence"] = 2
            elif change == "tick":
                events[2]["tick"] = 0
            elif change == "summary":
                events.pop()
            elif change == "count":
                events[-1]["event_count"] = 999
            elif change == "missing-count":
                del events[-1]["error_count"]
            elif change in ("cause", "result"):
                del events[2][change]
            else:
                events[-1][change] = 1
            with self.subTest(change=change), self.assertRaises(ValueError):
                m15.validate_events(events, "fresh", 1, 42)

    def test_json_duplicate_keys_nonfinite_and_truncated_streams_fail(self):
        for data in ('{"key":1,"key":2}', '{"value":NaN}', '{"value":Infinity}'):
            with self.assertRaises(ValueError):
                m15.parse_json(data)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events"
            path.write_text('{"event":"run-end"}')
            with self.assertRaisesRegex(ValueError, "truncated"):
                m15.read_events(path)

    def test_schema_keywords_cannot_silently_weaken_a_contract(self):
        with self.assertRaisesRegex(ValueError, "unsupported contract"):
            m15._shape({}, {"imaginaryConstraint": True}, "test")


class M15SessionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.engine = self.root / "openmw"
        self.engine.write_bytes(b"test external boundary, not engine acceptance")
        self.content = self.root / "fixture.esm"
        self.content.write_bytes(b"hermetic input fingerprint")
        self.output = self.root / "run"
        self.value = manifest()
        self.value["command"] = [str(self.engine), "--replace=config", "--config", str(self.output / "config")]
        self.value["files"][0]["content"] = (
            f'replace=config\nreplace=data\ndata="{self.root}"\ncontent=fixture.esm\n'
            f'user-data="{self.output}/userdata"\n')
        self.value["m15"]["inputs"][0].update(path=str(self.content), sha256=m15.digest(self.content))

    def session(self):
        return m15.Session(self.value, self.output, self.engine)

    def test_wrong_executable_launch_flags_config_hooks_and_input_hash_are_rejected(self):
        original = copy.deepcopy(self.value)
        for change in ("executable", "flag", "config", "binding", "hash", "userdata"):
            self.value = copy.deepcopy(original)
            if change == "executable":
                self.value["command"][0] = "/bin/sh"
            elif change == "flag":
                self.value["command"].append("--script-run=fake.txt")
            elif change == "config":
                self.value["files"][0]["content"] += 'script-run=fake.txt\n'
            elif change == "binding":
                self.value["files"][1]["content"] = '[Input]\nconsole = w\n'
            elif change == "hash":
                self.value["m15"]["inputs"][0]["sha256"] = "0" * 64
            else:
                self.value["files"][0]["content"] = self.value["files"][0]["content"].replace(str(self.output), str(self.root))
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.session()

    def test_inherited_script_and_preload_injection_are_removed(self):
        with mock.patch.dict(os.environ, {"OPENMW_OBSCRIPT_EVENTS": "fake.txt", "LD_PRELOAD": "fake.so"}):
            session = self.session()
            environment = session.environment()
        self.assertNotIn("OPENMW_OBSCRIPT_EVENTS", environment)
        self.assertNotIn("LD_PRELOAD", environment)
        self.assertEqual(environment["OPENMW_M15_RUN_ID"], session.run_id)
        self.assertEqual(session.command[-1], "--random-seed=15")
        self.assertNotIn("--random-seed=15", self.value["command"])

    def prepare_snapshot(self):
        session = self.session()
        session.start(42)
        save = self.output / "userdata/quick.omwsave"
        save.parent.mkdir()
        save.write_bytes(b"actual saved bytes")
        state = evidence()[1]["after"]["state"]
        live = self.output / "live-1-2.json"
        live.write_text(json.dumps(state))
        boundary = {"event": "save-complete", "run_id": session.run_id, "pid": 42, "epoch": 1,
                    "sequence": 2, "save": str(save), "save_sha256": m15.digest(save),
                    "live": live.name, "live_sha256": m15.digest(live)}
        event_file = self.output / "events.jsonl"
        event_file.write_text(json.dumps(boundary) + "\n")
        action = {"type": "m15_snapshot", "phase": "observe", "name": "before",
                  "save": "userdata/quick.omwsave"}
        return session, state, boundary, event_file, action

    def test_snapshot_requires_completed_save_and_independent_live_agreement(self):
        session, state, boundary, _, action = self.prepare_snapshot()
        with mock.patch.object(m15.tes4_runtime_state, "load_save", return_value=state):
            self.assertTrue(session.snapshot(action)["passed"])
            self.assertEqual(session.snapshots["before"]["event_sequence"], boundary["sequence"])
            with self.assertRaisesRegex(ValueError, "reuse"):
                session.snapshot({**action, "name": "after"})

    def test_snapshot_missing_stale_changed_ack_or_changed_live_state_fails(self):
        # Each negative case gets fresh output, just as a real course must.
        for fault in ("missing", "save_hash", "epoch", "pid", "run_id", "live_hash", "live_state"):
            with self.subTest(fault=fault):
                self.output = self.root / fault
                self.value["command"][-1] = str(self.output / "config")
                self.value["files"][0]["content"] = (
                    f'replace=config\nreplace=data\nuser-data={self.output}/userdata\n'
                    f'data={self.root}\ncontent=fixture.esm\n')
                session, state, boundary, events, action = self.prepare_snapshot()
                if fault == "save_hash": boundary["save_sha256"] = "0" * 64
                elif fault == "epoch": boundary["epoch"] = 2
                elif fault == "pid": boundary["pid"] = 99
                elif fault == "run_id": boundary["run_id"] = "old"
                elif fault == "live_hash": boundary["live_sha256"] = "0" * 64
                elif fault == "live_state": state["references"][0]["health"] += 1
                events.write_text("" if fault == "missing" else json.dumps(boundary) + "\n")
                with mock.patch.object(m15.tes4_runtime_state, "load_save", return_value=state):
                    with self.assertRaises(ValueError):
                        session.snapshot(action)

    def test_empty_world_cannot_satisfy_declared_fixture_reference(self):
        session, state, boundary, events, action = self.prepare_snapshot()
        session.settings["required_references"] = [{"key": VICTIM, "base": OTHER,
            "cell": "content:fixture.esm:000001", "enabled": True, "deleted": False}]
        state["references"] = []
        live = self.output / boundary["live"]
        live.write_text(json.dumps(state))
        boundary["live_sha256"] = m15.digest(live)
        events.write_text(json.dumps(boundary) + "\n")
        # A hash-consistent save/live pair is still insufficient if the named
        # world fixture is absent. This is independent of causal event counts.
        with mock.patch.object(m15.tes4_runtime_state, "load_save", return_value=state):
            with self.assertRaisesRegex(ValueError, "named actor"):
                session.snapshot(action)

    def test_declared_world_reference_requires_exact_base_cell_and_enabled_state(self):
        session, state, boundary, events, action = self.prepare_snapshot()
        required = {"key": VICTIM, "base": OTHER, "cell": "content:fixture.esm:000001",
                    "enabled": True, "deleted": False}
        session.settings["required_references"] = [required]
        state["references"][0].update(required)
        live = self.output / boundary["live"]
        live.write_text(json.dumps(state))
        boundary["live_sha256"] = m15.digest(live)
        events.write_text(json.dumps(boundary) + "\n")
        with mock.patch.object(m15.tes4_runtime_state, "load_save", return_value=state):
            self.assertTrue(session.snapshot(action)["passed"])

    def test_reused_outputs_and_symlink_escape_are_rejected(self):
        self.output.mkdir()
        (self.output / "old-events").write_text("old")
        with self.assertRaisesRegex(ValueError, "empty"):
            self.session()
        (self.output / "escape").symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            m15.output_path(self.output, "escape/outside")

    def test_freshness_uses_filesystem_epoch_and_still_rejects_stale_capture(self):
        with mock.patch.object(m15.time, "time_ns", return_value=10**20):
            session = self.session()
            session.start(42)
        fresh = self.output / "fresh.png"
        fresh.write_bytes(b"new capture")
        session._receipt("fresh.png")
        stale = self.output / "stale.png"
        stale.write_bytes(b"old capture")
        old = session.filesystem_start_ns - 1_000_000_000
        os.utime(stale, ns=(old, old))
        with self.assertRaisesRegex(ValueError, "stale"):
            session._receipt("stale.png")

    def prepare_evidence(self):
        session = self.session()
        session.start(42)
        events, snapshots = evidence()
        for event in events:
            event["run_id"] = session.run_id
        for snapshot in snapshots.values():
            snapshot["run_id"] = session.run_id
        session.snapshots = snapshots
        (self.output / "events.jsonl").write_text(''.join(json.dumps(e) + '\n' for e in events))
        for artifact in self.value["m15"]["artifacts"]:
            path = self.output / artifact["path"]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"mocked capture boundary; not visual acceptance")
            session._receipt(artifact["path"])
        return session

    def test_aggregate_requires_every_case_artifact_and_unchanged_inputs(self):
        session = self.prepare_evidence()
        result = session.finish("", True)
        self.assertTrue(result["passed"])
        self.assertEqual((result["case_count"], result["passed_count"]), (1, 1))
        (self.output / "capture.png").write_bytes(b"replaced screenshot")
        self.assertFalse(session.finish("", True)["passed"])

    def test_early_exit_and_engine_diagnostics_fail_even_with_correct_mocked_evidence(self):
        session = self.prepare_evidence()
        self.assertFalse(session.finish("", False)["passed"])
        for diagnostic in ("Error in frame", "deferred command=startcombat", "AddressSanitizer: failure",
                           "runtime error: bad access", "Unsupported ObScript command"):
            with self.subTest(diagnostic=diagnostic):
                self.assertFalse(session.finish(diagnostic, True)["passed"])

    def test_missing_summary_and_missing_required_capture_fail_closed(self):
        session = self.prepare_evidence()
        session.receipts.pop("capture.png")
        self.assertFalse(session.finish("", True)["passed"])
        (self.output / "events.jsonl").write_text("")
        self.assertFalse(session.finish("", True)["passed"])

    def test_initial_content_is_rechecked_after_execution(self):
        session = self.prepare_evidence()
        self.content.write_bytes(b"edited fixture")
        self.assertFalse(session.finish("", True)["passed"])


class M15TestInventoryTests(unittest.TestCase):
    def test_missing_filter_skipped_disabled_failed_and_duplicate_cases_cannot_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tests.xml"
            template = '<testsuites tests="1"><testcase classname="Combat" name="hit" status="run" result="completed">{}</testcase></testsuites>'
            path.write_text(template.format(""))
            self.assertTrue(m15.validate_test_results(path, ["Combat.hit"])["passed"])
            self.assertFalse(m15.validate_test_results(path, ["Combat.missing"])["passed"])
            self.assertFalse(m15.validate_test_results(path, [])["passed"])
            for child in ('<failure/>', '<skipped/>', '<error/>'):
                path.write_text(template.format(child))
                self.assertFalse(m15.validate_test_results(path, ["Combat.hit"])["passed"])
            path.write_text(template.format("").replace('status="run"', 'status="notrun"'))
            self.assertFalse(m15.validate_test_results(path, ["Combat.hit"])["passed"])
            path.write_text('<testsuites tests="0"/>')
            self.assertFalse(m15.validate_test_results(path, ["Combat.hit"])["passed"])


class M15ReplayTests(unittest.TestCase):
    setUp = M15SessionTests.setUp
    session = M15SessionTests.session

    def completed_run(self, restart_from=None):
        import struct
        from test_tes4_runtime_state import make_state
        case = self.value["m15"]["cases"][0]
        case.update(actor=PLAYER, target=PLAYER, action_id="observation-3", events=["save-complete"])
        case["deltas"] = [{"actor": PLAYER, "path": ["actor_values", "health.current"],
                           "event_field": "health", "expected": 0}]
        session = m15.Session(self.value, self.output, self.engine, restart_from)
        session.start(43 if restart_from else 42)
        (self.output / "process.log").write_text("normal mock engine boundary\n")
        payload = m15.tes4_runtime_state.encode_payload(make_state())
        subs = b"VERS" + struct.pack("<II", 4, 4) + b"DATA" + struct.pack("<I", len(payload)) + payload
        saved = b"T4ST" + struct.pack("<III", len(subs), 0, 0) + subs
        source = self.output / "userdata/quick.omwsave"
        source.parent.mkdir(exist_ok=True)
        source.write_bytes(saved)
        state = m15.tes4_runtime_state.load_save(source)
        common = {"run_id": session.run_id, "epoch": session.epoch, "pid": session.pid}
        events = [{**common, "event": "run-start", "sequence": 1, "tick": 0}]
        for index, name in enumerate(("before", "after"), 2):
            live = self.output / f"live-1-{index}.json"
            live.write_text(json.dumps(state))
            boundary = "load-complete" if restart_from and name == "before" else "save-complete"
            events.append({**common, "event": boundary, "sequence": index, "tick": index,
                           "actor": PLAYER, "target": PLAYER, "action_id": f"observation-{index}",
                           "cause": "normal-save", "result": "committed", "deltas": {"health": 0},
                           "save": str(source), "save_sha256": m15.digest(source),
                           "live": live.name, "live_sha256": m15.digest(live)})
            (self.output / "events.jsonl").write_text(''.join(json.dumps(e) + '\n' for e in events))
            session.snapshot({"type": "m15_snapshot", "phase": "observe", "name": name,
                              "save": "userdata/quick.omwsave", "boundary": boundary})
        events.append({**common, "event": "run-end", "sequence": 4, "tick": 3,
                       "event_count": 3, "error_count": 0, "unsupported_count": 0, "pending_count": 0})
        (self.output / "events.jsonl").write_text(''.join(json.dumps(e) + '\n' for e in events))
        # GUI capture is mocked here; the actual course independently exercises
        # ImageMagick inspection and the renderer. This tests receipt replay.
        (self.output / "capture.png").write_bytes(b"mock capture")
        session.record_action({"type": "screenshot", "phase": "observe", "name": "capture.png"}, {"passed": True})
        result = session.finish("normal mock engine boundary\n", True)
        self.assertTrue(result["passed"], result)
        recorded = {"passed": True, "actions_complete": True, "timed_out": False, "exit_code": 0,
                    "command": session.command, "m15": result,
                    "actions": [{"type": a["type"], "phase": a["phase"], "passed": True}
                                for a in self.value["actions"]]}
        (self.output / "scenario.json").write_text(json.dumps(recorded))
        return recorded

    def test_replay_decodes_native_save_and_accepts_consistent_captures(self):
        self.completed_run()
        result = m15.verify_run(self.output)
        self.assertTrue(result["passed"], result)
        self.assertEqual(result["case_count"], 1)

    def test_copied_evidence_rejects_edited_save_screenshot_state_log_events_and_metadata(self):
        import shutil
        self.completed_run()
        for name in ("snapshots/after.omwsave", "capture.png", "snapshots/after.json", "live-1-3.json",
                     "process.log", "events.jsonl", "m15-manifest.json", "m15-session.json"):
            with self.subTest(name=name):
                target = self.root / ("changed-" + name.replace("/", "-"))
                shutil.copytree(self.output, target)
                with (target / name).open("ab") as stream:
                    stream.write(b" ")
                self.assertFalse(m15.verify_run(target)["passed"])
        self.assertTrue(m15.verify_run(self.output)["passed"])

    def test_missing_receipts_skipped_actions_wrong_launch_and_early_exit_fail(self):
        recorded = self.completed_run()
        for change in ("receipt", "skip", "phase", "command", "exit", "timeout", "count"):
            changed = copy.deepcopy(recorded)
            if change == "receipt": del changed["m15"]["receipts"]["capture.png"]
            elif change == "skip": changed["actions"].pop()
            elif change == "phase": changed["actions"][1]["phase"] = "setup"
            elif change == "command": changed["command"].append("--script-run=hidden.txt")
            elif change == "exit": changed["exit_code"] = 1
            elif change == "timeout": changed["timed_out"] = True
            else: changed["m15"]["case_count"] = 0
            (self.output / "scenario.json").write_text(json.dumps(changed))
            with self.subTest(change=change):
                self.assertFalse(m15.verify_run(self.output)["passed"])

    def test_replay_rejects_stale_capture_even_if_its_bytes_match(self):
        self.completed_run()
        os.utime(self.output / "capture.png", ns=(1, 1))
        result = m15.verify_run(self.output)
        self.assertFalse(result["passed"])
        self.assertIn("predates", result["failures"][0])

    def test_bad_manifest_cli_exits_nonzero_before_creating_a_run(self):
        import subprocess
        bad = self.root / "bad.json"
        bad.write_text(json.dumps({"schema_version": 1, "name": "bad", "command": ["/bin/true"], "m15": {}}))
        result = subprocess.run([sys.executable, compat.__file__, "scenario", str(bad),
                                 "--output", str(self.output)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.output.exists())
        self.assertIn("M15", result.stderr)


class M15RestartTests(unittest.TestCase):
    setUp = M15SessionTests.setUp
    session = M15SessionTests.session
    completed_run = M15ReplayTests.completed_run

    def prepare_restart(self):
        self.completed_run()
        previous = self.output
        self.output = self.root / "second"
        self.value["command"][-1] = str(self.output / "config")
        self.value["files"][0]["content"] = self.value["files"][0]["content"].replace(str(previous), str(self.output))
        for action in self.value["actions"]:
            if action["phase"] in ("setup", "exercise"):
                action["phase"] = "restart-continuation"
        return previous

    def test_restart_transfers_verified_bytes_and_assigns_fresh_epoch(self):
        previous = self.prepare_restart()
        session = m15.Session(self.value, self.output, self.engine, (previous, "after"))
        prior = json.loads((previous / "m15-session.json").read_text())
        self.assertEqual((session.run_id, session.epoch), (prior["run_id"], 2))
        self.assertEqual((self.output / "restart-input.omwsave").read_bytes(),
                         (previous / "snapshots/after.omwsave").read_bytes())
        self.assertEqual(session.command[-1], "--load-savegame=" + str(self.output / session.restart["save"]))
        with self.assertRaisesRegex(ValueError, "different process"):
            session.start(42)
        session.start(43)
        self.assertEqual(session.environment()["OPENMW_M15_EPOCH"], "2")

    def test_continuation_cannot_launch_without_verified_first_process(self):
        previous = self.prepare_restart()
        with self.assertRaisesRegex(ValueError, "restart driver"):
            self.session()
        (previous / "snapshots/after.omwsave").write_bytes(b"changed save")
        with self.assertRaisesRegex(ValueError, "verification"):
            m15.Session(self.value, self.output, self.engine, (previous, "after"))
        self.assertFalse(self.output.exists())

    def test_restart_rejects_changed_engine_and_snapshot_path_escape(self):
        previous = self.prepare_restart()
        for name in ("../after", "", "after/extra"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "snapshot name"):
                m15.Session(self.value, self.output, self.engine, (previous, name))
        self.engine.write_bytes(b"changed executable")
        with self.assertRaisesRegex(ValueError, "same engine"):
            m15.Session(self.value, self.output, self.engine, (previous, "after"))

    def completed_pair(self):
        self.output = self.root / "first"
        self.value["command"][-1] = str(self.output / "config")
        self.value["files"][0]["content"] = self.value["files"][0]["content"].replace(str(self.root / "run"), str(self.output))
        previous = self.prepare_restart()
        self.value["actions"][0]["boundary"] = "load-complete"
        self.completed_run((previous, "after"))
        return previous

    def test_complete_pair_replays_and_requires_load_before_resave(self):
        self.completed_pair()
        result = m15.verify_restart(self.root, "after", "before", "after")
        self.assertTrue(result["passed"], result)
        for source, loaded, final in (("before", "before", "after"), ("after", "after", "before"),
                                      ("after", "missing", "after"), ("after", "before", "before")):
            with self.subTest(source=source, loaded=loaded, final=final):
                self.assertFalse(m15.verify_restart(self.root, source, loaded, final)["passed"])

    def test_restart_rejects_changed_transfer_or_second_process_identity(self):
        self.completed_pair()
        metadata = self.output / "m15-session.json"
        original = metadata.read_text()
        for field, value in (("pid", 42), ("epoch", 1), ("run_id", "old-run")):
            changed = json.loads(original); changed[field] = value
            metadata.write_text(json.dumps(changed))
            with self.subTest(field=field):
                self.assertFalse(m15.verify_restart(self.root, "after", "before", "after")["passed"])
        metadata.write_text(original)
        (self.output / "restart-input.omwsave").write_bytes(b"changed continuation")
        self.assertFalse(m15.verify_restart(self.root, "after", "before", "after")["passed"])

    def test_missing_second_process_cannot_pass_restart(self):
        self.completed_run()
        self.output.rename(self.root / "first")
        self.assertFalse(m15.verify_restart(self.root, "after", "before", "after")["passed"])


if __name__ == "__main__":
    unittest.main()
