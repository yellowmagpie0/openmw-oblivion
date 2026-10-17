import copy
import hashlib
import struct
import unittest

from tes4_m15_creature_melee_fixture import build
from tes4_m15_fixture import record, records, string, sub
from tes4_m14_audit import _subrecords


class CreatureMeleeFixtureTests(unittest.TestCase):
    def setUp(self):
        self.stats = sub("ACBS", bytes(range(16))) + sub("DATA", bytes(range(20))) + sub("RNAM", b"\x32")
        self.source = (record("TES4", 0, b"")
                       + record("CELL", 0x1234, string("EDID", "TestCell") + sub("DATA", b"\x01"))
                       + record("CREA", 0x2345, string("EDID", "Dog") + string("FULL", "Dog")
                                + self.stats + sub("PKID", struct.pack("<I", 12))
                                + sub("SCRI", struct.pack("<I", 13)) + sub("SPLO", struct.pack("<I", 14))))
        self.recipe = {"version": 1, "purpose": "Production stationary Creature observation",
                       "source_sha256": hashlib.sha256(self.source).hexdigest(), "master": "Oblivion.esm",
                       "cell": 0x1234, "template": 0x2345, "creature": 0x01000800,
                       "style": 0x01000801, "reference": 0x01000802, "position": [0, 40, 0, 0, 0, 0]}

    def test_preserves_stats_and_assets_with_only_owned_style_and_placement(self):
        output, report = build(self.source, self.recipe)
        rows = list(records(output))
        self.assertEqual([r[0] for r in rows], ["TES4", "CREA", "CSTY", "CELL", "ACRE"])
        creature = _subrecords(rows[1][2], "test", "CREA")
        self.assertEqual(b"".join(sub(s["name"], s["payload"]) for s in creature
                                  if s["name"] in {"ACBS", "DATA", "RNAM"}), self.stats)
        self.assertFalse(any(s["name"] in {"PKID", "SCRI", "SPLO"} for s in creature))
        self.assertEqual(next(s["payload"] for s in creature if s["name"] == "ZNAM"), struct.pack("<I", 0x01000801))
        placed = _subrecords(rows[4][2], "test", "ACRE")
        self.assertEqual(next(s["payload"] for s in placed if s["name"] == "NAME"), struct.pack("<I", 0x01000800))
        self.assertTrue(report["stats_and_assets_preserved"])
        self.assertEqual(report["script_count"], 0)
        self.assertEqual(build(self.source, self.recipe)[0], output)

    def test_rejects_unpinned_missing_invalid_or_colliding_identities(self):
        for change in ({"source_sha256": "0" * 64}, {"template": 0x9999}, {"creature": 0x800},
                       {"style": 0x01000800}, {"reference": True}, {"version": True},
                       {"position": [0, 0, float("inf"), 0, 0, 0]}, {"unexpected": 1}):
            recipe = copy.deepcopy(self.recipe); recipe.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                build(self.source, recipe)

    def test_rejects_nonobject_recipe(self):
        with self.assertRaises(ValueError):
            build(self.source, None)
