"""Audit actual ordinary Security level crossing on original Canosel Gate049bba."""
import argparse,json,math,pathlib,re,sys
sys.path.insert(0,'scripts')
import tes4_runtime_state as runtime
from verify_gate_security_use import shared,read,sha
GATE='content:oblivion.esm:049bba';PICK='content:oblivion.esm:00000a'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('course',type=pathlib.Path);p.add_argument('phase',choices=['gain','restart']);p.add_argument('--sanitized',action='store_true');args=p.parse_args();c=args.course
 d=read(c/'driver-process.json');assert d['exit_code']==0 and d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(c/'scenario.json')['passed']
 source=pathlib.Path(d['input_save']);target=c/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];b,a=runtime.load_save(source),runtime.load_save(target);bs,as_=shared(source),shared(target)
 assert a['schema_version']==99 and runtime.encode_payload(a)==runtime._find_runtime_record(target.read_bytes())[3]
 av=lambda s:next(v for v in s['native_actor_values'] if v['actor']=='dynamic:player:0000000000000001')['values'][30]
 gate=lambda s:next(r for r in s['references'] if r['key']==GATE)
 picks=lambda s:sum(i['count'] for i in s['player']['inventory'] if i['base']==PICK)
 assert av(b)[0]==bs['security_base'] and av(a)[0]==as_['security_base'] and av(b)[1:]==av(a)[1:]
 assert b['player']['cell']==a['player']['cell']=='content:oblivion.esm:0463f5';assert b['player']['actor_values']['health.current']==a['player']['actor_values']['health.current']>0
 unrelated=lambda s:[i for i in s['player']['inventory'] if i['base']!=PICK];assert unrelated(b)==unrelated(a)
 log=(c/'process.log').read_text();failed=log.count('kind=door result=lockpick_failed ref='+GATE);bg,ag=gate(b),gate(a);attempts=ag['custom_state'].get('lockpick_auto_attempts',0)-bg['custom_state'].get('lockpick_auto_attempts',0)
 advances=lambda s:s['player']['actor_values'].get('skill_advances.30',0)
 if args.phase=='gain':
  assert attempts==1 and failed in (0,1);assert picks(a)==picks(b)-failed
  assert bs['security_base']==5 and as_['security_base']==6 and math.isclose(bs['security_experience'],1.5)
  expected=3-((5*.35)**1.5*1.25);assert math.isclose(as_['security_experience'],expected,abs_tol=1e-5)
  assert as_['level_progress']==bs['level_progress']
  expected_attributes=bs['attribute_increases'][:];expected_attributes[3]+=1;assert as_['attribute_increases']==expected_attributes
  expected_specs=bs['specialization_increases'][:];expected_specs[2]+=1;assert as_['specialization_increases']==expected_specs
  assert advances(a)==advances(b)+1
  assert re.search(r'Security sample base=6 progress=0\.',log)
 else:
  assert attempts==failed==0 and picks(a)==picks(b) and bs==as_ and av(b)==av(a) and advances(b)==advances(a)==1
  assert ag==bg and 'kind=door result=lockpick_' not in log
 events=[json.loads(l) for l in (c/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
 epoch=read(c/'process-epoch.json');assert loads[0]['pid']==saves[0]['pid']==epoch['pid'];assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target);assert read(c/saves[0]['live'])==a
 if args.sanitized:
  env=read(c/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(c/'loaded-libraries.json');assert any('libasan' in v for v in libs) and any('libubsan' in v for v in libs)
 out={'passed':True,'phase':args.phase,'sanitized':args.sanitized,'gate':GATE,'binary_sha256':epoch['exe_sha256'],'input_sha256':sha(source),'save_sha256':sha(target),'before_shared':bs,'after_shared':as_,'attempts':attempts,'failed_attempts':failed,'picks_before':picks(b),'picks_after':picks(a),'skill_advances_before':advances(b),'skill_advances_after':advances(a)};(c/'advancement-audit.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
if __name__=='__main__':main()
