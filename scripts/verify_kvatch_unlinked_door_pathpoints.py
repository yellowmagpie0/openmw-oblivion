from pathlib import Path
import sys,json,math,re
sys.path.insert(0,str(Path.cwd()/'scripts'))
import tes4_runtime_state as rt
from verify_kvatch_faction_reaction import inventory,reactions,QUEST
from verify_kvatch_topic_learning import known_topics,read,sha
from verify_kvatch_harrada_animation import health
r=Path(sys.argv[1]);cold='cold' in r.name;player='dynamic:player:0000000000000001';ilend='content:oblivion.esm:098138';cell='content:oblivion.esm:01d0c7';door='content:oblivion.esm:01d126'
d=read(r/'driver-process.json');scenario=read(r/'scenario.json');source=Path(d['input_save']);target=r/'userdata/saves/Bendu_Olo/Quicksave.omwsave'
assert d['captured'] and d['input_unchanged'] and d['manifest_input_unchanged'] and d['exit_code']==scenario['exit_code']==0 and scenario['passed'] and scenario['actions_complete']
assert sha(source)==d['input_sha256'] and sha(r/'captured-input-manifest.json')==d['manifest_input_sha256']
a,b=rt.load_save(source),rt.load_save(target);assert b['schema_version']==106 and rt.encode_payload(b)==rt._find_runtime_record(target.read_bytes())[3]
assert inventory(a)==inventory(b) and reactions(a)==reactions(b) and a['quests']==b['quests'] and known_topics(source)==known_topics(target)
assert next(q for q in b['quests'] if q['quest']==QUEST)['stage']==25 and abs(health(a)-health(b))<.1
targets={'content:oblivion.esm:01d119','content:oblivion.esm:093ee0'}
def door_instances(state):
 return {x['context']:x for x in state['script_instances'] if x['unit'].startswith('content:oblivion.esm:005162@') and x['context'] in targets}
assert a.get('path_points',[])==b.get('path_points',[])
assert set(door_instances(b))==targets
for key,instance in door_instances(b).items():
 assert instance['locals']==[{'type':'number','value':0},{'type':'number','value':1}]
 placed=next(x for x in b['references'] if x['key']==key)
 assert placed['base']=='content:oblivion.esm:0182e8' and placed['cell']==cell and placed['enabled'] and not placed['deleted']
ref=lambda s:next(x for x in s['references'] if x['key']==ilend)
assert ref(a)['inventory']==ref(b)['inventory'] and ref(b)['base']=='content:oblivion.esm:09812f'
assert b['player']['cell']==ref(b)['cell']==cell and ref(b)['enabled'] and not ref(b)['deleted']
for actor in [player,ilend]:assert next(x for x in b['native_actor_life'] if x['actor']==actor)['phase']==0
ai=next(x for x in b['actor_ai'] if x['actor']==ilend)
assert ai['package']=='content:oblivion.esm:1778d1' and ai['target']==ai['companion_group']==player and ai['cell']==ai['last_valid_cell']==cell
assert ai['last_transition_door']==door and math.dist(ai['last_valid_position'][:3],ref(b)['position'][:3])<1
for actor,pos in [(player,b['player']['position']), (ilend,ref(b)['position'])]:
 movement=next(x for x in b['native_movement']['actors'] if x['actor']==actor)
 assert math.dist(movement['position'],pos[:3])<.1 and movement['collision_mode'] and movement['collision_body']
assert math.dist(b['player']['position'][:2],[12.505770683288574,-384.0819396972656])<2
horizontal=math.dist(b['player']['position'][:2],ref(b)['position'][:2]);assert 60<horizontal<400 and abs(b['player']['position'][2]-ref(b)['position'][2])<64
log=(r/'process.log').read_text();assert 'RendingHall follower clearance verified' in log
noops=re.findall(r'disablelinkedpathpoints object=content:oblivion.esm:(01d119|093ee0) changed=false',log)
assert sorted(noops)==([] if cold else ['01d119','093ee0'])
ai_events=[json.loads(l) for l in (r/'ai-events.jsonl').read_text().splitlines()]
transitions=[e for e in ai_events if e.get('actor')==ilend and e['event']=='door-transition' and 'low-process-state-transition' in e['reason']]
clearances=[e for e in ai_events if e.get('actor')==ilend and e['event']=='door-arrival-clearance']
assert len(transitions)==len(clearances)==(0 if cold else 1)
# Retain only the source fields needed below before parsing the large live snapshots.
source_doors=door_instances(a);source_points=a.get('path_points',[])
source_ilend_position=ref(a)['position'][:3];source_player_position=a['player']['position'][:3];source_health=health(a)
del a
epoch=read(r/'process-epoch.json');events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];loads=[e for e in events if e['event']=='load-complete'];saves=[e for e in events if e['event']=='save-complete'];assert len(loads)==len(saves)==1
for e in loads+saves:assert e['pid']==epoch['pid'] and sha(r/e['live'])==e['live_sha256']
assert loads[0]['save_sha256']==sha(source) and saves[0]['save_sha256']==sha(target) and read(r/saves[0]['live'])==b
if cold:
 loaded=read(r/loads[0]['live']);assert math.dist(loaded['player']['position'][:3],source_player_position)<.1;assert door_instances(loaded)==source_doors and loaded.get('path_points',[])==source_points; assert math.dist(ref(loaded)['position'][:3],source_ilend_position)<.1 and ref(loaded)['cell']==cell
 assert math.dist(source_ilend_position,ref(b)['position'][:3])<1 and abs(health(loaded)-source_health)<.001
 prior=read(source.parents[3]/'process-epoch.json');assert (epoch['pid'],epoch['start_ticks'],epoch['boot_id'])!=(prior['pid'],prior['start_ticks'],prior['boot_id'])
san=(r/'observed-sanitizer-environment.json').exists()
if san:
 env=read(r/'observed-sanitizer-environment.json')['environment'];libs=read(r/'loaded-libraries.json')
 assert 'halt_on_error=1' in env['ASAN_OPTIONS'] and 'halt_on_error=1' in env['UBSAN_OPTIONS'] and any('libasan' in p for p in libs) and any('libubsan' in p for p in libs)
result=dict(passed=True,cold=cold,sanitized=san,input_sha256=sha(source),save_sha256=sha(target),binary_sha256=epoch['exe_sha256'],player_health=health(b),player_position=b['player']['position'],ilend_position=ref(b)['position'],horizontal_separation=horizontal,door_transitions=len(transitions),arrival_clearances=len(clearances),door_initializer_states={k:[v['value'] for v in row['locals']] for k,row in door_instances(b).items()},path_points_unchanged=True,missing_link_noops=len(noops),scope='Actual original Rending Halls entry, two unlinked original door initializers and genuine own-save continuation. No full Hall/tower/gate completion.')
(r/'pathpoint-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
