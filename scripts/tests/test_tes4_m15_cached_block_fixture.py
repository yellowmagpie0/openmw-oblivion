import copy
import hashlib
import json
import struct
import sys
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_cached_block_fixture as fixture
from tes4_m15_fixture import record, records, string, sub
from tes4_m14_audit import _subrecords
import test_tes4_m15_stationary_fixture as stationary_tests


class CachedBlockFixtureTests(unittest.TestCase):
    def inputs(self, compressed=False):
        source, _, original_script = stationary_tests.StationaryFixtureTests().inputs()
        directory = Path(__file__).resolve().parents[1] / 'data/oblivion_compat'
        recipe = json.loads((directory / 'm15_cached_block_opponent_fixture.json').read_text())
        script = (directory / 'm15_cached_block_opponent.obscript').read_text()
        weapon = string('EDID', recipe['weapon']['editor_id'])
        weapon += sub('DATA', struct.pack('<B3xffIIIfH', 0, 1.2, .8, 0, 3, 56, 8, 5))
        weapon += sub('ZUNK', b'opaque\xff') + sub('ZUNK', b'repeated\x00')
        encoded = struct.pack('<I', len(weapon)) + zlib.compress(weapon) if compressed else weapon
        source += record('WEAP', recipe['weapon']['id'], encoded, 0x80 | (0x40000 if compressed else 0))
        recipe['source_sha256'] = hashlib.sha256(source).hexdigest()
        parent_recipe = copy.deepcopy(recipe)
        del parent_recipe['weapon']
        del parent_recipe['stationary_master']
        parent_recipe['version'] = 1
        parent_recipe['script']['sha256'] = hashlib.sha256(original_script.encode('cp1252')).hexdigest()
        parent, _ = fixture.stationary.build(source, parent_recipe, original_script)
        recipe['stationary_master']['sha256'] = hashlib.sha256(parent).hexdigest()
        return source, parent, recipe, script, weapon

    def test_only_speed_changes_and_unknown_fields_record_flags_and_base_fixture_survive(self):
        for compressed in (False, True):
            with self.subTest(compressed=compressed):
                source, parent, recipe, script, original = self.inputs(compressed)
                output, report = fixture.build(source, parent, recipe, script)
                self.assertEqual(fixture.build(source, parent, recipe, script), (output, report))
                self.assertEqual(report['sha256'], hashlib.sha256(output).hexdigest())
                self.assertEqual(report['record_count'], 2)
                self.assertEqual(report['weapon_flags'], 0x80)
                self.assertEqual(report['weapon_speed'], 20)
                rows = list(records(output))
                self.assertEqual([t for t, _, _ in rows], ['TES4', 'SCPT', 'WEAP'])
                old = _subrecords(original, 'source', 'WEAP')
                new = _subrecords(rows[-1][2], 'output', 'WEAP')
                self.assertEqual([(s['name'], s['payload']) for s in old if s['name'] != 'DATA'],
                                 [(s['name'], s['payload']) for s in new if s['name'] != 'DATA'])
                a = next(s['payload'] for s in old if s['name'] == 'DATA')
                b = next(s['payload'] for s in new if s['name'] == 'DATA')
                self.assertEqual(a[:4] + a[8:], b[:4] + b[8:])
                header = _subrecords(rows[0][2], 'output', 'TES4')
                self.assertEqual(struct.unpack_from('<I', next(s['payload'] for s in header if s['name'] == 'HEDR'), 4)[0], 2)
                base_recipe = copy.deepcopy(recipe)
                del base_recipe['weapon']
                del base_recipe['stationary_master']
                base_recipe['version'] = 1
                base, _ = fixture.stationary.build(source, base_recipe, script)
                self.assertEqual(rows[1:-1], [r for r in records(base) if r[0] == 'SCPT'])
                self.assertEqual([s['payload'] for s in header if s['name'] == 'MAST'], [b'Oblivion.esm\0', b'M15StationaryOpponent.esp\0'])

    def test_unpinned_invalid_speed_identity_and_unsupported_recipe_fail_closed(self):
        source, parent, recipe, script, _ = self.inputs()
        mutations = [lambda r: r.update(version=1), lambda r: r.update(version=True),
                     lambda r: r.update(extra=1), lambda r: r['stationary_master'].update(sha256='0'*64),
                     lambda r: r['stationary_master'].update(name='Other.esp'), lambda r: r.update(source_sha256='0'*64),
                     lambda r: r['weapon'].update(id=True), lambda r: r['weapon'].update(id=0x1000800),
                     lambda r: r['weapon'].update(editor_id='OtherSword')]
        mutations += [lambda r, v=v: r['weapon'].update(speed=v) for v in (True, 0, -1, 21, float('nan'), float('inf'))]
        for mutate in mutations:
            bad = copy.deepcopy(recipe)
            mutate(bad)
            with self.assertRaises(ValueError):
                fixture.build(source, parent, bad, script)
        changed = b''.join(record(tag, ident, payload.replace(b'fighting\0', b'changed_\0'))
                           for tag, ident, payload in records(parent))
        bad = copy.deepcopy(recipe)
        bad['stationary_master']['sha256'] = hashlib.sha256(changed).hexdigest()
        with self.assertRaisesRegex(ValueError, 'local layout'):
            fixture.build(source, changed, bad, script)

    def test_duplicate_missing_and_malformed_weapon_records_are_rejected(self):
        source, parent, recipe, script, weapon = self.inputs()
        base = b''.join(record(tag, ident, payload) for tag, ident, payload in records(source) if tag != 'WEAP')
        variants = [base, source + record('WEAP', recipe['weapon']['id'], weapon),
                    base + record('WEAP', recipe['weapon']['id'], weapon + sub('DATA', bytes(30))),
                    base + record('WEAP', recipe['weapon']['id'], string('EDID', recipe['weapon']['editor_id']) + sub('DATA', bytes(29)))]
        for data in variants:
            bad = copy.deepcopy(recipe)
            bad['source_sha256'] = hashlib.sha256(data).hexdigest()
            with self.assertRaises(ValueError):
                fixture.build(data, parent, bad, script)


if __name__ == '__main__':
    unittest.main()
