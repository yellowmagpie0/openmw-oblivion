import copy
import hashlib
import json
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_fixture as fixture


class ObservationFixtureTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads((Path(__file__).resolve().parents[1]
                                 / 'data/oblivion_compat/m15_observation_fixture.json').read_text())
        self.source = fixture.record('TES4', 0, fixture.sub('HEDR', struct.pack('<fII', 1, 3, 0x900000)))
        for record in self.recipe['records']:
            self.source += fixture.record(record['type'], record['id'], fixture.string('EDID', record['type']))
        self.recipe['source_sha256'] = hashlib.sha256(self.source).hexdigest()
        self.recipe['copy_types'] = []

    def test_reproducible_structural_readback_has_only_declared_boot_and_floor_records(self):
        output, metadata = fixture.build(self.source, self.recipe)
        self.assertEqual(fixture.build(self.source, self.recipe), (output, metadata))
        records = list(fixture.records(output))
        self.assertEqual(len(records), metadata['record_count'] + 1)
        self.assertEqual(metadata['placed_floors'], 25)
        self.assertEqual(sum(r[0] == 'REFR' for r in records), 25)
        self.assertFalse({'SCPT', 'QUST', 'PACK'} & {r[0] for r in records})
        self.assertEqual(metadata['sha256'], hashlib.sha256(output).hexdigest())

    def test_wrong_revision_missing_boot_and_duplicate_records_fail(self):
        with self.assertRaisesRegex(ValueError, 'revision'):
            fixture.build(self.source + b'changed', self.recipe)
        missing = fixture.record('TES4', 0, b'')
        self.recipe['source_sha256'] = hashlib.sha256(missing).hexdigest()
        with self.assertRaisesRegex(ValueError, 'missing'):
            fixture.build(missing, self.recipe)
        self.recipe['source_sha256'] = hashlib.sha256(self.source).hexdigest()
        self.recipe['records'].append(copy.deepcopy(self.recipe['records'][0]))
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            fixture.build(self.source, self.recipe)

    def test_outcome_scripts_and_packages_cannot_enter_fixture(self):
        for tag in ('SCPT', 'QUST', 'PACK'):
            with self.subTest(tag=tag):
                self.recipe['copy_types'] = [tag]
                with self.assertRaises(ValueError):
                    fixture.build(self.source, self.recipe)
        self.recipe['copy_types'] = []
        self.recipe['records'][0]['fields'].append('SCRI')
        source = self.source + fixture.record('NPC_', 7, fixture.sub('SCRI', struct.pack('<I', 1)))
        self.recipe['source_sha256'] = hashlib.sha256(source).hexdigest()
        with self.assertRaisesRegex(ValueError, 'scripts'):
            fixture.build(source, self.recipe)

    def test_truncated_or_out_of_bounds_groups_are_rejected(self):
        for data in (b'TES4', struct.pack('<4sIIII', b'GRUP', 19, 0, 0, 0),
                     struct.pack('<4sIIII', b'CELL', 100, 0, 1, 0)):
            with self.subTest(data=data), self.assertRaises(ValueError):
                list(fixture.records(data))

    def test_floor_bounds_and_identity_collisions_fail(self):
        for changes in ({'radius': 0}, {'radius': 9}, {'spacing': 0}, {'reference_start': 7}):
            candidate = copy.deepcopy(self.recipe)
            candidate['floor'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                fixture.build(self.source, candidate)


if __name__ == '__main__':
    unittest.main()
