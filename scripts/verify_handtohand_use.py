"""Audit original Skeleton Hand-to-Hand use, miss and cold-load courses."""
import argparse,hashlib,json,math,re,struct
from pathlib import Path
import tes4_runtime_state as runtime
PLAYER='dynamic:player:0000000000000001';TARGET='content:oblivion.esm:06c2ed'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY');starts=[i for i,(n,_,_) in enumerate(subs) if n==b'STBA'];assert len(starts)>=28
 fields={n:data[a+8:b] for n,a,b in subs[starts[26]:starts[27]]};result=dict(base=struct.unpack('<f',fields[b'STBA'])[0],experience=struct.unpack('<f',fields.get(b'STPR',bytes(4)))[0]);fields={n:data[a+8:b] for n,a,b in subs}
 result.update(level_progress=struct.unpack('<i',fields.get(b'LPRO',bytes(4)))[0],attributes=list(struct.unpack('<8i',fields.get(b'INCR',bytes(32)))),specializations=list(struct.unpack('<3i',fields.get(b'SPEC',bytes(12)))))
 return result

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['baseline','hit','restart','miss']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course;driver=read(p/'driver-process.json')
 assert driver['exit_code']==0 and driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(driver['input_save']);target=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==driver['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(target);b,a=shared(source),shared(target)
 assert after['schema_version']==101 and runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 native=lambda s:next(v for v in s['native_actor_values'] if v['actor']==PLAYER)['values'][17]
 assert native(before)==native(after) and native(after)[0]==b['base']==a['base']==35
 assert all(b[k]==a[k] for k in ['level_progress','attributes','specializations'])
 assert before['player']['actor_values'].get('skill_advances.17',0)==after['player']['actor_values'].get('skill_advances.17',0)
 for key in set(before['player']['actor_values']) | set(after['player']['actor_values']):
  if key.startswith(('skill_advances.', 'pc_misc_stat.')):
   assert before['player']['actor_values'].get(key,0)==after['player']['actor_values'].get(key,0)
 assert before['player'].get('awarded_skill_books',[])==after['player'].get('awarded_skill_books',[])
 assert before['player']['character_generation']==after['player']['character_generation']
 counts=lambda s:sorted((i['base'],i['count']) for i in s['player']['inventory']);assert counts(before)==counts(after)
 assert not any(i['equipped_slots'] & 65536 for i in after['player']['inventory'])
 actor=lambda s:next(v for v in s['native_actor_values'] if v['actor']==TARGET)
 health=lambda s:sum(v or 0 for v in actor(s)['values'][8])
 assert actor(before)['passive_abilities']==actor(after)['passive_abilities'] and len(actor(after)['passive_abilities'])==1 and len(actor(after)['passive_abilities'][0]['effects'])==5
 log=(p/'process.log').read_text();contacts=re.findall(r'M15 melee (?:hit|miss) committed: actor='+PLAYER+r' ([^\n]+)',log)
 if args.phase in ['baseline','hit']:
  assert len(contacts)==1 and 'victim='+TARGET in contacts[0] and b['experience']==0
  assert 0<health(before)-health(after)<10
  assert math.isclose(a['experience'],.6 if args.phase=='hit' else 0,abs_tol=1e-6)
  assert float(re.search(r'health_damage=([\d.eE+-]+)',contacts[0])[1])>0
 else:
  assert b==a and health(before)==health(after)
  if args.phase=='restart':assert not contacts and 'M15 melee action: actor='+PLAYER not in log
  else:assert len(contacts)==1 and 'health_damage=0' in contacts[0] and 'victim=none' in contacts[0]
 assert after['player']['actor_values']['health.current']>0
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1;epoch=read(p/'process-epoch.json')
 assert loads[0]['pid']==saves[0]['pid']==epoch['pid'] and loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],before=b,after=a,contacts=contacts,enemy_health_before=health(before),enemy_health_after=health(after))
 (p/'handtohand-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
