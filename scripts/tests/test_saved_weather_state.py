from __future__ import annotations

import copy
import struct
import tempfile
import unittest
from pathlib import Path

from scripts.saved_weather_state import decode_weather_state, read_weather_state
from scripts.tes4_runtime_state import RuntimeStateError


def sub(tag, value):
    return tag + struct.pack("<I", len(value)) + value


def record(tag, fields):
    body = b"".join(sub(tag, value) for tag, value in fields)
    return tag + struct.pack("<III", len(body), 0, 0) + body


def fields(native=True, swapped=False):
    values = [(b"CREG", b"\0"), (b"TMPS", struct.pack("<f", -1.25)), (b"FAST", b"\0"),
              (b"WUPD", struct.pack("<f", -3)), (b"TRFC", struct.pack("<f", .375)),
              (b"CWTH", struct.pack("<i", int(swapped))), (b"NWTH", struct.pack("<i", int(not swapped))),
              (b"QWTH", struct.pack("<i", -1))]
    keys = [b"content:A.ESM:1", b"content:b.esp:2"]
    if swapped:
        keys.reverse()
    if native:
        values += [(b"WXVR", struct.pack("<I", 1))] + [(b"WXID", key) for key in keys]
    values += [(b"RGNN", b"\x02climate"), (b"RGNW", struct.pack("<i", -1))]
    if swapped:
        values += [(b"RGDF", struct.pack("<i", 1))]
    for index, chance in enumerate([40, 60]):
        values += [(b"RGNC", bytes([chance]))]
        if swapped:
            values += [(b"RGIX", struct.pack("<i", 1 - index))]
    return values


class SavedWeatherStateTest(unittest.TestCase):
    def test_semantics_are_stable_across_catalog_order_and_explicit_bucket_mapping(self):
        old = decode_weather_state(record(b"WTHR", fields()))
        reordered = decode_weather_state(record(b"WTHR", fields(swapped=True)))
        self.assertEqual(old["current_weather"], "content:a.esm:000001")
        self.assertEqual(old["next_weather"], "content:b.esp:000002")
        self.assertEqual(old["regions"]["string:climate"]["selection_order"],
                         ["content:a.esm:000001", "content:b.esp:000002"])
        self.assertEqual(old["regions"]["string:climate"]["fallback"], "content:a.esm:000001")
        self.assertNotEqual(old.pop("catalog"), reordered.pop("catalog"))
        self.assertEqual(old, reordered)

    def test_region_form_keys_use_saved_masters_and_remain_stable_after_reordering(self):
        outputs = []
        for swapped in [False, True]:
            masters = [b"a.esm", b"b.esp"]
            if swapped:
                masters.reverse()
            weather = fields(swapped=swapped)
            weather[0] = (b"CREG", b"\x03" + struct.pack("<Ii", 0x881, int(swapped)))
            for i, (tag, _) in enumerate(weather):
                if tag == b"RGNN":
                    weather[i] = (tag, b"\x03" + struct.pack("<Ii", 0x882, int(swapped)))
            header = record(b"TES3", [(b"MAST", master + b"\0") for master in masters])
            state = decode_weather_state(header + record(b"WTHR", weather))
            self.assertEqual(state["current_region"], "content:a.esm:000881")
            self.assertIn("content:a.esm:000882", state["regions"])
            state.pop("catalog")
            outputs.append(state)
        self.assertEqual(*outputs)

    def test_legacy_weather_keeps_numeric_values_without_inventing_catalog_keys(self):
        state = decode_weather_state(record(b"WTHR", fields(native=False)))
        self.assertIsNone(state["catalog"])
        self.assertIsNone(state["identity_version"])
        self.assertEqual(state["current_weather"], 0)
        self.assertEqual(state["next_weather"], 1)
        self.assertIsNone(state["queued_weather"])
        self.assertEqual(state["regions"]["string:climate"]["selection_order"], [0, 1])

    def test_unreachable_tail_keeps_wire_chance_without_a_consumed_identity(self):
        weather = fields(swapped=True)
        chance = 0
        for i, (tag, _) in enumerate(weather):
            if tag == b"RGNC":
                weather[i] = (tag, bytes([100 if chance == 0 else 255]))
                chance += 1
            if tag == b"RGIX" and chance == 2:
                weather[i] = (tag, struct.pack("<i", -1))
        state = decode_weather_state(record(b"WTHR", weather))
        self.assertEqual(state["regions"]["string:climate"]["chances"], [100, 255])
        self.assertEqual(state["regions"]["string:climate"]["selection_order"],
                         ["content:a.esm:000001", None])

    def test_malformed_catalogs_orders_domains_and_record_framing_reject(self):
        valid = fields(swapped=True)
        faults = []
        for tag, payload in [(b"WXVR", struct.pack("<I", 2)), (b"WXVR", b"\x01"),
                             (b"WXID", b"dynamic:weather:1"), (b"WXID", b"broken"),
                             (b"CWTH", struct.pack("<i", -2)), (b"NWTH", struct.pack("<i", 2)),
                             (b"RGDF", struct.pack("<i", 2)), (b"WUPD", struct.pack("<f", float("nan")))]:
            fault = copy.deepcopy(valid)
            index = next(i for i, (name, _) in enumerate(fault) if name == tag)
            fault[index] = (tag, payload)
            faults.append(record(b"WTHR", fault))
        fault = copy.deepcopy(valid)
        catalog = [i for i, (tag, _) in enumerate(fault) if tag == b"WXID"]
        fault[catalog[1]] = fault[catalog[0]]
        faults.append(record(b"WTHR", fault))
        faults.append(record(b"WTHR", [x for x in valid if x[0] != b"WXVR"]))
        fault = copy.deepcopy(valid)
        order = [i for i, (tag, _) in enumerate(fault) if tag == b"RGIX"]
        del fault[order[-1]]
        faults.append(record(b"WTHR", fault))
        for index in [1, 2, -2]:
            fault = copy.deepcopy(valid)
            order = [i for i, (tag, _) in enumerate(fault) if tag == b"RGIX"]
            fault[order[-1]] = (b"RGIX", struct.pack("<i", index))
            faults.append(record(b"WTHR", fault))
        faults += [record(b"WTHR", valid) * 2, record(b"WTHR", valid)[:-1]]
        for index, data in enumerate(faults):
            with self.subTest(index=index), self.assertRaises(RuntimeStateError):
                decode_weather_state(data)

    def test_path_reader_does_not_change_the_save(self):
        data = record(b"WTHR", fields())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.omwsave"
            path.write_bytes(data)
            self.assertEqual(read_weather_state(path), decode_weather_state(data))
            self.assertEqual(path.read_bytes(), data)
