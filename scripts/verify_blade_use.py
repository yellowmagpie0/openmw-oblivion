"""Audit original Iron Claymore damaging Blade contacts and cold-load ownership."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
import tes4_runtime_state as runtime
PLAYER='dynamic:player:0000000000000001';TARGET='content:oblivion.esm:04bb73';WEAPON='content:oblivion.esm:01c6cd'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY');starts=[i for i,(n,_,_) in enumerate(subs) if n==b'STBA'];assert len(starts)>=28;result=[]
 for i in range(27):
  f={n:data[a+8:b] for n,a,b in subs[starts[i]:starts[i+1]]};result.append(dict(base=struct.unpack('<f',f[b'STBA'])[0],experience=struct.unpack('<f',f.get(b'STPR',bytes(4)))[0]))
 return result
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['baseline','hit','restart','miss']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course;driver=read(p/'driver-process.json')
 assert driver['exit_code']==0 and driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(driver['input_save']);save=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==driver['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(save);b,a=shared(source),shared(save)
 assert after['schema_version']==101 and runtime.encode_payload(after)==runtime._find_runtime_record(save.read_bytes())[3]
 actor=lambda s,key:next(v for v in s['native_actor_values'] if v['actor']==key)
 bp,pa=actor(before,PLAYER),actor(after,PLAYER);bt,ta=actor(before,TARGET),actor(after,TARGET)
 assert bp['values'][14]==pa['values'][14]==[35.,0.,0.,0.] and b[5]['base']==a[5]['base']==35
 assert b[4]==a[4] and b[26]==a[26], 'Blade must not train Blunt or Hand-to-Hand'
 assert bp['passive_abilities']==pa['passive_abilities'] and bt['passive_abilities']==ta['passive_abilities']
 assert next(v for v in after['native_actor_life'] if v['actor']==PLAYER)['phase']==0
 weapon=lambda s:next(i for i in s['player']['inventory'] if i['base']==WEAPON)
 bw,aw=weapon(before),weapon(after);assert bw['count']==aw['count']==1 and bw['equipped_slots']==aw['equipped_slots'] and aw['equipped_slots']&65536
 assert sorted((i['base'],i['count']) for i in before['player']['inventory'])==sorted((i['base'],i['count']) for i in after['player']['inventory'])
 taken=next(r for r in after['references'] if r['key']=='content:oblivion.esm:0c56f9');assert taken['deleted'] and taken['custom_state']['taken']
 log=(p/'process.log').read_text();assert log.count('Writing saved game')==1;pre=log.split('Writing saved game',1)[0];hits=re.findall(r'M15 melee hit committed: actor='+PLAYER+r' ([^\n]+)',pre);misses=re.findall(r'M15 melee miss committed: actor='+PLAYER+r' ([^\n]+)',pre)
 assert all('victim='+TARGET in h for h in hits);positive=[h for h in hits if float(re.search(r'health_damage=([\d.eE+-]+)',h)[1])>0]
 ids=[int(re.search(r'id=(\d+)',h)[1]) for h in hits+misses];assert len(ids)==len(set(ids)) and all(i>=before['physical_actions']['next'] or i in before['physical_actions']['pending'] for i in ids)
 if args.phase in ['baseline','hit']:assert 1<=len(positive)<=3 and b[5]['experience']==0 and 0<=aw['condition']<bw['condition']
 if args.phase=='miss':assert misses and not hits and aw['condition']==bw['condition']
 expected=b[5]['experience']+(.5*len(positive) if args.phase!='baseline' else 0);assert math.isclose(a[5]['experience'],expected,abs_tol=1e-5),(b[5],a[5],positive)
 damage=sum(float(re.search(r'health_damage=([\d.eE+-]+)',h)[1]) for h in hits);health=lambda v:sum(x or 0 for x in v['values'][8]);assert math.isclose(health(ta),health(bt)-damage,abs_tol=.02)
 incoming=re.findall(r'M15 melee hit committed: actor='+TARGET+r' ([^\n]+)',pre);debit=sum(float(re.search(r'health_damage=([\d.eE+-]+)',h)[1]) for h in incoming if 'victim='+PLAYER in h);assert health(pa)>0 and math.isclose(health(pa),health(bp)-debit,abs_tol=.02)
 # Real incoming dagger hits legitimately train the already implemented armor
 # skill. Preserve/report those gains rather than demanding synthetic stasis.
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];epoch=read(p/'process-epoch.json');assert len(loads)==len(saves)==1
 assert loads[0]['pid']==saves[0]['pid']==epoch['pid'] and loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(save) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(save),binary_sha256=epoch['exe_sha256'],blade_before=b[5],blade_after=a[5],heavy_before=b[3],heavy_after=a[3],hits=hits,misses=misses,weapon_condition_before=bw['condition'],weapon_condition_after=aw['condition'],player_health=health(pa),target_health_before=health(bt),target_health_after=health(ta));(p/'blade-use-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
