import copy
import hashlib
import json
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tes4_m15_stationary_fixture as fixture
from tes4_m15_fixture import record, records, string, sub
from tes4_m14_audit import _subrecords


class StationaryFixtureTests(unittest.TestCase):
    def inputs(self):
        directory = Path(__file__).resolve().parents[1] / 'data/oblivion_compat'
        recipe = json.loads((directory / 'm15_stationary_opponent_fixture.json').read_text())
        script = (directory / 'm15_stationary_opponent.obscript').read_text()
        npc = string('EDID', 'ValenDreth') + sub('ACBS', bytes(range(16)))
        npc += sub('DATA', bytes(range(33))) + sub('AIDT', bytes(range(12)))
        npc += sub('SNAM', b'faction-one') + sub('SNAM', b'faction-two') + sub('ZUNK', b'opaque\xff')
        npc += sub('ZNAM', struct.pack('<I', 0x900)) + sub('SCRI', struct.pack('<I', 0x901))
        source = record('TES4', 0, sub('HEDR', struct.pack('<fII', 1, 1, 0x25201)), 1)
        source += record('NPC_', recipe['npc']['id'], npc)
        recipe['source_sha256'] = hashlib.sha256(source).hexdigest()
        return source, recipe, script

    def test_reproducible_artifact_preserves_every_unrelated_npc_field_and_emits_bound_style_script(self):
        source, recipe, script = self.inputs()
        output, report = fixture.build(source, recipe, script)
        self.assertEqual(fixture.build(source, recipe, script), (output, report))
        self.assertEqual(report['sha256'], hashlib.sha256(output).hexdigest())
        rows = list(records(output))
        self.assertEqual([(t, i) for t, i, _ in rows], [('TES4', 0), ('CSTY', 0x1000880), ('NPC_', 0x25200), ('SCPT', 0x1000881)])
        old = _subrecords(list(records(source))[1][2], 'old', 'NPC_')
        new = _subrecords(next(p for t, _, p in rows if t == 'NPC_'), 'new', 'NPC_')
        self.assertEqual([x for x in old if x['name'] not in ('ZNAM', 'SCRI')], [x for x in new if x['name'] not in ('ZNAM', 'SCRI')])
        self.assertEqual([x['payload'] for x in new if x['name'] == 'ZNAM'], [struct.pack('<I', recipe['style']['id'])])
        self.assertEqual([x['payload'] for x in new if x['name'] == 'SCRI'], [struct.pack('<I', recipe['script']['id'])])
        style = report['style']['standard']
        for key, value in {'attack_chance':100, 'block_chance':0, 'dodge_chance':0, 'power_attack_chance':0,
                           'flags':34, 'do_not_acquire':True, 'idle_min':0, 'idle_max':0, 'hold_min':0, 'hold_max':0}.items():
            self.assertEqual(style[key], value)
        data = _subrecords(next(p for t, _, p in rows if t == 'SCPT'), 'script', 'SCPT')
        self.assertEqual(next(x['payload'] for x in data if x['name'] == 'SCTX'), script.encode('cp1252') + b'\0')
        self.assertEqual(next(x['payload'] for x in data if x['name'] == 'SCRO'), struct.pack('<I', 0x14))
        self.assertEqual(struct.unpack('<6I', next(x['payload'] for x in data if x['name'] == 'SLSD')), (1,0,0,0,1,0))
        self.assertNotIn('SCDA', [x['name'] for x in data]) # Fork source compilation, not original-game bytecode acceptance.

    def test_malformed_identity_profile_hash_and_script_inputs_fail_closed(self):
        source, recipe, script = self.inputs()
        mutations = [lambda x:x.update(extra=1), lambda x:x.update(version=True), lambda x:x.update(master='other.esm'),
                     lambda x:x.update(source_sha256='0'*64), lambda x:x['npc'].update(id=True),
                     lambda x:x['npc'].update(id=0x1000000), lambda x:x['npc'].update(editor_id='OtherActor'),
                     lambda x:x['style'].update(id=0x880), lambda x:x['script'].update(id=x['style']['id']),
                     lambda x:x['style'].update(editor_id='bad name'), lambda x:x['style'].update(attack_chance=True),
                     lambda x:x['style'].update(attack_chance=99), lambda x:x['style'].update(do_not_acquire=1),
                     lambda x:x['style'].update(idle=[0,float('nan')]), lambda x:x['style'].update(hold=[0]),
                     lambda x:x['style'].update(flags=0), lambda x:x['script'].update(sha256='0'*64)]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                bad = copy.deepcopy(recipe); mutation(bad)
                with self.assertRaises(ValueError):fixture.build(source,bad,script)
        for value in ('', script+'\0', script.replace('M15StationaryOpponent','OtherScript'), script+';changed'):
            with self.assertRaises(ValueError):fixture.build(source,recipe,value)

    def test_duplicate_missing_ambiguous_and_truncated_source_records_are_rejected(self):
        source, recipe, script = self.inputs()
        npc = list(records(source))[1][2]
        variants = [source+record('NPC_',recipe['npc']['id'],npc),
                    record('TES4',0,sub('HEDR',struct.pack('<fII',1,0,0x800)),1),
                    source[:-1], source+bytes(5),
                    record('NPC_',recipe['npc']['id'],npc+string('EDID','ValenDreth')),
                    record('NPC_',recipe['npc']['id'],npc+sub('ZNAM',bytes(4))),
                    record('NPC_',recipe['npc']['id'],npc+sub('SCRI',bytes(4)))]
        for data in variants:
            with self.subTest(size=len(data)):
                updated = copy.deepcopy(recipe);updated['source_sha256']=hashlib.sha256(data).hexdigest()
                with self.assertRaises(ValueError):fixture.build(data,updated,script)


if __name__ == '__main__':
    unittest.main()
