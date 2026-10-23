from pathlib import Path
import copy
import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tes4_m15_dialogue_order_fixture import build
from tes4_m15_fixture import record,sub,records,reader


class DialogueOrderFixtureTests(unittest.TestCase):
    def inputs(self):
        original=sub('DATA',b'\1\0\4')+sub('QSTI',struct.pack('<I',0x900))+sub('TRDT',bytes(16))+sub('NAM1',b'line\0')+sub('SCHR',bytes(20))
        source=record('TES4',0,b'')+record('INFO',0x150c7,original)
        recipe=dict(schema_version=1,source_sha256=hashlib.sha256(source).hexdigest(),source_info=0x150c7,
            source_payload_sha256=hashlib.sha256(original).hexdigest(),base_plugin='Order.esm',patch_plugin='Order.esp',
            topic=0x800,low_quest=0x801,high_quest=0x802,low_priority=10,initial_high_priority=5,winning_high_priority=75,
            placements=[dict(info=0x810,quest='low',previous='append'),dict(info=0x811,quest='high',previous='append'),
                        dict(info=0x812,quest='high',previous=None),dict(info=0x813,quest='high',previous=0x811)],
            override_info=0x811,assets=[dict(extension=e,source_file='source.'+e,sha256=hashlib.sha256(b'asset-'+e.encode()).hexdigest()) for e in ['mp3','lip']])
        return source,recipe,original

    def test_clones_keep_source_bytes_and_patch_reuses_info_identity_without_predecessor(self):
        source,recipe,original=self.inputs();base,patch,report=build(source,recipe)
        original_fields=reader._subrecords(original,'source','INFO')
        for blob,is_patch in [(base,False),(patch,True)]:
            loaded=list(records(blob));infos=[(i,b) for t,i,b in loaded if t=='INFO']
            self.assertEqual([i for i,b in infos],[0x1000811] if is_patch else [0x1000810,0x1000811,0x1000812,0x1000813])
            for ident,body in infos:
                fields=reader._subrecords(body,'output','INFO')
                self.assertEqual([(r['name'],r['payload']) for r in fields if r['name'] not in ['QSTI','PNAM']],
                                 [(r['name'],r['payload']) for r in original_fields if r['name'] not in ['QSTI','PNAM']])
                previous=[struct.unpack('<I',r['payload'])[0] for r in fields if r['name']=='PNAM']
                self.assertEqual(previous,[] if is_patch or ident in [0x1000810,0x1000811] else [0 if ident==0x1000812 else 0x1000811])
            high=[b for t,i,b in loaded if t=='QUST' and i==0x1000802];self.assertEqual(len(high),1)
            fields=reader._subrecords(high[0],'output','QUST');self.assertEqual(next(r['payload'] for r in fields if r['name']=='DATA'),b'\1'+bytes([75 if is_patch else 5]))
        self.assertTrue(report['structural_write_success']);self.assertFalse(report['runtime_acceptance'])

    def test_recipe_rejects_collisions_invalid_links_priorities_and_unpinned_source(self):
        source,recipe,_=self.inputs()
        edits=[('schema_version',True),('topic',True),('low_quest',recipe['topic']),('winning_high_priority',256),
               ('initial_high_priority',-1),('base_plugin','../Order.esm'),('override_info',0x999),('source_sha256','0'*64),('assets',[])]
        for field,value in edits:
            bad=copy.deepcopy(recipe);bad[field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):build(source,bad)
        bad=copy.deepcopy(recipe);bad['placements'][0]['previous']=0x811
        with self.assertRaises(ValueError):build(source,bad)
        bad=copy.deepcopy(recipe);bad['placements'][1]['info']=0x810
        with self.assertRaises(ValueError):build(source,bad)

    def test_target_distance_guard_is_an_added_target_run_condition_on_only_the_named_info(self):
        source,recipe,_=self.inputs()
        recipe['target_distance_guard']=dict(info=0x812,greater_than=12.5)
        base,patch,report=build(source,recipe)
        for blob in (base,patch):
            for tag,ident,body in records(blob):
                if tag!='INFO':continue
                conditions=[r['payload'] for r in reader._subrecords(body,'output','INFO') if r['name']=='CTDA']
                self.assertEqual(conditions,[struct.pack('<B3xfIIII',0x42,12.5,1,0,0,0)] if ident==0x1000812 else [])
        self.assertEqual(report['target_distance_guard'],recipe['target_distance_guard'])
        # The override must keep the guard too when its INFO is the guarded one.
        recipe['override_info']=0x812
        _,patch,_=build(source,recipe)
        body=next(b for t,i,b in records(patch) if t=='INFO')
        self.assertEqual(sum(r['name']=='CTDA' for r in reader._subrecords(body,'output','INFO')),1)
        for guard in (None,{},dict(info=True,greater_than=0),dict(info=0x999,greater_than=0),
                      dict(info=0x812,greater_than=True),dict(info=0x812,greater_than=-1),
                      dict(info=0x812,greater_than=float('nan')),dict(info=0x812,greater_than=float('inf')),
                      dict(info=0x812,greater_than=1e39)):
            recipe['target_distance_guard']=guard
            with self.subTest(guard=guard),self.assertRaises(ValueError):build(source,recipe)

    def test_cli_checks_all_asset_hashes_before_creating_output(self):
        source,recipe,_=self.inputs()
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'source.esm').write_bytes(source);(root/'recipe.json').write_text(json.dumps(recipe));assets=root/'assets';assets.mkdir()
            for asset in recipe['assets']:(assets/asset['source_file']).write_bytes(b'wrong')
            command=[sys.executable,str(Path(sys.modules[build.__module__].__file__)),str(root/'source.esm'),str(root/'recipe.json'),str(assets),str(root/'output')]
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0);self.assertFalse((root/'output').exists())
            for asset in recipe['assets']:(assets/asset['source_file']).write_bytes(b'asset-'+asset['extension'].encode())
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            for row in recipe['placements']:
                for asset in recipe['assets']:
                    path=root/'output/sound/voice/order.esm/imperial/f'/f"dialogueorder_{row['info']:08x}_1.{asset['extension']}"
                    self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),asset['sha256'])


if __name__=='__main__':unittest.main()
