import copy
import hashlib
import struct
import unittest

import tes4_m15_fatigue_fixture as fixture
from tes4_m15_fixture import group, record, records, string, sub
from tes4_m14_audit import _subrecords


class FatigueFixtureTests(unittest.TestCase):
    def source(self, duplicate=False):
        gmst = record("GMST", 0x1234, string("EDID", "fFatigueReturnBase") + sub("DATA", struct.pack("<f", 10)))
        return (record("TES4", 0, sub("HEDR", struct.pack("<fII", 1, 2, 0x2000)))
                + group(b"GMST", 0, gmst + (gmst if duplicate else b""))
                + record("NPC_", 0x1800, b""))

    def recipe(self, source):
        return {"version": 1, "purpose": "Observe expenditure without regeneration",
                "source_sha256": hashlib.sha256(source).hexdigest(), "master": "Oblivion.esm",
                "settings": {"fFatigueReturnBase": 0, "fFatigueReturnMult": 0}}

    def test_only_two_zero_gmsts_with_matching_master_and_override_identity(self):
        source = self.source()
        output, report = fixture.build(source, self.recipe(source))
        parsed = list(records(output))
        self.assertEqual([x[0] for x in parsed], ["TES4", "GMST", "GMST"])
        self.assertEqual([x[1] for x in parsed[1:]], [0x1234, 0x01000800])
        header = _subrecords(parsed[0][2], "fixture", "TES4")
        self.assertEqual([x["payload"] for x in header if x["name"] == "MAST"], [b"Oblivion.esm\0"])
        for tag, _, payload in parsed[1:]:
            fields = _subrecords(payload, "fixture", tag)
            self.assertEqual([x["name"] for x in fields], ["EDID", "DATA"])
            self.assertEqual(fields[1]["payload"], bytes(4))
        self.assertEqual(report["record_count"], 2)
        self.assertEqual(report["script_count"], 0)
        self.assertFalse(report["actor_or_item_changes"])
        self.assertEqual(hashlib.sha256(output).hexdigest(), report["sha256"])
        self.assertEqual(fixture.build(source, self.recipe(source))[0], output)

    def test_rejects_unpinned_or_outcome_changing_recipe(self):
        source = self.source()
        for changes in ({"source_sha256": "0" * 64}, {"master": "Morrowind.esm"}, {"version": True},
                        {"settings": {"fHandHealthMax": 0}},
                        {"settings": {"fFatigueReturnBase": 1, "fFatigueReturnMult": 0}},
                        {"settings": {"fFatigueReturnBase": False, "fFatigueReturnMult": 0}}):
            recipe = copy.deepcopy(self.recipe(source)); recipe.update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                fixture.build(source, recipe)

    def test_duplicate_setting_identity_is_rejected(self):
        source = self.source(duplicate=True)
        with self.assertRaises(ValueError):
            fixture.build(source, self.recipe(source))


if __name__ == "__main__":
    unittest.main()
