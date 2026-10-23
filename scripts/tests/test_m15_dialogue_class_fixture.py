import copy
import hashlib
import struct
import unittest

import tes4_m15_dialogue_class_fixture as fixture


class DialogueClassFixtureTest(unittest.TestCase):
    def source(self, actual=3, expected=3, existing_class=False):
        payloads = {
            'NPC_': fixture.sub('CNAM', struct.pack('<I', actual)),
            'CLAS': fixture.string('EDID', 'FixtureClass'),
            'DIAL': fixture.string('EDID', 'FixtureTopic'),
            'INFO': fixture.sub('DATA', bytes(3)) + fixture.sub('CTDA',
                struct.pack('<B3xfIIII', 0, 1., 68 if existing_class else 72, 1, 0, 0))
                + fixture.sub('SCHR', bytes(20)),
        }
        raw = fixture.record('TES4', 0, fixture.sub('HEDR', struct.pack('<fII', 1., 4, 0x800)))
        raw += fixture.record('NPC_', 1, payloads['NPC_'])
        raw += fixture.record('CLAS', expected, payloads['CLAS'])
        raw += fixture.group(b'DIAL', 0, fixture.record('DIAL', 2, payloads['DIAL'])
            + fixture.group(struct.pack('<I', 2), 7, fixture.record('INFO', 4, payloads['INFO'])))
        recipe = dict(schema_version=1, source_sha256=hashlib.sha256(raw).hexdigest(),
            source_plugin='Oblivion.esm', plugin_name='NativeClassCondition.esp', npc=1, topic=2, info=4,
            **{'class': expected}, source_payload_sha256={
                tag: hashlib.sha256(data).hexdigest() for tag, data in payloads.items()})
        return raw, recipe, payloads

    def test_adds_one_condition_and_preserves_response_result_and_topic(self):
        raw, recipe, before = self.source()
        saved = raw
        output, report = fixture.build(raw, recipe)
        records = {(tag, ident): data for tag, ident, data in fixture.records(output)}
        self.assertEqual(raw, saved)
        self.assertEqual(set(records), {('TES4', 0), ('DIAL', 2), ('INFO', 4)})
        self.assertEqual(records[('DIAL', 2)], before['DIAL'])
        after = fixture.reader._subrecords(records[('INFO', 4)], 'test', 'INFO')
        conditions = [s['payload'] for s in after if s['name'] == 'CTDA']
        self.assertEqual([struct.unpack_from('<I', c, 8)[0] for c in conditions], [72, 68])
        self.assertEqual(struct.unpack_from('<fII', conditions[1], 4), (1., 68, 3))
        restored = b''.join(fixture.sub(s['name'], s['payload']) for s in after
                            if not (s['name'] == 'CTDA' and s['payload'] == conditions[1]))
        self.assertEqual(restored, before['INFO'])
        self.assertTrue(report['structural_write_success'])
        self.assertFalse(report['runtime_acceptance'])

    def test_rejects_source_and_payload_drift(self):
        raw, recipe, _ = self.source()
        bad = copy.deepcopy(recipe); bad['source_sha256'] = '0' * 64
        with self.assertRaises(ValueError): fixture.build(raw, bad)
        bad = copy.deepcopy(recipe); bad['source_payload_sha256']['INFO'] = '0' * 64
        with self.assertRaises(ValueError): fixture.build(raw, bad)

    def test_rejects_a_class_other_than_the_npc_class(self):
        raw, recipe, _ = self.source(actual=3, expected=5)
        with self.assertRaisesRegex(ValueError, 'declared class'): fixture.build(raw, recipe)

    def test_rejects_duplicate_class_condition(self):
        raw, recipe, _ = self.source(existing_class=True)
        with self.assertRaisesRegex(ValueError, 'already exists'): fixture.build(raw, recipe)
