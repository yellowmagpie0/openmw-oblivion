"""Audit the original wounded Canosel Skeleton's provocation and continuing combat."""
import argparse,hashlib,json,math,re
from pathlib import Path
import tes4_runtime_state as runtime
from verify_blunt_use import shared
PLAYER='dynamic:player:0000000000000001';TARGET='content:oblivion.esm:06c2ed';WEAPON='content:oblivion.esm:0229bd'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('course',type=Path);ap.add_argument('phase',choices=['baseline','provoke','restart']);ap.add_argument('--sanitized',action='store_true');args=ap.parse_args();p=args.course;driver=read(p/'driver-process.json')
 assert driver['exit_code']==0 and driver['captured'] and driver['input_unchanged'] and driver['manifest_input_unchanged'] and read(p/'scenario.json')['passed']
 source=Path(driver['input_save']);target=p/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==driver['input_sha256'];before,after=runtime.load_save(source),runtime.load_save(target);b,a=shared(source),shared(target)
 assert after['schema_version']==99 and runtime.encode_payload(after)==runtime._find_runtime_record(target.read_bytes())[3]
 native=lambda s:next(v for v in s['native_actor_values'] if v['actor']==TARGET)
 ref=lambda s:next(v for v in s['references'] if v['key']==TARGET)
 health=lambda s:sum(v or 0 for v in native(s)['values'][8]);playerhealth=lambda s:s['player']['actor_values']['health.current']
 assert b['base']==a['base']==35 and all(b[k]==a[k] for k in ['level_progress','attributes','specializations'])
 assert before['player']['character_generation']==after['player']['character_generation'] and before['player']['actor_values'].get('skill_advances.16',0)==after['player']['actor_values'].get('skill_advances.16',0)
 counts=lambda i:sorted((x['base'],x['count']) for x in i)
 assert counts(before['player']['inventory'])==counts(after['player']['inventory']) and counts(ref(before)['inventory'])==counts(ref(after)['inventory'])
 assert native(before)['passive_abilities']==native(after)['passive_abilities'] and len(native(after)['passive_abilities'])==1 and len(native(after)['passive_abilities'][0]['effects'])==5
 assert native(after)['values'][33][0]==native(after)['values'][34][0]==100
 log=(p/'process.log').read_text();contacts=re.findall(r'M15 melee hit committed: actor=([^ ]+) id=(\d+) ([^\n]+)',log);players=[x for x in contacts if x[0]==PLAYER];incoming=[x for x in contacts if x[0]==TARGET and 'victim='+PLAYER in x[2] and float(re.search(r'health_damage=([\d.eE+-]+)',x[2])[1])>0]
 assert len({(x[0],x[1]) for x in contacts})==len(contacts)
 previous_strike=next(x for x in before['native_melee_states'] if x['actor']==TARGET)['strike']
 if args.phase=='restart' and previous_strike and previous_strike['contact_committed']:
  assert all(int(x[1])!=previous_strike['id'] for x in incoming)
 if args.phase=='restart':
  assert b==a and not players and health(before)==health(after) and 'M15 idle Creature provoked:' not in log
 else:
  assert len(players)==1 and 'victim='+TARGET in players[0][2] and 0<health(before)-health(after)<5
  assert math.isclose(a['experience']-b['experience'],.5,abs_tol=1e-6)
  assert TARGET not in before['combat_controllers']
 if args.phase=='baseline':
  assert not incoming and playerhealth(before)==playerhealth(after)==110 and TARGET not in after['combat_controllers']
 else:
  assert incoming and 0<playerhealth(after)<playerhealth(before)
  assert len(after['combat_controllers'][TARGET])==1 and after['combat_controllers'][TARGET][0]['target']==PLAYER
  assert any({x['first'],x['second']}=={PLAYER,TARGET} for x in after['native_combat_engagements'])
  equipped=next(x for x in ref(after)['inventory'] if x['base']==WEAPON);original=next(x for x in ref(before)['inventory'] if x['base']==WEAPON)
  assert equipped['equipped_slots']==65536 and 0<equipped['condition']<original['condition']
  assert ref(after)['actor_draw_state']==1
  if args.phase=='provoke':assert log.count('M15 idle Creature provoked: actor='+TARGET)==1
 events=[json.loads(l) for l in (p/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1;epoch=read(p/'process-epoch.json')
 assert loads[0]['pid']==saves[0]['pid']==epoch['pid'] and loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(p/saves[0]['live'])==after
 if args.sanitized:
  env=read(p/'observed-sanitizer-environment.json')['environment'];assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'];libs=read(p/'loaded-libraries.json');assert any('libasan' in x for x in libs) and any('libubsan' in x for x in libs)
 result=dict(passed=True,phase=args.phase,sanitized=args.sanitized,source_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],before=b,after=a,contacts=contacts,player_health_before=playerhealth(before),player_health_after=playerhealth(after),enemy_health_before=health(before),enemy_health_after=health(after),controller=after['combat_controllers'].get(TARGET),npc_weapon=next(x for x in ref(after)['inventory'] if x['base']==WEAPON),pending_strike_before=previous_strike,native_melee_state=next(x for x in after['native_melee_states'] if x['actor']==TARGET))
 (p/'retaliation-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
