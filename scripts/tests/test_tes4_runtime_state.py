from __future__ import annotations

import unittest
import copy
import math
import struct
import tempfile
from pathlib import Path

from scripts import tes4_runtime_state as state_io


def make_state() -> dict:
    return {
        "schema_version": 4,
        "profile": "oblivion",
        "next_dynamic_serial": 7,
        "content": [{"plugin": "Oblivion.esm", "fingerprint": "sha256:test"}],
        "clock": {"year": 3, "month": 4, "day": 5, "hour": 6.5, "time_scale": 30.0},
        "player": {
            "reference": "dynamic:player:0000000000000001",
            "cell": "content:oblivion.esm:01650f",
            "position": [0.0] * 6,
            "actor_values": {"health.current": 25.0},
            "inventory": [
                {
                    "base": "content:oblivion.esm:018baa",
                    "count": 1,
                    "condition": 125,
                    "charge": 80.0,
                    "equipped_slots": 4,
                    "hotkey": 2,
                    "owner": "content:oblivion.esm:000007",
                    "remaining_usage_time": 812.5,
                }
            ],
            "name": "Bendu Olo",
            "race": "content:oblivion.esm:000907",
            "class": "content:oblivion.esm:0237a8",
            "birthsign": "content:oblivion.esm:022a37",
            "female": False,
            "character_generation_flags": 15,
        },
        "globals": {},
        "references": [],
        "script_event_sequence": 19,
        "script_instances": [
            {
                "unit": "content:oblivion.esm:04e90e@oblivion.esm/object/unit=0",
                "context": "content:oblivion.esm:01fc41",
                "on_load_fired": True,
                "locals": [
                    None,
                    {"type": "number", "value": 7},
                    {"type": "number", "value": 2.5},
                    {"type": "string", "value": "named"},
                    {"type": "reference", "value": "content:oblivion.esm:02466e"},
                ],
            }
        ],
        "quests": [
            {
                "quest": "content:oblivion.esm:032a15",
                "stage": 19,
                "running": True,
                "completed_stages": [10, 19],
            }
        ],
    }


def make_m14_state() -> dict:
    state = make_state()
    state["schema_version"] = 5
    state["content"][0]["plugin"] = "oblivion.esm"
    state["ai_rng_state"] = 0x123456789ABCDEF0
    actor = {
        "actor": "content:oblivion.esm:000500",
        "base": "content:oblivion.esm:000501",
        "package": "content:oblivion.esm:000502",
        "script_package": "dynamic:m14-script:0000000000000001",
        "target": "content:oblivion.esm:000503",
        "target_base": "content:oblivion.esm:000504",
        "cell": "content:oblivion.esm:01650f",
        "pathgrid": "content:oblivion.esm:000505",
        "door": "content:oblivion.esm:000506",
        "destination_cell": "content:oblivion.esm:01650f",
        "destination_position": [128.0, -3.0, 0.0, 0.0, 0.0, 1.25],
        "last_valid_cell": "content:oblivion.esm:01650f",
        "last_valid_position": [12.5, 256.0, 0.0, 0.0, 0.0, 1.25],
        "action_item": "content:oblivion.esm:000507",
        "last_transition_door": "content:oblivion.esm:000508",
        "companion_group": "dynamic:m14-group:0000000000000001",
        "companion_side_with": "content:oblivion.esm:00050b",
        "mount": "content:oblivion.esm:000509",
        "rider": "content:oblivion.esm:00050a",
        "schedule_window": {
            "start": [3, 8, 17, 12.0],
            "end": [3, 8, 17, 16.0],
            "duration_hours": 4.0,
        },
        "condition_result": 0,
        "source": 2,
        "package_type": 3,
        "procedure": 4,
        "phase": 6,
        "tier": 1,
        "boundary": 0,
        "list_index": 7,
        "path_node": 4,
        "repath_attempts": 2,
        "formation_index": 3,
        "selection_generation": 11,
        "route_generation": 12,
        "transition_generation": 13,
        "action_timer": 1.5,
        "duration_remaining": 2.5,
        "no_progress_seconds": 0.25,
        "door_cooldown": 0.5,
        "low_process_timer": 0.75,
        "next_low_process_tick": 0.25,
        "restrained": True,
        "action_reserved": True,
        "has_destination": True,
        "interruption_reason": "m14-test",
    }
    state["actor_ai"] = [actor]
    state["path_points"] = [{"pathgrid": actor["pathgrid"], "node": 4, "enabled": False}]
    state["companions"] = [{
        "leader": actor["target"],
        "member": actor["actor"],
        "group": actor["companion_group"],
        "side_with": actor["companion_side_with"],
        "formation_index": 3,
    }]
    state["mounts"] = [{
        "horse": actor["mount"],
        "rider": actor["rider"],
        "owner": actor["base"],
        "last_ridden": actor["last_transition_door"],
        "mounted": True,
    }]
    state["detection_vectors"] = [{
        "observer": actor["actor"],
        "target": actor["target"],
        "score": 72.5,
        "detected": True,
        "line_of_sight": True,
    }]
    return state


class Tes4RuntimeStateTests(unittest.TestCase):
    def test_m7_payload_round_trip_preserves_typed_locals_and_quests(self) -> None:
        expected = make_state()
        expected["content"][0]["plugin"] = "oblivion.esm"
        payload = state_io.encode_payload(make_state())
        self.assertEqual(state_io.decode_payload(payload), expected)
        self.assertEqual(state_io.encode_payload(state_io.decode_payload(payload)), payload)

    def test_m14_payload_round_trip_preserves_ai_intent_relations_and_timers(self) -> None:
        expected = make_m14_state()
        payload = state_io.encode_payload(expected)
        self.assertEqual(state_io.decode_payload(payload), expected)
        self.assertEqual(state_io.encode_payload(state_io.decode_payload(payload)), payload)

    def test_scripted_look_target_persistence_and_validation(self) -> None:
        state = make_m14_state()
        state["references"] = [{
            "key": "content:oblivion.esm:000001", "base": "content:oblivion.esm:000002",
            "cell": state["player"]["cell"], "position": [0.0] * 6, "enabled": True,
            "deleted": False, "owner": None, "lock_level": 0, "inventory": [],
            "custom_state": {"obscript.look_target": state["player"]["reference"]},
        }]
        decoded = state_io.decode_payload(state_io.encode_payload(state))
        self.assertEqual(decoded["references"], state["references"])
        self.assertEqual(decoded["actor_ai"], state["actor_ai"])
        for value in (True, "null", "garbage", "content:oblivion.esm:000000",
                      "content:Oblivion.esm:000001", "dynamic:player:0000000000000000"):
            with self.subTest(value=value), self.assertRaises(state_io.RuntimeStateError):
                state["references"][0]["custom_state"]["obscript.look_target"] = value
                state_io.encode_payload(state)

    def test_version_six_preserves_pending_package_completion_fifo(self) -> None:
        state = make_m14_state()
        state["schema_version"] = 6
        actor = state["actor_ai"][0]
        event = {"actor": actor["actor"], "package": actor["package"]}
        state["pending_package_done"] = [event, {**event, "package": "content:oblivion.esm:000001"}, event]
        decoded = state_io.decode_payload(state_io.encode_payload(state))
        self.assertEqual(decoded, state)
        decoded["pending_package_done"].pop(0)
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(decoded)), decoded)
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(state_io.encode_payload(state)[:-1])
        state["schema_version"] = 5
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        state["schema_version"] = 6
        state["pending_package_done"] = [{"actor": "null", "package": event["package"]}]
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_m14_idle_actor_uses_unknown_package_type(self) -> None:
        expected = make_state()
        expected["schema_version"] = 5
        expected["ai_rng_state"] = 1
        expected["actor_ai"] = [{
            "actor": "content:oblivion.esm:000600",
            "base": "content:oblivion.esm:000601",
            "cell": expected["player"]["cell"],
            "source": 0,
            "package_type": 255,
            "procedure": 0,
        }]
        payload = state_io.encode_payload(expected)
        restored = state_io.decode_payload(payload)["actor_ai"][0]
        self.assertEqual(restored["package"], "null")
        self.assertEqual(restored["package_type"], 255)
        self.assertEqual(restored["procedure"], 0)

    def test_m14_rejects_non_reciprocal_mounted_actor_state(self) -> None:
        state = make_state()
        state["schema_version"] = 5
        state["ai_rng_state"] = 1
        horse = {
            "actor": "content:oblivion.esm:000610",
            "base": "content:oblivion.esm:000611",
            "cell": state["player"]["cell"],
            "rider": "content:oblivion.esm:000620",
            "source": 0,
            "package_type": 255,
            "procedure": 0,
        }
        rider = {
            "actor": "content:oblivion.esm:000620",
            "base": "content:oblivion.esm:000621",
            "cell": state["player"]["cell"],
            "mount": "null",
            "source": 0,
            "package_type": 255,
            "procedure": 0,
        }
        state["actor_ai"] = [horse, rider]
        state["mounts"] = [{
            "horse": horse["actor"],
            "rider": rider["actor"],
            "owner": horse["base"],
            "last_ridden": "null",
            "mounted": True,
        }]
        with self.assertRaisesRegex(state_io.RuntimeStateError, "reciprocal"):
            state_io.encode_payload(state)

    def test_version_one_payload_loads_with_empty_m7_state(self) -> None:
        old = make_state()
        old["script_event_sequence"] = 0
        old["script_instances"] = []
        old["quests"] = []
        old["schema_version"] = 1
        old["player"]["inventory"] = []
        for key in ("name", "race", "class", "birthsign", "female", "character_generation_flags"):
            del old["player"][key]
        loaded = state_io.decode_payload(state_io.encode_payload(old))
        self.assertEqual(loaded["schema_version"], 1)
        self.assertNotIn("script_event_sequence", loaded)
        self.assertNotIn("script_instances", loaded)
        self.assertNotIn("quests", loaded)

    def test_version_two_payload_loads_without_m12_character_state(self) -> None:
        old = make_state()
        old["schema_version"] = 2
        old["player"]["inventory"] = []
        for key in ("name", "race", "class", "birthsign", "female", "character_generation_flags"):
            del old["player"][key]
        loaded = state_io.decode_payload(state_io.encode_payload(old))
        self.assertEqual(loaded["schema_version"], 2)
        self.assertNotIn("race", loaded["player"])

    def test_version_three_payload_loads_without_m13_item_metadata(self) -> None:
        old = make_state()
        old["schema_version"] = 3
        old["player"]["inventory"] = [
            {"base": "content:oblivion.esm:018baa", "count": 2}
        ]
        loaded = state_io.decode_payload(state_io.encode_payload(old))
        self.assertEqual(loaded["schema_version"], 3)
        self.assertEqual(
            loaded["player"]["inventory"],
            [{"base": "content:oblivion.esm:018baa", "count": 2}],
        )

    def test_script_state_rejects_non_finite_and_canonicalizes_stages(self) -> None:
        invalid = make_state()
        invalid["script_instances"][0]["locals"][2]["value"] = float("nan")
        with self.assertRaisesRegex(state_io.RuntimeStateError, "not finite"):
            state_io.encode_payload(invalid)

        invalid = make_state()
        invalid["quests"][0]["completed_stages"] = [19, 19]
        # Encoding canonicalizes stages, and decoding independently rejects a
        # non-canonical wire representation; the canonical payload is unique.
        decoded = state_io.decode_payload(state_io.encode_payload(invalid))
        self.assertEqual(decoded["quests"][0]["completed_stages"], [19])

    def test_inventory_rejects_conflicts_duplicate_hotkeys_and_bad_metadata(self) -> None:
        for field, value, message in (
            ("charge", float("nan"), "metadata"),
            ("remaining_usage_time", -2.0, "metadata"),
            ("condition", -2, "metadata"),
            ("equipped_slots", 1 << 19, "metadata"),
            ("hotkey", 8, "metadata"),
        ):
            invalid = make_state()
            invalid["player"]["inventory"][0][field] = value
            with self.assertRaisesRegex(state_io.RuntimeStateError, message):
                state_io.encode_payload(invalid)

        invalid = make_state()
        duplicate = dict(invalid["player"]["inventory"][0])
        duplicate["base"] = "content:oblivion.esm:018bab"
        invalid["player"]["inventory"].append(duplicate)
        with self.assertRaisesRegex(state_io.RuntimeStateError, "Conflicting"):
            state_io.encode_payload(invalid)

        invalid["player"]["inventory"][1]["equipped_slots"] = 0
        with self.assertRaisesRegex(state_io.RuntimeStateError, "Duplicate"):
            state_io.encode_payload(invalid)

    def test_native_values_match_cpp_sparse_wire_layout_and_reject_corruption(self) -> None:
        state = make_state()
        state["schema_version"] = 9
        state["ai_rng_state"] = 1
        empty = state_io.encode_payload(state)
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 1, "values": [[0.0, None, None, None] for _ in range(72)]}
        actor["values"][-1] = [1.25, 0.0, None, -0.0]
        state["native_actor_values"] = [actor]
        payload = state_io.encode_payload(state)
        self.assertEqual(payload[-13:], bytes([0, 0, 160, 63, 5, 0, 0, 0, 0, 0, 0, 0, 128]))
        restored = state_io.decode_payload(payload)
        value = restored["native_actor_values"][0]["values"][-1]
        self.assertEqual(value, actor["values"][-1])
        self.assertEqual(math.copysign(1, value[1]), 1)
        self.assertEqual(math.copysign(1, value[3]), -1)
        self.assertEqual(state_io.encode_payload(restored), payload)
        for removed in range(1, 361):
            with self.subTest(removed=removed), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-removed])
        corrupt = bytearray(payload)
        corrupt[-9] = 8
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(corrupt))
        offset = len(empty) - 4
        corrupt = bytearray(payload)
        corrupt[offset:offset + 4] = b"\xff" * 4
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(corrupt))
        duplicate = bytearray(payload)
        duplicate[offset] = 2
        duplicate.extend(payload[offset + 4:])
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(duplicate))

    def test_shared_base_overrides_have_typed_canonical_version_eleven_wire(self) -> None:
        state = make_state()
        state["schema_version"] = 11
        state["ai_rng_state"] = 1
        base = {"base": "content:oblivion.esm:000007", "kind": 0,
                "values": [[8, 0, 16777217], [9, 0, 65535], [40, 1, 2147483648.0]]}
        state["native_actor_bases"] = [base]
        payload = state_io.encode_payload(state)
        key = base["base"].encode()
        suffix = (struct.pack("<II", 1, len(key)) + key + struct.pack("<BI", 0, 3)
                  + struct.pack("<BBi", 8, 0, 16777217) + struct.pack("<BBi", 9, 0, 65535)
                  + struct.pack("<BBf", 40, 1, 2147483648.0))
        self.assertTrue(payload.endswith(suffix))
        self.assertEqual(state_io.decode_payload(payload)["native_actor_bases"], [base])
        base["values"].reverse()
        self.assertEqual(state_io.encode_payload(state), payload)
        for cut in range(len(payload) - len(suffix), len(payload)):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:cut])
        for index, value in ((len(payload) - 5, 2), (len(payload) - 6, 72)):
            corrupt = bytearray(payload)
            corrupt[index] = value
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(bytes(corrupt))
        state["schema_version"] = 10
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        state["native_actor_bases"] = []
        legacy = state_io.decode_payload(state_io.encode_payload(state))
        self.assertNotIn("native_actor_bases", legacy)
        legacy["schema_version"] = 11
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(legacy))["native_actor_bases"], [])

    def test_shared_base_overrides_reject_bad_identity_kind_shape_and_storage(self) -> None:
        state = make_state()
        state["schema_version"] = 11
        state["ai_rng_state"] = 1
        valid = {"base": "content:oblivion.esm:000007", "kind": 0, "values": [[8, 0, 16777217]]}
        state["native_actor_bases"] = [valid]
        state_io.encode_payload(state)
        invalids = [dict(valid, base="null"), dict(valid, base="content:Oblivion.esm:000007"),
                    dict(valid, kind=2), dict(valid, kind=True), dict(valid, values=[]),
                    dict(valid, values=[[8, 0, 1], [8, 0, 2]]),
                    dict(valid, kind=1, values=[[28, 0, 1]])]
        for value in ([0, 0, -1], [7, 0, 256], [9, 0, 65536], [10, 0, -1], [8, 1, 1.0],
                      [40, 0, 1], [11, 0, 0], [37, 0, 0], [72, 1, 0], [40, 1, math.inf],
                      [8, 0, True], [8, False, 1], [8, 0, 2147483648]):
            invalids.append(dict(valid, values=[value]))
        for invalid in invalids:
            with self.subTest(invalid=invalid):
                state["native_actor_bases"] = [invalid]
                with self.assertRaises(state_io.RuntimeStateError):
                    state_io.encode_payload(state)
        state["native_actor_bases"] = [valid, valid]
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_player_form_values_match_cpp_version_ten_wire_and_preserve_legacy_absence(self) -> None:
        state = make_state()
        state["schema_version"] = 10
        state["ai_rng_state"] = 1
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 1, "values": [[0.0, None, None, None] for _ in range(72)],
                 "player_form_values": [-1, 0, -(1 << 31), (1 << 31) - 1]}
        actor["values"][8][0] = 123.0
        state["native_actor_values"] = [actor]
        payload = state_io.encode_payload(state)
        self.assertEqual(payload[-17:], bytes([1, 255, 255, 255, 255, 0, 0, 0, 0,
                                              0, 0, 0, 128, 255, 255, 255, 127]))
        self.assertEqual(state_io.decode_payload(payload)["native_actor_values"], [actor])
        nonplayer = copy.deepcopy(state)
        nonplayer_actor = nonplayer["native_actor_values"][0]
        nonplayer["references"] = [{
            "key": "content:oblivion.esm:000001", "base": "content:oblivion.esm:000002",
            "cell": state["player"]["cell"], "position": [0.0] * 6, "enabled": True,
            "deleted": False, "owner": None, "lock_level": 0, "inventory": [], "custom_state": {},
        }]
        nonplayer_actor.update(owner=1, actor=nonplayer["references"][0]["key"],
                               base=nonplayer["references"][0]["base"])
        with self.assertRaisesRegex(state_io.RuntimeStateError, "player ownership"):
            state_io.encode_payload(nonplayer)
        for removed in range(1, 18):
            with self.subTest(removed=removed), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-removed])
        corrupt = bytearray(payload)
        corrupt[-17] = 2
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(corrupt))
        for value in ([], [0] * 3, [0] * 5, [True, 0, 0, 0], [1.0, 0, 0, 0],
                      [1 << 31, 0, 0, 0], [-(1 << 31) - 1, 0, 0, 0], "0000"):
            actor["player_form_values"] = value
            with self.subTest(value=value), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(state)
        actor["player_form_values"] = [0] * 4
        state["schema_version"] = 9
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        del actor["player_form_values"]
        legacy = state_io.decode_payload(state_io.encode_payload(state))
        self.assertNotIn("player_form_values", legacy["native_actor_values"][0])
        legacy["schema_version"] = 10
        migrated = state_io.decode_payload(state_io.encode_payload(legacy))
        self.assertIsNone(migrated["native_actor_values"][0]["player_form_values"])
        self.assertEqual(migrated["native_actor_values"][0]["values"][8][0], 123.0)

    def test_native_values_validate_identity_shape_enums_and_numeric_domain(self) -> None:
        state = make_state()
        state["schema_version"] = 9
        state["ai_rng_state"] = 1
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 1, "values": [[0.0, None, None, None] for _ in range(72)]}
        cases = []
        for key, value in (("actor", "null"), ("base", ""), ("base", "broken"), ("base", "content:oblivion.esm:000000"), ("owner", 1), ("owner", 2),
                           ("owner", False), ("process", 2), ("process", 1.0), ("values", [])):
            invalid = copy.deepcopy(actor)
            invalid[key] = value
            cases.append([invalid])
        for value in ([None, None, None, None], [True, None, None, None],
                      [1, None, None], [1, 0, float("nan"), 0], [1e100, None, None, None],
                      [3e38, 3e38, None, None]):
            invalid = copy.deepcopy(actor)
            invalid["values"][-1] = value
            cases.append([invalid])
        cases.append([actor, actor])
        invalid = copy.deepcopy(actor)
        invalid["actor"] = "content:oblivion.esm:000100"
        invalid["owner"] = 1
        cases.append([invalid])
        for actors in cases:
            state["native_actor_values"] = actors
            with self.subTest(actors=actors), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(state)
        state["native_actor_values"] = [actor]
        state["schema_version"] = 8
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_all_legacy_schemas_have_no_native_modifier_categories(self) -> None:
        for version in range(1, 9):
            state = make_state()
            state["schema_version"] = version
            state["ai_rng_state"] = 1
            state["player"]["inventory"] = []
            if version < 3:
                for key in ("name", "race", "class", "birthsign", "female", "character_generation_flags"):
                    state["player"].pop(key)
            if version < 2:
                state["script_event_sequence"] = 0
                state["script_instances"] = []
                state["quests"] = []
            restored = state_io.decode_payload(state_io.encode_payload(state))
            self.assertNotIn("native_actor_values", restored)
            restored["schema_version"] = 9
            restored["ai_rng_state"] = 1
            restored["player"].setdefault("name", "")
            restored["player"]["race"] = state_io.DEFAULT_MIGRATION_RACE
            restored["player"]["class"] = state_io.DEFAULT_MIGRATION_CLASS
            promoted = state_io.decode_payload(state_io.encode_payload(restored))
            self.assertEqual(promoted["native_actor_values"], [])

    def test_version_eight_physical_actions_have_canonical_cpp_wire_layout(self) -> None:
        state = make_state()
        state["schema_version"] = 8
        state["ai_rng_state"] = 1
        state["physical_actions"] = {"next": 5, "pending": [3, 1]}
        payload = state_io.encode_payload(state)
        self.assertEqual(payload[-28:], bytes([
            5, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0]))
        restored = state_io.decode_payload(payload)
        self.assertEqual(restored["physical_actions"], {"next": 5, "pending": [1, 3]})
        self.assertEqual(state_io.encode_payload(restored), payload)
        for removed in range(1, 29):
            with self.subTest(removed=removed), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-removed])
        for offset, value in ((-28, 0), (-8, 1), (-20, 255)):
            corrupt = bytearray(payload)
            corrupt[offset] = value
            with self.subTest(offset=offset), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(bytes(corrupt))

    def test_version_eight_rejects_invalid_or_legacy_physical_actions(self) -> None:
        state = make_state()
        state["schema_version"] = 8
        state["ai_rng_state"] = 1
        for invalid in (
            None, {}, {"next": 0, "pending": []}, {"next": True, "pending": []},
            {"next": 2**64, "pending": []}, {"next": 5, "pending": [1, 1]},
            {"next": 5, "pending": [0]}, {"next": 5, "pending": [5]},
            {"next": 5, "pending": [True]}, {"next": 5, "pending": [1.0]},
            {"next": 5, "pending": "1"},
        ):
            state["physical_actions"] = invalid
            with self.subTest(invalid=invalid), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(state)
        state["physical_actions"] = {"next": 2**64 - 1, "pending": [2**64 - 2]}
        restored = state_io.decode_payload(state_io.encode_payload(state))
        self.assertEqual(restored["physical_actions"], state["physical_actions"])
        state["schema_version"] = 7
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_all_legacy_versions_promote_without_importing_ai_or_script_ids(self) -> None:
        for version in range(1, 8):
            with self.subTest(version=version):
                state = make_state()
                state["schema_version"] = version
                state["ai_rng_state"] = 1
                state["player"]["inventory"] = []
                if version < 3:
                    for key in ("name", "race", "class", "birthsign", "female", "character_generation_flags"):
                        state["player"].pop(key)
                if version < 2:
                    for key in ("script_event_sequence", "script_instances", "quests"):
                        state.pop(key)
                restored = state_io.decode_payload(state_io.encode_payload(state))
                self.assertNotIn("physical_actions", restored)
                payload = state_io.encode_payload(state)
                body = (struct.pack("<4sII", b"VERS", 4, version)
                        + struct.pack("<4sI", b"DATA", len(payload)) + payload)
                with tempfile.TemporaryDirectory() as directory:
                    source = Path(directory) / "old.omwsave"
                    target = Path(directory) / "new.omwsave"
                    source.write_bytes(struct.pack("<4sIII", b"T4ST", len(body), 0, 0) + body)
                    state_io.write_save(source, target, restored)
                    promoted = state_io.load_save(target)
                self.assertEqual(promoted["physical_actions"], {"next": 1, "pending": []})

    def test_lifecycle_preserves_fifo_and_fractional_timer(self) -> None:
        state = make_state()
        state["schema_version"] = 12
        state["ai_rng_state"] = 1
        actor = state["player"]["reference"]
        state["native_actor_life"] = [{"actor": actor, "base": "dynamic:player-base:0000000000000001",
                                       "phase": 2, "recovery_remaining": 3.125, "killer": "null"}]
        state["next_death_event"] = 9
        state["pending_death_events"] = [{"id": 2, "actor": actor, "killer": "null"},
                                          {"id": 8, "actor": actor, "killer": actor}]
        payload = state_io.encode_payload(state)
        loaded = state_io.decode_payload(payload)
        for key in ("native_actor_life", "next_death_event", "pending_death_events"):
            self.assertEqual(loaded[key], state[key])
        self.assertEqual(state_io.encode_payload(loaded), payload)
        for remove in range(1, 60):
            with self.subTest(remove=remove), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-remove])
        for field, value in (("phase", True), ("phase", 3), ("recovery_remaining", -1),
                             ("recovery_remaining", math.inf), ("recovery_remaining", True),
                             ("killer", "content:missing.esm:000001"), ("actor", actor.upper()), ("base", "null")):
            broken = copy.deepcopy(state)
            broken["native_actor_life"][0][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        for events in (list(reversed(state["pending_death_events"])), [state["pending_death_events"][0]] * 2,
                       [{"id": 0, "actor": actor, "killer": "null"}],
                       [{"id": 9, "actor": actor, "killer": "null"}],
                       [{"id": True, "actor": actor, "killer": "null"}],
                       [{"id": 2, "actor": "null", "killer": "null"}]):
            broken = copy.deepcopy(state)
            broken["pending_death_events"] = events
            with self.subTest(events=events), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        for identity in (0, True, 2**64):
            broken = copy.deepcopy(state)
            broken["next_death_event"] = identity
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        state["schema_version"] = 11
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_lifecycle_rejects_legacy_dead_conflict(self) -> None:
        state = make_state()
        state["schema_version"] = 12
        state["ai_rng_state"] = 1
        key, base = "content:oblivion.esm:000100", "content:oblivion.esm:000200"
        state["references"] = [{"key": key, "base": base, "cell": state["player"]["cell"],
                                "enabled": True, "deleted": False, "position": [0] * 6, "inventory": [],
                                "owner": None, "lock_level": 0, "custom_state": {"obscript.dead": True}}]
        state["native_actor_life"] = [{"actor": key, "base": base, "phase": 1,
                                       "recovery_remaining": 0, "killer": "null"}]
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))["native_actor_life"],
                         state["native_actor_life"])
        for dead in (False, 1, "true"):
            state["references"][0]["custom_state"]["obscript.dead"] = dead
            with self.subTest(dead=dead), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(state)
        state["references"][0]["custom_state"]["obscript.dead"] = False
        state["native_actor_life"][0]["phase"] = 2
        state["native_actor_life"][0]["recovery_remaining"] = .125
        state_io.encode_payload(state)

    def test_legacy_lifecycle_defaults_do_not_infer_death_from_health(self) -> None:
        for version in range(1, 12):
            state = make_state()
            state["schema_version"] = version
            state["ai_rng_state"] = 1
            state["player"]["inventory"] = []
            state["player"]["actor_values"]["health.current"] = -10
            if version < 3:
                for key in ("name", "race", "class", "birthsign", "female", "character_generation_flags"):
                    state["player"].pop(key)
            if version < 2:
                state["script_event_sequence"] = 0
                state["script_instances"] = []
                state["quests"] = []
            loaded = state_io.decode_payload(state_io.encode_payload(state))
            self.assertNotIn("native_actor_life", loaded)
            self.assertNotIn("pending_death_events", loaded)
            self.assertNotIn("next_death_event", loaded)

    def test_native_death_counter_storage_and_rejection(self):
        state = make_state()
        state["ai_rng_state"] = 1
        state["schema_version"] = 13
        state["native_death_counts"] = [{"base": "content:actors.esm:000123", "count": 65535},
                                         {"base": "dynamic:player-base:0000000000000001", "count": 32768}]
        encoded = state_io.encode_payload(state)
        self.assertEqual(state_io.decode_payload(encoded)["native_death_counts"], state["native_death_counts"])
        for count in [-1, 65536, True, 1.5]:
            broken = copy.deepcopy(state)
            broken["native_death_counts"][0]["count"] = count
            with self.subTest(count=count), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        for base in ["null", "content:Actors.esm:000123", "bad"]:
            broken = copy.deepcopy(state)
            broken["native_death_counts"][0]["base"] = base
            with self.subTest(base=base), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        broken = copy.deepcopy(state)
        broken["native_death_counts"].append(broken["native_death_counts"][0])
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(broken)
        state["schema_version"] = 12
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(encoded[:-1])

    def test_old_payload_does_not_invent_death_counts(self):
        state = make_state()
        state["ai_rng_state"] = 1
        for version in range(4, 13):
            state["schema_version"] = version
            self.assertNotIn("native_death_counts", state_io.decode_payload(state_io.encode_payload(state)))


if __name__ == "__main__":
    unittest.main()
