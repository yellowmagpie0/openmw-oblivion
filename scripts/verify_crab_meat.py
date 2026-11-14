"""Audit real Crab Meat ingestion and cold resume without curing disease."""
import hashlib,json,sys,re,math
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as progress
BASE='content:oblivion.esm:03366c';PLAYER='dynamic:player:0000000000000001'
def read(p):return json.loads(p.read_text())
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
r=Path(sys.argv[1]);phase=sys.argv[2];assert phase in ['pending','complete','cold']
d=read(r/'driver-process.json');assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(r/'scenario.json')['passed']
source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256'];assert sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
b=runtime.load_save(source);a=runtime.load_save(target);assert a['schema_version']==102
assert runtime.encode_payload(a)==runtime._find_runtime_record(target.read_bytes())[3]
retreat='--retreat' in sys.argv[3:]
log=(r/'process.log').read_text().split('Writing saved game',1)[0]
hits=[m for m in re.findall(r'M15 melee hit committed: ([^\n]+)',log) if 'victim='+PLAYER in m]
assert retreat or not hits
assert not any('actor='+PLAYER+' ' in m for m in re.findall(r'M15 melee hit committed: ([^\n]+)',log))
old_skills,new_skills=shared(source),shared(target)
for i,(old,new) in enumerate(zip(old_skills,new_skills)):
 assert old['base']==new['base']
 expected=old['experience']+(1.25*len(hits) if i==3 else 0)
 assert math.isclose(new['experience'],expected,abs_tol=1e-5),(i,old,new,hits)
assert progress(source)==progress(target)
count=lambda s:sum(i['count'] for i in s['player']['inventory'] if i['base']==BASE)
assert count(a)==0 and count(b)==(phase=='pending')
other=lambda s:[i for i in s['player']['inventory'] if i['base']!=BASE and i['count']]
wear=[]
assert len(other(a))==len(other(b))
for old,new in zip(other(b),other(a)):
 if old!=new:
  assert retreat and hits and old['equipped_slots'] and 0<=new['condition']<old['condition']
  assert dict(new,condition=old['condition'])==old
  wear.append(dict(base=old['base'],before=old['condition'],after=new['condition']))
assert len(wear)<=sum(int(re.search(r'armor_condition_writes=(\d+)',h)[1]) for h in hits)
if any(int(re.search(r'armor_condition_writes=(\d+)',h)[1]) for h in hits):assert wear
bp=next(v for v in b['native_actor_values'] if v['actor']==PLAYER);ap=next(v for v in a['native_actor_values'] if v['actor']==PLAYER)
assert ap['values'][:8]==bp['values'][:8] and ap['values'][11:]==bp['values'][11:]
assert ap.get('passive_abilities',[])==bp.get('passive_abilities',[])
assert ap['values'][5] in [[55,0,0,0],[55,-10,0,0]]
debit=sum(float(re.search(r'health_damage=([\d.eE+-]+)',h)[1]) for h in hits)
assert math.isclose(a['player']['actor_values']['health.current'],b['player']['actor_values']['health.current']-debit,abs_tol=.02)
assert a['quests']==b['quests']
# Original GLOB000038 GameHour is a short; ordinary time crossed 08:00.
clock_global='content:oblivion.esm:000038'
for state in [a,b]:assert state['globals'][clock_global]==int(state['clock']['hour'])
assert {k:v for k,v in a['globals'].items() if k!=clock_global}=={k:v for k,v in b['globals'].items() if k!=clock_global}
assert a['next_dynamic_serial']==b['next_dynamic_serial']
assert a['native_actor_life']==b['native_actor_life'] and a['native_death_counts']==b['native_death_counts']
for old in b['script_instances']:
 if old['unit'].startswith('content:oblivion.esm:0eed76@'):
  new=next(i for i in a['script_instances'] if i['unit']==old['unit'] and i['context']==old['context'])
  assert new['locals']==old['locals']
assert not any(i['base']==BASE and i.get('script_owner') for i in b['player']['inventory'])
fx=ap.get('ingredient_restoration',[])
if phase=='pending':
 assert len(fx)==1 and fx[0]['ingredient']==BASE and fx[0]['actor_value']==5 and 0<=fx[0]['elapsed']<1
 assert fx[0].get('magnitude',5)==5 and fx[0].get('duration',1)==1
else:assert not fx
logs=(r/'process.log').read_text();assert logs.count('TES4 ingredient consumed: ingredient='+BASE)==(phase=='pending')
if (r/'observed-sanitizer-environment.json').exists():
 env=read(r/'observed-sanitizer-environment.json')['environment']
 assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1'
 assert env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1'
 if 'sdl-input.so' in env.get('LD_PRELOAD',''):
  assert 'libasan' in env['LD_PRELOAD'].split(':')[0]
 libs=read(r/'loaded-libraries.json')
 assert any('libasan' in name for name in libs) and any('libubsan' in name for name in libs)
 assert not re.search(r'AddressSanitizer|runtime error:|UndefinedBehaviorSanitizer',logs)
epoch=read(r/'process-epoch.json');events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
assert all(e['pid']==epoch['pid'] for e in loads+saves)
assert read(r/loads[0]['live'])['player']['inventory']==b['player']['inventory']
result=dict(passed=True,phase=phase,source_sha256=sha(source),save_sha256=sha(target),process=epoch,endurance=ap['values'][5],pending=fx,incoming_hits=hits,armor_wear=wear,health=a['player']['actor_values']['health.current'],stock=count(a),credited_deaths=sum(v['phase']==1 and v['killer']==PLAYER for v in a['native_actor_life']))
(r/'crab-meat-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
