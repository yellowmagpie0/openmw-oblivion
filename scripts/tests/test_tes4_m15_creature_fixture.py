import copy
import hashlib
import struct
import unittest

from tes4_m15_creature_fixture import build
from tes4_m15_fixture import record, records, string, sub
from tes4_m14_audit import _subrecords


class CreaturePlacementTests(unittest.TestCase):
    def setUp(self):
        self.cell = string("EDID", "TestCell") + sub("DATA", b"\x01") + sub("XCLL", bytes(36))
        self.source = (record("TES4", 0, b"") + record("CELL", 0x1234, self.cell)
                       + record("CREA", 0x2345, string("EDID", "TestCreature")))
        self.recipe = {"version": 1, "purpose": "Observe unchanged Creature", "master": "Oblivion.esm",
                       "source_sha256": hashlib.sha256(self.source).hexdigest(), "cell": 0x1234,
                       "creature": 0x2345, "reference": 0x01000800, "position": [1, 2, 3, 0, 0, 0]}

    def test_preserves_cell_and_places_only_requested_master_base(self):
        output, report = build(self.source, self.recipe)
        rows = list(records(output))
        self.assertEqual([(r[0], r[1]) for r in rows], [("TES4", 0), ("CELL", 0x1234), ("ACRE", 0x01000800)])
        self.assertEqual(rows[1][2], self.cell)
        fields = _subrecords(rows[2][2], "test", "ACRE")
        self.assertEqual([s["name"] for s in fields], ["EDID", "NAME", "DATA"])
        self.assertEqual(fields[1]["payload"], struct.pack("<I", 0x2345))
        self.assertEqual(fields[2]["payload"], struct.pack("<6f", 1, 2, 3, 0, 0, 0))
        self.assertEqual(report["script_count"], 0)
        self.assertFalse(report["base_changes"])
        self.assertEqual(build(self.source, self.recipe)[0], output)

    def test_rejects_unpinned_invalid_or_missing_inputs(self):
        for change in ({"source_sha256": "0" * 64}, {"version": True}, {"cell": 0x9999},
                       {"creature": 0x9999}, {"reference": 0x800}, {"reference": True},
                       {"position": [0, 0, float("nan"), 0, 0, 0]}, {"position": [0] * 5},
                       {"position": [0, 0, 0, 0, 0, 7]}, {"master": "Other.esm"}):
            recipe = copy.deepcopy(self.recipe); recipe.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                build(self.source, recipe)

    def test_rejects_exterior_or_duplicate_source_cell(self):
        for source in (self.source.replace(b"DATA\x01\x00\x01", b"DATA\x01\x00\x00"),
                       self.source + record("CELL", 0x1234, self.cell)):
            recipe = dict(self.recipe, source_sha256=hashlib.sha256(source).hexdigest())
            with self.assertRaises(ValueError):
                build(source, recipe)
