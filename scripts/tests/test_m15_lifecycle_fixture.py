import copy
import hashlib
import json
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_lifecycle_fixture as fixture
from tes4_m14_audit import _subrecords


class LifecycleFixtureTests(unittest.TestCase):
    def setUp(self):
        data = Path(__file__).resolve().parents[1] / 'data/oblivion_compat'
        self.boot = json.loads((data / 'm15_observation_fixture.json').read_text())
        self.recipe = json.loads((data / 'm15_lifecycle_fixture.json').read_text())
        self.script = (data / 'm15_lifecycle_observer.obscript').read_text()
        self.source = fixture.record('TES4', 0, b'')
        for item in self.boot['records']:
            payload = fixture.string('EDID', 'Template')
            if item['type'] == 'NPC_':
                payload += fixture.string('MODL', 'Characters/_Male/Skeleton.NIF')
                payload += fixture.sub('RNAM', struct.pack('<I', 2311))
                payload += fixture.sub('CNAM', struct.pack('<I', 143590))
            self.source += fixture.record(item['type'], item['id'], payload)
        self.boot['source_sha256'] = hashlib.sha256(self.source).hexdigest()
        self.boot['copy_types'] = []

    def test_record_bindings_source_and_stats_survive_readback(self):
        output, report = fixture.build(self.source, self.boot, self.recipe, self.script)
        self.assertEqual(fixture.build(self.source, self.boot, self.recipe, self.script), (output, report))
        rows = list(fixture.records(output))
        self.assertEqual(len(rows), report['record_count'] + 1)
        fields = {(tag, ident): {s['name']: s['payload'] for s in _subrecords(payload, 'test', tag)}
                  for tag, ident, payload in rows}
        npc = fields['NPC_', self.recipe['npc']]
        self.assertEqual(struct.unpack('<I', npc['SCRI'])[0], self.recipe['script'])
        self.assertEqual(struct.unpack('<IHHHhHH', npc['ACBS']), (512, 0, 100, 0, 1, 0, 0))
        self.assertEqual(npc['DATA'][21:25], struct.pack('<I', 100))
        self.assertEqual(fields['ACHR', self.recipe['reference']]['NAME'], struct.pack('<I', self.recipe['npc']))
        script = fields['SCPT', self.recipe['script']]
        self.assertEqual(script['SCTX'].rstrip(b'\0').decode(), self.script)
        self.assertEqual(script['SCVR'], b'deaths\0')
        self.assertNotIn('SCDA', script)
        self.assertNotIn('QUST', {tag for tag, _, _ in rows})
        self.assertFalse(report['runtime_accepted'])

    def test_corrupt_recipe_and_missing_dependencies_fail(self):
        for changes in ({'npc': 7}, {'script': self.recipe['reference']}, {'reference': True},
                        {'position': [float('nan'), 0, 0]}, {'rotation': [0, 0]},
                        {'npc_template': 8}, {'unexpected': 1}):
            recipe = copy.deepcopy(self.recipe)
            recipe.update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                fixture.build(self.source, self.boot, recipe, self.script)
        with self.assertRaisesRegex(ValueError, 'revision'):
            fixture.build(self.source + b'changed', self.boot, self.recipe, self.script)
        for script in ('', 'a\0b'):
            with self.assertRaises(ValueError):
                fixture.build(self.source, self.boot, self.recipe, script)
        self.boot['records'][0]['fields'].remove('RNAM')
        with self.assertRaisesRegex(ValueError, 'appearance'):
            fixture.build(self.source, self.boot, self.recipe, self.script)


if __name__ == '__main__':
    unittest.main()
