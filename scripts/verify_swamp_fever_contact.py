"""Audit original Mudcrab Swamp Fever contact and cold-load ownership."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
import tes4_runtime_state as runtime
PLAYER='dynamic:player:0000000000000001';CARRIER='content:oblivion.esm:06c92a';DISEASE='content:oblivion.esm:098415'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def advancement(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY')
 starts=[i for i,(n,_,_) in enumerate(subs) if n==b'STBA'];assert len(starts)>=28
 return [(n.decode(),data[a+8:b].hex()) for n,a,b in subs[:starts[27]] if n in {b'STBA',b'STPR'}]+[(n.decode(),data[a+8:b].hex()) for n,a,b in subs if n in {b'LPRO',b'INCR',b'SPEC'}]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['infection','restart']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course;d=read(p/'driver-process.json')
 assert d['exit_code']==0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(d['input_save']);save=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(save)
 assert after['schema_version']==101 and runtime.encode_payload(after)==runtime._find_runtime_record(save.read_bytes())[3]
 actor=lambda s,key:next(v for v in s['native_actor_values'] if v['actor']==key)
 b,a=actor(before,PLAYER),actor(after,PLAYER);bc,ac=actor(before,CARRIER),actor(after,CARRIER)
 assert all(next(v for v in after['native_actor_life'] if v['actor']==key)['phase']==0 for key in [PLAYER,CARRIER])
 assert bc['leveled_contact_spells']==ac['leveled_contact_spells']==[DISEASE] and bc['passive_abilities']==ac['passive_abilities'] and len(ac['passive_abilities'])==1
 assert bc['values'][8]==ac['values'][8] and sum(x or 0 for x in ac['values'][8])==10
 grants=[g for g in a['passive_abilities'] if g['spell']==DISEASE];assert len(grants)==1
 assert grants[0]==dict(spell=DISEASE,spell_type=1,effects=[[0,int.from_bytes(b'DRAT','little'),0,-10.,10.],[1,int.from_bytes(b'DRAT','little'),5,-10.,10.]])
 assert [g for g in b['passive_abilities'] if g['spell']!=DISEASE]==[g for g in a['passive_abilities'] if g['spell']!=DISEASE]
 for i in [0,5]:assert a['values'][i]==[55.,-10.,0.,0.]
 assert [a['values'][i][0] for i in [8,9,10,11]]==[90.,80.,150.,225.]
 for i in set(range(len(a['values'])))-{0,5,8,10,11}:assert a['values'][i]==b['values'][i]
 assert advancement(source)==advancement(save)
 for key in set(before['player']['actor_values'])|set(after['player']['actor_values']):
  if key.startswith(('skill_advances.','pc_misc_stat.')):assert before['player']['actor_values'].get(key,0)==after['player']['actor_values'].get(key,0)
 assert before['player']['character_generation']==after['player']['character_generation'] and before['player'].get('awarded_skill_books',[])==after['player'].get('awarded_skill_books',[])
 assert sorted((x['base'],x['count']) for x in before['player']['inventory'])==sorted((x['base'],x['count']) for x in after['player']['inventory'])
 log=(p/'process.log').read_text();hits=re.findall(r'M15 melee hit committed: actor='+CARRIER+r' ([^\n]+)',log);damage=sum(float(re.search(r'health_damage=([\d.eE+-]+)',h)[1]) for h in hits);assert all('victim='+PLAYER in h for h in hits)
 if args.phase=='infection':
  assert hits and not any(g['spell']==DISEASE for g in b['passive_abilities']);assert b['values'][0]==b['values'][5]==[55.,0.,0.,0.];assert [b['values'][i][0] for i in [8,9,10,11]]==[110.,80.,170.,275.]
 else:assert b['passive_abilities']==a['passive_abilities']
 hp=sum(a['values'][8]);expected=sum(b['values'][8])-damage-(20 if args.phase=='infection' else 0);assert hp>0 and math.isclose(hp,expected,abs_tol=.02),(hp,expected)
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];epoch=read(p/'process-epoch.json');assert len(loads)==len(saves)==1
 assert loads[0]['pid']==saves[0]['pid']==epoch['pid'] and loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(save) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(save),binary_sha256=epoch['exe_sha256'],hits=hits,physical_damage=damage,health=hp,grant=grants[0],derived_bases={str(i):a['values'][i][0] for i in [8,9,10,11]});(p/'swamp-fever-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
