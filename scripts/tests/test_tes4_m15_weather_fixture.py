import copy
import hashlib
import json
import struct
import sys
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_weather_fixture as fixture
from tes4_m15_fixture import record, records, string, sub
from tes4_m14_audit import _subrecords


class WeatherFixtureTests(unittest.TestCase):
    def inputs(self, compressed=False):
        path = Path(__file__).resolve().parents[1] / 'data/oblivion_compat/m15_weather_a_fixture.json'
        recipe = json.loads(path.read_text())
        payload = string('EDID', 'Original') + sub('DATA', bytes(range(16)))
        payload += sub('SNAM', struct.pack('<II', 0x1234, 1)) + sub('ZUNK', b'opaque\xff')
        data = struct.pack('<I', len(payload)) + zlib.compress(payload) if compressed else payload
        source = record('TES4', 0, b'') + record('WTHR', recipe['template'], data, 0x40000 if compressed else 0)
        recipe['source_sha256'] = hashlib.sha256(source).hexdigest()
        return source, recipe, payload

    def test_clone_preserves_payload_and_declares_master_deterministically(self):
        for compressed in (False, True):
            with self.subTest(compressed=compressed):
                source, recipe, payload = self.inputs(compressed)
                output, report = fixture.build(source, recipe)
                self.assertEqual((output, report), fixture.build(source, recipe))
                rows = list(records(output))
                self.assertEqual([(t, i) for t, i, _ in rows], [('TES4', 0), ('WTHR', 0x01000800)])
                fields = _subrecords(rows[1][2], 'output', 'WTHR')
                self.assertEqual([(x['name'], x['payload']) for x in fields if x['name'] != 'EDID'],
                                 [(x['name'], x['payload']) for x in _subrecords(payload, 'source', 'WTHR') if x['name'] != 'EDID'])
                self.assertEqual(fields[0]['payload'], b'M15CatalogueA\0')
                header = _subrecords(rows[0][2], 'header', 'TES4')
                self.assertEqual(next(x['payload'] for x in header if x['name'] == 'MAST'), b'Oblivion.esm\0')
                self.assertEqual(report['output_sha256'], hashlib.sha256(output).hexdigest())

    def test_rejects_unreviewed_source_or_invalid_identity(self):
        source, recipe, _ = self.inputs()
        mutations = [lambda x:x.update(extra=1), lambda x:x.update(version=True),
                     lambda x:x.update(master='Other.esm'), lambda x:x.update(purpose=''),
                     lambda x:x.update(source_sha256='0'*64), lambda x:x.update(template=True),
                     lambda x:x.update(template=0x01000001), lambda x:x['weather'].update(id=True),
                     lambda x:x['weather'].update(id=0x800), lambda x:x['weather'].update(id=0x02000800),
                     lambda x:x['weather'].update(editor_id='bad name')]
        for mutation in mutations:
            bad = copy.deepcopy(recipe); mutation(bad)
            with self.subTest(recipe=bad), self.assertRaises(ValueError):
                fixture.build(source, bad)

    def test_rejects_missing_or_ambiguous_template(self):
        _, recipe, payload = self.inputs()
        for source in (record('TES4', 0, b''), record('WTHR', recipe['template'], payload) * 2,
                       record('WTHR', recipe['template'], payload + string('EDID', 'Duplicate'))):
            bad = copy.deepcopy(recipe); bad['source_sha256'] = hashlib.sha256(source).hexdigest()
            with self.subTest(source=source), self.assertRaises(ValueError):
                fixture.build(source, bad)


if __name__ == '__main__':
    unittest.main()
