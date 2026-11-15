"""Audit real original arrow-hit experience and its saved ownership."""
import argparse,hashlib,json,math,re,struct
from pathlib import Path
import tes4_runtime_state as runtime
PLAYER='dynamic:player:0000000000000001'
TARGET='content:oblivion.esm:06bfc2'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def shared(p):
 data=p.read_bytes();subs=next(s for _,_,tag,s in runtime._save_records(data) if tag==b'PLAY')
 starts=[i for i,(name,_,_) in enumerate(subs) if name==b'STBA'];assert len(starts)>=27
 fields={name:data[a+8:b] for name,a,b in subs[starts[23]:starts[24]]}
 result=dict(base=struct.unpack('<f',fields[b'STBA'])[0],experience=struct.unpack('<f',fields.get(b'STPR',bytes(4)))[0])
 fields={name:data[a+8:b] for name,a,b in subs}
 result.update(level_progress=struct.unpack('<i',fields.get(b'LPRO',bytes(4)))[0],
   attributes=list(struct.unpack('<8i',fields.get(b'INCR',bytes(32)))),
   specializations=list(struct.unpack('<3i',fields.get(b'SPEC',bytes(12)))))
 return result

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path)
 ap.add_argument('phase',choices=['baseline','hit','restart','miss','npc']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course
 driver=read(p/'driver-process.json');assert driver['exit_code']==0 and driver['captured']
 assert driver['input_unchanged'] and driver['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(driver['input_save']);target=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==driver['input_sha256']
 before,after=runtime.load_save(source),runtime.load_save(target);b,a=shared(source),shared(target)
 assert after['schema_version']==99 and runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 player=lambda state:next(v for v in state['native_actor_values'] if v['actor']==PLAYER)
 assert player(before)['values'][28]==player(after)['values'][28]
 assert player(after)['values'][28][0]==b['base']==a['base']==5
 assert all(b[k]==a[k] for k in ['level_progress','attributes','specializations'])
 assert before['player']['actor_values'].get('skill_advances.28',0)==after['player']['actor_values'].get('skill_advances.28',0)
 assert after['player']['actor_values']['health.current']>0
 ammo='content:oblivion.esm:017829';bow='content:oblivion.esm:047aca'
 count=lambda state,base:sum(i['count'] for i in state['player']['inventory'] if i['base']==base)
 arrows_before,arrows_after=count(before,ammo),count(after,ammo)
 assert arrows_after==arrows_before-(1 if args.phase in ('baseline','hit') else 0)
 other_counts=lambda state:sorted((i['base'],i['count']) for i in state['player']['inventory'] if i['base']!=ammo)
 assert other_counts(before)==other_counts(after)
 bows=[next(i for i in state['player']['inventory'] if i['base']==bow) for state in (before,after)]
 if args.phase in ('baseline','hit'):assert 0<bows[1]['condition']<=bows[0]['condition']
 else:assert bows[0]['condition']==bows[1]['condition']
 log=(p/'process.log').read_text();hits=re.findall(r'M15 Player arrow impact: id=(\d+) target=([^ ]+) damage=([\d.eE+-]+)',log)
 positive=[row for row in hits if float(row[2])>0]
 if args.phase in ('baseline','hit'):
  assert len(positive)==1 and positive[0][1]==TARGET
  assert b['experience']==0
  assert math.isclose(a['experience'],.8 if args.phase=='hit' else 0,abs_tol=1e-6)
  assert log.count('M15 Player arrow released:')==1
  if args.phase=='hit':
   normalized=.8/((5*.35)**1.5*1.25)
   observations=[float(x) for x in re.findall(r'Marksman sample base=5 progress=([\d.eE+-]+)',log)]
   assert any(math.isclose(x,normalized,rel_tol=1e-5) for x in observations)
 else:
  assert b==a and not positive
  if args.phase=='miss':
   assert hits and all(float(row[2])==0 for row in hits)
   actors={v['actor'] for v in after['native_actor_values']}
   assert all(row[1] not in actors for row in hits)
  elif args.phase=='npc':
   incoming=re.findall(r'M15 actor arrow impact: id=\d+ target='+PLAYER+r' damage=([\d.eE+-]+)',log)
   assert incoming and any(float(x)>0 for x in incoming)
   assert after['player']['actor_values']['health.current']<before['player']['actor_values']['health.current']
  else:assert not hits and log.count('M15 Player arrow released:')==0
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()]
 loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
 epoch=read(p/'process-epoch.json');assert loads[0]['pid']==saves[0]['pid']==epoch['pid']
 assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target)
 assert read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS']
  libraries=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libraries) and any('libubsan' in x for x in libraries)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),
   binary_sha256=epoch['exe_sha256'],before=b,after=a,player_impacts=hits,
   player_health_before=before['player']['actor_values']['health.current'],player_health_after=after['player']['actor_values']['health.current'],arrows_before=arrows_before,arrows_after=arrows_after,bow_condition_before=bows[0]['condition'],bow_condition_after=bows[1]['condition'])
 (p/'marksman-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
