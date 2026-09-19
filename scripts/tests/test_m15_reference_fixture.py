import copy
import hashlib
import json
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_reference_fixture as fixture
from tes4_m15_fixture import record, sub, string, records
from tes4_m14_audit import _subrecords


class ReferenceFixtureTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads((Path(__file__).resolve().parents[1]
            / 'data/oblivion_compat/m15_reference_fixture.json').read_text())
        npc = string('MODL', 'characters/_male/skeleton.nif')
        npc += sub('RNAM', struct.pack('<I', 0x907)) + sub('CNAM', struct.pack('<I', 0x230e6))
        npc += sub('SCRI', struct.pack('<I', 123)) + sub('SPLO', struct.pack('<I', 124))
        weapon = string('MODL', 'weapons/iron/longsword.nif') + string('ICON', 'test.dds')
        weapon += sub('ENAM', struct.pack('<I', 125))
        self.source = record('TES4', 0, sub('HEDR', struct.pack('<fII', 1, 2, 0x1000)))
        self.source += record('NPC_', 7, npc) + record('WEAP', 3084, weapon)
        self.recipe['source_sha256'] = hashlib.sha256(self.source).hexdigest()

    def test_writes_only_declared_records_and_preserves_full_health(self):
        self.recipe['target']['health'] = 100000
        data, metadata = fixture.build(self.source, self.recipe)
        self.assertEqual(fixture.build(self.source, self.recipe), (data, metadata))
        rows = list(records(data))
        self.assertEqual(len(rows), 9)
        self.assertEqual(metadata['record_count'], 8)
        self.assertEqual(metadata['script_count'], 0)
        self.assertEqual(metadata['quest_count'], 0)
        self.assertEqual(metadata['sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual({r[0] for r in rows}, {'TES4', 'CELL', 'STAT', 'REFR', 'NPC_', 'ACHR', 'CSTY', 'WEAP'})
        for tag, ident, payload in rows:
            subs = _subrecords(payload, 'fixture', tag)
            self.assertFalse({'SCRI', 'SCDA', 'SCTX', 'SPLO', 'PKID', 'ENAM'} & {s['name'] for s in subs}
                             if tag != 'NPC_' else {'SCRI', 'SCDA', 'SCTX', 'SPLO', 'PKID'} & {s['name'] for s in subs})
            byname = {s['name']:s['payload'] for s in subs}
            if tag == 'NPC_':
                self.assertEqual(struct.unpack_from('<I', byname['DATA'], 21)[0], 100000)
                self.assertEqual(len(byname['DATA']), 33)
                self.assertEqual(struct.unpack_from('<I', byname['ACBS'])[0] & (2 | 8 | 16 | 128), 0)
            if tag == 'WEAP':
                self.assertEqual(struct.unpack_from('<H', byname['DATA'], 28)[0], 100)
            if tag == 'TES4':
                self.assertEqual(byname['MAST'], b'Oblivion.esm\0')

    def test_rejects_changed_sources_missing_templates_and_identity_collisions(self):
        with self.assertRaises(ValueError):
            fixture.build(self.source + b'changed', self.recipe)
        for field in ('npc_template', 'weapon_template'):
            bad = copy.deepcopy(self.recipe);bad[field] = 123456
            with self.assertRaises(ValueError):fixture.build(self.source, bad)
        bad = copy.deepcopy(self.recipe);bad['target']['reference'] = bad['cell']['id']
        with self.assertRaises(ValueError):fixture.build(self.source, bad)
        bad = copy.deepcopy(self.recipe);bad['cell']['id'] = 0x800
        with self.assertRaises(ValueError):fixture.build(self.source, bad)

    def test_rejects_invalid_numeric_or_extensible_gameplay_payloads(self):
        for changes in ({'health':0}, {'health':-1}, {'health':2**32}, {'skill':256},
                        {'position':[float('nan'),0,0]}, {'script':'Kill player'}, {'essential':True}):
            bad = copy.deepcopy(self.recipe);bad['target'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):fixture.build(self.source,bad)
        for changes in ({'damage':65536}, {'weight':-1}, {'speed':float('inf')}, {'condition':0}):
            bad = copy.deepcopy(self.recipe);bad['weapon'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):fixture.build(self.source,bad)
