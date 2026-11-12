"""Audit actual owned Heal Minor Wounds ordinary-use and cold-load courses."""
import argparse,hashlib,json,math,re,struct
from pathlib import Path
import tes4_runtime_state as runtime
PLAYER='dynamic:player:0000000000000001';SPELL='content:oblivion.esm:000136'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY')
 starts=[i for i,(n,_,_) in enumerate(subs) if n==b'STBA'];assert len(starts)>=27
 fields={n:data[a+8:b] for n,a,b in subs[starts[15]:starts[16]]}
 result=dict(base=struct.unpack('<f',fields[b'STBA'])[0],experience=struct.unpack('<f',fields.get(b'STPR',bytes(4)))[0])
 fields={n:data[a+8:b] for n,a,b in subs}
 result.update(level_progress=struct.unpack('<i',fields.get(b'LPRO',bytes(4)))[0],attributes=list(struct.unpack('<8i',fields.get(b'INCR',bytes(32)))),specializations=list(struct.unpack('<3i',fields.get(b'SPEC',bytes(12)))))
 return result

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['baseline','use','restart','select']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course
 driver=read(p/'driver-process.json');assert driver['exit_code']==0 and driver['captured']
 assert driver['input_unchanged'] and driver['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(driver['input_save']);target=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==driver['input_sha256']
 before,after=runtime.load_save(source),runtime.load_save(target);b,a=shared(source),shared(target)
 assert after['schema_version']==99 and runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 native=lambda s:next(v for v in s['native_actor_values'] if v['actor']==PLAYER)['values'][25]
 assert native(before)==native(after) and native(after)[0]==b['base']==a['base']==5
 assert all(b[k]==a[k] for k in ['level_progress','attributes','specializations'])
 assert before['player']['actor_values'].get('skill_advances.25',0)==after['player']['actor_values'].get('skill_advances.25',0)
 assert before['player']['character_generation']==after['player']['character_generation']
 # Incoming arrows may legitimately wear armor; casting consumes no item.
 counts=lambda state:sorted((i['base'],i['count']) for i in state['player']['inventory'])
 assert counts(before)==counts(after)
 assert after['player']['actor_values']['health.current']>0
 log=(p/'process.log').read_text();releases=re.findall(r'M16 native cast release: spell=([^ ]+) applied=([01]) health=([\d.eE+-]+) magicka=([\d.eE+-]+)',log)
 if args.phase in ['use','baseline']:
  assert len(releases)==1 and releases[0][0]==SPELL and releases[0][1]=='1'
  assert log.count('M16 native cast begin: spell='+SPELL)==1 and b['experience']==0
  assert math.isclose(a['experience'],.6 if args.phase=='use' else 0,abs_tol=1e-6)
  prefix=log[:log.index('M16 native cast release:')]
  previous=float(re.findall(r'VilverinContinue healing sample health=([\d.eE+-]+)',prefix)[-1]);healed=float(releases[0][2])-previous
  assert 0<healed<=8.01
  if args.phase=='use':
   observed=[float(v) for v in re.findall(r'restoration=5 progress=([\d.eE+-]+)',log)]
   assert any(math.isclose(v,.6/((5*.35)**1.5*1.25),rel_tol=1e-5) for v in observed)
 else:
  assert b==a and not releases and 'M16 native cast begin:' not in log
  assert after['player']['actor_values']['magicka.current']>=before['player']['actor_values']['magicka.current']
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
 epoch=read(p/'process-epoch.json');assert loads[0]['pid']==saves[0]['pid']==epoch['pid']
 assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
  libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],before=b,after=a,releases=releases,health_before=before['player']['actor_values']['health.current'],health_after=after['player']['actor_values']['health.current'],pending_casts_before=before['native_spell_casts'],pending_casts=after['native_spell_casts'],magicka_before=before['player']['actor_values']['magicka.current'],magicka_after=after['player']['actor_values']['magicka.current'])
 (p/'restoration-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
