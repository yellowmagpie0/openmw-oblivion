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
    def bow_recipe(self):
        recipe = copy.deepcopy(self.recipe)
        recipe.update(version=2, ammo_template=0x17829)
        recipe['weapon']['type'] = 5
        recipe['ammo'] = dict(id=0x01000840, reference=0x01000841,
            editor_id='M15ReferenceArrow', name='M15 reference arrow', damage=20,
            speed=1, weight=0, count=20, position=[250, 40, -280], rotation=[0, 0, 0])
        source = self.source + record('AMMO', recipe['ammo_template'],
            string('MODL', 'weapons\\iron\\arrow.nif') + string('ICON', 'arrow.dds')
            + sub('ENAM', struct.pack('<I', 999)) + sub('SCRI', struct.pack('<I', 998)))
        recipe['source_sha256'] = hashlib.sha256(source).hexdigest()
        return source, recipe

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

    def test_room_does_not_clip_geometry_at_one_unit(self):
        data, _ = fixture.build(self.source, self.recipe)
        cell = next(payload for tag, _, payload in records(data) if tag == 'CELL')
        lighting = next(s['payload'] for s in _subrecords(cell, 'fixture', 'CELL') if s['name'] == 'XCLL')
        self.assertEqual(len(lighting), 36)
        # TES4 offsets 28/32 are directional fade and fog clip distance,
        # independently inspected in original prison/shop CELL records.
        self.assertEqual(struct.unpack_from('<ff', lighting, 28), (1., 0.))

    def test_room_uses_original_archive_path_separators(self):
        data, _ = fixture.build(self.source, self.recipe)
        room = next(payload for tag, _, payload in records(data) if tag == 'STAT')
        model = next(s['payload'] for s in _subrecords(room, 'fixture', 'STAT') if s['name'] == 'MODL')
        self.assertEqual(model, b'Architecture\\ImperialCity\\Interior\\ICGroundFloor01.NIF\0')

    def test_rejects_invalid_numeric_or_extensible_gameplay_payloads(self):
        for changes in ({'health':0}, {'health':-1}, {'health':2**32}, {'skill':256},
                        {'position':[float('nan'),0,0]}, {'script':'Kill player'}, {'essential':True}):
            bad = copy.deepcopy(self.recipe);bad['target'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):fixture.build(self.source,bad)
        for changes in ({'damage':65536}, {'weight':-1}, {'speed':float('inf')}, {'condition':0}):
            bad = copy.deepcopy(self.recipe);bad['weapon'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):fixture.build(self.source,bad)

    def test_version_one_bytes_remain_reproducible(self):
        # Captured from the pre-v2 writer and this hermetic source/recipe.
        data, _ = fixture.build(self.source, self.recipe)
        self.assertEqual(hashlib.sha256(data).hexdigest(),
            'aa86a690c71a64ce06157b5fb21e0c3b3be5eb41fa2bc1bb640fc846224de4a8')

    def test_bow_and_ammunition_are_real_script_free_pickups(self):
        source, recipe = self.bow_recipe()
        data, metadata = fixture.build(source, recipe)
        self.assertEqual(metadata['record_count'], 10)
        rows = {ident:(tag, {s['name']:s['payload'] for s in _subrecords(payload, 'fixture', tag)})
                for tag, ident, payload in records(data)}
        self.assertEqual(len(rows), 11)
        self.assertEqual(rows[recipe['weapon']['id']][1]['DATA'][0], 5)
        tag, ammo = rows[recipe['ammo']['id']]
        self.assertEqual(tag, 'AMMO')
        self.assertEqual(struct.unpack('<fB3xIfH', ammo['DATA']), (1., 0, 0, 0., 20))
        self.assertNotIn('ENAM', ammo)
        self.assertNotIn('SCRI', ammo)
        tag, placed = rows[recipe['ammo']['reference']]
        self.assertEqual(tag, 'REFR')
        self.assertEqual(struct.unpack('<I', placed['NAME'])[0], recipe['ammo']['id'])
        self.assertNotIn('XOWN', placed)
        self.assertNotIn('XESP', placed)
        self.assertEqual(struct.unpack('<i', placed['XCNT'])[0], 20)
        self.assertEqual(struct.unpack('<fII', rows[0][1]['HEDR'])[1], 10)
        self.assertEqual(fixture.build(source, recipe), (data, metadata))

    def test_bow_recipe_rejects_invalid_ammo_and_weapon_types(self):
        source, recipe = self.bow_recipe()
        for changes in ({'damage':65536}, {'count':0}, {'count':-1}, {'count':2**31},
                        {'speed':float('nan')}, {'weight':-1}, {'script':'Kill player'},
                        {'position':[0, float('inf'), 0]}, {'reference':recipe['target']['reference']}):
            bad = copy.deepcopy(recipe);bad['ammo'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):fixture.build(source, bad)
        for kind in (-1, 6, True, 5.0):
            bad = copy.deepcopy(recipe);bad['weapon']['type'] = kind
            with self.subTest(kind=kind), self.assertRaises(ValueError):fixture.build(source, bad)
        for change in ({'ammo_template':123456}, {'version':3}):
            bad = copy.deepcopy(recipe);bad.update(change)
            with self.assertRaises(ValueError):fixture.build(source, bad)
