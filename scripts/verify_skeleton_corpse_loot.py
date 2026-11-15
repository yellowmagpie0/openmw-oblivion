"""Audit original physical Skeleton Take All and empty-corpse restart ownership."""
import argparse,hashlib,json
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blunt_use import shared
TARGET='content:oblivion.esm:06c2ed'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['loot','restart']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course;d=read(p/'driver-process.json')
 assert d['exit_code']==0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(d['input_save']);target=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(target)
 assert after['schema_version']==99 and runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 ref=lambda s:next(x for x in s['references'] if x['key']==TARGET)
 life=lambda s:next(x for x in s['native_actor_life'] if x['actor']==TARGET)
 rig=lambda s:next(x for x in s['native_actor_ragdolls'] if x['actor']==TARGET)
 count=lambda s,base:sum(x['count'] for x in s['player']['inventory'] if x['base']==base)
 assert ref(after)['inventory']==[] and life(before)==life(after) and life(after)['phase']==1
 assert rig(before)['asset_hash']==rig(after)['asset_hash']=='d5618e8d5d15575282fffed5ffdc92b6' and len(rig(after)['bodies'])==17
 assert [(x['record'],x['node_record']) for x in rig(before)['bodies']]==[(x['record'],x['node_record']) for x in rig(after)['bodies']]
 assert rig(before)['internal_filters']==rig(after)['internal_filters'] and not any(x['actor']==TARGET for x in after['native_getup_playback'])
 assert shared(source)==shared(target) and before['player']['character_generation']==after['player']['character_generation']
 assert before['player']['actor_values']['health.current']==after['player']['actor_values']['health.current']>0
 assert before['native_death_counts']==after['native_death_counts'] and before['next_death_event']==after['next_death_event']
 log=(p/'process.log').read_text();transferred=[]
 if args.phase=='loot':
  assert sorted((x['base'],x['count']) for x in ref(before)['inventory'])==[('content:oblivion.esm:01ebff',1),('content:oblivion.esm:0229bd',1)]
  bases={x['base'] for x in before['player']['inventory']}|{x['base'] for x in after['player']['inventory']}
  for base in bases:assert count(after,base)==count(before,base)+sum(x['count'] for x in ref(before)['inventory'] if x['base']==base)
  for item in ref(before)['inventory']:
   fields=['condition','charge','owner','ownership_rank','ownership_global','extra_scale','bound_source','script_owner']
   assert any(x['base']==item['base'] and all(x.get(f)==item.get(f) for f in fields) for x in after['player']['inventory'])
   transferred.append(dict(base=item['base'],count=item['count'],condition=item['condition']))
  assert 'M5 activation focus: result=object ref='+TARGET in log and 'CryptSkeleton corpse loot transferred' in log
 else:
  assert ref(before)['inventory']==[] and before['player']['inventory']==after['player']['inventory'] and 'M5 activation focus:' not in log and 'CryptSkeleton corpse stays empty' in log
 events=[json.loads(x) for x in (p/'events.jsonl').read_text().splitlines()];loads=[x for x in events if x['event']=='load-complete'];saves=[x for x in events if x['event']=='save-complete'];assert len(loads)==len(saves)==1;epoch=read(p/'process-epoch.json')
 assert loads[0]['pid']==saves[0]['pid']==epoch['pid'] and loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],transferred=transferred,player_health=after['player']['actor_values']['health.current'],blunt=shared(target),bodies=17,inventory_empty=True)
 (p/'corpse-loot-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
