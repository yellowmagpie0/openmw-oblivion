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
    def test_passive_initial_magnitude_v19_wire_and_v18_unknown(self):
        state = make_state()
        state["schema_version"] = 18
        state["ai_rng_state"] = 1
        effect = [7, int.from_bytes(b"FOAT", "little"), 5, -10.]
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 0, "values": [[0., None, None, None] for _ in range(72)],
                 "player_form_values": None, "nonplayer_form_health": None,
                 "passive_abilities": [{"spell": "content:abilities.esp:000123", "effects": [effect]}]}
        state["native_actor_values"] = [actor]
        legacy = state_io.encode_payload(state)
        state["schema_version"] = 19
        state["ai_rng_state"] = 1
        effect.append(25.)
        expected = bytearray(legacy)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 19)
        expected[len(legacy) - 40:len(legacy) - 40] = b"\x01" + struct.pack("<f", 25.)
        payload = state_io.encode_payload(state)
        self.assertEqual(payload, bytes(expected))
        decoded = state_io.decode_payload(payload)
        self.assertEqual(decoded["native_actor_values"], [actor])
        self.assertEqual(state_io.encode_payload(decoded), payload)
        old = state_io.decode_payload(legacy)
        self.assertEqual(len(old["native_actor_values"][0]["passive_abilities"][0]["effects"][0]), 4)
        effect[4] = None
        unknown = state_io.encode_payload(state)
        expected = bytearray(legacy)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 19)
        expected[len(legacy) - 40:len(legacy) - 40] = b"\x00"
        self.assertEqual(unknown, bytes(expected))
        self.assertIsNone(state_io.decode_payload(unknown)["native_actor_values"][0]["passive_abilities"][0]["effects"][0][4])

    def test_passive_initial_magnitude_rejects_downgrade_nonfinite_and_presence(self):
        state = make_state()
        state["schema_version"] = 19
        state["ai_rng_state"] = 1
        effect = [7, int.from_bytes(b"FOAT", "little"), 5, -10., -0.]
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 0, "values": [[0., None, None, None] for _ in range(72)],
                 "player_form_values": None, "nonplayer_form_health": None,
                 "passive_abilities": [{"spell": "content:abilities.esp:000123", "effects": [effect]}]}
        state["native_actor_values"] = [actor]
        payload = state_io.encode_payload(state)
        restored_effect = state_io.decode_payload(payload)["native_actor_values"][0]["passive_abilities"][0]["effects"][0]
        self.assertEqual(struct.pack("<f", restored_effect[4]), b"\0\0\0\x80")
        state["schema_version"] = 18
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        state["schema_version"] = 19
        state["ai_rng_state"] = 1
        for value in (math.inf, math.nan, True):
            effect[4] = value
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(state)
        corrupt = bytearray(payload)
        corrupt[len(payload) - 45] = 2
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(corrupt))
        for size in range(5):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:len(payload) - 45 + size])

    def test_global_formkeys_reject_invalid_text_on_encode_and_decode(self):
        valid_key = "content:oblivion.esm:000001"
        source = make_state()
        source["globals"] = {valid_key: 1}
        payload = state_io.encode_payload(source)
        needle = struct.pack("<I", len(valid_key)) + valid_key.encode()
        self.assertEqual(payload.count(needle), 1)
        for key in ["chargenstate", "", "null", "unknown:global:1", "content::1",
                    "content:folder/:1", "content:oblivion.esm:0", "content:oblivion.esm:1000000",
                    "dynamic:global:0", "dynamic:global:10000000000000000",
                    "dynamic::1", "dynamic:global:+1", "dynamic:global:0x1",
                    "dynamic:global: 1", "dynamic:global:1:2", "dynamic:global:gg"]:
            with self.subTest(key=key):
                state = make_state()
                state["globals"] = {key: 1}
                with self.assertRaises(state_io.RuntimeStateError):
                    state_io.encode_payload(state)
                replacement = struct.pack("<I", len(key)) + key.encode()
                with self.assertRaises(state_io.RuntimeStateError):
                    state_io.decode_payload(payload.replace(needle, replacement))
        source["globals"] = {42: 1}
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(source)

    def test_global_formkeys_detect_native_normalization_collisions(self):
        for alias in ["content:Oblivion.esm:1", "content:folder/oblivion.esm:000001",
                      "content:oblivion.esm:00000001"]:
            with self.subTest(alias=alias):
                state = make_state()
                state["globals"] = {"content:oblivion.esm:000001": 1, alias: 2}
                with self.assertRaisesRegex(state_io.RuntimeStateError, "Duplicate"):
                    state_io.encode_payload(state)
        state = make_state()
        state["globals"] = {"content:oblivion.esm:000001": 1, "content:oblivion.esm:000002": 2}
        payload = state_io.encode_payload(state)
        # Distinct wire strings can represent one native key after hex parsing.
        with self.assertRaisesRegex(state_io.RuntimeStateError, "Duplicate"):
            state_io.decode_payload(payload.replace(b"content:oblivion.esm:000002", b"content:Oblivion.esm:000001"))

    def test_global_formkeys_preserve_compatible_legacy_spellings(self):
        state = make_state()
        state["globals"] = {"content:folder/Oblivion.esm:Ab": 1,
                            "dynamic:Global:FFFFFFFFFFFFFFFF": 2,
                            "dynamic:global:1": 3}
        payload = state_io.encode_payload(state)
        decoded = state_io.decode_payload(payload)
        self.assertEqual(decoded["globals"], state["globals"])
        self.assertEqual(state_io.encode_payload(decoded), payload)

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

    def test_passive_ownership_version_eighteen_matches_independent_wire_and_preserves_order(self):
        state = make_state()
        state["schema_version"] = 17
        state["ai_rng_state"] = 1
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 0, "values": [[0., None, None, None] for _ in range(72)],
                 "player_form_values": None, "nonplayer_form_health": None}
        state["native_actor_values"] = [actor]
        legacy = state_io.encode_payload(state)
        spell = "content:abilities.esp:000123"
        foat, fosp = [int.from_bytes(code.encode("ascii"), "little") for code in ("FOAT", "FOSP")]
        ability = {"spell": spell, "effects": [[7, foat, 5, -0.], [2, fosp, 9, 50.]]}
        state["schema_version"] = 18
        actor["passive_abilities"] = [ability]
        field = b"\x01" + struct.pack("<II", 1, len(spell)) + spell.encode("ascii") + struct.pack("<I", 2)
        field += struct.pack("<IIII", 7, foat, 5, 0x80000000)
        field += struct.pack("<IIIf", 2, fosp, 9, 50.)
        expected = bytearray(legacy)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 18)
        offset = len(legacy) - 40
        expected[offset:offset] = field
        payload = state_io.encode_payload(state)
        self.assertEqual(payload, bytes(expected))
        decoded = state_io.decode_payload(payload)
        self.assertEqual(decoded["native_actor_values"], [actor])
        self.assertEqual(struct.pack("<f", decoded["native_actor_values"][0]["passive_abilities"][0]["effects"][0][3]), b"\0\0\0\x80")
        self.assertEqual(state_io.encode_payload(decoded), payload)
        corrupt = bytearray(payload)
        corrupt[offset] = 2
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(corrupt))
        for size in range(len(field)):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:offset + size])
        state["schema_version"] = 17
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        old = state_io.decode_payload(legacy)
        self.assertNotIn("passive_abilities", old["native_actor_values"][0])
        self.assertEqual(state_io.encode_payload(old), legacy)
        old["schema_version"] = 18
        unknown = state_io.encode_payload(old)
        self.assertIsNone(state_io.decode_payload(unknown)["native_actor_values"][0]["passive_abilities"])
        old["native_actor_values"][0]["passive_abilities"] = []
        known = state_io.encode_payload(old)
        self.assertNotEqual(known, unknown)
        self.assertEqual(state_io.decode_payload(known)["native_actor_values"][0]["passive_abilities"], [])

    def test_passive_ownership_rejects_malformed_duplicates_and_unsupported_effects(self):
        state = make_state()
        state["schema_version"] = 18
        state["ai_rng_state"] = 1
        effect = [0, int.from_bytes(b"FOAT", "little"), 5, -10.]
        valid = {"spell": "content:abilities.esp:000123", "effects": [effect]}
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 0, "values": [[0., None, None, None] for _ in range(72)]}
        state["native_actor_values"] = [actor]
        invalids = [True, {}, [valid, valid], [{**valid, "spell": "null"}],
                    [{**valid, "spell": "content:Abilities.esp:000123"}], [{**valid, "effects": []}],
                    [{**valid, "effects": [effect, effect]}]]
        for index, values in ((0, (-1, 1 << 32, True)), (1, (0, int.from_bytes(b"SEFF", "little"), True)),
                              (2, (-1, 72, True)), (3, (math.inf, math.nan, True, "1"))):
            for value in values:
                bad_effect = list(effect)
                bad_effect[index] = value
                invalids.append([{**valid, "effects": [bad_effect]}])
        invalids.extend([{**valid, "effects": [malformed]}] for malformed in ([], [0, 1, 5], "bad"))
        for invalid in invalids:
            actor["passive_abilities"] = invalid
            with self.subTest(invalid=invalid), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(state)
        actor["passive_abilities"] = [valid]
        state_io.encode_payload(state)
        actor["passive_abilities"] = []
        state_io.encode_payload(state)
        state["schema_version"] = 17
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_nonplayer_form_health_matches_version_seventeen_wire_and_legacy_absence(self):
        state = make_state()
        state["schema_version"] = 17
        state["ai_rng_state"] = 1
        key, base = "content:oblivion.esm:000901", "content:oblivion.esm:000800"
        state["references"] = [{"key": key, "base": base, "cell": state["player"]["cell"],
            "enabled": True, "deleted": False, "position": [0.] * 6, "inventory": [],
            "custom_state": {}, "owner": None, "lock_level": 0}]
        actor = {"actor": key, "base": base, "owner": 1, "process": 1,
            "values": [[0., None, None, None] for _ in range(72)], "player_form_values": None}
        state["native_actor_values"] = [actor]
        for health in (-(1 << 31), -16777217, 0, 16777217, (1 << 31) - 1):
            actor["nonplayer_form_health"] = health
            actor["values"][8][0] = struct.unpack("<f", struct.pack("<f", health))[0]
            payload = state_io.encode_payload(state)
            offset = len(payload) - 40 - 5
            self.assertEqual(payload[offset:offset + 5], b"\x01" + struct.pack("<i", health))
            self.assertEqual(state_io.decode_payload(payload)["native_actor_values"], [actor])
            legacy_state = copy.deepcopy(state)
            legacy_state["schema_version"] = 16
            del legacy_state["native_actor_values"][0]["nonplayer_form_health"]
            legacy = state_io.encode_payload(legacy_state)
            # Independent v16 -> v17 wire insertion: version header and optional field only.
            expected = bytearray(legacy)
            struct.pack_into("<I", expected, len(state_io.MAGIC), 17)
            expected[-40:-40] = b"\x01" + struct.pack("<i", health)
            self.assertEqual(payload, bytes(expected))
            decoded = state_io.decode_payload(legacy)
            self.assertNotIn("nonplayer_form_health", decoded["native_actor_values"][0])
            self.assertEqual(state_io.encode_payload(decoded), legacy)
            decoded["schema_version"] = 17
            promoted = state_io.decode_payload(state_io.encode_payload(decoded))
            self.assertIsNone(promoted["native_actor_values"][0]["nonplayer_form_health"])
            corrupt = bytearray(payload)
            corrupt[offset] = 2
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(bytes(corrupt))
            for remove in range(1, 6):
                with self.assertRaises(state_io.RuntimeStateError):
                    state_io.decode_payload(payload[:offset + 5 - remove])
        for invalid in (True, 1., "1", -(1 << 31) - 1, 1 << 31):
            actor["nonplayer_form_health"] = invalid
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(state)
        actor["nonplayer_form_health"] = 16777217
        actor["values"][8][0] = 16777218.
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        actor["values"][8][0] = 16777216.
        state["schema_version"] = 16
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        state["schema_version"] = 17
        actor.update(actor=state["player"]["reference"], owner=0)
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

    def test_native_breath_round_trip_and_strict_validation(self):
        state = make_state()
        state["schema_version"] = 14
        state["ai_rng_state"] = 1
        actor = state["player"]["reference"]
        state["native_actor_values"] = [{"actor": actor,
            "base": "dynamic:player-base:0000000000000001", "owner": 0, "process": 1,
            "values": [[0, None, None, None] for _ in range(72)]}]
        for remaining in (0, -.125, .125, 20, 3.4028234663852886e38):
            state["native_actor_breath"] = [{"actor": actor, "remaining": remaining}]
            encoded = state_io.encode_payload(state)
            restored = state_io.decode_payload(encoded)
            self.assertEqual(restored["native_actor_breath"], state["native_actor_breath"])
            self.assertEqual(state_io.encode_payload(restored), encoded)
        for entry in ({"actor": actor, "remaining": float("nan")},
                      {"actor": actor, "remaining": float("inf")},
                      {"actor": actor, "remaining": -float("inf")},
                      {"actor": actor, "remaining": 1e40},
                      {"actor": actor, "remaining": True},
                      {"actor": "dynamic:missing:0000000000000001", "remaining": 1},
                      {"actor": "null", "remaining": 1},
                      {"actor": actor}, {"actor": actor, "remaining": 1, "extra": 1}):
            broken = copy.deepcopy(state)
            broken["native_actor_breath"] = [entry]
            with self.subTest(entry=entry), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        state["native_actor_breath"] = [{"actor": actor, "remaining": .125}] * 2
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)
        state["native_actor_breath"].pop()
        state["schema_version"] = 13
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_native_breath_rejects_corrupt_binary_tail(self):
        state = make_state()
        state["schema_version"] = 14
        state["ai_rng_state"] = 1
        actor = state["player"]["reference"]
        state["native_actor_values"] = [{"actor": actor,
            "base": "dynamic:player-base:0000000000000001", "owner": 0, "process": 1,
            "values": [[0, None, None, None] for _ in range(72)]}]
        state["native_actor_breath"] = [{"actor": actor, "remaining": .125}]
        payload = state_io.encode_payload(state)
        entry = struct.pack("<I", len(actor)) + actor.encode() + struct.pack("<f", .125)
        self.assertEqual(payload[-len(entry)-4:], struct.pack("<I", 1) + entry)
        prefix = payload[:-len(entry)-4]
        for tail in (struct.pack("<I", 2) + entry * 2,
                     struct.pack("<I", 0xffffffff),
                     struct.pack("<I", 1) + entry[:-4] + struct.pack("<f", float("inf")),
                     struct.pack("<I", 1) + entry.replace(b"dynamic:", b"Dynamic:")):
            with self.subTest(tail=tail), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(prefix + tail)
        for count in range(1, len(entry) + 5):
            with self.subTest(count=count), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-count])

    def test_legacy_payloads_leave_native_breath_absent(self):
        for version in range(1, 14):
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
            loaded = state_io.decode_payload(state_io.encode_payload(state))
            self.assertNotIn("native_actor_breath", loaded)



    def engagement_state(self):
        state = make_state()
        state["schema_version"] = 15
        state["ai_rng_state"] = 1
        actor, base = "content:actors.esm:000001", "content:actors.esm:000002"
        state["references"] = [{"key": actor, "base": base, "cell": state["player"]["cell"],
                                "position": [0.0] * 6, "enabled": True, "deleted": False,
                                "inventory": [], "custom_state": {}, "owner": None, "lock_level": 0}]
        player = state["player"]["reference"]
        state["native_actor_values"] = [
            {"actor": key, "base": record, "owner": owner, "process": 1,
             "values": [[0, None, None, None] for _ in range(72)]}
            for key, record, owner in [(actor, base, 1), (player, "dynamic:player-base:0000000000000001", 0)]]
        state["native_actor_life"] = [
            {"actor": value["actor"], "base": value["base"], "phase": 0, "recovery_remaining": 0, "killer": "null"}
            for value in state["native_actor_values"]]
        state["native_combat_engagements"] = [{"first": actor, "second": player}]
        return state

    def test_owned_physical_action_v20_wire_legacy_and_invalid_bindings(self):
        state = self.engagement_state()
        state["schema_version"] = 20
        actor = state["native_actor_values"][0]["actor"]
        state["physical_actions"] = {"next": 7, "pending": [5, 2, 1]}
        state["physical_action_owners"] = [{"id": 5, "actor": actor}, {"id": 2, "actor": actor}]
        payload = state_io.encode_payload(state)
        decoded = state_io.decode_payload(payload)
        self.assertEqual(decoded["physical_action_owners"], list(reversed(state["physical_action_owners"])))
        self.assertEqual(state_io.encode_payload(decoded), payload)
        entry = struct.pack("<QI", 2, len(actor)) + actor.encode()
        tail = struct.pack("<I", 2) + entry + struct.pack("<QI", 5, len(actor)) + actor.encode()
        self.assertEqual(payload[-len(tail):], tail)
        prefix = payload[:-len(tail)]
        for bad in (struct.pack("<I", 2) + entry * 2, struct.pack("<I", 0xffffffff),
                    tail.replace(b"content:", b"Content:", 1)):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(prefix + bad)
        for cut in range(1, len(tail) + 1):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-cut])
        for change in (lambda x: x.update(schema_version=19),
                       lambda x: x["physical_action_owners"][0].update(id=0),
                       lambda x: x["physical_action_owners"][0].update(id=3),
                       lambda x: x["physical_action_owners"][0].update(id=True),
                       lambda x: x["physical_action_owners"][0].update(actor="null"),
                       lambda x: x["physical_action_owners"][0].update(actor="content:missing.esm:000001"),
                       lambda x: x["native_actor_values"].clear(),
                       lambda x: x["native_actor_life"].clear(),
                       lambda x: x["native_actor_life"][0].update(phase=1),
                       lambda x: x["native_actor_life"][0].update(phase=2)):
            broken = copy.deepcopy(state); change(broken)
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        state["schema_version"] = 19
        state["physical_action_owners"] = []
        old = state_io.encode_payload(state)
        self.assertNotIn("physical_action_owners", state_io.decode_payload(old))
        state["schema_version"] = 20
        expected = bytearray(old)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 20)
        self.assertEqual(state_io.encode_payload(state), bytes(expected) + struct.pack("<I", 0))

    def test_native_combat_engagement_roundtrip_and_validation(self):
        state = self.engagement_state()
        restored = state_io.decode_payload(state_io.encode_payload(state))
        self.assertEqual(restored["native_combat_engagements"], state["native_combat_engagements"])
        first, second = state["native_combat_engagements"][0].values()
        for entries in ([{"first": first, "second": first}], [{"first": second, "second": first}],
                        [{"first": "null", "second": second}], [{"first": first}],
                        [{"first": first, "second": second, "extra": 1}],
                        state["native_combat_engagements"] * 2):
            invalid = copy.deepcopy(state)
            invalid["native_combat_engagements"] = entries
            with self.subTest(entries=entries), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(invalid)
        for change in (lambda s: s.update(schema_version=14), lambda s: s["native_actor_values"].pop(),
                       lambda s: s["native_actor_life"].pop(),
                       lambda s: s["native_actor_life"][0].update(phase=1)):
            invalid = copy.deepcopy(state)
            change(invalid)
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(invalid)
        state["native_actor_life"][0]["phase"] = 2
        state_io.encode_payload(state)

    def test_native_combat_engagement_corrupt_wire_tail(self):
        state = self.engagement_state()
        payload = state_io.encode_payload(state)
        pair = state["native_combat_engagements"][0]
        entry = b"".join(struct.pack("<I", len(key)) + key.encode() for key in pair.values())
        self.assertEqual(payload[-len(entry)-4:], struct.pack("<I", 1) + entry)
        prefix = payload[:-len(entry)-4]
        for tail in (struct.pack("<I", 2) + entry * 2, struct.pack("<I", 0xffffffff),
                     struct.pack("<I", 1) + entry.replace(b"content:", b"Content:")):
            with self.subTest(tail=tail), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(prefix + tail)
        for cut in range(1, len(entry) + 5):
            with self.subTest(cut=cut), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-cut])

    def test_legacy_versions_do_not_invent_combat_membership(self):
        for version in range(1, 15):
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
            loaded = state_io.decode_payload(state_io.encode_payload(state))
            self.assertNotIn("native_combat_engagements", loaded)

    @staticmethod
    def clock_state():
        state = make_state()
        state["schema_version"] = 16
        state["ai_rng_state"] = 1
        actor = state["player"]["reference"]
        state["native_actor_values"] = [{"actor": actor,
            "base": "dynamic:player-base:0000000000000001", "owner": 0, "process": 1,
            "values": [[0, None, None, None] for _ in range(72)]}]
        state["native_actor_manager_time"] = .125
        state["native_actor_update_times"] = [{"actor": actor, "time": 100000.0}]
        return state

    def test_native_clock_round_trip_and_invalid_state(self):
        state = self.clock_state()
        for value in (0.0, -0.0, -.125, .125, 100000, -3.4028234663852886e38):
            state["native_actor_manager_time"] = value
            state["native_actor_update_times"][0]["time"] = value
            payload = state_io.encode_payload(state)
            restored = state_io.decode_payload(payload)
            self.assertEqual(restored["native_actor_manager_time"], value)
            self.assertEqual(math.copysign(1, restored["native_actor_manager_time"]), math.copysign(1, value))
            self.assertEqual(restored["native_actor_update_times"], state["native_actor_update_times"])
            self.assertEqual(state_io.encode_payload(restored), payload)
        for invalid in (100000.0078125, float("inf"), -float("inf"), float("nan"), 1e100, True, "0", None):
            broken = self.clock_state()
            broken["native_actor_manager_time"] = invalid
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
            broken = self.clock_state()
            broken["native_actor_update_times"][0]["time"] = invalid
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        for entries in ([{"actor": "dynamic:missing:0000000000000001", "time": 0}],
                        state["native_actor_update_times"] * 2, [{"actor": state["player"]["reference"]}]):
            broken = self.clock_state()
            broken["native_actor_update_times"] = entries
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(broken)
        state = self.clock_state()
        state["schema_version"] = 15
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(state)

    def test_native_clock_wire_negative_cases(self):
        state = self.clock_state()
        payload = state_io.encode_payload(state)
        entry = state["native_actor_update_times"][0]
        wire_entry = struct.pack("<I", len(entry["actor"])) + entry["actor"].encode() + struct.pack("<f", entry["time"])
        suffix = struct.pack("<fI", .125, 1) + wire_entry
        self.assertEqual(payload[-len(suffix):], suffix)
        prefix = payload[:-len(suffix)]
        for tail in (struct.pack("<fI", .125, 2) + wire_entry * 2,
                     struct.pack("<fI", .125, 0xffffffff),
                     struct.pack("<fI", float("inf"), 1) + wire_entry,
                     suffix[:-4] + struct.pack("<f", float("nan")),
                     suffix.replace(b"dynamic:", b"Dynamic:", 1)):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(prefix + tail)
        for cut in range(1, len(suffix) + 1):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-cut])

    def test_legacy_versions_do_not_invent_actor_clocks(self):
        for version in range(1, 16):
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
            loaded = state_io.decode_payload(state_io.encode_payload(state))
            self.assertNotIn("native_actor_manager_time", loaded)
            self.assertNotIn("native_actor_update_times", loaded)



    def melee_state(self):
        state = self.engagement_state()
        state["schema_version"] = 21
        actor = state["native_actor_values"][0]["actor"]
        state["physical_actions"] = {"next": 7, "pending": [5, 2, 1]}
        state["physical_action_owners"] = [{"id": 5, "actor": actor}, {"id": 2, "actor": actor}]
        state["native_melee_states"] = [{"actor": actor,
            "input": {"held_seconds": .25, "input_held": True, "prefer_left": False, "queued": 2},
            "strike": {"id": 2, "kind": 3, "weapon_base": "content:oblivion.esm:000400",
                "animation_group": "onehandattackforwardpower", "playback_speed": 1.25,
                "animation_time": .5, "contact_committed": False}}]
        return state

    def test_melee_v21_exact_wire_and_all_kinds_queues_commit_continuation(self):
        state = self.melee_state()
        entry = state["native_melee_states"][0]
        actor = entry["actor"]
        legacy = copy.deepcopy(state); legacy["schema_version"] = 20; legacy["native_melee_states"] = []
        prefix = bytearray(state_io.encode_payload(legacy))
        struct.pack_into("<I", prefix, len(state_io.MAGIC), 21)
        gear = entry["strike"]["weapon_base"]
        group = entry["strike"]["animation_group"]
        tail = (struct.pack("<II", 1, len(actor.encode())) + actor.encode() + struct.pack("<fBBBBQB", .25, 1, 0, 2, 1, 2, 3)
            + struct.pack("<I", len(gear.encode())) + gear.encode()
            + struct.pack("<I", len(group.encode())) + group.encode() + struct.pack("<ffB", 1.25, .5, 0))
        payload = state_io.encode_payload(state)
        self.assertEqual(payload, prefix + tail)
        for kind in range(7):
            for queued in range(3):
                for committed in (False, True):
                    with self.subTest(kind=kind, queued=queued, committed=committed):
                        candidate = copy.deepcopy(state)
                        melee = candidate["native_melee_states"][0]
                        melee["input"].update(queued=queued, input_held=False)
                        melee["strike"].update(kind=kind, contact_committed=committed, weapon_base="null")
                        if committed:
                            candidate["physical_actions"]["pending"].remove(2)
                            candidate["physical_action_owners"] = [x for x in candidate["physical_action_owners"] if x["id"] != 2]
                        encoded = state_io.encode_payload(candidate)
                        decoded = state_io.decode_payload(encoded)
                        self.assertEqual(decoded["native_melee_states"], candidate["native_melee_states"])
                        self.assertEqual(state_io.encode_payload(decoded), encoded)
        entry["strike"] = None
        self.assertIsNone(state_io.decode_payload(state_io.encode_payload(state))["native_melee_states"][0]["strike"])
        self.assertNotIn("native_melee_states", state_io.decode_payload(state_io.encode_payload(legacy)))
        promoted = copy.deepcopy(legacy); promoted["schema_version"] = 21
        self.assertEqual(state_io.encode_payload(promoted), prefix + bytes(4))

    def test_melee_v22_phase_wire_old_save_promotion_and_corruption(self):
        old = self.melee_state()
        old_payload = state_io.encode_payload(old)
        state = copy.deepcopy(old)
        state_io._upgrade_melee_phases(state)
        state["schema_version"] = 22
        expected = bytearray(old_payload)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 22)
        expected.append(0)
        self.assertEqual(state_io.encode_payload(state), bytes(expected))
        for phase in range(4):
            for committed in (False, True):
                candidate = copy.deepcopy(state)
                candidate["native_melee_states"][0]["strike"].update(ordinary_phase=phase, contact_committed=committed)
                if committed:
                    candidate["physical_actions"]["pending"].remove(2)
                    candidate["physical_action_owners"] = [x for x in candidate["physical_action_owners"] if x["id"] != 2]
                payload = state_io.encode_payload(candidate)
                self.assertEqual(payload[-1], phase)
                self.assertEqual(state_io.decode_payload(payload)["native_melee_states"], candidate["native_melee_states"])
        for value in (-1, 4, 255, True, 1., None):
            bad = copy.deepcopy(state)
            bad["native_melee_states"][0]["strike"]["ordinary_phase"] = value
            with self.subTest(value=value), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(bad)
        bad = copy.deepcopy(state); del bad["native_melee_states"][0]["strike"]["ordinary_phase"]
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(bad)
        state_io._upgrade_melee_phases(bad) # Current malformed fields must not be repaired.
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.encode_payload(bad)
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(expected[:-1]))
        expected[-1] = 255
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(expected))
        self.assertEqual(state_io.encode_payload(old), old_payload)
        body = (struct.pack("<4sII", b"VERS", 4, 21)
                + struct.pack("<4sI", b"DATA", len(old_payload)) + old_payload)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "old.omwsave"
            target = Path(directory) / "new.omwsave"
            untouched = struct.pack("<4sIII", b"TEST", 4, 0, 0) + b"keep"
            source.write_bytes(untouched + struct.pack("<4sIII", b"T4ST", len(body), 0, 0) + body)
            state_io.write_save(source, target, state_io.load_save(source))
            saved = state_io.load_save(target)
            self.assertEqual(saved["schema_version"], state_io.CURRENT_VERSION)
            self.assertEqual(saved["native_melee_states"][0]["strike"]["ordinary_phase"], 0)
            self.assertEqual(saved["physical_actions"]["pending"], [1, 2, 5])
            self.assertEqual(target.read_bytes()[:len(untouched)], untouched)
        promoted = copy.deepcopy(old); state_io._upgrade_melee_phases(promoted)
        self.assertEqual(promoted["native_melee_states"][0]["strike"]["ordinary_phase"], 0)
        self.assertEqual(promoted["physical_actions"], old["physical_actions"])
        self.assertEqual(promoted["physical_action_owners"], old["physical_action_owners"])

    def test_melee_v21_rejects_malformed_duplicate_replay_and_corrupt_tail(self):
        state = self.melee_state()
        changes = [lambda x: x.update(schema_version=20),
            lambda x: x["native_melee_states"].append(copy.deepcopy(x["native_melee_states"][0])),
            lambda x: x["native_melee_states"][0].update(actor="null"),
            lambda x: x["native_melee_states"][0].update(actor="content:missing.esm:000001"),
            lambda x: x["native_actor_life"][0].update(phase=2),
            lambda x: x["physical_action_owners"].clear(),
            lambda x: x["native_melee_states"][0]["strike"].update(contact_committed=True)]
        for key, values in (("held_seconds", (-1, math.nan, math.inf, True, 1e100)),
                            ("input_held", (0, 2, None)), ("prefer_left", (1, None)), ("queued", (-1, 3, True, .5))):
            for value in values:
                changes.append(lambda x, key=key, value=value: x["native_melee_states"][0]["input"].update({key: value}))
        for key, values in (("id", (0, 1, 6, 7, True)), ("kind", (-1, 7, True, .5)),
                            ("animation_time", (-1, math.nan, math.inf, True, 1e100)),
                            ("contact_committed", (0, 2, None)), ("weapon_base", (None, "Content:oblivion.esm:000400")),
                            ("animation_group", ("", "bad\0name", None)),
                            ("playback_speed", (0, -1, math.nan, math.inf, True, 1e100))):
            for value in values:
                changes.append(lambda x, key=key, value=value: x["native_melee_states"][0]["strike"].update({key: value}))
        for index, change in enumerate(changes):
            with self.subTest(change=index):
                broken = copy.deepcopy(state); change(broken)
                with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(broken)
        payload = state_io.encode_payload(state)
        legacy = copy.deepcopy(state); legacy["schema_version"] = 20; legacy["native_melee_states"] = []
        start = len(state_io.encode_payload(legacy))
        for end in range(start, len(payload)):
            with self.subTest(cut=end), self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(payload[:end])
        offset = start + 8 + len(state["native_melee_states"][0]["actor"].encode())
        for index in (offset+4, offset+5, offset+7, len(payload)-1):
            broken = bytearray(payload); broken[index] = 2
            with self.subTest(boolean=index), self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(broken)
        duplicate = bytearray(payload); struct.pack_into("<I", duplicate, start, 2); duplicate += payload[start+4:]
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(duplicate)

    def test_knockback_v30_wire_migration_and_rejection(self):
        state = self.melee_state()
        state_io._upgrade_melee_phases(state)
        state_io._upgrade_melee_timing(state)
        state_io._upgrade_melee_ai(state)
        state_io._upgrade_actor_draw(state)
        state["schema_version"] = 29
        old = state_io.encode_payload(state)
        state["schema_version"] = 30
        expected = bytearray(old)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 30)
        expected += bytes(4)
        self.assertEqual(state_io.encode_payload(state), expected)
        self.assertNotIn("native_actor_knockback", state_io.decode_payload(old))
        actor = state["native_actor_values"][0]["actor"]
        pulse = {"actor": actor, "acceleration": [-0., -2., 3.], "remaining": .125}
        state["native_actor_knockback"] = [pulse]
        payload = state_io.encode_payload(state)
        expected = expected[:-4] + struct.pack("<II", 1, len(actor.encode())) + actor.encode() + struct.pack("<ffff", -0., -2., 3., .125)
        self.assertEqual(payload, expected)
        restored = state_io.decode_payload(payload)
        self.assertEqual(restored["native_actor_knockback"], [pulse])
        self.assertEqual(math.copysign(1., restored["native_actor_knockback"][0]["acceleration"][0]), -1.)
        mutations = [lambda x: x.update(schema_version=29),
            lambda x: x["native_actor_knockback"].append(copy.deepcopy(pulse)),
            lambda x: x["native_actor_knockback"][0].update(actor="null"),
            lambda x: x["native_actor_knockback"][0].update(acceleration=[1, 2]),
            lambda x: x["native_actor_knockback"][0].update(acceleration=[1, float("inf"), 3])]
        for value in (-1., float("nan"), float("inf"), True, None):
            mutations.append(lambda x, v=value: x["native_actor_knockback"][0].update(remaining=v))
        for index, mutate in enumerate(mutations):
            bad = copy.deepcopy(state); mutate(bad)
            with self.subTest(index=index), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(bad)
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(payload[:-1])

    def ragdoll_state(self):
        state = self.melee_state()
        state_io._upgrade_melee_phases(state)
        state_io._upgrade_melee_timing(state)
        state_io._upgrade_melee_ai(state)
        state_io._upgrade_actor_draw(state)
        state["schema_version"] = 31
        owner = state["native_actor_values"][0]
        body = {"record": 12, "node_record": 8, "rotation": [1., 0., 0., 0., 1., 0., 0., 0., 1.],
                "position": [-0., -2., 3.], "linear_velocity": [1., 2., 3.], "angular_velocity": [4., 5., 6.]}
        state["native_actor_ragdolls"] = [{"actor": owner["actor"], "base": owner["base"],
            "model": "meshes/characters/_male/skeleton.nif", "asset_hash": "00112233445566778899aabbccddeeff",
            "bodies": [body]}]
        return state

    def test_ragdoll_v31_independent_wire_and_migration(self):
        state = self.ragdoll_state()
        old = copy.deepcopy(state)
        old["schema_version"] = 30
        del old["native_actor_ragdolls"]
        legacy = state_io.encode_payload(old)
        self.assertNotIn("native_actor_ragdolls", state_io.decode_payload(legacy))
        expected = bytearray(legacy)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 31)
        expected += struct.pack("<I", 1)
        entry = state["native_actor_ragdolls"][0]
        for field in ("actor", "base", "model", "asset_hash"):
            text = entry[field].encode()
            expected += struct.pack("<I", len(text)) + text
        expected += struct.pack("<III18f", 1, 12, 8, 1, 0, 0, 0, 1, 0, 0, 0, 1, -0., -2, 3, 1, 2, 3, 4, 5, 6)
        payload = state_io.encode_payload(state)
        self.assertEqual(payload, expected)
        restored = state_io.decode_payload(payload)
        self.assertEqual(restored["native_actor_ragdolls"], state["native_actor_ragdolls"])
        self.assertEqual(math.copysign(1., restored["native_actor_ragdolls"][0]["bodies"][0]["position"][0]), -1.)
        for size in range(len(legacy), len(payload)):
            with self.subTest(size=size), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:size])
        state_io._upgrade_actor_knockback(old)
        self.assertEqual(old["native_actor_ragdolls"], [])

    def test_ragdoll_v31_rejects_invalid_owner_asset_and_geometry(self):
        state = self.ragdoll_state()
        def pose(s): return s["native_actor_ragdolls"][0]
        def body(s): return pose(s)["bodies"][0]
        mutations = [lambda s: s.update(schema_version=30),
            lambda s: s["native_actor_ragdolls"].append(copy.deepcopy(pose(s))),
            lambda s: pose(s).update(actor="null"), lambda s: pose(s).update(base="null"),
            lambda s: pose(s).update(bodies=[]),
            lambda s: pose(s)["bodies"].append(copy.deepcopy(body(s))),
            lambda s: body(s).update(record=-1), lambda s: body(s).update(node_record=True),
            lambda s: body(s).update(rotation=[-1., 0., 0., 0., 1., 0., 0., 0., 1.]),
            lambda s: body(s).update(rotation=[1., .5, 0., 0., 1., 0., 0., 0., 1.]),
            lambda s: body(s).update(position=[0., 1.])]
        for model in ("", "../skeleton.nif", "/skeleton.nif", "Meshes/skeleton.nif", "meshes\\skeleton.nif", "meshes//skeleton.nif"):
            mutations.append(lambda s, m=model: pose(s).update(model=m))
        for digest in ("", "invalid", "00112233445566778899AABBCCDDEEFF"):
            mutations.append(lambda s, h=digest: pose(s).update(asset_hash=h))
        for field, size in [("rotation", 9), ("position", 3), ("linear_velocity", 3), ("angular_velocity", 3)]:
            for value in (float("inf"), float("nan"), True, None):
                mutations.append(lambda s, f=field, n=size, v=value: body(s).update({f: [v] + [0.] * (n-1)}))
        for index, mutate in enumerate(mutations):
            bad = copy.deepcopy(state)
            mutate(bad)
            with self.subTest(index=index), self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(bad)
        bad = copy.deepcopy(state)
        bad["schema_version"] = 30
        with self.assertRaises(state_io.RuntimeStateError):
            state_io._upgrade_actor_knockback(bad)

    def test_animation_v23_exact_wire_and_independent_clock_lifetime(self):
        state = self.melee_state()
        state_io._upgrade_melee_phases(state)
        state["schema_version"] = 22
        old = state_io.encode_payload(state)
        actor = state["native_actor_values"][0]["actor"]
        state_io._upgrade_melee_timing(state)
        state["schema_version"] = 23
        state["native_animation_clocks"] = [{"actor": actor, "clock": 1000.25}]
        timing = {"easing": True, "offset": None, "ease_start": None, "last_input": None,
                  "ease_end": .125, "weighted_time": 0., "output_time": 0.}
        state["native_melee_states"][0]["strike"]["sequence_timing"] = timing
        expected = bytearray(old)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 23)
        expected += bytes([1, 1, 0, 0, 0]) + struct.pack("<fff", .125, 0, 0)
        expected += struct.pack("<II", 1, len(actor.encode())) + actor.encode() + struct.pack("<f", 1000.25)
        payload = state_io.encode_payload(state)
        self.assertEqual(payload, expected)
        decoded = state_io.decode_payload(payload)
        self.assertEqual(decoded["native_animation_clocks"], state["native_animation_clocks"])
        self.assertEqual(decoded["native_melee_states"], state["native_melee_states"])
        timing.update(offset=-1000.25, ease_start=1000.25, last_input=.25, weighted_time=.25, output_time=.25)
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))["native_melee_states"], state["native_melee_states"])
        state["native_melee_states"] = []
        state["physical_actions"]["pending"] = []
        state["physical_action_owners"] = []
        state["native_actor_life"][0]["phase"] = 1
        state["native_combat_engagements"] = []
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))["native_animation_clocks"], state["native_animation_clocks"])
        migrated = state_io.decode_payload(old)
        state_io._upgrade_melee_timing(migrated)
        state_io._upgrade_melee_ai(migrated)
        migrated["schema_version"] = state_io.CURRENT_VERSION
        migrated = state_io.decode_payload(state_io.encode_payload(migrated))
        self.assertEqual(migrated["schema_version"], state_io.CURRENT_VERSION)
        self.assertEqual(migrated["native_animation_clocks"], [])
        self.assertIsNone(migrated["native_melee_states"][0]["strike"]["sequence_timing"])

    def test_animation_v23_rejects_bad_clocks_timing_and_bounded_wire_mutations(self):
        state = self.melee_state()
        state_io._upgrade_melee_phases(state)
        state_io._upgrade_melee_timing(state)
        state["schema_version"] = 23
        actor = state["native_actor_values"][0]["actor"]
        state["native_animation_clocks"] = [{"actor": actor, "clock": 0.}]
        state["native_melee_states"][0]["strike"]["sequence_timing"] = {
            "easing": True, "offset": None, "ease_start": None, "last_input": None,
            "ease_end": .125, "weighted_time": 0., "output_time": 0.}
        mutations = [lambda x: x["native_animation_clocks"].clear(),
            lambda x: x["native_animation_clocks"].append(copy.deepcopy(x["native_animation_clocks"][0])),
            lambda x: x["native_animation_clocks"][0].update(actor="null"),
            lambda x: x["native_animation_clocks"][0].update(actor="dynamic:missing:0000000000000001"),
            lambda x: x["native_melee_states"][0]["strike"]["sequence_timing"].update(offset=0),
            lambda x: x["native_melee_states"][0]["strike"]["sequence_timing"].update(easing=1)]
        for value in [-1., float("nan"), float("inf"), float("-inf"), True, 1e40]:
            mutations.append(lambda x, v=value: x["native_animation_clocks"][0].update(clock=v))
        for key in ["offset", "ease_start", "last_input", "ease_end", "weighted_time", "output_time"]:
            for value in [float("nan"), float("inf"), True, 1e40]:
                mutations.append(lambda x, k=key, v=value: x["native_melee_states"][0]["strike"]["sequence_timing"].update({k:v}))
        for i, mutate in enumerate(mutations):
            bad = copy.deepcopy(state); mutate(bad)
            with self.subTest(mutation=i), self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(bad)
        payload = state_io.encode_payload(state)
        for cut in range(1, 40):
            with self.subTest(cut=cut), self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(payload[:-cut])
        tail = 4 + 4 + len(actor.encode()) + 4
        start = len(payload)-tail
        duplicate = bytearray(payload); struct.pack_into("<I", duplicate, start, 2)
        duplicate += payload[start+4:]
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(duplicate)
        overflow = bytearray(payload); struct.pack_into("<I", overflow, start, 0xffffffff)
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(overflow)

    def test_condition24_native_bits_legacy_limits_and_invalid_metadata(self):
        for bits in [0, 0x80000000, 1, 0x33800000, 0x3eaaaaab, 0x42c7ffff, 0x43000000, 0x7f7fffff]:
            value = struct.unpack("<f", struct.pack("<I", bits))[0]
            state = make_state()
            state["ai_rng_state"] = 1
            state["schema_version"] = 24
            state["player"]["inventory"][0]["condition"] = value
            payload = state_io.encode_payload(state)
            restored = state_io.decode_payload(payload)
            result = restored["player"]["inventory"][0]["condition"]
            self.assertEqual(struct.unpack("<I", struct.pack("<f", result))[0], bits)
            self.assertEqual(state_io.encode_payload(restored), payload)
            if bits not in [0, 0x43000000]:
                state["schema_version"] = 23
                with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        state = make_state()
        state["ai_rng_state"] = 1
        state["schema_version"] = 23
        state["player"]["inventory"][0]["condition"] = 2147483647
        payload = state_io.encode_payload(state)
        self.assertEqual(state_io.encode_payload(state_io.decode_payload(payload)), payload)
        state["schema_version"] = 24
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))["player"]["inventory"][0]["condition"], 2147483648.)
        for bad in [-.5, -2., float("nan"), float("inf"), True, 1e40]:
            state["player"]["inventory"][0]["condition"] = bad
            with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)

    def test_native_process_knocked25_signed_wire_and_legacy_unknown(self):
        state = make_state()
        state["schema_version"] = 24
        state["ai_rng_state"] = 1
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 1, "values": [[0.0, None, None, None] for _ in range(72)]}
        state["native_actor_values"] = [actor]
        legacy = state_io.encode_payload(state)
        restored = state_io.decode_payload(legacy)
        self.assertNotIn("process_knocked_state", restored["native_actor_values"][0])
        self.assertEqual(state_io.encode_payload(restored), legacy)
        state["schema_version"] = 25
        unknown = state_io.encode_payload(state)
        self.assertIsNone(state_io.decode_payload(unknown)["native_actor_values"][0]["process_knocked_state"])
        for raw in range(-128, 128):
            actor["process_knocked_state"] = raw
            payload = state_io.encode_payload(state)
            decoded = state_io.decode_payload(payload)
            self.assertEqual(decoded["native_actor_values"][0]["process_knocked_state"], raw)
            self.assertEqual(state_io.encode_payload(decoded), payload)
            marker = next(i for i, (a, b) in enumerate(zip(unknown, payload)) if a != b)
            self.assertEqual(payload, unknown[:marker] + b"\x01" + struct.pack("<b", raw) + unknown[marker+1:])
            corrupt = bytearray(payload)
            corrupt[marker] = 2
            with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(corrupt)
        for bad in [-129, 128, 0.0, True, False, "1"]:
            actor["process_knocked_state"] = bad
            with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        actor["process_knocked_state"] = 0
        actor["process"] = 0
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        actor["process"] = 1
        state["schema_version"] = 24
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)

    def test_native_process_action26_signed_wire_and_legacy_unknown(self):
        state = make_state()
        state["schema_version"] = 25
        state["ai_rng_state"] = 1
        actor = {"actor": state["player"]["reference"], "base": "content:oblivion.esm:000007",
                 "owner": 0, "process": 1, "values": [[0.0, None, None, None] for _ in range(72)]}
        state["native_actor_values"] = [actor]
        legacy = state_io.encode_payload(state)
        restored = state_io.decode_payload(legacy)
        self.assertNotIn("process_action", restored["native_actor_values"][0])
        self.assertEqual(state_io.encode_payload(restored), legacy)
        state["schema_version"] = 26
        unknown = state_io.encode_payload(state)
        self.assertIsNone(state_io.decode_payload(unknown)["native_actor_values"][0]["process_action"])
        for raw in range(-32768, 32768):
            actor["process_action"] = raw
            payload = state_io.encode_payload(state)
            decoded = state_io.decode_payload(payload)
            self.assertEqual(decoded["native_actor_values"][0]["process_action"], raw)
            self.assertEqual(state_io.encode_payload(decoded), payload)
            marker = next(i for i, (a, b) in enumerate(zip(unknown, payload)) if a != b)
            self.assertEqual(payload, unknown[:marker] + b"\x01" + struct.pack("<h", raw) + unknown[marker+1:])
        corrupt = bytearray(payload)
        corrupt[marker] = 2
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(corrupt)
        for bad in [-32769, 32768, 6.0, True, False, "6"]:
            actor["process_action"] = bad
            with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        actor["process_action"] = -1
        actor["process"] = 0
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        actor["process"] = 1
        state["schema_version"] = 25
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)

    def test_combat_random27_unsigned_seed_wire_and_legacy_migration(self):
        for seed in (0, 1, 0x7fffffff, 0x80000000, 0xffffffff):
            state = make_m14_state()
            state["schema_version"] = 27
            state["combat_rng_state"] = seed
            payload = state_io.encode_payload(state)
            self.assertEqual(payload[-4:], struct.pack("<I", seed))
            loaded = state_io.decode_payload(payload)
            self.assertEqual(loaded["combat_rng_state"], seed)
            self.assertEqual(loaded["ai_rng_state"], state["ai_rng_state"])
            self.assertEqual(state_io.encode_payload(loaded), payload)
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-1])
            state["schema_version"] = 26
            if seed == 1:
                old = state_io.decode_payload(state_io.encode_payload(state))
                self.assertNotIn("combat_rng_state", old)
            else:
                with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        for bad in (-1, 1 << 32, 1.0, True, "1", None):
            state = make_state()
            state["schema_version"] = 27
            state["combat_rng_state"] = bad
            with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)

    def test_melee_ai28_exact_wire_validation_and_legacy_migration(self):
        state = self.melee_state()
        state_io._upgrade_melee_phases(state)
        state_io._upgrade_melee_timing(state)
        state_io._upgrade_melee_ai(state)
        state["schema_version"] = 28
        entry = state["native_melee_states"][0]
        entry["strike"] = None
        target = state["player"]["reference"]
        style = "content:oblivion.esm:000600"
        entry["ai_intent"] = {"target": target, "style": style}
        payload = state_io.encode_payload(state)
        decoded = state_io.decode_payload(payload)
        self.assertEqual(decoded["native_melee_states"], state["native_melee_states"])
        self.assertEqual(state_io.encode_payload(decoded), payload)
        old = copy.deepcopy(state)
        old["schema_version"] = 27
        del old["native_melee_states"][0]["ai_intent"]
        prefix = bytearray(state_io.encode_payload(old))
        struct.pack_into("<I", prefix, len(state_io.MAGIC), 28)
        tail = b"\x01" + struct.pack("<I", len(target)) + target.encode() + struct.pack("<I", len(style)) + style.encode()
        self.assertEqual(payload, bytes(prefix[:-8]) + tail + bytes(prefix[-8:]))
        offset = len(prefix) - 8
        bad = bytearray(payload); bad[offset] = 2
        with self.assertRaises(state_io.RuntimeStateError):
            state_io.decode_payload(bytes(bad))
        for cut in range(1, len(tail) + 9):
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:-cut])
        for changes in [
                lambda x: x["native_melee_states"][0]["ai_intent"].update(target=entry["actor"]),
                lambda x: x["native_melee_states"][0]["ai_intent"].update(target="content:missing.esm:000001"),
                lambda x: x["native_melee_states"][0]["ai_intent"].update(style="null"),
                lambda x: x["native_melee_states"][0]["ai_intent"].update(extra=1),
                lambda x: x["native_melee_states"][0].update(ai_intent=False),
                lambda x: x.update(native_combat_engagements=[]),
                lambda x: x.update(schema_version=27),
                lambda x: x["native_melee_states"][0].update(actor=target)]:
            invalid = copy.deepcopy(state); changes(invalid)
            with self.assertRaises(state_io.RuntimeStateError):
                state_io.encode_payload(invalid)
        downgrade = copy.deepcopy(state); downgrade["schema_version"] = 27
        with self.assertRaises(state_io.RuntimeStateError):
            state_io._upgrade_melee_ai(downgrade)
        for version in range(21, 28):
            legacy = copy.deepcopy(old); legacy["schema_version"] = version
            restored = state_io.decode_payload(state_io.encode_payload(legacy))
            self.assertNotIn("ai_intent", restored["native_melee_states"][0])
            state_io._upgrade_melee_ai(restored)
            self.assertIsNone(restored["native_melee_states"][0]["ai_intent"])

    def draw_state(self):
        state = self.engagement_state()
        state["schema_version"] = 28
        state["native_combat_engagements"] = []
        state["references"][0]["custom_state"] = {"boundary": "native-draw-wire-boundary"}
        return state_io.decode_payload(state_io.encode_payload(state))

    def test_actor_draw29_exact_nullable_wire_and_corrupt_presence_enum(self):
        legacy = self.draw_state()
        old = state_io.encode_payload(legacy)
        marker = b"native-draw-wire-boundary"
        boundary = old.index(marker) + len(marker)
        for draw in (None, 0, 1, 2):
            with self.subTest(draw=draw):
                state = copy.deepcopy(legacy)
                state["schema_version"] = 29
                state["references"][0]["actor_draw_state"] = draw
                expected = bytearray(old)
                struct.pack_into("<I", expected, len(state_io.MAGIC), 29)
                expected[boundary:boundary] = bytes([0]) if draw is None else bytes([1, draw])
                payload = state_io.encode_payload(state)
                self.assertEqual(payload, bytes(expected))
                restored = state_io.decode_payload(payload)
                self.assertEqual(restored, state)
                invalid = bytearray(payload); invalid[boundary] = 2
                with self.assertRaises(state_io.RuntimeStateError):state_io.decode_payload(bytes(invalid))
                if draw is not None:
                    invalid = bytearray(payload); invalid[boundary + 1] = 255
                    with self.assertRaises(state_io.RuntimeStateError):state_io.decode_payload(bytes(invalid))
                    with self.assertRaises(state_io.RuntimeStateError):state_io.decode_payload(payload[:boundary + 1])

    def test_actor_draw29_strict_types_native_ownership_and_legacy_label_rejection(self):
        state = self.draw_state(); state["schema_version"] = 29
        state["references"][0]["actor_draw_state"] = 1
        state_io.encode_payload(state)
        for draw in (-1, 3, 255, 0.0, 1.0, False, True, "1", [], {}):
            bad = copy.deepcopy(state); bad["references"][0]["actor_draw_state"] = draw
            with self.subTest(draw=draw), self.assertRaises(state_io.RuntimeStateError):state_io.encode_payload(bad)
        for mutate in (lambda x:x.update(schema_version=28), lambda x:x.update(native_actor_values=[]),
                       lambda x:x.update(native_actor_life=[]),
                       lambda x:x["native_actor_life"][0].update(base="content:actors.esm:000003"),
                       lambda x:x["references"].append(dict(x["references"][0],key="content:actors.esm:000003")),
                       lambda x:x["references"][0].update(key=x["player"]["reference"])):
            bad = copy.deepcopy(state); mutate(bad)
            with self.assertRaises(state_io.RuntimeStateError):state_io.encode_payload(bad)
        mislabeled = copy.deepcopy(state); mislabeled["schema_version"] = 28
        with self.assertRaises(state_io.RuntimeStateError):state_io._upgrade_actor_draw(mislabeled)

    def test_actor_draw_legacy_versions_have_no_fabricated_draw_intent(self):
        for version in range(1, 29):
            state = make_state(); state["schema_version"] = version; state["ai_rng_state"] = 1
            state["player"]["inventory"] = []
            if version < 3:
                for key in ("name", "race", "class", "birthsign", "female", "character_generation_flags"):
                    state["player"].pop(key)
            if version < 2:
                state.pop("script_event_sequence"); state.pop("script_instances"); state.pop("quests")
            state["references"] = [{"key":"content:oblivion.esm:000100", "base":"content:oblivion.esm:000200",
                "cell":state["player"]["cell"], "position":[0.0]*6, "enabled":True,"deleted":False,
                "inventory":[],"custom_state":{},"owner":None,"lock_level":0}]
            loaded = state_io.decode_payload(state_io.encode_payload(state))
            self.assertNotIn("actor_draw_state", loaded["references"][0])
            state_io._upgrade_actor_draw(loaded)
            self.assertIsNone(loaded["references"][0]["actor_draw_state"])
            state["references"][0]["actor_draw_state"] = 0
            with self.assertRaises(state_io.RuntimeStateError):state_io.encode_payload(state)


    def test_ragdoll_v32_packed_wire_and_legacy_absence(self):
        old = self.ragdoll_state()
        legacy = state_io.encode_payload(old)
        state = copy.deepcopy(old); state["schema_version"] = 32
        native = {"linear": [66.83216857910156, -133.66433715820312, 200.41783142089844, -0.],
                  "angular": [26.377605438232422, -52.755210876464844, 79.13282012939453, .2110208421945572]}
        state["native_actor_ragdolls"][0]["bodies"][0]["native_packed_velocity"] = native
        expected = bytearray(legacy); struct.pack_into("<I", expected, len(state_io.MAGIC), 32)
        expected += struct.pack("<B8f", 1, *native["linear"], *native["angular"])
        self.assertEqual(state_io.encode_payload(state), expected)
        decoded = state_io.decode_payload(expected)
        self.assertEqual(decoded["native_actor_ragdolls"], state["native_actor_ragdolls"])
        self.assertEqual(math.copysign(1., decoded["native_actor_ragdolls"][0]["bodies"][0]["native_packed_velocity"]["linear"][3]), -1.)
        self.assertNotIn("native_packed_velocity", state_io.decode_payload(legacy)["native_actor_ragdolls"][0]["bodies"][0])
        old["schema_version"] = 32; expected = bytearray(legacy)
        struct.pack_into("<I", expected, len(state_io.MAGIC), 32); expected += b"\x00"
        self.assertEqual(state_io.encode_payload(old), expected)
        self.assertNotIn("native_packed_velocity", state_io.decode_payload(expected)["native_actor_ragdolls"][0]["bodies"][0])

    def test_ragdoll_v32_packed_rejects_version_presence_and_payload(self):
        state = self.ragdoll_state(); state["schema_version"] = 32
        body = state["native_actor_ragdolls"][0]["bodies"][0]
        body["native_packed_velocity"] = {"linear": [1., 2., 3., -0.], "angular": [4., 5., 6., 8.]}
        payload = state_io.encode_payload(state)
        for cut in range(len(payload) - 33, len(payload)):
            with self.subTest(cut=cut), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:cut])
        malformed = bytearray(payload); malformed[-33] = 2
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(malformed)
        invalid = copy.deepcopy(state); invalid["schema_version"] = 31
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(invalid)
        invalid = copy.deepcopy(state); invalid["native_actor_ragdolls"][0]["bodies"][0]["native_packed_velocity"]["angular"][3] = float("nan")
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(invalid)
        extra = copy.deepcopy(body); extra["record"] = 13; extra["node_record"] = 9
        del extra["native_packed_velocity"]; state["native_actor_ragdolls"][0]["bodies"].append(extra)
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)

    def motion_ragdoll_state(self):
        state = self.ragdoll_state(); state["schema_version"] = 33
        body = state["native_actor_ragdolls"][0]["bodies"][0]
        body["native_packed_velocity"] = {"linear": [1., 2., 3., -0.], "angular": [4., 5., 6., 8.]}
        body["native_motion"] = 6
        return state

    def test_ragdoll_v33_motion_wire_and_legacy_absence(self):
        state = self.motion_ragdoll_state()
        old = copy.deepcopy(state); old["schema_version"] = 32
        del old["native_actor_ragdolls"][0]["bodies"][0]["native_motion"]
        legacy = state_io.encode_payload(old)
        expected = bytearray(legacy); struct.pack_into("<I", expected, len(state_io.MAGIC), 33)
        expected += b"\x01\x06"
        self.assertEqual(state_io.encode_payload(state), expected)
        self.assertEqual(state_io.decode_payload(expected)["native_actor_ragdolls"], state["native_actor_ragdolls"])
        self.assertNotIn("native_motion", state_io.decode_payload(legacy)["native_actor_ragdolls"][0]["bodies"][0])
        old["schema_version"] = 33; expected.pop(); expected[-1] = 0
        self.assertEqual(state_io.encode_payload(old), expected)
        self.assertNotIn("native_motion", state_io.decode_payload(expected)["native_actor_ragdolls"][0]["bodies"][0])
        state["native_actor_ragdolls"][0]["bodies"][0]["native_motion"] = 1
        expected[-1] = 1; expected += b"\x01"
        self.assertEqual(state_io.encode_payload(state), expected)

    def test_ragdoll_v33_motion_rejects_version_presence_and_payload(self):
        state = self.motion_ragdoll_state(); payload = state_io.encode_payload(state)
        for cut in range(len(payload) - 2, len(payload)):
            with self.subTest(cut=cut), self.assertRaises(state_io.RuntimeStateError):
                state_io.decode_payload(payload[:cut])
        malformed = bytearray(payload); malformed[-2] = 2
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(malformed)
        malformed = bytearray(payload); malformed[-1] = 2
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(malformed)
        for value in (0, 2, 255, True, "6"):
            invalid = copy.deepcopy(state); invalid["native_actor_ragdolls"][0]["bodies"][0]["native_motion"] = value
            with self.subTest(value=value), self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(invalid)
        invalid = copy.deepcopy(state); invalid["schema_version"] = 32
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(invalid)
        invalid = copy.deepcopy(state); del invalid["native_actor_ragdolls"][0]["bodies"][0]["native_packed_velocity"]
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(invalid)
        body = copy.deepcopy(state["native_actor_ragdolls"][0]["bodies"][0]); body["record"] = 13; body["node_record"] = 9
        del body["native_motion"]; state["native_actor_ragdolls"][0]["bodies"].append(body)
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)


if __name__ == "__main__":
    unittest.main()
