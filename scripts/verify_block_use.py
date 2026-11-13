"""Audit original Mudcrab blocked contacts and Player Block experience."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
import tes4_runtime_state as runtime
PLAYER='dynamic:player:0000000000000001';TARGET='content:oblivion.esm:06c927'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY');starts=[i for i,(n,_,_) in enumerate(subs) if n==b'STBA'];assert len(starts)>=28
 fields={n:data[a+8:b] for n,a,b in subs[starts[0]:starts[1]]};result=dict(base=struct.unpack('<f',fields[b'STBA'])[0],experience=struct.unpack('<f',fields.get(b'STPR',bytes(4)))[0]);result['other_skills']=[(n.decode(),data[a+8:b].hex()) for n,a,b in subs[starts[1]:starts[27]] if n in {b'STBA',b'STPR'}];result['counters']=[(n.decode(),data[a+8:b].hex()) for n,a,b in subs if n in {b'LPRO',b'INCR',b'SPEC'}];return result
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['baseline','hit','restart','unblocked','hold']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course;driver=read(p/'driver-process.json')
 assert driver['exit_code']==0 and driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(driver['input_save']);target=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==driver['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(target);b,a=shared(source),shared(target)
 assert after['schema_version']==101 and runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 actor=lambda s,key:next(v for v in s['native_actor_values'] if v['actor']==key)
 bp,ap=actor(before,PLAYER),actor(after,PLAYER);assert bp['values'][15]==ap['values'][15]==[30.,0.,0.,0.] and b['base']==a['base']==30
 assert b['other_skills']==a['other_skills'] and b['counters']==a['counters'];assert bp['passive_abilities']==ap['passive_abilities']
 for key in set(before['player']['actor_values'])|set(after['player']['actor_values']):
  if key.startswith(('skill_advances.','pc_misc_stat.')):assert before['player']['actor_values'].get(key,0)==after['player']['actor_values'].get(key,0)
 assert before['player']['character_generation']==after['player']['character_generation'] and before['player'].get('awarded_skill_books',[])==after['player'].get('awarded_skill_books',[])
 assert sorted((i['base'],i['count']) for i in before['player']['inventory'])==sorted((i['base'],i['count']) for i in after['player']['inventory'])
 assert all(next(v for v in after['native_actor_life'] if v['actor']==key)['phase']==0 for key in [PLAYER,TARGET] if any(v['actor']==key for v in after['native_actor_life']))
 if args.phase!='hold':
  bc,ac=actor(before,TARGET),actor(after,TARGET);assert bc['values'][8]==ac['values'][8] and bc['passive_abilities']==ac['passive_abilities'] and bc['leveled_contact_spells']==ac['leveled_contact_spells']==[]
 log=(p/'process.log').read_text();contacts=re.findall(r'M15 melee hit committed: actor='+TARGET+r' ([^\n]+)',log);assert all('victim='+PLAYER in c for c in contacts)
 blocks=[c for c in contacts if float(re.search(r'block_fraction=([\d.eE+-]+)',c)[1])>0];assert all('victim_action=6' in c for c in blocks)
 if args.phase in ['baseline','hit']:assert 1<=len(blocks)<=8 and 'M15 block begin:' in log
 if args.phase=='unblocked':assert contacts and not blocks
 if args.phase=='hold':assert not contacts and 'M15 block begin:' in log
 expected=b['experience']+(1.25*len(blocks) if args.phase not in ['baseline','hold','unblocked'] else 0);assert math.isclose(a['experience'],expected,abs_tol=1e-5),(b,a,len(blocks))
 damage=sum(float(re.search(r'health_damage=([\d.eE+-]+)',c)[1]) for c in contacts);hp=sum(ap['values'][8]);assert hp>0 and math.isclose(hp,sum(bp['values'][8])-damage,abs_tol=.02)
 for i in set(range(len(bp['values'])))-{8,10}:assert bp['values'][i]==ap['values'][i]
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];epoch=read(p/'process-epoch.json');assert len(loads)==len(saves)==1
 assert loads[0]['pid']==saves[0]['pid']==epoch['pid'] and loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],block_base=a['base'],experience_before=b['experience'],experience_after=a['experience'],blocked_contacts=blocks,all_contacts=contacts,health=hp);(p/'block-use-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
