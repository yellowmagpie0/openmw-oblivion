"""Audit the genuine partially spent original Torch through native quantity transfers."""
import hashlib,json,math,re,sys
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as progress
BASE='content:oblivion.esm:02cf9f';PLAYER='dynamic:player:0000000000000001';CLUB='content:oblivion.esm:159829'
def read(p):return json.loads(p.read_text())
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
r=Path(sys.argv[1]);phase=sys.argv[2];assert phase in ['drop','world','pickup','inventory']
d=read(r/'driver-process.json');assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(r/'scenario.json')['passed'];assert sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256']
b=runtime.load_save(source);a=runtime.load_save(target);assert b['schema_version']==a['schema_version']==103;assert runtime.encode_payload(a)==runtime._find_runtime_record(target.read_bytes())[3]
assert shared(source)==shared(target)
assert progress(source)==progress(target)
items=lambda state:[i for i in state['player']['inventory'] if i['base']==BASE and i['count']]
other=lambda state:[i for i in state['player']['inventory'] if i['base']!=BASE and i['count']]
assert other(a)==other(b)
assert {k:v for k,v in a['player']['actor_values'].items() if k!='encumbrance'}=={k:v for k,v in b['player']['actor_values'].items() if k!='encumbrance'}
weight_delta=0
assert math.isclose(a['player']['actor_values']['encumbrance'],b['player']['actor_values']['encumbrance']+weight_delta,abs_tol=1e-4)
for item in items(a)+items(b):
 assert item['count'] in [1,2,3] and item['condition']==-1.0
 assert item['equipped_slots']==0 and item['charge']==-1 and item['script_owner'] is None
 assert item['owner']=='null' and item['ownership_rank'] is None and item['ownership_global']=='null'
 assert 0 < item['remaining_usage_time'] <= 1000
 assert item['extra_scale'] is None and item['bound_source'] is None and item['hotkey']==-1
for state in [a,b]:
 club=next(i for i in state['player']['inventory'] if i['base']==CLUB)
 assert club==next(i for i in b['player']['inventory'] if i['base']==CLUB)
 assert math.isclose(state['player']['actor_values']['health.current'],b['player']['actor_values']['health.current'],abs_tol=1e-4)
bp=next(v for v in b['native_actor_values'] if v['actor']==PLAYER);ap=next(v for v in a['native_actor_values'] if v['actor']==PLAYER)
assert ap['values']==bp['values'] and ap.get('passive_abilities')==bp.get('passive_abilities')
assert a['native_actor_life']==b['native_actor_life'] and a['native_death_counts']==b['native_death_counts']
assert a['quests']==b['quests'];hour='content:oblivion.esm:000038'
for state in [a,b]:assert state['globals'][hour]==int(state['clock']['hour'])
assert {k:v for k,v in a['globals'].items() if k!=hour}=={k:v for k,v in b['globals'].items() if k!=hour}
for old in b['script_instances']:
 if old['unit'].startswith('content:oblivion.esm:0eed76@'):
  new=next(i for i in a['script_instances'] if i['context']==old['context'] and i['unit']==old['unit']);assert new['locals']==old['locals']
key=f"dynamic:native-reference:{(b['next_dynamic_serial'] if phase=='drop' else b['next_dynamic_serial']-1):016x}"
world=lambda state:[v for v in state['references'] if v['key']==key]
assert sum(i['count'] for i in items(b))==(3 if phase in ['drop','inventory'] else 2)
assert sum(i['count'] for i in items(a))==(2 if phase in ['drop','world'] else 3)
fuel_key='native.light.remaining_usage_time'
if phase in ['drop','inventory']:
 fuel=min(i['remaining_usage_time'] for i in items(b))
else:
 fuel=world(b)[0]['custom_state'].get(fuel_key,1000)
assert 0 < fuel <= 1000
if len(sys.argv)<4 or sys.argv[3]!='fresh':assert fuel<1000

def check_stock(state, ground):
 stock=items(state)
 if ground:
  assert len(stock)==1 and stock[0]['count']==2 and stock[0]['remaining_usage_time']==1000
 elif fuel<1000:
  assert len(stock)==2
  used=[i for i in stock if i['remaining_usage_time']==fuel]
  fresh=[i for i in stock if i['remaining_usage_time']==1000]
  assert len(used)==len(fresh)==1 and used[0]['count']==1 and fresh[0]['count']==2
 else:assert len(stock)==1 and stock[0]['count']==3 and stock[0]['remaining_usage_time']==1000
check_stock(b,phase in ['world','pickup'])
check_stock(a,phase in ['drop','world'])
refs=world(a);live=[v for v in refs if not v['deleted'] and v['custom_state'].get('count',1)>0]
# Earlier native item owners stay retired and are never reused.
for old in b['references']:
 if old['key'].startswith('dynamic:native-reference:') and old['key']!=key:
  assert next(v for v in a['references'] if v['key']==old['key'])==old
assert len(refs)==1
assert refs[0]['custom_state'].get(fuel_key,1000)==fuel
if fuel==1000:assert fuel_key not in refs[0]['custom_state']
key=refs[0]['key'];assert key.startswith('dynamic:native-reference:')
assert refs[0]['custom_state']['record_type']==(int.from_bytes(b'LIGH','little') | 0x00800000)
assert refs[0]['item_condition'] is None
assert refs[0]['item_charge'] is None and refs[0]['dropper']==PLAYER
player_refs=[v for v in a['references'] if v['key']==PLAYER]
rigs=[v for v in a['native_loose_weapons'] if v['reference']==key]
if phase in ['drop','world']:
 assert len(live)==1 and len(rigs)==1
 assert live[0]['custom_state']['count']==1
 assert live[0]['enabled'] and not live[0]['inventory'] and live[0]['owner'] is None
 assert live[0]['ownership_rank'] is None and live[0]['ownership_global']=='null'
 assert len(player_refs)==1 and key in player_refs[0]['dropped_items']
 rig=rigs[0];assert rig['base']==BASE and rig['model']=='meshes/lights/torch02.nif'
 assert len(rig['asset_hash'])==32 and rig['body']['node_record']==0
 assert rig['body']['record']==4 and not rig.get('additional_bodies',[])
 bodies=[rig['body']]
 for body in bodies:
  assert all(math.isfinite(v) for name in ['position','linear_velocity','angular_velocity'] for v in body[name])
  assert body['native_motion']==1
  assert math.dist(body['position'],a['player']['position'][:3])<200
 assert math.isfinite(rig['physical_scale']) and rig['physical_scale']>0
 assert math.dist(live[0]['position'][:3],a['player']['position'][:3])<150, 'Dropped Torch must remain near Player rather than fall through terrain'
 if phase=='drop':
  assert key==f"dynamic:native-reference:{b['next_dynamic_serial']:016x}"
 else:
  old=next(v for v in b['native_loose_weapons'] if v['reference']==key)
  for name in ['base','model','asset_hash','physical_scale','scene_collision_flags']:assert rig[name]==old[name]
  for name in ['record','node_record']:assert rig['body'][name]==old['body'][name]

else:
 assert not live and not rigs and refs[0]['custom_state'].get('count',0)==0
 assert not player_refs or key not in (player_refs[0]['dropped_items'] or [])
 if phase=='pickup':assert items(b)[0]['count']==2
 else:assert items(a)==items(b)
assert a['next_dynamic_serial']==b['next_dynamic_serial']+(phase=='drop')
epoch=read(r/'process-epoch.json');events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1 and all(e['pid']==epoch['pid'] for e in loads+saves)
loaded=read(r/loads[0]['live']);assert sha(r/loads[0]['live'])==loads[0]['live_sha256']
assert loaded['player']['inventory']==b['player']['inventory']
def physical_close(actual,expected):
 if isinstance(expected,dict):
  assert actual.keys()==expected.keys()
  for k in expected:physical_close(actual[k],expected[k])
 elif isinstance(expected,list):
  assert len(actual)==len(expected)
  for v,w in zip(actual,expected):physical_close(v,w)
 elif isinstance(expected,float):assert math.isclose(actual,expected,rel_tol=1e-6,abs_tol=1e-4)
 else:assert actual==expected
# Tick-zero load observation verifies full physical continuation before settling.
physical_close(loaded['native_loose_weapons'],b.get('native_loose_weapons',[]))
log=(r/'process.log').read_text();assert 'unidentified standing reference' not in log
if phase=='drop':assert 'TES4 inventory light dropped: base='+BASE in log
if (r/'observed-sanitizer-environment.json').exists():
 env=read(r/'observed-sanitizer-environment.json')['environment'];assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1' and env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1'
 if 'sdl-input.so' in env.get('LD_PRELOAD',''):assert 'libasan' in env['LD_PRELOAD'].split(':')[0]
 libs=read(r/'loaded-libraries.json');assert any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
 assert not re.search(r'AddressSanitizer|runtime error:|UndefinedBehaviorSanitizer',log)
cg=lambda state:[v for v in state['script_instances'] if v['unit'].startswith('content:oblivion.esm:0661bf@')]
assert cg(a)==cg(b) and len(cg(a))==1 and cg(a)[0]['context']=='dynamic:inventory-script:0000000000000009'
for state in [a,b]:
 assert not any(v['key']=='dynamic:native-reference:0000000000000009' for v in state['references'])
 mq=next(v for v in state['script_instances'] if v['unit']=='content:oblivion.esm:0297ad@oblivion.esm/quest/unit=0');assert [v['value'] for v in mq['locals'][11:13]]==[1,1]
assert 'M7 inventory add event: item=content:oblivion.esm:0661c1' not in log and 'M7 inventory equip event: item=content:oblivion.esm:0661c1' not in log
result=dict(remaining_usage_time=fuel,passed=True,phase=phase,process=epoch,source_sha256=sha(source),save_sha256=sha(target),inventory=items(a),world=refs,physical=rigs,next_dynamic_serial=a['next_dynamic_serial'],credited_deaths=sum(v['phase']==1 and v['killer']==PLAYER for v in a['native_actor_life']))
(r/'spent-torch-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
