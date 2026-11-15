"""Independent actual-book read/save audit, including shared progression bookkeeping."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
import tes4_runtime_state as runtime
import verify_nirnroot_pickup as retained

def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY')
 starts=[i for i,(n,_,_) in enumerate(subs) if n==b'STBA'];assert len(starts)>=27
 f={n:data[a+8:b] for n,a,b in subs[starts[26]:starts[27] if len(starts)>27 else len(subs)]};all_fields={n:data[a+8:b] for n,a,b in subs}
 return dict(base=struct.unpack('<f',f[b'STBA'])[0],experience=struct.unpack('<f',f.get(b'STPR',bytes(4)))[0],level_progress=struct.unpack('<i',all_fields.get(b'LPRO',bytes(4)))[0],attributes=list(struct.unpack('<8i',all_fields.get(b'INCR',bytes(32)))),specializations=list(struct.unpack('<3i',all_fields.get(b'SPEC',bytes(12)))))

def main():
 p=argparse.ArgumentParser();p.add_argument('course',type=Path);p.add_argument('phase',choices=['award','repeat']);p.add_argument('--reads',type=int,default=2);p.add_argument('--sanitized',action='store_true');args=p.parse_args();r=args.course
 d=read(r/'driver-process.json');assert d['exit_code']==0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(r/'scenario.json')['passed']
 source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256']
 a,b=runtime.load_save(source),runtime.load_save(target);old,new=shared(source),shared(target);assert b['schema_version']==101 and runtime.encode_payload(b)==runtime._find_runtime_record(target.read_bytes())[3]
 key='content:oblivion.esm:0243fc';book='content:oblivion.esm:066cc1';award=args.phase=='award'
 assert b['player']['awarded_skill_books']==[key]
 assert (key in a['player'].get('awarded_skill_books',[])) != award
 assert old['base']==(35 if award else 36) and new['base']==36 and new['experience']==old['experience']==0
 assert new['level_progress']==old['level_progress']+int(award)
 assert new['attributes']==[v+(int(award) if i==0 else 0) for i,v in enumerate(old['attributes'])]
 assert new['specializations']==[v+(int(award) if i==0 else 0) for i,v in enumerate(old['specializations'])]
 values=lambda s:next(v for v in s['native_actor_values'] if v['owner']==0)['values']
 assert values(b)[17][0]==36 and values(b)[17][1:]==values(a)[17][1:]
 assert b['player']['inventory']==a['player']['inventory']
 assert b['player']['actor_values']['health.current']==a['player']['actor_values']['health.current']==83
 for index,delta in ((17,args.reads),(18,int(award)),(2,int(award))):
  k='pc_misc_stat.'+str(index);assert b['player']['actor_values'][k]==a['player']['actor_values'].get(k,0)+delta
 assert b['player']['actor_values']['skill_advances.17']==a['player']['actor_values'].get('skill_advances.17',0)+int(award)
 assert b['player']['character_generation']==a['player']['character_generation']
 assert a['native_death_counts']==b['native_death_counts'];assert retained.actor(a,'native_actor_life')==retained.actor(b,'native_actor_life');assert len(retained.actor(b,'native_actor_ragdolls')['bodies'])==17
 assert retained.actor(a,'native_actor_values')['passive_abilities']==retained.actor(b,'native_actor_values')['passive_abilities']
 assert next(v for v in b['references'] if v['key']==book)['custom_state']['read']
 events=[json.loads(x) for x in (r/'events.jsonl').read_text().splitlines()];loads=[x for x in events if x['event']=='load-complete'];saves=[x for x in events if x['event']=='save-complete'];assert len(loads)==len(saves)==1
 epoch=read(r/'process-epoch.json');assert all(x['pid']==epoch['pid'] and x['epoch']==1 for x in loads+saves)
 assert read(r/saves[0]['live'])==b
 assert read(r/loads[0]['live'])['player'].get('awarded_skill_books',[])==a['player'].get('awarded_skill_books',[])
 log=(r/'process.log').read_text();assert log.count('M5 interaction: kind=read result=read ref='+book)==args.reads
 assert log.count('M16 skill book read: book='+key+' skill=17 awarded=true')==int(award)
 assert log.count('M16 skill book read: book='+key+' skill=17 awarded=false')==args.reads-int(award)
 if args.sanitized:
  env=read(r/'observed-sanitizer-environment.json')['environment'];assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1';assert env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1'
  libs=read(r/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 out=dict(passed=True,phase=args.phase,reads=args.reads,input_sha256=sha(source),output_sha256=sha(target),before=old,after=new,book_base=key,history=b['player']['awarded_skill_books'],game_sha256=epoch['exe_sha256'])
 (r/'skill-book-audit.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
if __name__=='__main__':main()
