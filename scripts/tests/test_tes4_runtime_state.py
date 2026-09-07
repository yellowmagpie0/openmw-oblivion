from __future__ import annotations

import unittest

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


if __name__ == "__main__":
    unittest.main()
