"""Author isolated, voiced INFO clones to exercise native quest/PNAM ordering.

The recipe and code are editable source. Game response/audio bytes are copied
only into the private generated fixture, never into repository source.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
from tes4_m15_fixture import records, record, group, sub, string, reader
from tes4_m15_speech_history_fixture import build as validate_source


def build(source: bytes, recipe: dict) -> tuple[bytes, bytes, dict]:
    required={'schema_version','source_sha256','source_info','source_payload_sha256','base_plugin','patch_plugin',
              'topic','low_quest','high_quest','low_priority','initial_high_priority','winning_high_priority',
              'placements','override_info','assets'}
    if not isinstance(recipe,dict) or not required <= set(recipe) or set(recipe) - required - {'target_distance_guard'} or type(recipe['schema_version']) is not int or recipe['schema_version']!=1:
        raise ValueError('dialogue order fixture recipe shape differs')
    for field,suffix in [('base_plugin','.esm'),('patch_plugin','.esp')]:
        name=recipe[field]
        if not isinstance(name,str) or not name.endswith(suffix) or any(c in name for c in '/\\\x00'):
            raise ValueError('dialogue order plugin name differs')
    for field in ['topic','low_quest','high_quest','override_info']:
        if type(recipe[field]) is not int or not 0x800<=recipe[field]<0x1000000:
            raise ValueError('dialogue order FormID is outside authored range')
    for field in ['low_priority','initial_high_priority','winning_high_priority']:
        if type(recipe[field]) is not int or not 0<=recipe[field]<=255:
            raise ValueError('dialogue order priority is outside uint8')
    placements=recipe['placements']
    if not isinstance(placements,list) or not 1<=len(placements)<=64:
        raise ValueError('dialogue order placements differ')
    ids={recipe[k] for k in ['topic','low_quest','high_quest']}
    if len(ids)!=3:raise ValueError('dialogue order definitions collide')
    loaded=set()
    for row in placements:
        if not isinstance(row,dict) or set(row)!={'info','quest','previous'} or type(row['info']) is not int or not 0x800<=row['info']<0x1000000 or row['info'] in ids or row['quest'] not in ('low','high'):
            raise ValueError('dialogue order placement differs')
        previous=row['previous']
        if previous is not None and previous!='append' and (type(previous) is not int or previous not in loaded):
            raise ValueError('dialogue order predecessor must already be loaded')
        loaded.add(row['info']);ids.add(row['info'])
    if recipe['override_info'] not in loaded:raise ValueError('dialogue order override is not an existing INFO')
    guard=recipe.get('target_distance_guard')
    if 'target_distance_guard' in recipe:
        if not isinstance(guard,dict) or set(guard)!={'info','greater_than'} or type(guard['info']) is not int or guard['info'] not in loaded:
            raise ValueError('dialogue order target-distance guard identity differs')
        threshold=guard['greater_than']
        if type(threshold) not in (int,float) or not math.isfinite(threshold) or not 0<=threshold<=3.4028234663852886e38:
            raise ValueError('dialogue order target-distance guard threshold differs')
    if not isinstance(recipe['assets'],list) or {r.get('extension') for r in recipe['assets']}!={'mp3','lip'} or len(recipe['assets'])!=2:
        raise ValueError('dialogue order voice assets differ')
    for asset in recipe['assets']:
        if set(asset)!={'extension','source_file','sha256'} or Path(asset['source_file']).name!=asset['source_file'] or not asset['source_file'].endswith('.'+asset['extension']):
            raise ValueError('dialogue order asset name differs')
    raw_id=lambda local:0x1000000+local
    # Reuse the strict pinned-source/SayOnce/response contract before cloning.
    validate_source(source,dict(schema_version=1,source_sha256=recipe['source_sha256'],source_plugin='Oblivion.esm',
        plugin_name=recipe['patch_plugin'],source_info=recipe['source_info'],source_payload_sha256=recipe['source_payload_sha256'],
        topic=raw_id(recipe['topic']),quest=raw_id(recipe['low_quest']),topic_editor_id='M15DialogueOrder',quest_editor_id='M15DialogueOrderLow'))
    payload=next(b for t,i,b in records(source) if t=='INFO' and i==recipe['source_info'])
    fields=reader._subrecords(payload,'source','INFO');data=next(r['payload'] for r in fields if r['name']=='DATA')
    if sum(r['name']=='TRDT' for r in fields)!=1:raise ValueError('dialogue order fixture requires one original response')
    def quest(local,priority):
        return record('QUST',raw_id(local),string('EDID','M15DialogueOrderQuest'+str(local))+sub('DATA',bytes([1,priority])))
    def topic():
        return record('DIAL',raw_id(recipe['topic']),string('EDID','M15DialogueOrderTopic')+sub('DATA',data[:1]))
    def info(row,override=False):
        owner=recipe[row['quest']+'_quest']
        out=b''.join(sub(r['name'],struct.pack('<I',raw_id(owner)) if r['name']=='QSTI' else r['payload']) for r in fields)
        if not override and row['previous']!='append':
            previous=0 if row['previous'] is None else raw_id(row['previous'])
            out+=sub('PNAM',struct.pack('<I',previous))
        if guard is not None and row['info']==guard['info']:
            # Greater-than + run-on-target, required reference argument zero:
            # native56AC50 must bind the original speaker as the comparison.
            out+=sub('CTDA',struct.pack('<B3xfIIII',0x42,guard['greater_than'],1,0,0,0))
        return record('INFO',raw_id(row['info']),out)
    def header(name,count,masters,esm):
        body=sub('HEDR',struct.pack('<fII',1.,count,max(ids)+1))+string('CNAM','OpenMW dialogue order fixture')
        for master,size in masters:body+=string('MAST',master)+sub('DATA',struct.pack('<Q',size))
        return record('TES4',0,body,1 if esm else 0)
    children=group(struct.pack('<I',raw_id(recipe['topic'])),7,b''.join(info(r) for r in placements))
    base=header(recipe['base_plugin'],3+len(placements),[('Oblivion.esm',len(source))],True)
    base+=group(b'QUST',0,quest(recipe['low_quest'],recipe['low_priority'])+quest(recipe['high_quest'],recipe['initial_high_priority']))
    base+=group(b'DIAL',0,topic()+children)
    overridden=next(r for r in placements if r['info']==recipe['override_info'])
    patch=header(recipe['patch_plugin'],3,[('Oblivion.esm',len(source)),(recipe['base_plugin'],len(base))],False)
    patch+=group(b'QUST',0,quest(recipe['high_quest'],recipe['winning_high_priority']))
    patch+=group(b'DIAL',0,topic()+group(struct.pack('<I',raw_id(recipe['topic'])),7,info(overridden,True)))
    report=dict(schema_version=1,structural_write_success=True,base_sha256=hashlib.sha256(base).hexdigest(),
        patch_sha256=hashlib.sha256(patch).hexdigest(),source_info_sha256=recipe['source_payload_sha256'],
        original_response_conditions_and_result_bytes_preserved=True,original_info_unchanged=True,
        mutations=['Authored topic and two quests','Four independent voiced INFO identities with authored quest/PNAM',
                   'Patch changes high quest priority and overrides one INFO without PNAM'],
        compiler_world_validation=False,semantic_reinspection=False,runtime_acceptance=False)
    if guard is not None:
        report['mutations'].append('Added target-run GetDistance > threshold with null first reference argument')
        report['target_distance_guard']=guard
    return base,patch,report


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source',type=Path);parser.add_argument('recipe',type=Path)
    parser.add_argument('assets',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    if args.output.exists():raise FileExistsError(args.output)
    recipe=json.loads(args.recipe.read_text());base,patch,report=build(args.source.read_bytes(),recipe)
    for asset in recipe['assets']:
        if hashlib.sha256((args.assets/asset['source_file']).read_bytes()).hexdigest()!=asset['sha256']:
            raise ValueError('dialogue order voice source differs')
    args.output.mkdir(parents=True,exist_ok=False);(args.output/recipe['base_plugin']).write_bytes(base);(args.output/recipe['patch_plugin']).write_bytes(patch)
    voice=args.output/'sound'/'voice'/recipe['base_plugin'].lower()/'imperial'/'f';voice.mkdir(parents=True)
    for row in recipe['placements']:
        for asset in recipe['assets']:shutil.copyfile(args.assets/asset['source_file'],voice/f"dialogueorder_{row['info']:08x}_1.{asset['extension']}")
    (args.output/'fixture-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

if __name__=='__main__':main()
