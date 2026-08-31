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


class Tes4RuntimeStateTests(unittest.TestCase):
    def test_m7_payload_round_trip_preserves_typed_locals_and_quests(self) -> None:
        expected = make_state()
        expected["content"][0]["plugin"] = "oblivion.esm"
        payload = state_io.encode_payload(make_state())
        self.assertEqual(state_io.decode_payload(payload), expected)
        self.assertEqual(state_io.encode_payload(state_io.decode_payload(payload)), payload)

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
