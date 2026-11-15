"""Audit original Rusty Iron body armor wear and Heavy Armor experience."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
import tes4_runtime_state as runtime
PLAYER='dynamic:player:0000000000000001';TARGET='content:oblivion.esm:06c927';ARMOR={'content:oblivion.esm:0661c1','content:oblivion.esm:0661c3'}
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY');starts=[i for i,(n,_,_) in enumerate(subs) if n==b'STBA'];assert len(starts)>=28;result=[]
 for i in range(27):
  f={n:data[a+8:b] for n,a,b in subs[starts[i]:starts[i+1]]};result.append(dict(base=struct.unpack('<f',f[b'STBA'])[0],experience=struct.unpack('<f',f.get(b'STPR',bytes(4)))[0]))
 return result,[(n.decode(),data[a+8:b].hex()) for n,a,b in subs if n in {b'LPRO',b'INCR',b'SPEC'}]
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('course',type=Path);parser.add_argument('phase',choices=['baseline','hit','restart','blocked']);parser.add_argument('--sanitized',action='store_true');args=parser.parse_args();p=args.course;d=read(p/'driver-process.json')
 assert d['exit_code']==0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(d['input_save']);save=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(save);b,bc=shared(source);a,ac=shared(save)
 assert after['schema_version']==101 and runtime.encode_payload(after)==runtime._find_runtime_record(save.read_bytes())[3]
 actor=lambda s,key:next(v for v in s['native_actor_values'] if v['actor']==key)
 bp,ap=actor(before,PLAYER),actor(after,PLAYER);bt,at=actor(before,TARGET),actor(after,TARGET)
 assert bp['values'][18]==ap['values'][18]==[40.,0.,0.,0.] and b[3]['base']==a[3]['base']==40
 assert bc==ac and bp['passive_abilities']==ap['passive_abilities'];assert bt['values'][8]==at['values'][8] and bt['passive_abilities']==at['passive_abilities'] and bt['leveled_contact_spells']==at['leveled_contact_spells']==[]
 assert all(next(v for v in after['native_actor_life'] if v['actor']==key)['phase']==0 for key in [PLAYER,TARGET])
 for key in set(before['player']['actor_values'])|set(after['player']['actor_values']):
  if key.startswith(('skill_advances.','pc_misc_stat.')):assert before['player']['actor_values'].get(key,0)==after['player']['actor_values'].get(key,0)
 assert before['player']['character_generation']==after['player']['character_generation'] and before['player'].get('awarded_skill_books',[])==after['player'].get('awarded_skill_books',[])
 assert sorted((i['base'],i['count']) for i in before['player']['inventory'])==sorted((i['base'],i['count']) for i in after['player']['inventory'])
 worn=lambda s:{i['base']:i['condition'] for i in s['player']['inventory'] if i['base'] in ARMOR and i['equipped_slots']};old,new=worn(before),worn(after);assert old.keys()==new.keys()==ARMOR
 log=(p/'process.log').read_text();assert log.count('Writing saved game')==1;presave=log.split('Writing saved game',1)[0];hits=re.findall(r'M15 melee hit committed: actor='+TARGET+r' ([^\n]+)',presave);assert all('victim='+PLAYER in h for h in hits)
 ids=[int(re.search(r'id=(\d+)',h)[1]) for h in hits];assert len(ids)==len(set(ids))
 assert all(i>=before['physical_actions']['next'] or i in before['physical_actions']['pending'] for i in ids)
 fraction=lambda h:float(re.search(r'block_fraction=([\d.eE+-]+)',h)[1]);blocks=[h for h in hits if fraction(h)>0];body=[h for h in hits if fraction(h)==0 and int(re.search(r'armor_condition_writes=(\d+)',h)[1])>0]
 if args.phase in ['baseline','hit']:assert 1<=len(body)<=8 and not blocks
 if args.phase=='blocked':assert 1<=len(blocks)<=8 and not body and all('victim_action=6' in h for h in blocks)
 if args.phase!='restart':assert any(new[k]<old[k] for k in ARMOR)
 for i in range(27):
  expected=b[i]['experience']
  if i==3 and args.phase!='baseline':expected+=1.25*len(body)
  if i==0:expected+=1.25*len(blocks)
  assert a[i]['base']==b[i]['base'] and math.isclose(a[i]['experience'],expected,abs_tol=1e-5),(i,b[i],a[i],expected)
 hp=sum(ap['values'][8]);damage=sum(float(re.search(r'health_damage=([\d.eE+-]+)',h)[1]) for h in hits);assert hp>0 and math.isclose(hp,sum(bp['values'][8])-damage,abs_tol=.02)
 for i in set(range(len(bp['values'])))-{8,10}:assert bp['values'][i]==ap['values'][i]
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];epoch=read(p/'process-epoch.json');assert len(loads)==len(saves)==1
 assert loads[0]['pid']==saves[0]['pid']==epoch['pid'] and loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(save) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(save),binary_sha256=epoch['exe_sha256'],heavy_before=b[3],heavy_after=a[3],block_before=b[0],block_after=a[0],armor_before=old,armor_after=new,body_contacts=body,blocked_contacts=blocks,all_contacts=hits,health=hp);(p/'armor-use-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
