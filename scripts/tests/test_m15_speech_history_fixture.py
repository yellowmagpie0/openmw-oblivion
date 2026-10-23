from pathlib import Path
import copy
import hashlib
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tes4_m15_speech_history_fixture import build
from tes4_m15_fixture import record, sub, records, reader

class SpeechHistoryFixtureTests(unittest.TestCase):
    def source(self, *, flags=4, extra=b''):
        info = sub('DATA', bytes([1,0,flags])) + sub('QSTI', struct.pack('<I', 0x900))
        info += sub('TRDT', bytes(16)) + sub('NAM1', b'line\0') + sub('SCHR', bytes(20)) + extra
        source = record('TES4',0,b'') + record('INFO',0x150c7,info)
        recipe = dict(schema_version=1,source_sha256=hashlib.sha256(source).hexdigest(),source_plugin='Oblivion.esm',
            plugin_name='M15SpeechHistory.esp',source_info=0x150c7,source_payload_sha256=hashlib.sha256(info).hexdigest(),
            topic=0x1000800,quest=0x1000801,topic_editor_id='M15SpeechHistoryTopic',quest_editor_id='M15SpeechHistoryQuest')
        return source,recipe,info
    def test_only_info_quest_binding_changes_and_authored_records_are_consistent(self):
        source,recipe,info=self.source()
        output,report=build(source,recipe)
        loaded={tag:(ident,payload) for tag,ident,payload in records(output)}
        self.assertEqual(set(loaded),{'TES4','INFO','DIAL','QUST'})
        before=reader._subrecords(info,'source','INFO')
        after=reader._subrecords(loaded['INFO'][1],'output','INFO')
        self.assertEqual(loaded['INFO'][0],0x150c7)
        for a,b in zip(before,after):
            self.assertEqual(a['name'],b['name'])
            self.assertEqual(b['payload'],struct.pack('<I',0x1000801) if a['name']=='QSTI' else a['payload'])
        self.assertEqual(len(before),len(after))
        topic=reader._subrecords(loaded['DIAL'][1],'output','DIAL')
        self.assertEqual(next(s['payload'] for s in topic if s['name']=='QSTI'),struct.pack('<I',0x1000801))
        quest=reader._subrecords(loaded['QUST'][1],'output','QUST')
        self.assertEqual(next(s['payload'] for s in quest if s['name']=='DATA'),b'\1\x32')
        self.assertTrue(report['structural_write_success'])
        self.assertFalse(report['semantic_reinspection'])
        self.assertFalse(report['runtime_acceptance'])
    def test_recipe_and_source_identity_are_bounded(self):
        source,recipe,_=self.source()
        for field,value in [('source_sha256','0'*64),('source_payload_sha256','0'*64),('schema_version',True),
                ('source_plugin','Other.esm'),('topic',True),('quest',recipe['topic']),('topic',0x800),
                ('plugin_name','../Other.esp'),('topic_editor_id','bad id'),('quest_editor_id','')]:
            bad=copy.deepcopy(recipe);bad[field]=value
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):build(source,bad)
    def test_non_once_and_callback_suppression_or_topic_links_reject(self):
        for flags,extra in [(0,b''),(12,b''),(20,b''),(4,sub('NAME',struct.pack('<I',1))),
                (4,sub('PNAM',struct.pack('<I',1))),(4,sub('QSTI',struct.pack('<I',2)))]:
            source,recipe,_=self.source(flags=flags,extra=extra)
            with self.subTest(flags=flags,extra=extra),self.assertRaises(ValueError):build(source,recipe)

if __name__=='__main__':unittest.main()
