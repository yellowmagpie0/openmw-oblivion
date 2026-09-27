import hashlib
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_water_fixture as fixture
from tes4_m15_fixture import record, group, sub, records
from tes4_m14_audit import _subrecords


class WaterFixtureTests(unittest.TestCase):
    def setUp(self):
        self.cell = record('CELL', 0x900000, sub('DATA', b'\x01') + sub('FULL', b'Room\0'))
        self.other = record('SCPT', 0x900102, sub('SCTX', b'unchanged observer\0'))
        self.source = record('TES4', 0, b'') + group(b'CELL', 0, group(struct.pack('<I', 0), 2, self.cell)) + self.other
        self.recipe = {'source_sha256': hashlib.sha256(self.source).hexdigest(), 'cell': 0x900000, 'water_height': 1024}

    def test_reproducible_nested_water_edit_preserves_unrelated_records(self):
        result, report = fixture.build(self.source, self.recipe)
        self.assertEqual(fixture.build(self.source, self.recipe), (result, report))
        rows = list(records(result))
        self.assertEqual(rows[-1], list(records(self.other))[0])
        fields = {s['name']: s['payload'] for s in _subrecords(rows[1][2], 'test', 'CELL')}
        self.assertEqual(fields, {'DATA': b'\x03', 'FULL': b'Room\0', 'XCLW': struct.pack('<f', 1024)})
        self.assertEqual(report['sha256'], hashlib.sha256(result).hexdigest())
        self.assertFalse(report['runtime_accepted'])

    def test_wrong_source_malformed_recipe_and_target_are_rejected(self):
        for changes in ({'source_sha256': '0' * 64}, {'cell': True}, {'cell': 1}, {'cell': 0x900001},
                        {'water_height': float('nan')}, {'water_height': float('inf')},
                        {'water_height': True}, {'water_height': 4097}, {'unexpected': 0}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                fixture.build(self.source, self.recipe | changes)
        for data in (b'CELL', self.source[:-1], self.source + self.cell,
                     record('CELL', 0x900000, sub('DATA', b'')),
                     record('CELL', 0x900000, sub('DATA', b'\x01') * 2),
                     record('CELL', 0x900000, sub('DATA', b'\x01') + sub('XCLW', bytes(4)) * 2)):
            with self.subTest(data=data[:20]), self.assertRaises(ValueError):
                fixture.build(data, self.recipe | {'source_sha256': hashlib.sha256(data).hexdigest()})


if __name__ == '__main__':
    unittest.main()
