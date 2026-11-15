"""Inspect real Gate049bbc Security use and cold-load courses."""
import argparse,hashlib,json,math,pathlib,re,struct,sys
sys.path.insert(0,'scripts')
import tes4_runtime_state as runtime
GATE='content:oblivion.esm:049bbc';PICK='content:oblivion.esm:00000a'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();players=[subs for _,_,tag,subs in runtime._save_records(data) if tag==b'PLAY'];assert len(players)==1
 subs=players[0];starts=[i for i,(name,_,_) in enumerate(subs) if name==b'STBA'];assert len(starts)>=27
 # Player NpcState writes 27 ordered skill StatStates before CreatureStats.
 i=starts[18];end=starts[19];fields={name:data[a+8:b] for name,a,b in subs[i:end]}
 result={'security_base':struct.unpack('<f',fields[b'STBA'])[0],'security_experience':struct.unpack('<f',fields.get(b'STPR',bytes(4)))[0]}
 fields={name:data[a+8:b] for name,a,b in subs}
 result.update(level_progress=struct.unpack('<i',fields.get(b'LPRO',bytes(4)))[0],attribute_increases=list(struct.unpack('<8i',fields.get(b'INCR',bytes(32)))),specialization_increases=list(struct.unpack('<3i',fields.get(b'SPEC',bytes(12)))))
 return result

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('course',type=pathlib.Path);parser.add_argument('phase',choices=['cancel','one-attempt','restart']);parser.add_argument('--sanitized',action='store_true');args=parser.parse_args();course=args.course
 driver=read(course/'driver-process.json');assert driver['exit_code']==0 and driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged'];assert read(course/'scenario.json')['passed']
 source=pathlib.Path(driver['input_save']);target=course/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==driver['input_sha256']
 before,after=runtime.load_save(source),runtime.load_save(target);bs,as_=shared(source),shared(target);assert after['schema_version']==99;assert runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 gate=lambda s:next(r for r in s['references'] if r['key']==GATE)
 picks=lambda s:sum(i['count'] for i in s['player']['inventory'] if i['base']==PICK)
 av=lambda s:next(v for v in s['native_actor_values'] if v['actor']=='dynamic:player:0000000000000001')['values'][30]
 b,a=gate(before),gate(after);assert av(before)[0]==bs['security_base'] and av(after)[0]==as_['security_base'];assert av(before)[1:]==av(after)[1:]
 assert before['player']['actor_values']['health.current']==after['player']['actor_values']['health.current']>0
 assert before['player']['cell']==after['player']['cell']=='content:oblivion.esm:0463f5'
 unrelated=lambda s:[i for i in s['player']['inventory'] if i['base']!=PICK];assert unrelated(before)==unrelated(after)
 log=(course/'process.log').read_text();failed=log.count('kind=door result=lockpick_failed ref='+GATE);attempts=a['custom_state'].get('lockpick_auto_attempts',0)-b['custom_state'].get('lockpick_auto_attempts',0)
 if args.phase=='one-attempt':
  assert attempts==1 and failed in (0,1);assert picks(after)==picks(before)-failed
  assert bs['security_base']==as_['security_base']==5 and bs['security_experience']==0
  assert math.isclose(as_['security_experience'],1.5,rel_tol=1e-6)
  # The one-use fixture must also test Lua's native (rather than TES3) normalization.
  normalized=1.5/((5*.35)**1.5*1.25)
  observations=[(float(b),float(p)) for b,p in re.findall(r'Security sample base=([\d.]+) progress=([\d.eE+-]+)',log)]
  assert any(b==5 and math.isclose(p,normalized,rel_tol=1e-5) for b,p in observations)
  assert all(as_[k]==bs[k] for k in ['level_progress','attribute_increases','specialization_increases'])
 else:
  assert attempts==failed==0 and picks(before)==picks(after) and bs==as_ and av(before)==av(after)
  if args.phase=='cancel':assert log.count('kind=door result=lockpick_cancelled ref='+GATE)==1
  else:assert 'kind=door result=lockpick_' not in log;assert a==b
 events=[json.loads(l) for l in (course/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
 epoch=read(course/'process-epoch.json');assert loads[0]['pid']==saves[0]['pid']==epoch['pid'];assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target);assert read(course/saves[0]['live'])==after
 if args.sanitized:
  env=read(course/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(course/'loaded-libraries.json');assert any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
 result={'passed':True,'phase':args.phase,'sanitized':args.sanitized,'input_sha256':sha(source),'save_sha256':sha(target),'binary_sha256':epoch.get('sha256',epoch.get('exe_sha256')),'before_shared':bs,'after_shared':as_,'attempts':attempts,'failed_attempts':failed,'picks_before':picks(before),'picks_after':picks(after),'gate_locked':a['custom_state'].get('locked'),'schema_version':99};(course/'security-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
