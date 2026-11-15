"""Audit a real worn Rusty Iron War Axe through native drop/world/pickup saves."""
import hashlib,json,math,re,sys
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as runtime
from verify_blade_use import shared
from verify_skill_book_read import shared as progress
BASE='content:oblivion.esm:090613';PLAYER='dynamic:player:0000000000000001';CLUB='content:oblivion.esm:159829'
def read(p):return json.loads(p.read_text())
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
r=Path(sys.argv[1]);phase=sys.argv[2];assert phase in ['drop','world','pickup','inventory']
d=read(r/'driver-process.json');assert d['captured'] and d['exit_code']==0 and d['input_unchanged'] and d['manifest_input_unchanged'];assert read(r/'scenario.json')['passed'];assert sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave';assert sha(source)==d['input_sha256']
b=runtime.load_save(source);a=runtime.load_save(target);assert b['schema_version']==a['schema_version']==102;assert runtime.encode_payload(a)==runtime._find_runtime_record(target.read_bytes())[3]
assert shared(source)==shared(target) and progress(source)==progress(target)
items=lambda state:[i for i in state['player']['inventory'] if i['base']==BASE and i['count']]
other=lambda state:[i for i in state['player']['inventory'] if i['base']!=BASE and i['count']]
assert other(a)==other(b)
for item in items(a)+items(b):
 assert item['count']==1 and math.isclose(item['condition'],22.49466323852539,abs_tol=1e-5)
 assert item['equipped_slots']==0 and item['charge']==-1 and item['script_owner'] is None
 assert item['owner']=='null' and item['ownership_rank'] is None and item['ownership_global']=='null'
 assert item['extra_scale'] is None and item['bound_source'] is None and item['hotkey']==-1
for state in [a,b]:
 club=next(i for i in state['player']['inventory'] if i['base']==CLUB)
 assert club['count']==1 and club['equipped_slots']==65536 and math.isclose(club['condition'],59.50004577636719,abs_tol=1e-5)
 assert state['player']['actor_values']['health.current']==110
 assert state['player']['actor_values']['pc_misc_stat.27']==2
 assert next(q for q in state['quests'] if q['quest']=='content:oblivion.esm:04e936')['stage']==10
bp=next(v for v in b['native_actor_values'] if v['actor']==PLAYER);ap=next(v for v in a['native_actor_values'] if v['actor']==PLAYER)
assert ap['values']==bp['values'] and ap.get('passive_abilities')==bp.get('passive_abilities')
assert a['native_actor_life']==b['native_actor_life'] and a['native_death_counts']==b['native_death_counts']
assert a['quests']==b['quests'];hour='content:oblivion.esm:000038'
for state in [a,b]:assert state['globals'][hour]==int(state['clock']['hour'])
assert {k:v for k,v in a['globals'].items() if k!=hour}=={k:v for k,v in b['globals'].items() if k!=hour}
for old in b['script_instances']:
 if old['unit'].startswith('content:oblivion.esm:0eed76@'):
  new=next(i for i in a['script_instances'] if i['context']==old['context'] and i['unit']==old['unit']);assert new['locals']==old['locals']
world=lambda state:[v for v in state['references'] if v['base']==BASE and v['key'].startswith('dynamic:')]
refs=world(a);live=[v for v in refs if not v['deleted'] and v['custom_state'].get('count',1)>0]
assert len(refs)==1
key=refs[0]['key'];assert key.startswith('dynamic:native-reference:')
assert math.isclose(refs[0]['item_condition'],22.49466323852539,abs_tol=1e-5)
assert refs[0]['item_charge'] is None and refs[0]['dropper']==PLAYER
player_refs=[v for v in a['references'] if v['key']==PLAYER]
rigs=[v for v in a['native_loose_weapons'] if v['reference']==key]
if phase in ['drop','world']:
 assert not items(a) and len(live)==1 and len(rigs)==1
 assert live[0]['enabled'] and not live[0]['inventory'] and live[0]['owner'] is None
 assert live[0]['ownership_rank'] is None and live[0]['ownership_global']=='null'
 assert len(player_refs)==1 and key in player_refs[0]['dropped_items']
 rig=rigs[0];assert rig['base']==BASE and rig['model']=='meshes/weapons/iron/waraxe.nif'
 assert len(rig['asset_hash'])==32 and rig['body']['node_record']==0
 assert math.isfinite(rig['physical_scale']) and rig['physical_scale']>0
 assert math.dist(live[0]['position'][:3],a['player']['position'][:3])<150, 'Dropped axe must remain near Player rather than fall through terrain'
 if phase=='drop':
  assert len(items(b))==1 and key==f"dynamic:native-reference:{b['next_dynamic_serial']:016x}"
 else:
  old=next(v for v in b['native_loose_weapons'] if v['reference']==key)
  for name in ['base','model','asset_hash','physical_scale','scene_collision_flags']:assert rig[name]==old[name]
  for name in ['record','node_record']:assert rig['body'][name]==old['body'][name]
else:
 assert len(items(a))==1 and not live and refs[0]['custom_state'].get('count',0)==0
 assert not player_refs or key not in (player_refs[0]['dropped_items'] or [])
 if phase=='pickup':assert not items(b)
 else:assert items(a)==items(b)
assert a['next_dynamic_serial']==b['next_dynamic_serial']+(phase=='drop')
epoch=read(r/'process-epoch.json');events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1 and all(e['pid']==epoch['pid'] for e in loads+saves)
assert read(r/loads[0]['live'])['player']['inventory']==b['player']['inventory']
log=(r/'process.log').read_text();assert 'unidentified standing reference' not in log
if phase=='drop':assert 'TES4 inventory weapon dropped: base='+BASE in log
if (r/'observed-sanitizer-environment.json').exists():
 env=read(r/'observed-sanitizer-environment.json')['environment'];assert env['ASAN_OPTIONS']=='detect_leaks=0:halt_on_error=1' and env['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1'
 if 'sdl-input.so' in env.get('LD_PRELOAD',''):assert 'libasan' in env['LD_PRELOAD'].split(':')[0]
 libs=read(r/'loaded-libraries.json');assert any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
 assert not re.search(r'AddressSanitizer|runtime error:|UndefinedBehaviorSanitizer',log)
result=dict(passed=True,phase=phase,process=epoch,source_sha256=sha(source),save_sha256=sha(target),inventory=items(a),world=refs,physical=rigs,next_dynamic_serial=a['next_dynamic_serial'],credited_deaths=sum(v['phase']==1 and v['killer']==PLAYER for v in a['native_actor_life']))
(r/'worn-axe-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
