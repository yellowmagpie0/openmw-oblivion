"""Audit original Skeleton corpse physical admission, restoration and fresh kills."""
import argparse,hashlib,json,math,re
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blunt_use import shared
PLAYER='dynamic:player:0000000000000001';TARGET='content:oblivion.esm:06c2ed';BASE='content:oblivion.esm:024ddb';ASSET='d5618e8d5d15575282fffed5ffdc92b6'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['admit','restart','hot']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course;d=read(p/'driver-process.json')
 assert d['exit_code']==0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(d['input_save']);target=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(target);b,a=shared(source),shared(target)
 assert after['schema_version']==99 and runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 life=lambda s:next(x for x in s['native_actor_life'] if x['actor']==TARGET)
 actor=lambda s:next(x for x in s['native_actor_values'] if x['actor']==TARGET)
 ref=lambda s:next(x for x in s['references'] if x['key']==TARGET)
 rig=lambda s:next((x for x in s['native_actor_ragdolls'] if x['actor']==TARGET),None)
 corpse=rig(after);assert corpse and corpse['base']==BASE and corpse['asset_hash']==ASSET and corpse['model']=='meshes/creatures/skeleton/skeleton.nif' and len(corpse['bodies'])==17
 assert len({x['record'] for x in corpse['bodies']})==len({x['node_record'] for x in corpse['bodies']})==17
 assert life(after)['phase']==1 and life(after)['killer']==PLAYER and life(after)['base']==BASE
 assert not any(x['actor']==TARGET for x in after['native_getup_playback'])
 assert not any(x['actor']==TARGET for x in after['native_getup_idles'])
 assert actor(before)['passive_abilities']==actor(after)['passive_abilities'] and len(actor(after)['passive_abilities'])==1
 counts=lambda i:sorted((x['base'],x['count']) for x in i)
 assert counts(before['player']['inventory'])==counts(after['player']['inventory']) and counts(ref(before)['inventory'])==counts(ref(after)['inventory'])
 assert before['player']['character_generation']==after['player']['character_generation'] and b['base']==a['base']==35 and all(b[k]==a[k] for k in ['level_progress','attributes','specializations'])
 log=(p/'process.log').read_text();contacts=re.findall(r'M15 melee hit committed: actor=([^ ]+) id=(\d+) ([^\n]+)',log);kills=re.findall(r'M15 native death callback: id=(\d+) actor='+TARGET+r' killer='+PLAYER,log)
 assert log.count('M15 death body admitted: actor='+TARGET)==1
 if args.phase=='hot':
  assert life(before)['phase']==0 and not rig(before) and len(kills)==1 and after['next_death_event']==before['next_death_event']+1
  hits=[x for x in contacts if x[0]==PLAYER and 'victim='+TARGET in x[2] and float(re.search(r'health_damage=([\d.eE+-]+)',x[2])[1])>0]
  assert hits and len({x[1] for x in hits})==len(hits) and math.isclose(a['experience']-b['experience'],.5*len(hits),abs_tol=1e-6)
  assert 0<after['player']['actor_values']['health.current']<=before['player']['actor_values']['health.current']
 else:
  assert life(before)==life(after) and b==a and not contacts and not kills
  assert before['native_death_counts']==after['native_death_counts'] and before['next_death_event']==after['next_death_event']
  assert before['player']['inventory']==after['player']['inventory'] and ref(before)['inventory']==ref(after)['inventory']
  assert before['player']['actor_values']['health.current']==after['player']['actor_values']['health.current']
  if args.phase=='admit':assert not rig(before)
  else:
   previous=rig(before);assert previous and [(x['record'],x['node_record']) for x in previous['bodies']]==[(x['record'],x['node_record']) for x in corpse['bodies']]
   assert previous['internal_filters']==corpse['internal_filters'] and previous['asset_hash']==corpse['asset_hash']
 bounds=re.findall(r'CryptSkeleton corpse bounds=\(([-\d.eE+]+), ([-\d.eE+]+), ([-\d.eE+]+)\)',log)
 if bounds:
  height=float(bounds[-1][2])-ref(after)['position'][2];assert -80<height<65
 else:height=None
 events=[json.loads(x) for x in (p/'events.jsonl').read_text().splitlines()];load=[x for x in events if x['event']=='load-complete'];save=[x for x in events if x['event']=='save-complete'];assert len(load)==len(save)==1;epoch=read(p/'process-epoch.json');assert load[0]['pid']==save[0]['pid']==epoch['pid'] and load[0]['save_sha256']==sha(source) and save[0]['save_sha256']==sha(target) and read(p/save[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],player_health=after['player']['actor_values']['health.current'],blunt_before=b,blunt_after=a,life=life(after),bodies=len(corpse['bodies']),asset_hash=corpse['asset_hash'],internal_filters=corpse['internal_filters'],rendered_center_height=height,contacts=contacts,death_callbacks=kills,inventory=ref(after)['inventory'])
 (p/'death-pose-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
